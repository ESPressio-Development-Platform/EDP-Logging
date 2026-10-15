#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#include <ESPressio_Logging.hpp>

namespace Test {

    using namespace ESPressio::Logging;

    void GoldenSegmentHeaderAndRoundTrip() {
        EdlsHeader header{};
        header.Slot = 2U;
        header.Generation = 7U;
        for (std::uint8_t index = 0U; index < 16U; ++index) {
            header.DeviceIdentifier[index] = std::byte{static_cast<std::uint8_t>(index + 1U)};
        }
        header.RuntimeIncarnation = 9U;

        constexpr std::array<std::uint8_t, EdlsHeaderBytes> golden{
            0x45U, 0x44U, 0x4CU, 0x53U,
            0x01U, 0x00U, 0x02U, 0x00U,
            0x07U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
            0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
            0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU, 0x10U,
            0x09U, 0x00U, 0x00U, 0x00U,
            0xAEU, 0x4EU, 0x0CU, 0x51U
        };

        std::array<std::byte, EdlsHeaderBytes> output{};
        assert(WriteEdlsHeader(header, output) == EdlsHeaderStatus::Succeeded);
        for (std::size_t i = 0U; i < golden.size(); ++i) {
            assert(std::to_integer<std::uint8_t>(output[i]) == golden[i]);
        }
        EdlsHeader decoded{};
        assert(ReadEdlsHeader(output, decoded) == EdlsHeaderStatus::Succeeded);
        assert(decoded.Slot == header.Slot);
        assert(decoded.Generation == header.Generation);
        assert(decoded.DeviceIdentifier == header.DeviceIdentifier);
        assert(decoded.RuntimeIncarnation == header.RuntimeIncarnation);

        output[16U] ^= std::byte{0x01U};
        EdlsHeader unchanged{};
        unchanged.Generation = 123U;
        assert(ReadEdlsHeader(output, unchanged) == EdlsHeaderStatus::ChecksumMismatch);
        assert(unchanged.Generation == 123U);
    }

    void TransactionalFailurePaths() {
        EdlsHeader invalid{};
        std::array<std::byte, EdlsHeaderBytes> buffer{};
        buffer.fill(std::byte{0xAAU});
        assert(WriteEdlsHeader(invalid, buffer) == EdlsHeaderStatus::InvalidArguments);
        for (const auto value : buffer) {
            assert(value == std::byte{0xAAU});
        }
        invalid.Generation = 1U;
        invalid.RuntimeIncarnation = 1U;
        invalid.DeviceIdentifier[0U] = std::byte{0x01U};
        assert(
            WriteEdlsHeader(invalid, std::span<std::byte>(buffer.data(), buffer.size() - 1U))
            == EdlsHeaderStatus::InsufficientCapacity
        );
        for (const auto value : buffer) {
            assert(value == std::byte{0xAAU});
        }
    }

} // Test

int main() {
    Test::GoldenSegmentHeaderAndRoundTrip();
    Test::TransactionalFailurePaths();
    return 0;
}
