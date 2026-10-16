#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>

using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,3U>;

int main() {
    ESPressio::Bounded::Bytes<8U> wrongFirst;
    ESPressio::Bounded::String<16U> wrongSecond;
    const ESPressio::System::TypeIdentifier type{};
    const ESPressio::System::FieldIdentifier field{0U};
    std::array<std::byte,64U> destination{};
    const auto encoded =
        ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
            std::span<std::byte>(destination),
            wrongFirst,wrongSecond,type,field
        );
    static_cast<void>(encoded);
}
