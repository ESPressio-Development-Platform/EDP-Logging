#include <cassert>
#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string_view>
#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>

using ActualGovernedFixture =
    Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 3U>;

static_assert(ESPressio::Logging::ScalarDiagnosticSchemaMatches<
    ActualGovernedFixture, std::uint16_t, bool
>());
static_assert(!ESPressio::Logging::ScalarDiagnosticSchemaMatches<
    ActualGovernedFixture, std::uint32_t, bool
>());
static_assert(!ESPressio::Logging::ScalarDiagnosticSchemaMatches<
    ActualGovernedFixture, std::uint16_t, bool, std::uint8_t
>());

int main() {
    std::array<std::byte, 20> data{};
    const auto cbor = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<ActualGovernedFixture>(
        std::span<std::byte>(data), std::uint16_t{300U}, true
    );
    assert(cbor.IsSuccessful());
    assert(cbor.BytesWritten == 5U);
    const std::array<std::uint8_t,5> expected{0x82U,0x19U,0x01U,0x2CU,0xF5U};
    for(std::size_t i=0;i<expected.size();++i)
        assert(std::to_integer<std::uint8_t>(data[i])==expected[i]);
    const auto json = ESPressio::Logging::EncodeDiagnosticArgumentsJson<ActualGovernedFixture>(
        std::span<std::byte>(data), std::uint16_t{300U}, true
    );
    assert(json.IsSuccessful());
    assert(std::string_view(reinterpret_cast<const char*>(data.data()),json.BytesWritten)
        == "[300,true]");
}
