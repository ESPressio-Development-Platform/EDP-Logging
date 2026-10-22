#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/GeneratedLogRecordWireProjection.hpp>

inline void ReportG4TestStack(const char* phase) noexcept {
    std::printf("EDP_LOGGING_HIL|G4_STACK_PHASE|%s|minimum_remaining_bytes=%u\n",
                phase, static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

inline bool CheckG4CompleteSchemaTypedEnvelopeOnDevice() noexcept {
    namespace L=ESPressio::Logging;
    namespace C=ESPressio::Clock;
    namespace S=ESPressio::System;
    namespace Serializer=ESPressio::Serialisation;
    using Schema=Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,5U>;
    using Root=L::GeneratedLogRecordWireProjection<
        L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>;
    static_assert(Root::Fields::Count==15U);
    static_assert(S::TypeIdentifierOf<Root> ==
                  S::TypeIdentifierOf<L::BasicLogRecord<
                      L::AttemptSequenceWidth::Bits16,2U,12U>>);
    S::Identity::DeviceIdentifier::Storage id{};
    id[0]=0xA5U;
    S::Identity::DeviceIdentifier device{id};
    const L::QualifiedLogTime injected{
        C::MonotonicTimestamp::FromNanoseconds(998877U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    L::BasicLogRecord<L::AttemptSequenceWidth::Bits16,2U,12U> source{};
    auto observedIncarnation=S::Identity::RuntimeIncarnationId{0x01020304U};
    auto observedEra=C::ClockEra::FromValue(0x0102030405060708ULL);
    if(L::AssembleLogRecordSource<Schema>(
        source,L::Severity::Warning,device,
        S::Identity::RuntimeIncarnationId{7U},
        L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::FromValidatedValue(27U),
        injected,observedIncarnation,observedEra
    )!=L::LogRecordAssemblyStatus::Prepared) return false;
    if(L::ValidateLogRecord(source)!=L::LogRecordValidation::Valid)
        return false;
    observedIncarnation=S::Identity::RuntimeIncarnationId{1U};
    observedEra=C::ClockEra{};
    Root projection{};
    L::OwnedPositionalScratch<S::Identity::RuntimeIncarnationId,C::ClockEra>
        scratch{};
    if(!L::ProjectGeneratedLogRecord<
        Schema,L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>(
        source,projection,scratch).IsSuccessful()) return false;
    if(std::get<0>(projection.Arguments)!=0x01020304U ||
       std::get<1>(projection.Arguments)!=0x0102030405060708ULL)
       return false;
    if(std::get<0>(scratch).has_value() || std::get<1>(scratch).has_value())
       return false;
    ReportG4TestStack("projection");
    std::array<std::byte,1024U> bytes{};
    const auto cb=Serializer::Serialise<
        Serializer::Cbor,Serializer::RootProfile::TypedEnvelope>(
        projection,bytes);
    ReportG4TestStack("cbor_encode");
    if(!cb.IsSuccessful() || cb.BytesWritten==0U) return false;
    Root decoded{};
    const auto cbDecoded=Serializer::Deserialise<
        Serializer::Cbor,Serializer::RootProfile::TypedEnvelope>(
        std::span<const std::byte>(bytes.data(),cb.BytesWritten),decoded);
    ReportG4TestStack("cbor_decode");
    if(!cbDecoded.IsSuccessful() || decoded.Arguments!=projection.Arguments ||
       decoded.DiagnosticSchemaFingerprint!=source.DiagnosticSchemaFingerprint ||
       decoded.Message.String!=source.Message.String)
       return false;
    const auto js=Serializer::Serialise<
        Serializer::Json,Serializer::RootProfile::TypedEnvelope>(
        projection,bytes);
    ReportG4TestStack("json_encode");
    if(!js.IsSuccessful() || js.BytesWritten==0U) return false;
    const std::string_view view{
        reinterpret_cast<const char*>(bytes.data()),js.BytesWritten};
    if(view.find("\"13\":[16909060,72623859790382856]")==
            std::string_view::npos ||
       view.find("0000010000000001")==std::string_view::npos)
       return false;
    decoded={};
    const auto jsDecoded=Serializer::Deserialise<
        Serializer::Json,Serializer::RootProfile::TypedEnvelope>(
        std::span<const std::byte>(bytes.data(),js.BytesWritten),decoded);
    ReportG4TestStack("json_decode");
    if(!jsDecoded.IsSuccessful() || decoded.Arguments!=projection.Arguments ||
       decoded.Attempt.Value()!=source.Attempt.Value()) return false;
    // Schema fingerprint is the historical position interpretation key.
    auto mismatched=source;
    mismatched.DiagnosticSchemaFingerprint[0]^=1U;
    if(L::ProjectGeneratedLogRecord<
        Schema,L::AttemptSequenceWidth::Bits16,
        S::Identity::RuntimeIncarnationId,C::ClockEra>(
        mismatched,projection,scratch).Status!=
        L::GeneratedLogRecordProjectionStatus::DiagnosticFingerprintMismatch)
        return false;
    return !std::get<0>(scratch).has_value() &&
           !std::get<1>(scratch).has_value();
}
