#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>
#include <unistd.h>

#include "StrictAckKeyValueFake.hpp"
#include <VfsFileStorage.hpp>
#include <logging/BoundedEdlsBatch.hpp>
#include <memory/ByteOperationsProvider.hpp>

namespace {


    namespace L = ESPressio::Logging;
    namespace P = ESPressio::Persistence;
    namespace C = ESPressio::Clock;
    namespace F = ESPressio::System::CompositionFramework;
    using Bytes = ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
    using Ledger = L::StrictAckTests::TestAckKeyValueStorage;

    /// Deterministic canonical Clock provider for exact deadline boundaries.
    struct TestClock final : F::Provider<C::Domain, F::Offers<F::Offer<C::MonotonicClock>>> {

        /// Canonical nanosecond coordinate controlled by the host fixture.
        std::uint64_t Tick{0U};

        /// Samples the same Clock-domain coordinate used by the batching owner.
        C::MonotonicTimestamp Now() const noexcept {
            return C::MonotonicTimestamp::FromNanoseconds(Tick);
        }
    };

    /// Distinct host VFS binding for the existing concrete file-operation owner.
    struct Binding final {};

    using Profile = P::EspIdf::VfsBindingProfile<
        P::RetentionLevel::Restart,
        P::TextCaseSensitivity::CaseSensitive,
        P::MediaRemovability::Fixed,
        64U,
        64U,
        4096U>;
    using Files = P::EspIdf::VfsFileStorage<Binding, Profile, Bytes>;
    using Archive = L::PowerLossAcknowledgedEdls<3U, 2U, 32U, 256U, Files, Ledger>;
    using Batch = L::BoundedEdlsBatch<4U, 128U, Archive, TestClock, Bytes>;
    constexpr auto P0 = P::FilePathView::Validate("slot0");
    constexpr auto P1 = P::FilePathView::Validate("slot1");
    constexpr auto P2 = P::FilePathView::Validate("slot2");
    constexpr auto Key = P::KeyView::Validate("a");
    constexpr std::array<P::FilePathView, 3U> Paths{P0.Value, P1.Value, P2.Value};

    /// Actual temporary filesystem, real VFS owner and explicit fake NVS ledger.
    struct Fixture final {

        /// Private native directory for this independent lifecycle.
        char Directory[64U] = "/tmp/edls-sealed-batch-XXXXXX";

        /// Portable Memory provider shared by the native VFS concrete.
        Bytes ByteOperations{};

        /// Existing VFS operation implementation; no file mock.
        Files Storage;

        /// Fake of the existing independently PowerLoss-atomic KeyValue contract.
        Ledger Watermark{};

        /// External whole-segment scratch; not a nested worker-stack allocation.
        std::array<std::byte, 256U> Scratch{};

        /// One independently CRC-framed canonical CBOR [1,2].
        std::array<std::byte, 23U> Frame{};

        /// Exact canonical Clock used by admission and worker service.
        TestClock Clock{};

        /// Provisions a new empty filesystem and genesis ledger explicitly.
        Fixture()
            : Storage(
                ::mkdtemp(Directory),
                ByteOperations
            ) {}

        /// Removes only this test's native files after all owners have stopped.
        ~Fixture() {
            for (std::size_t index = 0U; index < Paths.size(); ++index) {
                char path[128U]{};
                assert(std::snprintf(
                    path,
                    sizeof(path),
                    "%s/slot%zu",
                    Directory,
                    index
                ) > 0);
                assert(::unlink(path) == 0 || errno == ENOENT);
            }
            assert(::rmdir(Directory) == 0);
        }
    };

    /// Builds one canonical test identity without a new runtime identity scheme.
    std::array<std::byte, 16U> MakeIdentity() {
        std::array<std::byte, 16U> identity{};
        identity.fill(std::byte{0xA6U});
        return identity;
    }

    /// Emits canonical EDLG framing using only the production owner.
    void MakeFrame(std::array<std::byte, 23U>& frame) {
        const std::array<std::byte, 3U> payload{
            std::byte{0x82U},
            std::byte{0x01U},
            std::byte{0x02U}};
        std::size_t used = 0U;
        assert(L::WriteEdlgFrame(
            L::EdlgCodec::Cbor,
            payload,
            frame,
            used,
            32U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
        ) == L::EdlgFrameWriteResult::Succeeded);
        assert(used == frame.size());
    }

    /// Reopens actual file bytes for independent sealed-count/CRC verification.
    void CheckWindow(Fixture& fixture, std::uint64_t newest) {
        Files reopened(
            fixture.Directory,
            fixture.ByteOperations
        );
        const auto archive = L::InspectEdlsArchive<3U, 32U>(
            reopened,
            Paths
        );
        assert(archive.Condition == L::EdlsArchiveCondition::Unambiguous);
        std::size_t found = 0U;
        for (const auto& slot : archive.Slots) {
            if (slot.Condition != L::EdlsSlotCondition::Clean) {
                continue;
            }
            ++found;
            assert(slot.Header.Profile == L::EdlsContentProfile::SealedFrames);
            assert(slot.VerifiedFrames > 0U && slot.VerifiedFrames <= 4U);
            assert(slot.Header.Generation <= newest);
            assert(slot.Header.Generation >= (newest > 1U ? newest - 1U : 1U));
        }
        assert(found == (newest == 1U ? 1U : 2U));
    }

    /// Configurable dwell, no premature ACK, exact deadline and finite rolls.
    void TimingAndRetention() {
        Fixture f{};
        MakeFrame(f.Frame);
        const auto identity = MakeIdentity();
        Archive owner(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U
        );
        assert(owner.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        Batch batch(
            owner,
            f.Clock,
            f.Scratch
        );
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::NotConfigured);
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(-1)) ==
               L::EdlsBatchConfigurationStatus::InvalidDwell);
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000001)) ==
               L::EdlsBatchConfigurationStatus::InvalidDwell);
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        assert(!batch.NextFlushDeadline().has_value());
        f.Clock.Tick = 10U;
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(batch.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::PendingBatch);
        f.Clock.Tick = 99999999U;
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(batch.NextFlushDeadline()->Nanoseconds() == 100000010U);
        assert(batch.Service().Status == L::EdlsBatchStatus::Waiting);
        assert(owner.Read().AcknowledgedGeneration == 0U);
        f.Clock.Tick = 100000010U;
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::FlushRequired);
        const auto first = batch.Service();
        assert(first.Status == L::EdlsBatchStatus::DurablyAcknowledged);
        assert(first.AcknowledgedRecords == 2U && first.PendingRecords == 0U);
        assert(first.Archive.AcknowledgedGeneration == 1U);
        assert(owner.Read().AcknowledgedGeneration == 1U);
        CheckWindow(
            f,
            1U
        );

        // Zero dwell is immediate but still never ACKs admission into RAM.
        assert(batch.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::Configured);
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(owner.Read().AcknowledgedGeneration == 1U);
        assert(batch.Service().AcknowledgedRecords == 1U);
        CheckWindow(
            f,
            2U
        );

        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        for (std::uint32_t generation = 3U; generation <= 8U; ++generation) {
            for (std::size_t record = 0U; record < 4U; ++record) {
                assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
            }
            assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::FlushRequired);
            const auto committed = batch.Service();
            assert(committed.AcknowledgedRecords == 4U);
            assert(committed.Archive.AcknowledgedGeneration == generation);
            CheckWindow(
                f,
                generation
            );
        }
        assert(batch.Flush().Status == L::EdlsBatchStatus::Empty);
        assert(f.Watermark.stores == 9U); // genesis + eight SEGMENTS, not 27 records

        // Existing strict single-frame call does not silently accept two frames.
        std::array<std::byte, 46U> two{};
        for (std::size_t i = 0U; i < f.Frame.size(); ++i) {
            two[i] = f.Frame[i];
            two[i + f.Frame.size()] = f.Frame[i];
        }
        const auto before = owner.Read().AcknowledgedGeneration;
        assert(owner.Publish(
            two,
            f.Scratch
        )
                   .Status ==
               L::EdlsAcknowledgedStatus::PublishedNotAcknowledged);
        assert(owner.Read().AcknowledgedGeneration == before);

        // Truncate a previously ACKed batch exactly at an EDLG boundary:
        // remaining complete frames MUST NOT masquerade as complete history.
        const auto inspected = L::InspectEdlsArchive<3U, 32U>(
            f.Storage,
            Paths
        );
        const auto slot = inspected.NewestSlot;
        std::array<std::byte, 256U> saved{};
        const auto read = f.Storage.ReadFileAt(
            Paths[slot],
            P::StorageOffset{},
            P::DestinationBufferView{saved.data(), saved.size()}
        );
        assert(read.Status == P::FileReadStatus::Succeeded);
        const auto prefix = L::EdlsHeaderBytes + 3U * f.Frame.size();
        assert(f.Storage.ReplaceFile(
            Paths[slot],
            P::SourceBufferView{saved.data(), prefix}
        ) == P::FileReplaceStatus::Succeeded);
        assert(L::InspectEdlsSlot<32U>(
            f.Storage,
            Paths[slot],
            static_cast<std::uint16_t>(slot)
        )
                   .Condition == L::EdlsSlotCondition::DamagedTail);
        assert(owner.Recover(f.Scratch).Status == L::EdlsAcknowledgedStatus::ArchiveAmbiguous);
        assert(owner.Read().AcknowledgedGeneration == 8U);
        assert(f.Storage.ReplaceFile(
            Paths[slot],
            P::SourceBufferView{saved.data(), read.BytesTransferred}
        ) == P::FileReplaceStatus::Succeeded);
        assert(owner.Recover(f.Scratch).Status == L::EdlsAcknowledgedStatus::Ready);

        // A wrong sealed count is rejected even when its complete CRC is recomputed.
        const auto sealOffset = read.BytesTransferred - L::EdlsSealBytes;
        L::Detail::WriteEdlsLe(
            saved,
            sealOffset + 4U,
            3U,
            4U
        );
        const auto crc = L::Detail::EdlgFrameCrc32C(
            std::span<const std::byte>(saved).first(read.BytesTransferred - 4U));
        L::Detail::WriteEdlsLe(
            saved,
            read.BytesTransferred - 4U,
            crc,
            4U
        );
        assert(f.Storage.ReplaceFile(
            Paths[slot],
            P::SourceBufferView{saved.data(), read.BytesTransferred}
        ) == P::FileReplaceStatus::Succeeded);
        assert(owner.Recover(f.Scratch).Status == L::EdlsAcknowledgedStatus::ArchiveAmbiguous);
    }

    /// Maximum supported count and partial deadline share the same real VFS/NVS transaction.
    void SixtyFourRecordConfiguration() {
        Fixture f{};
        MakeFrame(f.Frame);
        const auto identity = MakeIdentity();
        using LargeArchive = L::PowerLossAcknowledgedEdls<
            3U,
            2U,
            32U,
            2104U,
            Files,
            Ledger>;
        using LargeBatch = L::BoundedEdlsBatch<
            64U,
            2048U,
            LargeArchive,
            TestClock,
            Bytes>;
        std::array<std::byte, 2104U> scratch{};
        LargeArchive archive(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U
        );
        assert(archive.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        LargeBatch batch(
            archive,
            f.Clock,
            scratch
        );
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        for (std::uint64_t generation = 1U; generation <= 4U; ++generation) {
            for (std::size_t record = 0U; record < 64U; ++record) {
                assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
            }
            assert(archive.Read().AcknowledgedGeneration == generation - 1U);
            assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::FlushRequired);
            const auto committed = batch.Service();
            assert(committed.Status == L::EdlsBatchStatus::DurablyAcknowledged);
            assert(committed.AcknowledgedRecords == 64U);
            assert(committed.Archive.AcknowledgedGeneration == generation);
            Files reopened(
                f.Directory,
                f.ByteOperations
            );
            const auto actual = L::InspectEdlsArchiveWithScratch<3U, 32U>(
                reopened,
                Paths,
                scratch
            );
            assert(actual.Condition == L::EdlsArchiveCondition::Unambiguous);
            std::size_t retained = 0U;
            for (const auto& slot : actual.Slots) {
                if (slot.Condition == L::EdlsSlotCondition::Empty) {
                    continue;
                }
                assert(slot.Condition == L::EdlsSlotCondition::Clean);
                assert(slot.Header.Profile == L::EdlsContentProfile::SealedFrames);
                assert(slot.VerifiedFrames == 64U);
                assert(slot.Header.Generation <= generation);
                assert(slot.Header.Generation >= (generation > 1U ? generation - 1U : 1U));
                ++retained;
            }
            assert(retained == (generation == 1U ? 1U : 2U));
        }
        assert(batch.ConfigureDwell(C::Duration::FromNanoseconds(50000000)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        f.Clock.Tick = 25000000U;
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        f.Clock.Tick = 26000000U;
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(batch.NextFlushDeadline()->Nanoseconds() == 75000000U);
        f.Clock.Tick = 74999999U;
        assert(batch.Service().Status == L::EdlsBatchStatus::Waiting);
        assert(archive.Read().AcknowledgedGeneration == 4U);
        f.Clock.Tick = 75000000U;
        const auto partial = batch.Service();
        assert(partial.AcknowledgedRecords == 2U);
        assert(partial.Archive.AcknowledgedGeneration == 5U);
        const auto actual = L::InspectEdlsArchiveWithScratch<3U, 32U>(
            f.Storage,
            Paths,
            scratch
        );
        std::size_t records = 0U;
        for (const auto& slot : actual.Slots) {
            if (slot.Condition == L::EdlsSlotCondition::Empty) {
                continue;
            }
            assert(slot.Condition == L::EdlsSlotCondition::Clean);
            assert((slot.Header.Generation == 4U && slot.VerifiedFrames == 64U) ||
                   (slot.Header.Generation == 5U && slot.VerifiedFrames == 2U));
            records += slot.VerifiedFrames;
        }
        assert(records == 66U && f.Watermark.stores == 6U);
    }

    /// The unchanged NVS ledger representation cannot adopt a file-body profile.
    void WatermarkProfileIsIndependent() {
        Fixture f{};
        const auto identity = MakeIdentity();
        Archive owner(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U
        );
        assert(owner.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        auto ledgerBytes = std::span<std::byte>(f.Watermark.bytes[0U]).first(L::EdlsHeaderBytes);
        L::EdlsHeader header{};
        assert(L::ReadEdlsHeader(
            ledgerBytes,
            header
        ) == L::EdlsHeaderStatus::Succeeded);
        header.Profile = L::EdlsContentProfile::SealedFrames;
        assert(L::WriteEdlsHeader(
            header,
            ledgerBytes
        ) == L::EdlsHeaderStatus::Succeeded);
        const auto stores = f.Watermark.stores;
        assert(owner.Read().Status == L::EdlsAcknowledgedStatus::InvalidWatermark);
        assert(owner.Recover(f.Scratch).Status == L::EdlsAcknowledgedStatus::InvalidWatermark);
        assert(f.Watermark.stores == stores);
        for (const auto path : Paths) {
            assert(f.Storage.GetFileSize(path).Status == P::FileSizeStatus::NotFound);
        }
        header.Profile = L::EdlsContentProfile::Unsealed;
        assert(L::WriteEdlsHeader(
            header,
            ledgerBytes
        ) == L::EdlsHeaderStatus::Succeeded);
        assert(owner.Read().Status == L::EdlsAcknowledgedStatus::Ready);
    }

    /// Input refusal, capacity bounds and canonical clock overflow/regression.
    void RefusalAndClock() {
        Fixture f{};
        MakeFrame(f.Frame);
        const auto identity = MakeIdentity();
        Archive owner(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U
        );
        assert(owner.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        Batch badScratch(
            owner,
            f.Clock,
            std::span<std::byte>(f.Scratch).first(10U)
        );
        assert(badScratch.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::InsufficientScratch);

        using SmallBatch = L::BoundedEdlsBatch<4U, 40U, Archive, TestClock, Bytes>;
        SmallBatch small(
            owner,
            f.Clock,
            f.Scratch
        );
        assert(small.ConfigureDwell(C::Duration::FromNanoseconds(17)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        auto corrupt = f.Frame;
        corrupt[17U] ^= std::byte{1U};
        assert(small.Offer(corrupt).Status == L::EdlsBatchStatus::InvalidFrame);
        assert(small.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(small.Offer(f.Frame).Status == L::EdlsBatchStatus::FlushRequired);
        assert(small.Flush().AcknowledgedRecords == 1U);

        Batch overflow(
            owner,
            f.Clock,
            f.Scratch
        );
        assert(overflow.ConfigureDwell(C::Duration::FromNanoseconds(10)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        f.Clock.Tick = std::numeric_limits<std::uint64_t>::max() - 9U;
        assert(overflow.Offer(f.Frame).Status == L::EdlsBatchStatus::ClockRangeExhausted);
        assert(overflow.Flush().Status == L::EdlsBatchStatus::Empty);

        f.Clock.Tick = 100U;
        assert(overflow.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        f.Clock.Tick = 99U;
        assert(overflow.Service().Status == L::EdlsBatchStatus::ClockRegressed);
        assert(overflow.Flush().Status == L::EdlsBatchStatus::RecoveryRequired);
        assert(owner.Read().AcknowledgedGeneration == 1U);
    }

    /// A failed ACK preserves protected segments; uncertain commits never auto-retry.
    void FailedAndUncertainAcknowledgement() {
        Fixture f{};
        MakeFrame(f.Frame);
        const auto identity = MakeIdentity();
        Archive owner(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U
        );
        assert(owner.InitializeEmpty().Status == L::EdlsAcknowledgedStatus::Initialized);
        Batch batch(
            owner,
            f.Clock,
            f.Scratch
        );
        assert(batch.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::Configured);
        for (unsigned segment = 1U; segment <= 2U; ++segment) {
            assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
            assert(batch.Flush().AcknowledgedRecords == 1U);
        }
        f.Watermark.failStore = true;
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        auto failed = batch.Flush();
        assert(failed.Status == L::EdlsBatchStatus::RecoveryRequired);
        assert(failed.AcknowledgedRecords == 0U && failed.PendingRecords == 1U);
        assert(owner.Read().AcknowledgedGeneration == 2U);
        const auto attempts = f.Watermark.stores;
        assert(batch.Flush().Status == L::EdlsBatchStatus::RecoveryRequired);
        assert(batch.Offer(f.Frame).Status == L::EdlsBatchStatus::RecoveryRequired);
        assert(f.Watermark.stores == attempts);
        f.Watermark.failStore = false;
        assert(owner.Recover(f.Scratch).Status ==
               L::EdlsAcknowledgedStatus::UnacknowledgedCandidateRemoved);
        CheckWindow(
            f,
            2U
        );

        Batch uncertain(
            owner,
            f.Clock,
            f.Scratch
        );
        assert(uncertain.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::Configured);
        f.Watermark.commitThenFail = true;
        assert(uncertain.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        failed = uncertain.Flush();
        assert(failed.Status == L::EdlsBatchStatus::RecoveryRequired);
        assert(failed.AcknowledgedRecords == 0U);
        assert(owner.Read().AcknowledgedGeneration == 3U); // ACK really committed
        const auto stored = f.Watermark.stores;
        assert(uncertain.Flush().Status == L::EdlsBatchStatus::RecoveryRequired);
        assert(f.Watermark.stores == stored);
        f.Watermark.commitThenFail = false;
        assert(owner.Recover(f.Scratch).Status ==
               L::EdlsAcknowledgedStatus::CommittedOldestRetired);
        CheckWindow(
            f,
            3U
        );
    }

    /// Independently total all sink-owned file values and the persisted ACK value.
    std::uint64_t SinkBytes(Fixture& f) {
        const auto ledger = f.Watermark.GetValueSize(Key.Value);
        std::uint64_t total = ledger.Status == P::KeyValueSizeStatus::Succeeded
                                  ? ledger.Size.RawValue
                                  : 0U;
        for (const auto path : Paths) {
            const auto size = f.Storage.GetFileSize(path);
            assert(size.Status == P::FileSizeStatus::Succeeded ||
                   size.Status == P::FileSizeStatus::NotFound);
            if (size.Status == P::FileSizeStatus::Succeeded)
                total += size.Size.RawValue;
        }
        return total;
    }

    /// Exact-byte boundaries, immutable construction, rolling and interrupted ACK.
    void ImmutableByteCapacity() {
        using Status = L::EdlsAcknowledgedStatus;
        static_assert(!std::is_copy_assignable_v<Archive>);
        static_assert(!std::is_move_assignable_v<Archive>);
        Fixture f{};
        MakeFrame(f.Frame);
        const auto identity = MakeIdentity();
        Archive invalid(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U,
            0U
        );
        assert(invalid.InitializeEmpty().Status == Status::InvalidByteCapacity);
        assert(invalid.Publish(
            f.Frame,
            f.Scratch
        )
                   .Status == Status::InvalidByteCapacity);
        assert(f.Watermark.stores == 0U && SinkBytes(f) == 0U);

        // 40-byte ledger + three 79-byte sealed segments, including the spare.
        constexpr std::uint64_t capacity = 277U;
        Archive owner(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U,
            capacity
        );
        assert(owner.ByteCapacity() == capacity);
        assert(owner.ConfiguredSegmentByteCapacity() == 79U);
        assert(owner.InitializeEmpty().Status == Status::Initialized);
        Archive shortByOne(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            7U,
            capacity - 1U
        );
        const auto stores = f.Watermark.stores;
        assert(shortByOne.PublishBatch<4U>(
            f.Frame,
            f.Scratch
        )
                   .Status == Status::ByteCapacityExceeded);
        assert(f.Watermark.stores == stores && SinkBytes(f) == 40U);
        for (std::uint64_t generation = 1U; generation <= 100U; ++generation) {
            const auto result = owner.PublishBatch<4U>(f.Frame, f.Scratch);
            assert(result.Status == (generation <= 2U
                                         ? Status::AcknowledgedWithinCapacity
                                         : Status::AcknowledgedAndRetired));
            assert(owner.Read().AcknowledgedGeneration == generation);
            assert(SinkBytes(f) <= capacity);
        }
        CheckWindow(
            f,
            100U
        );

        // A failed watermark leaves ALL THREE segments resident at the exact cap.
        f.Watermark.failStore = true;
        assert(owner.PublishBatch<4U>(
            f.Frame,
            f.Scratch
        )
                   .Status == Status::WatermarkStoreRejected);
        assert(SinkBytes(f) == capacity);
        assert(owner.Read().AcknowledgedGeneration == 100U);
        f.Watermark.failStore = false;
        const auto before = SinkBytes(f);
        assert(shortByOne.Recover(f.Scratch).Status == Status::ByteCapacityExceeded);
        assert(SinkBytes(f) == before); // incompatible restart cannot delete history
        Archive reboot(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            8U,
            capacity
        );
        assert(reboot.Recover(f.Scratch).Status == Status::UnacknowledgedCandidateRemoved);
        assert(SinkBytes(f) == 198U);
        f.Watermark.commitThenFail = true;
        assert(reboot.PublishBatch<4U>(
            f.Frame,
            f.Scratch
        )
                   .Status == Status::WatermarkStoreRejected);
        assert(SinkBytes(f) == capacity);
        assert(reboot.Read().AcknowledgedGeneration == 101U);
        f.Watermark.commitThenFail = false;
        assert(reboot.Recover(f.Scratch).Status == Status::CommittedOldestRetired);
        CheckWindow(
            f,
            101U
        );

        // Larger frames refuse without consuming the spare or advancing the ACK.
        std::array<std::byte, 46U> two{};
        for (std::size_t i = 0U; i < two.size(); ++i)
            two[i] = f.Frame[i % f.Frame.size()];
        const auto priorBytes = SinkBytes(f);
        assert(reboot.PublishBatch<4U>(
            two,
            f.Scratch
        )
                   .Status == Status::ByteCapacityExceeded);
        assert(SinkBytes(f) == priorBytes && reboot.Read().AcknowledgedGeneration == 101U);
        Archive maximum(
            f.Storage,
            f.Watermark,
            Paths,
            Key.Value,
            identity,
            8U,
            std::numeric_limits<std::uint64_t>::max()
        );
        assert(maximum.ConfiguredSegmentByteCapacity() == Archive::MaximumSegmentBytes());
        assert(maximum.Recover(f.Scratch).Status == Status::Ready);

        // Admission respects the smaller archive share, flushing before overflow.
        Batch bounded(
            reboot,
            f.Clock,
            f.Scratch
        );
        assert(bounded.ConfigureDwell(C::Duration::FromNanoseconds(100000000)) ==
               L::EdlsBatchConfigurationStatus::Configured);
        assert(bounded.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(bounded.NextFlushDeadline() == f.Clock.Now());
        assert(bounded.Offer(f.Frame).Status == L::EdlsBatchStatus::FlushRequired);
        assert(bounded.Service().AcknowledgedRecords == 1U);
        assert(bounded.Offer(f.Frame).Status == L::EdlsBatchStatus::Buffered);
        assert(bounded.Flush().AcknowledgedRecords == 1U);
        assert(SinkBytes(f) <= capacity);
        Batch invalidBatch(
            invalid,
            f.Clock,
            f.Scratch
        );
        assert(invalidBatch.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::InvalidByteCapacity);
        Batch undersized(
            shortByOne,
            f.Clock,
            f.Scratch
        );
        assert(undersized.ConfigureDwell(C::Duration{}) ==
               L::EdlsBatchConfigurationStatus::Configured);
        assert(undersized.Offer(f.Frame).Status == L::EdlsBatchStatus::FrameTooLarge);

        // Single-frame API has no seal and uses the same immutable budget owner.
        Fixture single{};
        MakeFrame(single.Frame);
        Archive plain(
            single.Storage,
            single.Watermark,
            Paths,
            Key.Value,
            identity,
            7U,
            229U
        );
        assert(plain.InitializeEmpty().Status == Status::Initialized);
        for (unsigned i = 0U; i < 5U; ++i) {
            const auto result = plain.Publish(
                single.Frame,
                single.Scratch
            );
            assert(result.Status == (i < 2U ? Status::AcknowledgedWithinCapacity : Status::AcknowledgedAndRetired));
            assert(SinkBytes(single) <= 229U);
        }
        std::puts("IMMUTABLE_BYTE_CAPACITY_BOUNDARIES_ROLL_RECOVERY=PASS");
    }

} // namespace

int main() {
    ImmutableByteCapacity();
    TimingAndRetention();
    SixtyFourRecordConfiguration();
    WatermarkProfileIsIndependent();
    RefusalAndClock();
    FailedAndUncertainAcknowledgement();
    std::puts("SEALED_EDLS_BATCH_TIMING_RETENTION_NEGATIVES=PASS");
}
