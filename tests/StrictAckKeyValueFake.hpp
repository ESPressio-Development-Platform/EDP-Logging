#pragma once
// Test-only strict-capability fake of the EXISTING EDP-Persistence
// PowerLoss-atomic KeyValueStorage contract; no alternative runtime owner.
#include <array>
#include <cstddef>
#include <cstdint>
#include <ESPressio_Persistence.hpp>

namespace ESPressio::Logging::StrictAckTests {
    namespace PK=ESPressio::Persistence;
    namespace FK=PK::Composition;
    using AckOffer=FK::Offer<PK::KeyValueStorage,
        FK::PropertyValue<PK::KeyValueAccessMode,PK::AccessMode::ReadWrite>,
        FK::PropertyValue<PK::KeyValueRetention,PK::RetentionLevel::PowerLoss>,
        FK::PropertyValue<PK::KeyCaseSensitivity,PK::TextCaseSensitivity::CaseSensitive>,
        FK::PropertyValue<PK::KeyValueMediaRemovability,PK::MediaRemovability::Fixed>,
        FK::PropertyValue<PK::MaximumKeyBytes,std::size_t{15U}>,
        FK::PropertyValue<PK::MaximumKeyValueSize,PK::StorageSize{512U}>,
        FK::PropertyValue<PK::KeyEnumerationSupport,PK::Support::Unsupported>,
        FK::PropertyValue<PK::ReadValueAtSupport,PK::Support::Unsupported>,
        FK::PropertyValue<PK::ClearAllSupport,PK::Support::Unsupported>,
        FK::PropertyValue<PK::KeyValueCapacityReportingSupport,PK::Support::Unsupported>,
        FK::PropertyValue<PK::KeyValueInvocationConcurrency,PK::InvocationConcurrency::CallerSerialized>,
        FK::PropertyValue<PK::KeyValueFailurePreservation,PK::FailurePreservation::MayModify>,
        FK::PropertyValue<PK::KeyValueInterruptionAtomicity,PK::InterruptionAtomicity::PowerLoss>
    >;
    struct TestAckKeyValueStorage final : FK::Provider<PK::Domain,FK::Offers<AckOffer>> {
        std::array<std::array<std::byte,512U>,8U> bytes{};
        std::array<std::size_t,8U> sizes{};
        bool failStore{false};
        /// Test-only lost response after an actually committed atomic write.
        bool commitThenFail{false};
        bool failRead{false};
        bool tornValue{false};
        std::size_t stores{0U};
        [[nodiscard]] bool IsKeyValueStorageReady() const noexcept {return true;}
        [[nodiscard]] std::size_t Which(PK::KeyView key) const noexcept {
            return static_cast<std::size_t>(key.Data()[0]-'a');
        }
        [[nodiscard]] PK::KeyValueSizeResult GetValueSize(PK::KeyView key) const noexcept {
            const auto i=Which(key);
            if(sizes[i]==0U)return {PK::KeyValueSizeStatus::NotFound,{}};
            return {PK::KeyValueSizeStatus::Succeeded,PK::StorageSize{sizes[i]}};
        }
        [[nodiscard]] PK::KeyValueReadResult ReadValue(
            PK::KeyView key,PK::DestinationBufferView out
        ) const noexcept {
            if(failRead)return {PK::KeyValueReadStatus::IoFailure,0U,0U,{}};
            const auto i=Which(key);
            if(sizes[i]==0U)return {PK::KeyValueReadStatus::NotFound,0U,0U,{}};
            const auto count=sizes[i]<out.Capacity?sizes[i]:out.Capacity;
            for(std::size_t j=0U;j<count;++j)
                static_cast<std::byte*>(out.Address)[j]=bytes[i][j];
            return {PK::KeyValueReadStatus::Succeeded,
                static_cast<std::uint8_t>(PK::ReadFact::AvailableDataSizeIsKnown),
                count,PK::StorageSize{sizes[i]}};
        }
        [[nodiscard]] PK::KeyValueStoreStatus StoreValue(
            PK::KeyView key,PK::SourceBufferView source
        ) noexcept {
            ++stores;
            const auto i=Which(key);
            if(source.Size>512U)return PK::KeyValueStoreStatus::ValueTooLarge;
            if(failStore) {
                if(tornValue) {
                    sizes[i]=10U;
                    for(std::size_t j=0U;j<10U;++j)
                        bytes[i][j]=static_cast<const std::byte*>(source.Address)[j];
                }
                return PK::KeyValueStoreStatus::IoFailure;
            }
            for(std::size_t j=0U;j<source.Size;++j)
                bytes[i][j]=static_cast<const std::byte*>(source.Address)[j];
            sizes[i]=source.Size;
            return commitThenFail
                ? PK::KeyValueStoreStatus::IoFailure
                : PK::KeyValueStoreStatus::Succeeded;
        }
        [[nodiscard]] PK::KeyValueRemoveStatus RemoveKey(PK::KeyView key) noexcept {
            sizes[Which(key)]=0U;
            return PK::KeyValueRemoveStatus::Succeeded;
        }
    };


} // namespace ESPressio::Logging::StrictAckTests
