#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <ESPressio_Memory.hpp>
#include <logging/OwnedRepresentationFanout.hpp>
#include <logging/EdlgFrameReader.hpp>
#include <serial/BoundedEdlgFrameQueue.hpp>
#include <GeneratedLocalisationContract.hpp>
#include <logging/GeneratedLogRecordWireProjection.hpp>

namespace Test {
/// G4→G10 real owner-composed pipeline: source projection -> EDP-Memory
/// leased representation -> independently owned Serial FIFO + copying
/// archive-style test sink -> later EDLG CRC + full G4 decode.
/// The archive callback is ONLY in-memory copying, not durable storage.
template<class TResource,class TMutex,class TSignal,class TAllocator,
         class TComposition,class TSerialMutex,class TSerialStateSpinLock>
bool CheckG4OwnedRootFanoutOnDevice() noexcept {
    namespace M=ESPressio::Memory;
    namespace L=ESPressio::Logging;
    namespace C=ESPressio::Clock;
    namespace S=ESPressio::System;
    namespace Sr=ESPressio::Serialisation;
    namespace F=ESPressio::Serial;

    using Schema=Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,5U>;
    using Root=L::GeneratedLogRecordWireProjection<
        L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>;
    using Source=L::BasicLogRecord<L::AttemptSequenceWidth::Bits16,2U,12U>;
    constexpr std::size_t Capacity=1024U;
    static_assert(Root::Fields::Count==15U);
    S::Identity::DeviceIdentifier::Storage rawDevice{};
    rawDevice[0]=0xA5U;
    const S::Identity::DeviceIdentifier device{rawDevice};
    const L::QualifiedLogTime captured{
        C::MonotonicTimestamp::FromNanoseconds(998877U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    Source source{};
    const auto inc=S::Identity::RuntimeIncarnationId{0x01020304U};
    const auto era=C::ClockEra::FromValue(0x0102030405060708ULL);
    if(L::AssembleLogRecordSource<Schema>(
        source,L::Severity::Warning,device,
        S::Identity::RuntimeIncarnationId{7U},
        L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::FromValidatedValue(27U),
        captured,inc,era)!=L::LogRecordAssemblyStatus::Prepared) return false;
    Root projection{};
    L::OwnedPositionalScratch<S::Identity::RuntimeIncarnationId,C::ClockEra> scratch{};
    if(!L::ProjectGeneratedLogRecord<
        Schema,L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>(
        source,projection,scratch).IsSuccessful()) return false;

    using Encoded=L::OwnedEncodedPayload<Capacity>;
    using Topology=M::MemoryTopology<
        TResource,M::SharedReserve<0U,TResource>,
        M::ObjectPoolSpec<Encoded,M::DedicatedInstances<1U>>>;
    using Runtime=M::MemoryRuntime<Topology,TComposition,TMutex,TSignal>;
    TResource resource;
    TMutex mutex;
    TAllocator allocator;
    Runtime runtime(mutex,allocator,resource);
    M::MemoryTopologyInitializationFailure failure{};
    if(runtime.Initialize(failure)!=M::MemoryTopologyInitializationResult::Succeeded)
        return false;
    auto& pool=runtime.template ObjectPoolFor<Encoded>();
    typename std::remove_reference_t<decltype(pool)>::LeaseType lease;

    TSerialMutex serialMutex;
    TSerialStateSpinLock serialStateSpinLock;
    F::BoundedEdlgFrameQueue<
        1U,
        Capacity,
        TSerialMutex,
        TSerialStateSpinLock
    > serial(
        serialMutex,
        serialStateSpinLock
    );
    struct ArchiveCopy final {
        std::array<std::byte,Capacity> Bytes{};
        std::size_t Used{0U};
        unsigned Calls{0U};
        L::RecordSinkSubmission operator()(
            L::EdlgCodec, std::span<const std::byte> payload
        ) noexcept {
            ++Calls;
            if(payload.size()>Bytes.size())
                return L::RecordSinkSubmission::Rejected;
            Used=payload.size();
            for(std::size_t i=0U;i<Used;++i) Bytes[i]=payload[i];
            return L::RecordSinkSubmission::Accepted;
        }
    } archive;
    struct Endpoint final : F::Framework::Provider<
        F::Domain,F::Framework::Offers<
            F::Framework::Offer<F::ByteStreamWriter>>> {
        std::array<std::byte,Capacity+96U> Bytes{};
        std::size_t Used{0U};
        F::ByteWriteResult TryWrite(
            std::span<const std::byte> input
        ) noexcept {
            if(input.size()>Bytes.size()-Used)
                return {F::ByteWriteStatus::ProviderFailure,0U};
            for(auto b:input) Bytes[Used++]=b;
            return {F::ByteWriteStatus::Accepted,input.size()};
        }
    } endpoint;
    auto serialSink=[&serial](L::EdlgCodec codec,std::span<const std::byte> bytes)
        noexcept -> L::RecordSinkSubmission {
        const auto offered=serial.TryEnqueue(codec,bytes);
        return offered.Status==F::EdlgQueueOfferStatus::Queued
            ? L::RecordSinkSubmission::Accepted
            : offered.FrameWasQueued
                ? L::RecordSinkSubmission::Partial
                : L::RecordSinkSubmission::Rejected;
    };
    unsigned cborCalls=0U;
    auto cborEncoder=[&cborCalls](const Root& root,std::span<std::byte> output)
        noexcept -> L::RepresentationEncodeResult {
        ++cborCalls;
        const auto result=Sr::Serialise<Sr::Cbor,Sr::RootProfile::TypedEnvelope>(
            root,output);
        return {result.IsSuccessful()
            ? L::RepresentationEncodeStatus::Succeeded
            : L::RepresentationEncodeStatus::Rejected,result.BytesWritten};
    };
    unsigned jsonCalls=0U;
    auto jsonEncoder=[&jsonCalls](const Root& root,std::span<std::byte> output)
        noexcept -> L::RepresentationEncodeResult {
        ++jsonCalls;
        const auto result=Sr::Serialise<Sr::Json,Sr::RootProfile::TypedEnvelope>(
            root,output);
        return {result.IsSuccessful()
            ? L::RepresentationEncodeStatus::Succeeded
            : L::RepresentationEncodeStatus::Rejected,result.BytesWritten};
    };
    // Exercise both representations against actual queued independent copies.
    for(unsigned pass=0U;pass<2U;++pass) {
        const auto codec=pass==0U?L::EdlgCodec::Cbor:L::EdlgCodec::Json;
        const auto outcome=pass==0U
            ? L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Cbor,Capacity>(
                pool,lease,projection,cborEncoder,serialSink,archive)
            : L::EncodeOnceAndSubmitToSinks<L::EdlgCodec::Json,Capacity>(
                pool,lease,projection,jsonEncoder,serialSink,archive);
        if(outcome.Status!=L::OwnedRepresentationStatus::SubmittedAndReleased ||
           outcome.EncoderInvocations!=1U || outcome.SinkInvocations!=2U ||
           outcome.Accepted!=2U || outcome.EncodedBytes==0U ||
           outcome.EncodedBytes!=archive.Used || !lease.IsEmpty() ||
           serial.Depth()!=1U) return false;
        endpoint.Used=0U;
        const auto pumped=serial.PumpNext(endpoint);
        if(pumped.Status!=F::EdlgQueuePumpStatus::Complete ||
           !pumped.FrameCompleted || serial.Depth()!=0U) return false;
        L::EdlgFrameView frame{};
        if(L::ReadEdlgFrame(
            std::span<const std::byte>(endpoint.Bytes.data(),endpoint.Used),
            frame,Capacity)!=L::EdlgFrameReadResult::Succeeded ||
           frame.Codec!=codec || frame.Payload.size()!=archive.Used)
            return false;
        for(std::size_t i=0U;i<archive.Used;++i)
            if(frame.Payload[i]!=archive.Bytes[i]) return false;
        Root decoded{};
        const auto parsed=pass==0U
            ? Sr::Deserialise<Sr::Cbor,Sr::RootProfile::TypedEnvelope>(
                frame.Payload,decoded)
            : Sr::Deserialise<Sr::Json,Sr::RootProfile::TypedEnvelope>(
                frame.Payload,decoded);
        if(!parsed.IsSuccessful() ||
           decoded.Arguments!=projection.Arguments ||
           decoded.Attempt.Value()!=projection.Attempt.Value() ||
           decoded.DiagnosticSchemaFingerprint!=
             projection.DiagnosticSchemaFingerprint)
            return false;
    }
    return cborCalls==1U && jsonCalls==1U && archive.Calls==2U &&
        lease.IsEmpty() &&
        runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded;
}
} // namespace Test
