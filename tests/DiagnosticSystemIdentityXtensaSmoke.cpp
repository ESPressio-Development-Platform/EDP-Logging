#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/DiagnosticArgumentEncoding.hpp>

enum class K : std::uint8_t {
    Boolean,Signed8,Signed16,Signed32,Signed64,
    Unsigned8,Unsigned16,Unsigned32,Unsigned64,
    Float32,Float64,Utf8,Bytes,TypeIdentifier,FieldIdentifier,
    StringIdentifier,DeviceIdentifier,RuntimeIncarnationId,ClockEra
};
struct Schema final {
    static constexpr bool Declared=true;
    static constexpr std::size_t ArgumentCount=2U;
    inline static constexpr std::array<K,2U> ArgumentKinds{
        K::DeviceIdentifier,K::RuntimeIncarnationId
    };
};
using Device=ESPressio::System::Identity::DeviceIdentifier;
using Incarnation=ESPressio::System::Identity::RuntimeIncarnationId;
static_assert(ESPressio::Logging::IsDiagnosticSchemaMatching<Schema,Device,Incarnation>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<Schema,Incarnation,Device>());
void Test(std::span<std::byte> output,const Device& device,const Incarnation& incarnation) noexcept {
    const auto status=ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        output,device,incarnation
    );
    static_cast<void>(status);
}
