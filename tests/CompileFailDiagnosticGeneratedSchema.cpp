#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>
using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 3U>;
int main() {
 std::array<std::byte,32U> output{};
 const auto result = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
  std::span<std::byte>(output),std::uint32_t{300U},true
 );
 return result.IsSuccessful() ? 0 : 1;
}
