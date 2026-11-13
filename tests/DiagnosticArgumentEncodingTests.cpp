#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <logging/DiagnosticArgumentEncoding.hpp>

namespace GeneratedFixture {

    enum class DiagnosticParameterKind : std::uint8_t {
        Boolean = 0U,
        Signed8 = 1U,
        Signed16 = 2U,
        Signed32 = 3U,
        Signed64 = 4U,
        Unsigned8 = 5U,
        Unsigned16 = 6U,
        Unsigned32 = 7U,
        Unsigned64 = 8U,
        Float32 = 9U,
        Float64 = 10U
    };

    // Mirrors only the generated descriptor SHAPE from the real
    // EDP-Localisation generator; actual catalogue integration remains open.
    template<std::uint32_t, std::uint32_t, std::uint32_t>
    struct DiagnosticMessageSchema final {
        static constexpr bool Declared = false;
        static constexpr std::size_t ArgumentCount = 0U;
        inline static constexpr std::array<DiagnosticParameterKind, 0U> ArgumentKinds{};
    };

    template<>
    struct DiagnosticMessageSchema<1U, 0U, 3U> final {
        static constexpr bool Declared = true;
        static constexpr std::size_t ArgumentCount = 3U;
        inline static constexpr std::array<DiagnosticParameterKind, 3U> ArgumentKinds{
            DiagnosticParameterKind::Unsigned16,
            DiagnosticParameterKind::Boolean,
            DiagnosticParameterKind::Float32
        };
    };

}
using Schema = GeneratedFixture::DiagnosticMessageSchema<1U, 0U, 3U>;

static_assert(ESPressio::Logging::IsDiagnosticSchemaMatching<
    Schema, std::uint16_t, bool, float
>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<
    Schema, std::uint32_t, bool, float
>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<
    Schema, std::uint16_t, bool
>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<
    GeneratedFixture::DiagnosticMessageSchema<1U, 0U, 4U>, std::uint16_t
>());

int main() {
    std::array<std::byte, 64U> encoded{};
    const auto cbor = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(encoded),
        std::uint16_t{300U}, true, float{1.5f}
    );
    assert(cbor.IsSuccessful());
    assert(cbor.BytesWritten == 10U);
    const std::array<std::uint8_t, 10U> actualExpected{
        0x83U, 0x19U, 0x01U, 0x2CU, 0xF5U, 0xFAU,
        0x3FU, 0xC0U, 0x00U, 0x00U
    };
    for (std::size_t i=0; i<actualExpected.size(); ++i)
        assert(std::to_integer<std::uint8_t>(encoded[i]) == actualExpected[i]);

    std::array<std::byte, 64U> json{};
    const auto result = ESPressio::Logging::EncodeDiagnosticArgumentsJson<Schema>(
        std::span<std::byte>(json),
        std::uint16_t{300U}, true, float{1.5f}
    );
    assert(result.IsSuccessful());
    const auto written = std::string_view(
        reinterpret_cast<const char*>(json.data()), result.BytesWritten
    );
    assert(written == "[300,true,1.5]");

    std::array<std::byte, 1U> tiny{std::byte{0xCD}};
    const auto tooSmall = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(tiny),
        std::uint16_t{300U}, true, float{1.5f}
    );
    assert(tooSmall.Status == ESPressio::Serialisation::SerialisationStatus::OutputBufferTooSmall);
    assert(tiny[0] == std::byte{0xCD});
}
