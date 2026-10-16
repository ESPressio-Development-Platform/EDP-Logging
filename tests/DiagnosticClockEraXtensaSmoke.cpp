#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <logging/DiagnosticClockEraArguments.hpp>

enum class GeneratedKind : std::uint8_t {
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
    Float64 = 10U,
    Utf8 = 11U,
    Bytes = 12U,
    TypeIdentifier = 13U,
    FieldIdentifier = 14U,
    StringIdentifier = 15U,
    DeviceIdentifier = 16U,
    RuntimeIncarnationId = 17U,
    ClockEra = 18U
};

struct CompileOnlyClockEraSchema final {
    static constexpr bool Declared = true;
    static constexpr std::size_t ArgumentCount = 1U;
    inline static constexpr std::array<GeneratedKind,1U> ArgumentKinds{
        GeneratedKind::ClockEra
    };
};

using Era = ESPressio::Clock::ClockEra;
static_assert(ESPressio::Logging::DiagnosticSchemaMatches<
    CompileOnlyClockEraSchema,Era
>());
static_assert(!ESPressio::Logging::DiagnosticSchemaMatches<
    CompileOnlyClockEraSchema,std::uint64_t
>());

void CompileOnly(
    std::span<std::byte> output,
    const Era& era
) noexcept {
    // This only proves C++20 target template compilation, not Era authority.
    const auto encoded = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<
        CompileOnlyClockEraSchema
    >(output,era);
    static_cast<void>(encoded);
}
