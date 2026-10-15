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
            WriteEdlgFrame(EdlgCodec::Cbor, payload, output, written, 16U)
                == EdlgFrameWriteResult::Succeeded
        );
        assert(written == golden.size());
        for (std::size_t i = 0U; i < output.size(); ++i) {
            assert(std::to_integer<std::uint8_t>(output[i]) == golden[i]);
        }
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
            WriteEdlgFrame(EdlgCodec::Cbor, payload, output, written, 16U)
                == EdlgFrameWriteResult::InsufficientCapacity
        );
        assert(written == 99U);
        for (const std::byte value : output) {
            assert(value == std::byte{0xAAU});
        }

        std::array<std::byte, 23U> sufficient{};
        assert(
            WriteEdlgFrame(EdlgCodec::Cbor, payload, sufficient, written, 2U)
                == EdlgFrameWriteResult::InvalidPayload
        );
    }

} // Test

int main() {
    Test::GoldenFrame();
    Test::CapacityIsTransactional();
    return 0;
}
