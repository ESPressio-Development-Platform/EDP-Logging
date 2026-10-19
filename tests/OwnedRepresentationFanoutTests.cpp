#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"
#include <logging/OwnedRepresentationFanout.hpp>
#include <logging/EdlgFrameReader.hpp>
#include <serial/BoundedEdlgFrameQueue.hpp>
#include <type_traits>

namespace {
    namespace L = ESPressio::Logging;
    namespace M = ESPressio::Memory;
    namespace P = ESPressio::Platform;
    using Buffer = L::OwnedEncodedPayload<32U>;
    using Top = M::MemoryTopology<
        Resource,M::SharedReserve<0U,Resource>,
        M::ObjectPoolSpec<Buffer,M::DedicatedInstances<1U>>
    >;
    using Buffers = M::MemoryRuntime<
        Top,MemoryComposition,TestSupport::TestMutexProvider,
        TestSupport::TestSignalProvider
    >;

    struct Source final {std::uint8_t Code;};

    struct Encoder final {
        int Calls{};
        std::size_t DeclaredSize{3U};
        L::RepresentationEncodeStatus Force{L::RepresentationEncodeStatus::Succeeded};
        L::RepresentationEncodeResult operator()(
            const Source& source,std::span<std::byte> output
        ) noexcept {
            ++Calls;
            output[0] = std::byte{0x83U};
            output[1] = std::byte{source.Code};
            output[2] = std::byte{0x01U};
            return {Force,DeclaredSize};
        }
    };

    struct CopyingSink final {
        std::array<std::byte,32U> Copy{};
        std::size_t Used{};
        int Calls{};
        bool Reject{};
        L::EdlgCodec LastCodec{L::EdlgCodec::Json};
        L::RecordSinkSubmission operator()(
            L::EdlgCodec codec,std::span<const std::byte> bytes
        ) noexcept {
            ++Calls; LastCodec=codec;
            if (Reject) return L::RecordSinkSubmission::Rejected;
            for(std::size_t i=0U;i<bytes.size();++i) Copy[i]=bytes[i];
            Used=bytes.size();
            return L::RecordSinkSubmission::Accepted;
        }
    };

    void TestRealSerialFrameOwnership() {
        namespace S=ESPressio::Serial;
        namespace T=ESPressio::Threading;
        struct SerialIdentity final {};
        using SerialMutex=T::OrdinaryMutexProvider<
            SerialIdentity,TestSupport::TestMutexProvider
        >;
        SerialMutex mutex;
        S::BoundedEdlgFrameQueue<2U,32U,SerialMutex> serial(mutex);
        struct Endpoint final : S::Framework::Provider<
            S::Domain,S::Framework::Offers<
                S::Framework::Offer<S::ByteStreamWriter>
            >
        > {
            std::array<std::byte,160U> Stored{};
            std::size_t Used{0U};
            S::ByteWriteResult TryWrite(
                std::span<const std::byte> source
            ) noexcept {
                assert(Used+source.size()<=Stored.size());
                for(const auto item:source) Stored[Used++]=item;
                return {S::ByteWriteStatus::Accepted,source.size()};
            }
        } endpoint;
        static_assert(S::ByteWriteEndpoint<Endpoint>);

        Resource resource;
        TestSupport::TestMutexProvider poolMutex;
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator;
        Buffers runtime(poolMutex,allocator,resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool=runtime.ObjectPoolFor<Buffer>();
        using Lease=std::remove_reference_t<decltype(pool)>::LeaseType;
        Lease lease;
        Encoder encoder;
        CopyingSink archive;
        auto serialSink=[&](L::EdlgCodec codec,
                            std::span<const std::byte> payload) noexcept
                -> L::RecordSinkSubmission {
            const auto offered=serial.TryEnqueue(codec,payload);
            return offered.Status==S::EdlgQueueOfferStatus::Queued
                ? L::RecordSinkSubmission::Accepted
                : offered.FrameWasQueued ? L::RecordSinkSubmission::Partial
                                        : L::RecordSinkSubmission::Rejected;
        };
        Source source{0x21U};
        for(std::uint8_t i=0U;i<3U;++i) {
            source.Code=static_cast<std::uint8_t>(0x21U+i);
            const auto result=L::EncodeOnceAndSubmitToSinks<
                L::EdlgCodec::Cbor,32U>(
                pool,lease,source,encoder,serialSink,archive
            );
            assert(lease.IsEmpty());
            assert(result.EncoderInvocations==1U);
            assert(result.SinkInvocations==2U);
            assert(archive.Copy[1]==std::byte{source.Code});
            if(i<2U) {
                assert(result.Status==L::OwnedRepresentationStatus::SubmittedAndReleased);
                assert(result.Accepted==2U);
            } else {
                assert(result.Status==L::OwnedRepresentationStatus::SubmissionNotAccepted);
                assert(result.Accepted==1U && serial.Depth()==2U);
                assert(result.Submissions[0]==L::RecordSinkSubmission::Rejected);
            }
        }
        source.Code=0x99U; // original producer no longer owns accepted serial bytes
        for(std::uint8_t i=0U;i<2U;++i) {
            const auto pumped=serial.PumpNext(endpoint);
            assert(pumped.Status==S::EdlgQueuePumpStatus::Complete);
            assert(pumped.FrameCompleted);
        }
        assert(serial.Depth()==0U);
        assert(serial.PumpNext(endpoint).Status==S::EdlgQueuePumpStatus::Empty);
        std::size_t offset=0U;
        for(std::uint8_t i=0U;i<2U;++i) {
            L::EdlgFrameView view{};
            assert(L::ReadEdlgFrame(
                std::span<const std::byte>(endpoint.Stored).subspan(offset),
                view,32U
            )==L::EdlgFrameReadResult::Succeeded);
            assert(view.Codec==L::EdlgCodec::Cbor);
            assert(view.Payload.size()==3U);
            assert(view.Payload[1]==std::byte{static_cast<std::uint8_t>(0x21U+i)});
            offset+=view.Consumed;
        }
        assert(offset==endpoint.Used);
        assert(encoder.Calls==3);
        assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
    }

    void TestMemoryOwnedRepresentation() {
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator;
        Buffers runtime(mutex,allocator,resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool=runtime.ObjectPoolFor<Buffer>();
        using Lease = std::remove_reference_t<decltype(pool)>::LeaseType;
        Lease lease;
        Encoder encoder;
        CopyingSink serial,archive;
        Source source{0x52U};
        auto submit=[&] {
            return L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Cbor,32U>(
                pool,lease,source,encoder,serial,archive
            );
        };
        auto first=submit();
        assert(first.Status==L::OwnedRepresentationStatus::SubmittedAndReleased);
        assert(first.EncoderInvocations==1U && encoder.Calls==1);
        assert(first.SinkInvocations==2U && first.Accepted==2U);
        assert(first.EncodedBytes==3U && lease.IsEmpty());
        assert(first.Release==M::ObjectPoolLeaseReleaseResult::Released);
        assert(serial.Used==3U && archive.Used==3U);
        assert(serial.LastCodec==L::EdlgCodec::Cbor);
        assert(serial.Copy[1]==std::byte{0x52U});
        source.Code=0x53U; // sink copies cannot alias original source
        assert(serial.Copy[1]==std::byte{0x52U});

        archive.Reject=true;
        auto partial=submit();
        assert(partial.Status==L::OwnedRepresentationStatus::SubmissionNotAccepted);
        assert(partial.SinkInvocations==2U && partial.Accepted==1U);
        assert(partial.Submissions[0]==L::RecordSinkSubmission::Accepted);
        assert(partial.Submissions[1]==L::RecordSinkSubmission::Rejected);
        assert(serial.Copy[1]==std::byte{0x53U});
        assert(archive.Copy[1]==std::byte{0x52U});
        assert(lease.IsEmpty() && encoder.Calls==2);

        const auto acquired=pool.Acquire(
            lease,P::Synchronization::WaitTimeout::NoWait()
        );
        assert(acquired==M::ObjectPoolAcquisitionResult::Succeeded);
        const auto occupied=submit();
        assert(occupied.Status==L::OwnedRepresentationStatus::OutputLeaseOccupied);
        assert(occupied.EncoderInvocations==0U && encoder.Calls==2);
        assert(!lease.IsEmpty());
        // A distinct lease cannot overcommit the one-slot pool.
        Lease another;
        const auto exhausted=L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Cbor,32U>(
            pool,another,source,encoder,serial,archive
        );
        assert(exhausted.Status==L::OwnedRepresentationStatus::PoolCapacityUnavailable);
        assert(another.IsEmpty());
        assert(lease.Release()==M::ObjectPoolLeaseReleaseResult::Released);

        encoder.DeclaredSize=33U; // malformed owner encoder must never reach sinks
        const auto beforeSerial=serial.Calls;
        const auto beforeArchive=archive.Calls;
        auto invalid=submit();
        assert(invalid.Status==L::OwnedRepresentationStatus::EncoderLengthInvalid);
        assert(invalid.SinkInvocations==0U && encoder.Calls==3);
        assert(serial.Calls==beforeSerial && archive.Calls==beforeArchive);
        assert(lease.IsEmpty());

        encoder.DeclaredSize=3U;
        encoder.Force=L::RepresentationEncodeStatus::Rejected;
        invalid=submit();
        assert(invalid.Status==L::OwnedRepresentationStatus::EncoderRejected);
        assert(invalid.SinkInvocations==0U && lease.IsEmpty());

        archive.Reject=false;
        encoder.Force=L::RepresentationEncodeStatus::Succeeded;
        const auto json=L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Json,32U>(
            pool,lease,source,encoder,serial,archive
        );
        assert(json.Status==L::OwnedRepresentationStatus::SubmittedAndReleased);
        assert(serial.LastCodec==L::EdlgCodec::Json);
        assert(encoder.Calls==5);
        assert(lease.IsEmpty());
        assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
    }

    // The single-sink configuration is valid: Serial-only, Archive-only,
    // or one selected representation must not require a dummy second sink.
    void TestSingleSinkComposition() {
        Resource resource;
        TestSupport::TestMutexProvider mutex;
        MemoryComposition::Select<
            M::SharedReserveAllocationRequirement,
            ESPressio::System::CompositionFramework::SelectUnique
        > allocator;
        Buffers runtime(mutex, allocator, resource);
        M::MemoryTopologyInitializationFailure failure{};
        assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
        auto& pool=runtime.ObjectPoolFor<Buffer>();
        using Lease=std::remove_reference_t<decltype(pool)>::LeaseType;
        Lease lease;
        Encoder encoder;
        CopyingSink soleSink;
        Source source{0x54U};

        const auto accepted=L::EncodeOnceAndSubmitToSinks<
            L::EdlgCodec::Cbor,32U
        >(pool,lease,source,encoder,soleSink);
        static_assert(std::tuple_size_v<decltype(accepted.Submissions)> == 1U);
        assert(accepted.Status==L::OwnedRepresentationStatus::SubmittedAndReleased);
        assert(accepted.EncoderInvocations==1U && encoder.Calls==1);
        assert(accepted.SinkInvocations==1U && accepted.Accepted==1U);
        assert(accepted.Submissions[0]==L::RecordSinkSubmission::Accepted);
        assert(accepted.Release==M::ObjectPoolLeaseReleaseResult::Released);
        assert(lease.IsEmpty() && soleSink.Calls==1 && soleSink.Used==3U);
        assert(soleSink.Copy[1]==std::byte{0x54U});

        // The caller can mutate its input immediately after the single
        // copying sink accepted; no use-after-release or alias is allowed.
        source.Code=0x55U;
        assert(soleSink.Copy[1]==std::byte{0x54U});
        soleSink.Reject=true;
        const auto rejected=L::EncodeOnceAndSubmitToSinks<
            L::EdlgCodec::Cbor,32U
        >(pool,lease,source,encoder,soleSink);
        assert(rejected.Status==L::OwnedRepresentationStatus::SubmissionNotAccepted);
        assert(rejected.EncoderInvocations==1U && rejected.SinkInvocations==1U);
        assert(rejected.Accepted==0U);
        assert(rejected.Submissions[0]==L::RecordSinkSubmission::Rejected);
        assert(rejected.Release==M::ObjectPoolLeaseReleaseResult::Released);
        assert(lease.IsEmpty() && soleSink.Calls==2 && encoder.Calls==2);
        assert(soleSink.Copy[1]==std::byte{0x54U});

        // An encoder failure must never reach even the sole sink.
        encoder.Force=L::RepresentationEncodeStatus::ProviderFailure;
        const auto failed=L::EncodeOnceAndSubmitToSinks<
            L::EdlgCodec::Json,32U
        >(pool,lease,source,encoder,soleSink);
        assert(failed.Status==L::OwnedRepresentationStatus::EncoderProviderFailure);
        assert(failed.EncoderInvocations==1U && failed.SinkInvocations==0U);
        assert(failed.Accepted==0U && soleSink.Calls==2 && lease.IsEmpty());

        // Even with just one sink, a pre-held buffer prevents duplicate
        // pool ownership; no new encoding or sink call can take place.
        assert(pool.Acquire(lease,P::Synchronization::WaitTimeout::NoWait())==
            M::ObjectPoolAcquisitionResult::Succeeded);
        Lease second;
        const auto exhausted=L::EncodeOnceAndSubmitToSinks<
            L::EdlgCodec::Cbor,32U
        >(pool,second,source,encoder,soleSink);
        assert(exhausted.Status==L::OwnedRepresentationStatus::PoolCapacityUnavailable);
        assert(exhausted.EncoderInvocations==0U && exhausted.SinkInvocations==0U);
        assert(second.IsEmpty() && !lease.IsEmpty());
        assert(lease.Release()==M::ObjectPoolLeaseReleaseResult::Released);
        assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
    }
}
int main(){TestMemoryOwnedRepresentation();TestRealSerialFrameOwnership();TestSingleSinkComposition();}
