#include <cassert>
#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string_view>
#include <type_traits>
#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>
#include <logging/LogRecordSourceAssembly.hpp>

using ActualGovernedFixture =
    Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 3U>;

static_assert(ESPressio::Logging::ScalarDiagnosticSchemaMatches<
    ActualGovernedFixture, std::uint16_t, bool
>());
static_assert(!ESPressio::Logging::ScalarDiagnosticSchemaMatches<
    ActualGovernedFixture, std::uint32_t, bool
>());
static_assert(!ESPressio::Logging::ScalarDiagnosticSchemaMatches<
    ActualGovernedFixture, std::uint16_t, bool, std::uint8_t
>());
static_assert(ActualGovernedFixture::SourceDomain == 1U);
static_assert(ActualGovernedFixture::SourceSubDomain == 0U);
static_assert(ActualGovernedFixture::SourceString == 3U);
static_assert(std::is_same_v<
    ActualGovernedFixture::OwningContract,
    Fixture::Diagnostic::GeneratedLocalisationContract
>);

int main() {
    namespace L = ESPressio::Logging;
    namespace S = ESPressio::System;
    namespace C = ESPressio::Clock;

    S::Identity::DeviceIdentifier::Storage deviceBytes{};
    deviceBytes[0U] = 0xA5U;
    const S::Identity::DeviceIdentifier device{deviceBytes};
    const S::Identity::RuntimeIncarnationId incarnation{7U};
    const auto temporal = L::QualifiedLogTime{
        C::MonotonicTimestamp::FromNanoseconds(123456U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    using Small = L::BasicLogRecord<L::AttemptSequenceWidth::Bits16, 2U, 16U>;
    const auto attempt = L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::
        FromValidatedValue(27U);
    Small record{};
    assert((L::AssembleLogRecordSource<
        ActualGovernedFixture
    >(
        record,
        L::Severity::Info,
        device,
        incarnation,
        attempt,
        temporal,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::Prepared));
    assert(record.Message.Domain == 1U);
    assert(record.Message.SubDomain == 0U);
    assert(record.Message.String == 3U);
    assert(record.Attempt.Value() == 27U);
    assert(record.SequenceWidth == 16U);
    assert(!record.Synchronized.has_value());
    assert(record.Monotonic.Nanoseconds() == 123456U);
    assert(record.Arguments.Count() == 2U);
    assert(record.DiagnosticSchemaFingerprint ==
        Fixture::Diagnostic::GeneratedLocalisationContract::DiagnosticSchemaFingerprint);
    assert(L::ValidateLogRecord(record) == L::LogRecordValidation::Valid);
    assert((L::AssembleLogRecordSource<
        ActualGovernedFixture
    >(
        record,
        L::Severity::Info,
        device,
        incarnation,
        attempt,
        temporal,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::OutputOccupied));

    Small invalidTime{};
    auto broken = temporal;
    broken.Status = L::LogTemporalEvidenceStatus::Incoherent;
    assert((L::AssembleLogRecordSource<
        ActualGovernedFixture
    >(
        invalidTime,
        L::Severity::Info,
        device,
        incarnation,
        attempt,
        broken,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::InvalidTemporalEvidence));
    assert(!invalidTime.Attempt.IsValid() && invalidTime.Arguments.Count() == 0U);
    broken = temporal;
    broken.Era = C::ClockEra::FromValue(4U);
    assert((L::AssembleLogRecordSource<ActualGovernedFixture>(
        invalidTime,
        L::Severity::Info,
        device,
        incarnation,
        attempt,
        broken,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::InvalidTemporalEvidence));
    broken = temporal;
    broken.Uncertainty = C::SynchronizationUncertainty::Zero();
    assert((L::AssembleLogRecordSource<ActualGovernedFixture>(
        invalidTime,
        L::Severity::Info,
        device,
        incarnation,
        attempt,
        broken,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::InvalidTemporalEvidence));
    assert(!invalidTime.Attempt.IsValid() &&
        invalidTime.Arguments.Count() == 0U);

    using TooSmall = L::BasicLogRecord<L::AttemptSequenceWidth::Bits16, 2U, 2U>;
    TooSmall insufficient{};
    assert((L::AssembleLogRecordSource<
        ActualGovernedFixture
    >(
        insufficient,
        L::Severity::Info,
        device,
        incarnation,
        attempt,
        temporal,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::ArgumentBytesExceeded));
    assert(!insufficient.Attempt.IsValid() &&
        insufficient.Arguments.Count() == 0U);

    Small invalidDevice{};
    assert((L::AssembleLogRecordSource<
        ActualGovernedFixture
    >(
        invalidDevice,
        L::Severity::Info,
        S::Identity::DeviceIdentifier{},
        incarnation,
        attempt,
        temporal,
        std::uint16_t{300U},
        true
    ) == L::LogRecordAssemblyStatus::InvalidSourceRecord));
    assert(!invalidDevice.Attempt.IsValid() &&
        invalidDevice.Arguments.Count() == 0U);

    std::array<std::byte, 20> data{};
    const auto cbor = ESPressio::Logging::EncodeDiagnosticArgumentsCbor<ActualGovernedFixture>(
        std::span<std::byte>(data), std::uint16_t{300U}, true
    );
    assert(cbor.IsSuccessful());
    assert(cbor.BytesWritten == 5U);
    const std::array<std::uint8_t,5> expected{0x82U,0x19U,0x01U,0x2CU,0xF5U};
    for(std::size_t i=0;i<expected.size();++i)
        assert(std::to_integer<std::uint8_t>(data[i])==expected[i]);
    const auto json = ESPressio::Logging::EncodeDiagnosticArgumentsJson<ActualGovernedFixture>(
        std::span<std::byte>(data), std::uint16_t{300U}, true
    );
    assert(json.IsSuccessful());
    assert(std::string_view(reinterpret_cast<const char*>(data.data()),json.BytesWritten)
        == "[300,true]");
}
