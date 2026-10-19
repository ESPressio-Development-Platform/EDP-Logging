#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <localisation/LocalisationTypes.hpp>
#include <type_traits>
#include <ESPressio_System.hpp>
#include <logging/LogRecord.hpp>

namespace S=ESPressio::System;
namespace L=ESPressio::Logging;

static_assert(S::IdentifiedType<L::LogRecord>);
static_assert(S::SchemaType<L::LogRecord>);
static_assert(S::SchemaType<L::BasicLogRecord<L::AttemptSequenceWidth::Bits16,2U,16U>>);
static_assert(S::SchemaType<L::BasicLogRecord<L::AttemptSequenceWidth::Bits64,16U,512U>>);
static_assert(std::is_default_constructible_v<L::LogRecord>);
static_assert(S::TypeIdentifierOf<L::LogRecord>.Bytes()==
    S::TypeIdentifier::Storage{0U,0U,1U,0U,0U,0U,0U,1U});
static_assert(S::TypeIdentifierOf<L::LogRecord>.Authority().Bytes()==
    S::TypeAuthorityIdentifier::Storage{0U,0U,1U});
static_assert(S::TypeIdentifierOf<L::LogRecord>.LocalIdentifier().Bytes()==
    S::TypeLocalIdentifier::Storage{0U,0U,0U,0U,1U});
static_assert(L::LogRecord::Fields::Count==15U);

// Real owner-defined Localisation widths (1/2/4), never a Logging-invented
// reinterpretation or hash of the generated message StringIdentifier.
struct TestLocalisationContract final {
    static constexpr std::size_t DomainIdentifierBytes=1U;
    static constexpr std::size_t SubDomainIdentifierBytes=2U;
    static constexpr std::size_t StringIdentifierBytes=4U;
    static constexpr std::size_t TypeIdentifierBytes=0U;
    static constexpr std::size_t FieldIdentifierBytes=0U;
};
using GeneratedIds=
    ESPressio::Localisation::ContractIdentifiers<TestLocalisationContract>;
static constexpr GeneratedIds::GeneralStringIdentifier GeneratedMessage{
    GeneratedIds::DomainIdentifier{9U},
    GeneratedIds::SubDomainIdentifier{259U},
    GeneratedIds::StringIdentifierValue{0x12345678U}
};
static constexpr auto WidenedMessage=
    L::LogMessageIdentifier::From<GeneratedIds>(GeneratedMessage);
static_assert(WidenedMessage.Domain==9U &&
              WidenedMessage.SubDomain==259U &&
              WidenedMessage.String==0x12345678U);

consteval std::array<std::uint8_t,15U> FieldIds() {
    std::array<std::uint8_t,15U> values{};
    std::size_t index=0U;
    S::ForEachField<L::LogRecord>([&]<class TField>() {
        values[index++]=TField::Identifier.Value();
    });
    return values;
}
static_assert(FieldIds()==
    std::array<std::uint8_t,15U>{0U,1U,2U,3U,4U,5U,6U,7U,
                                  8U,9U,10U,11U,12U,13U,14U});

int main() {
    using Result=L::OwnedArgumentResult;
    L::OwnedDiagnosticArguments<4U,28U> arguments;
    assert(arguments.AddScalar(std::uint64_t{0x1122334455667788ULL})==
           Result::Added);
    char text[]={'h','e','l','l','o'};
    assert(arguments.AddUtf8({text,5U})==Result::Added);
    text[0]='X';
    assert(arguments.Count()==2U);
    std::span<const std::byte> borrowed{};
    assert(arguments.GetArgumentBytes(1U,borrowed)==
           L::OwnedArgumentAccessResult::Available);
    assert(borrowed.size()==5U);
    assert(borrowed[0U]==std::byte{'h'});
    assert(arguments.AddUtf8({"\xC0\xAF",2U})==Result::InvalidUtf8);
    assert(arguments.Count()==2U);
    std::array<std::byte,20U> tooBig{};
    assert(arguments.AddBytes(tooBig)==Result::BytesExceeded);
    assert(arguments.Count()==2U);
    assert(arguments.AddScalar(std::int16_t{-27})==Result::Added);
    assert(arguments.AddScalar(true)==Result::Added);
    assert(arguments.AddScalar(std::uint8_t{1U})==Result::CountExceeded);
    borrowed={};
    assert(arguments.GetArgumentBytes(100U,borrowed)==
           L::OwnedArgumentAccessResult::OutOfRange);
    assert(borrowed.empty());
    L::OwnedArgumentDescriptor descriptor{};
    assert(arguments.GetDescriptor(100U,descriptor)==
           L::OwnedArgumentAccessResult::OutOfRange);
    assert(arguments.GetDescriptor(1U,descriptor)==
           L::OwnedArgumentAccessResult::Available);
    assert(descriptor.Kind==L::OwnedArgumentKind::Utf8);
    const auto copied=arguments;
    assert(copied.Count()==4U);
    std::span<const std::byte> copiedBytes{};
    assert(copied.GetArgumentBytes(1U,copiedBytes)==
           L::OwnedArgumentAccessResult::Available);
    assert(copiedBytes.front()==std::byte{'h'});

    L::OwnedDiagnosticArguments<1U,8U> bounded;
    std::array<std::byte,9U> nine{};
    assert(bounded.AddBytes(nine)==Result::BytesExceeded);
    assert(bounded.Count()==0U && bounded.BytesUsed()==0U);
    std::array<std::byte,8U> eight{};
    assert(bounded.AddBytes(eight)==Result::Added);
    assert(bounded.Count()==1U && bounded.BytesUsed()==8U);
    assert(bounded.AddScalar(std::uint8_t{1U})==Result::CountExceeded);
    assert(bounded.Count()==1U && bounded.BytesUsed()==8U);
    L::OwnedDiagnosticArguments<2U,8U> validText;
    assert(validText.AddUtf8({"\xED\xA0\x80",3U})==Result::InvalidUtf8);
    assert(validText.AddUtf8({"\xF4\x90\x80\x80",4U})==Result::InvalidUtf8);
    assert(validText.AddUtf8({"\xE2\x82",2U})==Result::InvalidUtf8);
    assert(validText.Count()==0U && validText.BytesUsed()==0U);
    assert(validText.AddUtf8({"\xF0\x9F\x98\x80",4U})==Result::Added);
    assert(validText.Count()==1U && validText.BytesUsed()==4U);

    L::LogRecord record{};
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidDevice);
    auto device=S::Identity::DeviceIdentifier::Storage{};
    device[0U]=0xABU;
    record.Device=S::Identity::DeviceIdentifier{device};
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidIncarnation);
    record.Incarnation=S::Identity::RuntimeIncarnationId{1U};
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidAttempt);
    record.Attempt=L::AttemptSequence<L::AttemptSequenceWidth::Bits32>::
        FromValidatedValue(42U);
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidMessage);
    record.Message.String=3U;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);
    record.SequenceWidth=16U;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidSequenceWidth);
    record.SequenceWidth=32U;
    record.SyncState=ESPressio::Clock::SynchronizationState::Synchronized;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidSyncEvidence);
    record.Synchronized=ESPressio::Clock::SynchronizedTimestamp::FromNanoseconds(0U);
    record.Era=ESPressio::Clock::ClockEra::FromValue(2U);
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);
    record.Era={};
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidSyncEvidence);
    record.Era=ESPressio::Clock::ClockEra::FromValue(2U);
    record.Level=static_cast<L::Severity>(254U);
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidSeverity);
    record.Level=L::Severity::Info;
    record.SyncState=static_cast<ESPressio::Clock::SynchronizationState>(255U);
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidSyncEvidence);
    record.SyncState=ESPressio::Clock::SynchronizationState::Synchronized;
    record.SequenceWidth=32U;
    record.Message.String=2U;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidMessage);
    record.Message.String=3U;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);

    // Unknown source fact bits must never become certified persistent
    // semantics merely because the containing FieldIdentifiers are issued.
    record.Provenance.Projection=static_cast<ESPressio::Clock::ClockCorrelationProjectionStatus>(255U);
    assert(L::ValidateLogRecord(record)==
           L::LogRecordValidation::InvalidTemporalProvenance);
    record.Provenance.Projection={};
    record.Facts.Value=1U;
    assert(L::ValidateLogRecord(record)==
           L::LogRecordValidation::ReservedRecordFacts);
    record.Facts.Value=0U;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);

    // Incoherent/missing synchronized observations MUST retain saturated,
    // unknown uncertainty, never imply a precise synchronized coordinate.
    record.SyncState=ESPressio::Clock::SynchronizationState::NeverSynchronized;
    record.Era={};
    record.Synchronized.reset();
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);
    record.Uncertainty=ESPressio::Clock::SynchronizationUncertainty::Zero();
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::InvalidSyncEvidence);
    record.Uncertainty=ESPressio::Clock::SynchronizationUncertainty::Maximum();
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);
    record.SyncState=ESPressio::Clock::SynchronizationState::LostSynchronization;
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);
    return 0;
}
