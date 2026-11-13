#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#include <logging/EdlgCheckpoint.hpp>

namespace {

    namespace Logging = ESPressio::Logging;
    namespace Persistence = ESPressio::Persistence;

    struct InMemoryKeyValue final {
        std::array<std::byte, 128U> Bytes{};
        std::size_t Size{0U};
        bool Present{false};
        bool FailStore{false};
        bool FailRead{false};
        bool MissingKnownSizeFact{false};
        bool MisreportTransferredBytes{false};
        std::size_t StoreCalls{0U};

        [[nodiscard]] Persistence::KeyValueStoreStatus StoreValue(
            Persistence::KeyView,
            Persistence::SourceBufferView source
        ) noexcept {
            ++StoreCalls;
            if (FailStore) return Persistence::KeyValueStoreStatus::IoFailure;
            if (source.Size > Bytes.size()) {
                return Persistence::KeyValueStoreStatus::ValueTooLarge;
            }
            for (std::size_t i = 0U; i < source.Size; ++i) {
                Bytes[i] = static_cast<const std::byte*>(source.Address)[i];
            }
            Size = source.Size;
            Present = true;
            return Persistence::KeyValueStoreStatus::Succeeded;
        }

        [[nodiscard]] Persistence::KeyValueReadResult ReadValue(
            Persistence::KeyView,
            Persistence::DestinationBufferView output
        ) const noexcept {
            if (FailRead) {
                return {Persistence::KeyValueReadStatus::ProviderFailure, 0U, 0U, {}};
            }
            if (!Present) {
                return {Persistence::KeyValueReadStatus::NotFound, 0U, 0U, {}};
            }
            const auto count = output.Capacity < Size ? output.Capacity : Size;
            for (std::size_t i = 0U; i < count; ++i) {
                static_cast<std::byte*>(output.Address)[i] = Bytes[i];
            }
            return {
                Persistence::KeyValueReadStatus::Succeeded,
                MissingKnownSizeFact ? std::uint8_t{0U} :
                    static_cast<std::uint8_t>(Persistence::ReadFact::AvailableDataSizeIsKnown),
                MisreportTransferredBytes ? output.Capacity + 1U : count,
                Persistence::StorageSize{Size}
            };
        }
    };

    void Run() {
        constexpr auto key = Persistence::KeyView::Validate("logging");
        static_assert(key.IsValuePresent);
        InMemoryKeyValue provider{};
        constexpr std::array<std::byte, 3U> payload{
            std::byte{0x01U}, std::byte{0x02U}, std::byte{0x03U}
        };
        std::array<std::byte, 64U> source{};
        std::size_t written{0U};
        assert(Logging::WriteEdlgFrame(
            Logging::EdlgCodec::Cbor, payload, source, written, 32U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}
        ) == Logging::EdlgFrameWriteResult::Succeeded);
        const auto good = std::span<const std::byte>(source.data(), written);
        std::array<std::byte, 64U> recovery{};
        Logging::EdlgFrameView decoded{};
        decoded.Consumed = 887U;
        auto result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::NotFound);
        assert(decoded.Consumed == 887U);
        source[16U] ^= std::byte{0x01U};
        auto store = Logging::WriteEdlgCheckpoint(
            provider, key.Value, good, 32U
        );
        assert(store.Status == Logging::EdlgCheckpointWriteStatus::InvalidFrame);
        assert(provider.StoreCalls == 0U);
        source[16U] ^= std::byte{0x01U};
        store = Logging::WriteEdlgCheckpoint(
            provider, key.Value, good, 2U
        );
        assert(store.Status == Logging::EdlgCheckpointWriteStatus::FrameTooLarge);
        assert(provider.StoreCalls == 0U);
        provider.FailStore = true;
        store = Logging::WriteEdlgCheckpoint(provider, key.Value, good, 32U);
        assert(store.Status == Logging::EdlgCheckpointWriteStatus::ProviderRejected);
        assert(store.Store == Persistence::KeyValueStoreStatus::IoFailure);
        provider.FailStore = false;
        store = Logging::WriteEdlgCheckpoint(provider, key.Value, good, 32U);
        assert(store.Status == Logging::EdlgCheckpointWriteStatus::Committed);
        assert(provider.StoreCalls == 2U);
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, std::span<std::byte>(recovery).first(10U), 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::BufferTooSmall);
        assert(decoded.Consumed == 887U);
        provider.FailRead = true;
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::ProviderFailure);
        provider.FailRead = false;
        provider.MissingKnownSizeFact = true;
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::ProviderFailure);
        provider.MissingKnownSizeFact = false;
        provider.MisreportTransferredBytes = true;
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::ProviderFailure);
        provider.MisreportTransferredBytes = false;
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::Recovered);
        assert(result.FrameBytes == written && decoded.Consumed == written);
        assert(decoded.Payload.size() == payload.size());
        provider.Bytes[16U] ^= std::byte{0x01U};
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::CorruptFrame);
        assert(decoded.Consumed == written);
        provider.Bytes[16U] ^= std::byte{0x01U};
        provider.Bytes[written] = std::byte{0xFFU};
        ++provider.Size;
        result = Logging::ReadEdlgCheckpoint(
            provider, key.Value, recovery, 32U, decoded
        );
        assert(result.Status == Logging::EdlgCheckpointReadStatus::CorruptFrame);
        assert(decoded.Consumed == written);
    }
}
int main() { Run(); }
