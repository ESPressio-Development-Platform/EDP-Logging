#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <ESPressio_Persistence.hpp>

#include "EdlgFrameReader.hpp"
#include "EdlsSegment.hpp"

namespace ESPressio::Logging {

    /// Bounded snapshot archive using individual PowerLoss-atomic Persistence keys.
    /// This is a small-record reference profile, not an unbounded EDLS file journal.
    enum class DurableEdlsStatus : std::uint8_t {
        Empty = 0U,
        Clean = 1U,
        Degraded = 2U,
        Ambiguous = 3U,
        ProviderFailure = 4U,
        InvalidRecord = 5U,
        NoCapacity = 6U,
        StorageRejected = 7U,
        ReadbackUnconfirmed = 8U,
        GenerationExhausted = 9U,
        Committed = 10U
    };

    template<std::size_t TSlots>
    struct DurableEdlsObservation final {
        DurableEdlsStatus Status{DurableEdlsStatus::ProviderFailure};
        std::size_t NewestSlot{TSlots};
        std::uint64_t Generation{0U};
        std::array<std::byte,16U> Identity{};
        std::array<DurableEdlsStatus,TSlots> SlotStatus{};
        std::array<std::uint64_t,TSlots> SlotGenerations{};
    };

    template<std::size_t TSlots>
    struct DurableEdlsCommitOutcome final {
        DurableEdlsStatus Status{DurableEdlsStatus::StorageRejected};
        ESPressio::Persistence::KeyValueStoreStatus Native{
            ESPressio::Persistence::KeyValueStoreStatus::NotReady
        };
        std::size_t Slot{TSlots};
        std::uint64_t Generation{0U};
    };

    /// Exact bounded retrieval of one physically present, CRC-valid generation.
    /// This reports only verified frame bytes, not any durability acknowledgement.
    struct DurableEdlsFrameReadOutcome final {
        /// Whether the requested generation was recovered and checked.
        DurableEdlsStatus Status{DurableEdlsStatus::ProviderFailure};

        /// Number of validated EDLG bytes written to caller storage.
        std::size_t FrameBytes{0U};
    };


    /// One immutable generation with ONE EDLG frame per value.
    /// The policy intentionally retires the previous acknowledged generation
    /// once a newer generation has been acknowledged. One outstanding
    /// generation is protected while the OTHER key is being overwritten.
    ///
    /// Storage must implement the real EDP-Persistence KeyValueStorage
    /// contract and explicitly offer PowerLoss retention and atomicity,
    /// such as an app-selected ESP-IDF NVS namespace. A key-value provider
    /// advertising Restart-only is rejected at compile time.
    ///
    /// Bootstrap owns the native Persistence provider/namespace, the keys,
    /// exclusive access to them and admission serialization. Do not share
    /// the namespace/keys with unrelated writers. No new flash driver,
    /// lock, allocator, worker, clock, or storage implementation is created.
    template<std::size_t TSlots, std::size_t TMaximumValueBytes,
             std::size_t TMaximumFramePayloadBytes, class TStorage>
    class PowerLossEdlsSnapshots final {
        using Capability = ESPressio::Persistence::KeyValueStorage;
        using Properties = typename TStorage::CompositionOffers::template PropertiesFor<Capability>;


        static_assert(TSlots >= 2U && TSlots <= 8U);
        static_assert(TMaximumFramePayloadBytes >= 1U && TMaximumFramePayloadBytes <= 4096U);
        static_assert(TMaximumValueBytes >= EdlsHeaderBytes + 20U);
        static_assert(TMaximumValueBytes <= 512U, "Bounded NVS reference profile maximum");
        static_assert(ESPressio::Persistence::KeyValueStorageProvider<TStorage>);
        static_assert(
            Properties::template Value<ESPressio::Persistence::KeyValueAccessMode>
                == ESPressio::Persistence::AccessMode::ReadWrite
        );
        static_assert(
            Properties::template Value<ESPressio::Persistence::KeyValueRetention>
                == ESPressio::Persistence::RetentionLevel::PowerLoss,
            "Strict EDLS snapshots require PowerLoss retention"
        );
        static_assert(
            Properties::template Value<ESPressio::Persistence::KeyValueInterruptionAtomicity>
                == ESPressio::Persistence::InterruptionAtomicity::PowerLoss,
            "Strict EDLS snapshots reject non-PowerLoss-atomic providers"
        );
        static_assert(
            Properties::template Value<ESPressio::Persistence::MaximumKeyValueSize>
                >= ESPressio::Persistence::StorageSize{TMaximumValueBytes},
            "EDLS snapshot exceeds provider's declared per-key maximum"
        );

        TStorage* storage_;
        std::array<ESPressio::Persistence::KeyView,TSlots> keys_;

        /// Duplicate/empty physical keys would alias protected and inactive
        /// storage, invalidating the entire atomic two-key safety argument.
        /// Detect at the boundary before any storage operation or mutation.
        [[nodiscard]] bool IsKeySetValid() const noexcept {
            for(std::size_t i=0U;i<TSlots;++i) {
                if(keys_[i].Data()==nullptr || keys_[i].Size()==0U)
                    return false;
                for(std::size_t j=0U;j<i;++j) {
                    if(keys_[j].Size()!=keys_[i].Size())continue;
                    bool equal=true;
                    for(std::size_t k=0U;k<keys_[i].Size();++k) {
                        if(keys_[i].Data()[k]!=keys_[j].Data()[k]) {
                            equal=false;break;
                        }
                    }
                    if(equal)return false;
                }
            }
            return true;
        }

        struct Slot final {
            DurableEdlsStatus Status{DurableEdlsStatus::ProviderFailure};
            EdlsHeader Header{};
            std::size_t TotalBytes{0U};
        };

        [[nodiscard]] Slot ReadSlot(
            std::size_t slot,
            std::array<std::byte,TMaximumValueBytes>& buffer
        ) const noexcept {
            namespace P = ESPressio::Persistence;
            const auto native = storage_->ReadValue(
                keys_[slot],P::DestinationBufferView{buffer.data(),buffer.size()}
            );
            if (native.Status == P::KeyValueReadStatus::NotFound) {
                return {DurableEdlsStatus::Empty};
            }
            if (native.Status != P::KeyValueReadStatus::Succeeded ||
                !P::HasFact(native.Facts,P::ReadFact::AvailableDataSizeIsKnown) ||
                native.BytesTransferred > buffer.size() ||
                native.AvailableDataSize.RawValue != native.BytesTransferred) {
                return {DurableEdlsStatus::ProviderFailure};
            }
            if (native.BytesTransferred < EdlsHeaderBytes+20U) {
                return {DurableEdlsStatus::Degraded};
            }
            EdlsHeader header{};
            const auto data = std::span<const std::byte>(
                buffer.data(),native.BytesTransferred
            );
            if (ReadEdlsHeader(data.first(EdlsHeaderBytes),header) !=
                    EdlsHeaderStatus::Succeeded ||
                header.Slot != slot) {
                return {DurableEdlsStatus::Degraded};
            }
            const auto frame = data.subspan(EdlsHeaderBytes);
            EdlgFrameView validated{};
            if (ReadEdlgFrame(frame,validated,TMaximumFramePayloadBytes) !=
                    EdlgFrameReadResult::Succeeded ||
                validated.Consumed != frame.size()) {
                return {DurableEdlsStatus::Degraded};
            }
            return {DurableEdlsStatus::Clean,header,native.BytesTransferred};
        }

    public:
        PowerLossEdlsSnapshots(
            TStorage& storage,
            const std::array<ESPressio::Persistence::KeyView,TSlots>& keys
        ) noexcept : storage_(&storage),keys_(keys) {}

        /// Read-only bounded scan. A corrupt non-newest key is explicitly
        /// degraded; never rewrite it silently to make recovery look clean.
        [[nodiscard]] DurableEdlsObservation<TSlots> Inspect() const noexcept {
            DurableEdlsObservation<TSlots> result{};
            if(!IsKeySetValid()) {
                result.Status=DurableEdlsStatus::Ambiguous;
                return result;
            }
            result.Status=DurableEdlsStatus::Empty;
            std::array<std::byte,TMaximumValueBytes> buffer{};
            bool degraded=false;
            for (std::size_t i=0U;i<TSlots;++i) {
                const auto slot=ReadSlot(i,buffer);
                result.SlotStatus[i]=slot.Status;
                if (slot.Status==DurableEdlsStatus::ProviderFailure) {
                    result.Status=DurableEdlsStatus::ProviderFailure;
                    return result;
                }
                if (slot.Status==DurableEdlsStatus::Degraded) {
                    degraded=true;
                    continue;
                }
                if (slot.Status==DurableEdlsStatus::Empty) continue;
                if (result.NewestSlot!=TSlots) {
                    if (slot.Header.DeviceIdentifier!=result.Identity ||
                        slot.Header.Generation==0U) {
                        result.Status=DurableEdlsStatus::Ambiguous;
                        return result;
                    }
                    // A generation duplicate can be in ANY older key, not
                    // necessarily the previously scanned newest key.
                    for (std::size_t prior=0U;prior<i;++prior) {
                        if (result.SlotStatus[prior]==DurableEdlsStatus::Clean &&
                            result.SlotGenerations[prior]==slot.Header.Generation) {
                            result.Status=DurableEdlsStatus::Ambiguous;
                            return result;
                        }
                    }
                } else {
                    result.Identity=slot.Header.DeviceIdentifier;
                }
                result.SlotGenerations[i]=slot.Header.Generation;
                if (result.NewestSlot==TSlots ||
                    slot.Header.Generation>result.Generation) {
                    result.Generation=slot.Header.Generation;
                    result.NewestSlot=i;
                }
            }
            result.Status=degraded
                ? (result.NewestSlot==TSlots
                    ? DurableEdlsStatus::Ambiguous
                    : DurableEdlsStatus::Degraded)
                : (result.NewestSlot==TSlots
                    ? DurableEdlsStatus::Empty
                    : DurableEdlsStatus::Clean);
            return result;
        }

        /// Safely recover the newest valid generation. The caller must regard
        /// Degraded as requiring explicit intervention before new writes.
        /// Caller-buffer contents are untrusted unless Status is Clean/Degraded.
        [[nodiscard]] DurableEdlsObservation<TSlots> RecoverNewest(
            std::span<std::byte> destination,
            std::size_t& used
        ) const noexcept {
            used=0U;
            auto outcome=Inspect();
            if ((outcome.Status!=DurableEdlsStatus::Clean &&
                 outcome.Status!=DurableEdlsStatus::Degraded) ||
                outcome.NewestSlot==TSlots) {
                return outcome;
            }
            std::array<std::byte,TMaximumValueBytes> bytes{};
            const auto observed=ReadSlot(outcome.NewestSlot,bytes);
            if (observed.Status!=DurableEdlsStatus::Clean ||
                observed.Header.Generation!=outcome.Generation ||
                observed.Header.DeviceIdentifier!=outcome.Identity ||
                destination.size()<observed.TotalBytes-EdlsHeaderBytes) {
                outcome.Status=DurableEdlsStatus::ReadbackUnconfirmed;
                return outcome;
            }
            used=observed.TotalBytes-EdlsHeaderBytes;
            for(std::size_t i=0U;i<used;++i) {
                destination[i]=bytes[EdlsHeaderBytes+i];
            }
            return outcome;
        }

        /// Recover a specific physical EDLS generation through the existing
        /// bounded slot parser. Caller chooses which generations remain in its
        /// retention policy; this method never implies historic entitlement.
        /// The destination is not published unless all bytes were reverified.
        [[nodiscard]] DurableEdlsFrameReadOutcome RecoverGeneration(
            std::uint64_t generation,
            std::span<std::byte> destination
        ) const noexcept {
            DurableEdlsFrameReadOutcome result{};
            const auto observation=Inspect();
            result.Status=observation.Status;
            if (observation.Status!=DurableEdlsStatus::Clean &&
                observation.Status!=DurableEdlsStatus::Degraded) {
                return result;
            }
            if (generation==0U) {
                result.Status=DurableEdlsStatus::InvalidRecord;
                return result;
            }
            for (std::size_t index=0U;index<TSlots;++index) {
                if (observation.SlotStatus[index]!=DurableEdlsStatus::Clean ||
                    observation.SlotGenerations[index]!=generation) {
                    continue;
                }
                std::array<std::byte,TMaximumValueBytes> bytes{};
                const auto slot=ReadSlot(
                    index,
                    bytes
                );
                if (slot.Status!=DurableEdlsStatus::Clean ||
                    slot.Header.Generation!=generation ||
                    slot.Header.DeviceIdentifier!=observation.Identity ||
                    slot.TotalBytes<EdlsHeaderBytes ||
                    slot.TotalBytes-EdlsHeaderBytes>destination.size()) {
                    result.Status=DurableEdlsStatus::ReadbackUnconfirmed;
                    return result;
                }
                result.FrameBytes=slot.TotalBytes-EdlsHeaderBytes;
                for (std::size_t i=0U;i<result.FrameBytes;++i) {
                    destination[i]=bytes[EdlsHeaderBytes+i];
                }
                result.Status=DurableEdlsStatus::Clean;
                return result;
            }
            result.Status=DurableEdlsStatus::InvalidRecord;
            return result;
        }

        /// Writes exactly one new complete, checksummed EDLS snapshot into an
        /// inactive *PowerLoss-atomic* key. Does not overwrite latest generation.
        /// Only successful native StoreValue AND matching independent readback
        /// may return Committed, and only that newest generation is retained.
        [[nodiscard]] DurableEdlsCommitOutcome<TSlots> Commit(
            const std::array<std::byte,16U>& identity,
            std::uint32_t incarnation,
            std::span<const std::byte> frame
        ) noexcept {
            namespace P = ESPressio::Persistence;
            DurableEdlsCommitOutcome<TSlots> result{};
            if(frame.empty() || frame.size()>TMaximumFramePayloadBytes+20U ||
                frame.size()>TMaximumValueBytes-EdlsHeaderBytes) {
                result.Status=DurableEdlsStatus::NoCapacity;
                return result;
            }
            EdlgFrameView parsed{};
            if(ReadEdlgFrame(frame,parsed,TMaximumFramePayloadBytes)!=
                    EdlgFrameReadResult::Succeeded ||
                parsed.Consumed!=frame.size()) {
                result.Status=DurableEdlsStatus::InvalidRecord;
                return result;
            }
            EdlsHeader header{};
            header.DeviceIdentifier=identity;
            header.RuntimeIncarnation=incarnation;
            header.Generation=1U;
            if(!Detail::IsValidEdlsIdentity(header)) {
                result.Status=DurableEdlsStatus::InvalidRecord;
                return result;
            }
            const auto current=Inspect();
            if(current.Status!=DurableEdlsStatus::Clean &&
               current.Status!=DurableEdlsStatus::Empty) {
                result.Status=current.Status;
                return result;
            }
            if(current.Status==DurableEdlsStatus::Clean) {
                if(current.Identity!=identity) {
                    result.Status=DurableEdlsStatus::Ambiguous;
                    return result;
                }
                if(current.Generation==std::numeric_limits<std::uint64_t>::max()) {
                    result.Status=DurableEdlsStatus::GenerationExhausted;
                    return result;
                }
                header.Generation=current.Generation+1U;
            }
            std::size_t target=TSlots;
            if(current.NewestSlot==TSlots) {
                target=0U;
            } else {
                for(std::size_t advance=1U;advance<TSlots;++advance) {
                    const auto candidate=(current.NewestSlot+advance)%TSlots;
                    if(current.SlotStatus[candidate]==DurableEdlsStatus::Empty) {
                        target=candidate;break;
                    }
                }
                if(target==TSlots)target=(current.NewestSlot+1U)%TSlots;
            }
            header.Slot=static_cast<std::uint16_t>(target);
            result.Slot=target;
            result.Generation=header.Generation;
            std::array<std::byte,TMaximumValueBytes> bytes{};
            if(WriteEdlsHeader(header,bytes)!=EdlsHeaderStatus::Succeeded) {
                result.Status=DurableEdlsStatus::InvalidRecord;
                return result;
            }
            for(std::size_t i=0U;i<frame.size();++i)
                bytes[EdlsHeaderBytes+i]=frame[i];
            const auto total=EdlsHeaderBytes+frame.size();
            result.Native=storage_->StoreValue(
                keys_[target],P::SourceBufferView{bytes.data(),total}
            );
            if(result.Native!=P::KeyValueStoreStatus::Succeeded) {
                result.Status=DurableEdlsStatus::StorageRejected;
                return result;
            }
            std::array<std::byte,TMaximumValueBytes> reread{};
            const auto stored=ReadSlot(target,reread);
            if(stored.Status!=DurableEdlsStatus::Clean ||
                stored.Header.Generation!=header.Generation ||
                stored.Header.DeviceIdentifier!=identity ||
                stored.TotalBytes!=total) {
                result.Status=DurableEdlsStatus::ReadbackUnconfirmed;
                return result;
            }
            for(std::size_t i=0U;i<total;++i) {
                if(bytes[i]!=reread[i]) {
                    result.Status=DurableEdlsStatus::ReadbackUnconfirmed;
                    return result;
                }
            }
            result.Status=DurableEdlsStatus::Committed;
            return result;
        }
    };

} // namespace ESPressio::Logging
