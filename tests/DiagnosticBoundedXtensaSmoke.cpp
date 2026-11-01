#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <logging/DiagnosticArgumentEncoding.hpp>

enum class DiagnosticKind : std::uint8_t {
    Boolean, Signed8, Signed16, Signed32, Signed64,
    Unsigned8, Unsigned16, Unsigned32, Unsigned64,
    Float32, Float64, Utf8, Bytes, TypeIdentifier, FieldIdentifier
};
struct DiagnosticSchema final {
    static constexpr bool Declared=true;
    static constexpr std::size_t ArgumentCount=4U;
    inline static constexpr std::array<DiagnosticKind,4U> ArgumentKinds{
        DiagnosticKind::Utf8, DiagnosticKind::Bytes,
        DiagnosticKind::TypeIdentifier, DiagnosticKind::FieldIdentifier
    };
};
static_assert(ESPressio::Logging::IsDiagnosticSchemaMatching<
    DiagnosticSchema,
    ESPressio::Bounded::String<12U>,
    ESPressio::Bounded::Bytes<8U>,
    ESPressio::System::TypeIdentifier,
    ESPressio::System::FieldIdentifier
>());
void Probe(
    const ESPressio::Bounded::String<12U>& text,
    const ESPressio::Bounded::Bytes<8U>& bytes,
    const ESPressio::System::TypeIdentifier& type,
    const ESPressio::System::FieldIdentifier& field,
    std::span<std::byte> buffer
) noexcept {
    const auto result = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<
        DiagnosticSchema
    >(buffer,text,bytes,type,field);
    static_cast<void>(result);
}
