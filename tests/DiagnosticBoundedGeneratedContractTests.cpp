#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/OwnedDiagnosticPositionalReencoding.hpp>

using Schema = Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,3U>;
using Text = ESPressio::Bounded::String<16U>;
using Bytes = ESPressio::Bounded::Bytes<8U>;
using TypeId = ESPressio::System::TypeIdentifier;
using FieldId = ESPressio::System::FieldIdentifier;

static_assert(ESPressio::Serialisation::SerialisableType<Text>);
static_assert(ESPressio::Serialisation::SerialisableType<Bytes>);
static_assert(ESPressio::Serialisation::SerialisableType<TypeId>);
static_assert(ESPressio::Serialisation::SerialisableType<FieldId>);
static_assert(ESPressio::Logging::IsDiagnosticSchemaMatching<
    Schema,Text,Bytes,TypeId,FieldId
>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<
    Schema,Bytes,Text,TypeId,FieldId
>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<
    Schema,Text,Bytes,FieldId,TypeId
>());
static_assert(!ESPressio::Logging::IsDiagnosticSchemaMatching<
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
    const auto expectedCbor=destination;
    const auto cborSize=cbor.BytesWritten;

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
    const auto expectedJson=destination;
    const auto jsonSize=json.BytesWritten;

    // Real generated source schema: all four heterogeneous types remain
    // fully owned and correctly kind-tagged in the PRIVATE source arena.
    namespace L = ESPressio::Logging;
    namespace S = ESPressio::System;
    namespace C = ESPressio::Clock;
    S::Identity::DeviceIdentifier::Storage deviceBytes{};
    deviceBytes[0U] = 0x77U;
    const S::Identity::DeviceIdentifier device{deviceBytes};
    const S::Identity::RuntimeIncarnationId incarnation{4U};
    const L::QualifiedLogTime time{
        C::MonotonicTimestamp::FromNanoseconds(91U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    const auto ticket = L::AttemptSequence<>::
        FromValidatedValue(9U);
    L::BasicLogRecord<L::AttemptSequenceWidth::Bits32, 4U, 32U> record{};
    assert((L::AssembleLogRecordSource<Schema>(
        record, L::Severity::Info, device, incarnation, ticket,
        time, text, bytes, type, field
    ) == L::LogRecordAssemblyStatus::Prepared));
    assert(record.Message.Domain == 1U && record.Message.String == 3U);
    assert(record.DiagnosticSchemaFingerprint ==
        Schema::OwningContract::DiagnosticSchemaFingerprint);
    assert(record.Arguments.Count() == 4U && record.Arguments.BytesUsed() == 13U);
    L::OwnedArgumentDescriptor descriptor{};
    assert(record.Arguments.GetDescriptor(0U, descriptor) ==
        L::OwnedArgumentAccessResult::Available);
    assert(descriptor.Kind == L::OwnedArgumentKind::Utf8);
    assert(record.Arguments.GetDescriptor(1U, descriptor) ==
        L::OwnedArgumentAccessResult::Available);
    assert(descriptor.Kind == L::OwnedArgumentKind::Bytes);
    assert(record.Arguments.GetDescriptor(2U, descriptor) ==
        L::OwnedArgumentAccessResult::Available);
    assert(descriptor.Kind == L::OwnedArgumentKind::TypeIdentifier);
    assert(record.Arguments.GetDescriptor(3U, descriptor) ==
        L::OwnedArgumentAccessResult::Available);
    assert(descriptor.Kind == L::OwnedArgumentKind::FieldIdentifier);
    assert(L::ValidateLogRecord(record) == L::LogRecordValidation::Valid);
    assert(text.Assign(std::string_view("Modified")) ==
        ESPressio::Bounded::StringAssignmentResult::Succeeded);
    assert(bytes.Assign(std::array<std::uint8_t,1U>{0xFFU}) ==
        ESPressio::Bounded::BytesAssignmentResult::Succeeded);
    std::span<const std::byte> copied{};
    assert(record.Arguments.GetArgumentBytes(0U, copied) ==
        L::OwnedArgumentAccessResult::Available);
    assert(copied.size() == 2U && copied[0U] == std::byte{'H'});
    assert(record.Arguments.GetArgumentBytes(1U, copied) ==
        L::OwnedArgumentAccessResult::Available);
    assert(copied.size() == 2U && copied[0U] == std::byte{0xA0U});

    // The source's private bounded text/bytes must rehydrate into owner
    // Types and produce identical CBOR/JSON even after input mutation.
    L::OwnedPositionalScratch<Text,Bytes,TypeId,FieldId> ownedScratch{};
    const auto rebuiltCbor=L::EncodeOwnedDiagnosticArgumentsCbor<
        Schema,Text,Bytes,TypeId,FieldId
    >(record.Arguments,destination,ownedScratch);
    assert(rebuiltCbor.IsSuccessful() &&
           rebuiltCbor.Encoding.BytesWritten==cborSize);
    for (std::size_t i=0U;i<cborSize;++i)
        assert(destination[i]==expectedCbor[i]);
    assert(!std::get<0>(ownedScratch).has_value());
    assert(!std::get<3>(ownedScratch).has_value());

    const auto rebuiltJson=L::EncodeOwnedDiagnosticArgumentsJson<
        Schema,Text,Bytes,TypeId,FieldId
    >(record.Arguments,destination,ownedScratch);
    assert(rebuiltJson.IsSuccessful() &&
           rebuiltJson.Encoding.BytesWritten==jsonSize);
    for (std::size_t i=0U;i<jsonSize;++i)
        assert(destination[i]==expectedJson[i]);
    assert(!std::get<0>(ownedScratch).has_value());
    assert(!std::get<3>(ownedScratch).has_value());

    L::BasicLogRecord<L::AttemptSequenceWidth::Bits32, 4U, 12U> insufficient{};
    assert((L::AssembleLogRecordSource<Schema>(
        insufficient, L::Severity::Info, device, incarnation, ticket,
        time, text, bytes, type, field
    ) == L::LogRecordAssemblyStatus::ArgumentBytesExceeded));
    assert(!insufficient.Attempt.IsValid() && insufficient.Arguments.Count() == 0U);
    L::BasicLogRecord<L::AttemptSequenceWidth::Bits32, 4U, 32U> badStrong{};
    assert((L::AssembleLogRecordSource<Schema>(
        badStrong, L::Severity::Info, device, incarnation, ticket,
        time, text, bytes, TypeId{}, field
    ) == L::LogRecordAssemblyStatus::InvalidOwnedArgument));
    assert(!badStrong.Attempt.IsValid() && badStrong.Arguments.Count() == 0U);
}
