#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>

using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,3U>;
using Text = ESPressio::Bounded::String<16U>;
using Bytes = ESPressio::Bounded::Bytes<8U>;
using TypeId = ESPressio::System::TypeIdentifier;
using FieldId = ESPressio::System::FieldIdentifier;

static_assert(ESPressio::Serialisation::SerialisableType<Text>);
static_assert(ESPressio::Serialisation::SerialisableType<Bytes>);
static_assert(ESPressio::Serialisation::SerialisableType<TypeId>);
static_assert(ESPressio::Serialisation::SerialisableType<FieldId>);
static_assert(ESPressio::Logging::DiagnosticSchemaMatches<
    Schema,Text,Bytes,TypeId,FieldId
>());
static_assert(!ESPressio::Logging::DiagnosticSchemaMatches<
    Schema,Bytes,Text,TypeId,FieldId
>());
static_assert(!ESPressio::Logging::DiagnosticSchemaMatches<
    Schema,Text,Bytes,FieldId,TypeId
>());
static_assert(!ESPressio::Logging::DiagnosticSchemaMatches<
    Schema,Text,Bytes,TypeId
>());

int main() {
    Text text;
    assert(text.Assign(std::string_view("Hi")) ==
        ESPressio::Bounded::StringAssignmentResult::Succeeded);
    Bytes bytes;
    assert(bytes.Assign(std::array<std::uint8_t,2U>{0xA0U,0x01U}) ==
        ESPressio::Bounded::BytesAssignmentResult::Succeeded);
    constexpr TypeId::Storage typeBytes{
        0x01U,0x02U,0x03U,0x04U,0x05U,0x06U,0x07U,0x08U
    };
    const TypeId type{typeBytes}; // TEST FIXTURE ONLY; not LogRecord identity.
    const FieldId field{0U};

    std::array<std::byte,160U> destination{};
    destination.fill(std::byte{0xA5U});
    const auto tooSmall =
        ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
            std::span<std::byte>(destination.data(),1U),text,bytes,type,field
        );
    assert(!tooSmall.IsSuccessful());
    for(const auto value:destination) assert(value==std::byte{0xA5U});

    const auto cbor =
        ESPressio::Logging::EncodeDiagnosticArgumentsCbor<Schema>(
            destination,text,bytes,type,field
        );
    assert(cbor.IsSuccessful());
    assert(cbor.BytesWritten != 0U);
    // Four separately typed positional elements in a definite CBOR array.
    assert(destination[0U] == std::byte{0x84U});

    const auto json =
        ESPressio::Logging::EncodeDiagnosticArgumentsJson<Schema>(
            destination,text,bytes,type,field
        );
    assert(json.IsSuccessful());
    assert(json.BytesWritten > 4U);
    const std::string_view result{
        reinterpret_cast<const char*>(destination.data()),json.BytesWritten
    };
    assert(result.starts_with("[\"Hi\","));
    assert(result.ends_with("]"));
}
