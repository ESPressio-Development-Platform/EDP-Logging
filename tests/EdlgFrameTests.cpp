#include <logging/EdlgFrameWriter.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#include <ESPressio_Logging.hpp>

namespace Test {

    using namespace ESPressio::Logging;

    /// Canonical Python/C++ frame golden vector for CBOR byte payload "abc".
    void GoldenFrame() {
        constexpr std::array<std::byte, 3U> payload{
            std::byte{'a'}, std::byte{'b'}, std::byte{'c'}
        };
        constexpr std::array<std::uint8_t, 23U> golden{
            0x45U, 0x44U, 0x4CU, 0x47U,
            0x01U, 0x01U, 0x00U, 0x00U,
            0x03U, 0x00U, 0x00U, 0x00U,
            0x80U, 0x63U, 0x7FU, 0xF5U,
            0x61U, 0x62U, 0x63U,
            0x47U, 0x4CU, 0x44U, 0x45U
        };
        std::array<std::byte, 23U> output{};
        std::size_t written = 0U;
        assert(
            WriteEdlgFrame(EdlgCodec::Cbor, payload, output, written, 16U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})
                == EdlgFrameWriteResult::Succeeded
        );
        assert(written == golden.size());
        for (std::size_t i = 0U; i < output.size(); ++i) {
            assert(std::to_integer<std::uint8_t>(output[i]) == golden[i]);
        }
    }

    /// Validates the portable envelope in place, preserving the borrowed payload.
    void ReadFrameStrictly() {
        constexpr std::array<std::byte, 3U> payload{
            std::byte{'a'}, std::byte{'b'}, std::byte{'c'}
        };
        std::array<std::byte, 23U> wire{};
        std::size_t written = 0U;
        assert(
            WriteEdlgFrame(EdlgCodec::Cbor, payload, wire, written, payload.size(),
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{}) ==
            EdlgFrameWriteResult::Succeeded
        );
        EdlgFrameView view{};
        assert(ReadEdlgFrame(wire, view, 3U) == EdlgFrameReadResult::Succeeded);
        assert(view.Codec == EdlgCodec::Cbor);
        assert(view.Consumed == wire.size());
        assert(view.Payload.data() == wire.data() + 16U);
        assert(view.Payload.size() == 3U);
        assert(view.Payload[0] == std::byte{'a'});

        std::array<std::byte, 24U> withTrailing{};
        for (std::size_t i = 0U; i < wire.size(); ++i) {
            withTrailing[i] = wire[i];
        }
        withTrailing[23U] = std::byte{0xFFU};
        assert(
            ReadEdlgFrame(withTrailing, view, 3U) == EdlgFrameReadResult::Succeeded
        );
        assert(view.Consumed == 23U);

        EdlgFrameView unchanged{};
        unchanged.Consumed = 71U;
        auto check = [&](EdlgFrameReadResult expected) {
            assert(ReadEdlgFrame(wire, unchanged, 3U) == expected);
            assert(unchanged.Consumed == 71U);
        };

        wire[6U] = std::byte{1U};
        check(EdlgFrameReadResult::InvalidFlags);
        wire[6U] = std::byte{0U};
        wire[5U] = std::byte{9U};
        check(EdlgFrameReadResult::InvalidCodec);
        wire[5U] = std::byte{1U};
        wire[16U] ^= std::byte{1U};
        check(EdlgFrameReadResult::ChecksumMismatch);
        wire[16U] ^= std::byte{1U};
        wire[22U] ^= std::byte{1U};
        check(EdlgFrameReadResult::InvalidCompletion);
        wire[22U] ^= std::byte{1U};
        assert(ReadEdlgFrame(wire, unchanged, 2U) == EdlgFrameReadResult::PayloadTooLarge);
        assert(ReadEdlgFrame(std::span<const std::byte>(wire.data(), 19U), unchanged, 3U)
            == EdlgFrameReadResult::IncompleteFrame);
    }

    /// Rejects insufficient capacity without modifying output or its written count.
    void CapacityIsTransactional() {
        constexpr std::array<std::byte, 3U> payload{
            std::byte{1U}, std::byte{2U}, std::byte{3U}
        };
        std::array<std::byte, 22U> output{};
        output.fill(std::byte{0xAAU});
        std::size_t written = 99U;
        assert(
            WriteEdlgFrame(EdlgCodec::Cbor, payload, output, written, 16U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})
                == EdlgFrameWriteResult::InsufficientCapacity
        );
        assert(written == 99U);
        for (const std::byte value : output) {
            assert(value == std::byte{0xAAU});
        }

        std::array<std::byte, 23U> sufficient{};
        assert(
            WriteEdlgFrame(EdlgCodec::Cbor, payload, sufficient, written, 2U,
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider{})
                == EdlgFrameWriteResult::InvalidPayload
        );
    }

} // Test

int main() {
    Test::GoldenFrame();
    Test::CapacityIsTransactional();
    Test::ReadFrameStrictly();
    return 0;
}
