#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>

using Era = ESPressio::Clock::ClockEra;
using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,3U>;

static_assert(ESPressio::Serialisation::SerialisableType<Era>);
static_assert(ESPressio::Logging::IsDiagnosticSchemaMatching<Schema,Era>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<Schema,std::uint64_t>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<Schema,Era,Era>());

int main() {
    const Era sample = Era::FromValue(0x0102030405060708ULL);
    std::array<std::byte,64U> bytes{};
    bytes.fill(std::byte{0xA5U});
    const auto rejected = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(bytes.data(),2U), sample
    );
    assert(!rejected.IsSuccessful());
    for(const auto byte:bytes) assert(byte==std::byte{0xA5U});

    const auto encoded = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(bytes),sample
    );
    assert(encoded.IsSuccessful());
    assert(encoded.BytesWritten==10U);
    constexpr std::array<std::uint8_t,10U> expected{
        0x81U,0x1BU,0x01U,0x02U,0x03U,0x04U,0x05U,0x06U,0x07U,0x08U
    };
    for(std::size_t i=0U;i<expected.size();++i) {
        assert(bytes[i]==std::byte{expected[i]});
    }
    const auto json = ESPressio::Logging::EncodeDiagnosticArgumentsJson<Schema>(
        std::span<std::byte>(bytes),sample
    );
    assert(json.IsSuccessful());
    const std::string_view value{
        reinterpret_cast<const char*>(bytes.data()),json.BytesWritten
    };
    assert(value=="[72623859790382856]");

    const Era noProvenance{};
    const auto zero = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(bytes),noProvenance
    );
    assert(zero.IsSuccessful());
    assert(zero.BytesWritten==2U);
    assert(bytes[0U]==std::byte{0x81U} && bytes[1U]==std::byte{0x00U});
    // An encoded invalid-zero Era must never become a valid synchronized Era.
}
