#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>

using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,3U>;
using Device = ESPressio::System::Identity::DeviceIdentifier;
using Incarnation = ESPressio::System::Identity::RuntimeIncarnationId;

static_assert(ESPressio::Serialisation::SerialisableType<Device>);
static_assert(ESPressio::Serialisation::SerialisableType<Incarnation>);
static_assert(ESPressio::Logging::DiagnosticSchemaMatches<Schema,Device,Incarnation>());
static_assert(!ESPressio::Logging::DiagnosticSchemaMatches<Schema,Incarnation,Device>());
static_assert(!ESPressio::Logging::DiagnosticSchemaMatches<Schema,Device>());

int main() {
    constexpr Device::Storage bytes{
        0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,10U,11U,12U,13U,14U,15U
    };
    const Device device{bytes};
    const Incarnation incarnation{0x01020304U};
    std::array<std::byte,80U> destination{};
    destination.fill(std::byte{0xACU});
    const auto denied = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(destination.data(),4U),device,incarnation
    );
    assert(denied.Status==ESPressio::Serialisation::SerialisationStatus::OutputBufferTooSmall);
    for (const auto byte:destination) assert(byte==std::byte{0xACU});

    const auto encoded = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
        std::span<std::byte>(destination),device,incarnation
    );
    assert(encoded.Status==ESPressio::Serialisation::SerialisationStatus::Succeeded);
    assert(encoded.BytesWritten==23U);
    assert(destination[0]==std::byte{0x82U});
    assert(destination[1]==std::byte{0x50U});
    for (std::size_t i=0U; i<16U; ++i) {
        assert(destination[i+2U]==std::byte{static_cast<std::uint8_t>(i)});
    }
    assert(destination[18]==std::byte{0x1AU});
    assert(destination[19]==std::byte{0x01U});
    assert(destination[20]==std::byte{0x02U});
    assert(destination[21]==std::byte{0x03U});
    assert(destination[22]==std::byte{0x04U});

    const auto json = ESPressio::Logging::EncodeDiagnosticArgumentsJson<Schema>(
        std::span<std::byte>(destination),device,incarnation
    );
    assert(json.Status==ESPressio::Serialisation::SerialisationStatus::Succeeded);
    const std::string_view output{
        reinterpret_cast<const char*>(destination.data()),json.BytesWritten
    };
    assert(output=="[\"AAECAwQFBgcICQoLDA0ODw==\",16909060]");
}
