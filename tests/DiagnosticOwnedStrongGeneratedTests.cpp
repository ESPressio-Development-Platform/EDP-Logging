#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticClockEraArguments.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/OwnedDiagnosticPositionalReencoding.hpp>

using StrongFixture =
    Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 5U>;

namespace L = ESPressio::Logging;
namespace S = ESPressio::System;
namespace C = ESPressio::Clock;

static_assert(
    L::DiagnosticSchemaMatches<
        StrongFixture,
        S::Identity::RuntimeIncarnationId,
        C::ClockEra
    >()
);
static_assert(
    !L::DiagnosticSchemaMatches<
        StrongFixture,
        std::uint32_t,
        C::ClockEra
    >()
);
static_assert(
    !L::DiagnosticSchemaMatches<
        StrongFixture,
        S::Identity::RuntimeIncarnationId,
        std::uint64_t
    >()
);

template<std::size_t TSize, class TValue>
void CheckNativeOwnedValue(
    std::span<const std::byte> stored,
    TValue value
) {
    static_assert(sizeof(TValue) == TSize);
    assert(stored.size() == TSize);
    std::array<std::byte, TSize> bytes{};
    for (std::size_t index = 0U; index < TSize; ++index) {
        bytes[index] = stored[index];
    }
    assert(std::bit_cast<TValue>(bytes) == value);
}

int main() {
    using Source = L::BasicLogRecord<
        L::AttemptSequenceWidth::Bits16,
        2U,
        12U
    >;

    S::Identity::DeviceIdentifier::Storage deviceBytes{};
    deviceBytes[0U] = 0xA5U;
    const S::Identity::DeviceIdentifier device{deviceBytes};

    const L::QualifiedLogTime temporal{
        C::MonotonicTimestamp::FromNanoseconds(123456U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };

    const auto attempt =
        L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::FromValidatedValue(
            27U
        );

    auto runtimeArgument = S::Identity::RuntimeIncarnationId{0x01020304U};
    auto eraArgument = C::ClockEra::FromValue(0x0102030405060708ULL);
    Source record{};

    assert((
        L::AssembleLogRecordSource<StrongFixture>(
            record,
            L::Severity::Info,
            device,
            S::Identity::RuntimeIncarnationId{7U},
            attempt,
            temporal,
            runtimeArgument,
            eraArgument
        ) == L::LogRecordAssemblyStatus::Prepared
    ));

    assert(record.Message.String == 5U);
    assert(record.Arguments.Count() == 2U);
    assert(record.Arguments.BytesUsed() == 12U);
    assert(L::ValidateLogRecord(record) == L::LogRecordValidation::Valid);

    // Destroy the original values' meaning before reading the owned bytes:
    // the source record must have copied both arguments transactionally.
    runtimeArgument = S::Identity::RuntimeIncarnationId{1U};
    eraArgument = C::ClockEra{};

    L::OwnedArgumentDescriptor first{};
    L::OwnedArgumentDescriptor second{};
    assert(record.Arguments.GetDescriptor(0U, first) ==
           L::OwnedArgumentAccessResult::Available);
    assert(record.Arguments.GetDescriptor(1U, second) ==
           L::OwnedArgumentAccessResult::Available);
    assert(first.Kind == L::OwnedArgumentKind::RuntimeIncarnationId);
    assert(second.Kind == L::OwnedArgumentKind::ClockEra);
    assert(first.Offset == 0U && first.Size == 4U);
    assert(second.Offset == 4U && second.Size == 8U);

    std::span<const std::byte> firstBytes{};
    std::span<const std::byte> secondBytes{};
    assert(record.Arguments.GetArgumentBytes(0U, firstBytes) ==
           L::OwnedArgumentAccessResult::Available);
    assert(record.Arguments.GetArgumentBytes(1U, secondBytes) ==
           L::OwnedArgumentAccessResult::Available);
    CheckNativeOwnedValue<4U>(firstBytes, std::uint32_t{0x01020304U});
    CheckNativeOwnedValue<8U>(
        secondBytes,
        std::uint64_t{0x0102030405060708ULL}
    );

    // The SAME generated input values have independent owner-encoded CBOR
    // and JSON representations; native arena bytes are not written to wire.
    const S::Identity::RuntimeIncarnationId incarnationWire{0x01020304U};
    const auto eraWire = C::ClockEra::FromValue(0x0102030405060708ULL);
    std::array<std::byte, 96U> output{};

    const auto cbor = L::EncodeDiagnosticArgumentsCbor<StrongFixture>(
        output,
        incarnationWire,
        eraWire
    );
    assert(cbor.IsSuccessful());
    const std::array<std::uint8_t, 15U> canonical{
        0x82U, 0x1AU, 0x01U, 0x02U, 0x03U, 0x04U,
        0x1BU, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U
    };
    assert(cbor.BytesWritten == canonical.size());
    for (std::size_t index = 0U; index < canonical.size(); ++index) {
        assert(std::to_integer<std::uint8_t>(output[index]) == canonical[index]);
    }

    const auto json = L::EncodeDiagnosticArgumentsJson<StrongFixture>(
        output,
        incarnationWire,
        eraWire
    );
    assert(json.IsSuccessful());
    assert(
        std::string_view(
            reinterpret_cast<const char*>(output.data()),
            json.BytesWritten
        ) == "[16909060,72623859790382856]"
    );

    // Source values were mutated above; re-encode ONLY the internally owned
    // bytes using the exact generated schema and owner strong Types.
    L::OwnedPositionalScratch<
        S::Identity::RuntimeIncarnationId,C::ClockEra> scratch{};
    auto ownedCbor = L::EncodeOwnedDiagnosticArgumentsCbor<
        StrongFixture,S::Identity::RuntimeIncarnationId,C::ClockEra
    >(record.Arguments,output,scratch);
    assert(ownedCbor.IsSuccessful());
    assert(ownedCbor.Encoding.BytesWritten==canonical.size());
    for (std::size_t i=0U;i<canonical.size();++i)
        assert(std::to_integer<std::uint8_t>(output[i])==canonical[i]);
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<1>(scratch).has_value());

    const auto ownedJson=L::EncodeOwnedDiagnosticArgumentsJson<
        StrongFixture,S::Identity::RuntimeIncarnationId,C::ClockEra
    >(record.Arguments,output,scratch);
    assert(ownedJson.IsSuccessful());
    assert(std::string_view(
        reinterpret_cast<const char*>(output.data()),
        ownedJson.Encoding.BytesWritten) ==
        "[16909060,72623859790382856]");
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<1>(scratch).has_value());

    // The native kind alone cannot be used as a wire tag: the generated
    // schema must match, despite the same private scalar byte width.
    L::OwnedDiagnosticArguments<2U,12U> wrongKind{};
    assert(wrongKind.AddScalar(std::uint32_t{0x01020304U}) ==
        L::OwnedArgumentResult::Added);
    assert(wrongKind.AddClockEra(eraWire) ==
        L::OwnedArgumentResult::Added);
    for(auto& byte:output) byte=std::byte{0xCCU};
    const auto wrong=L::EncodeOwnedDiagnosticArgumentsCbor<
        StrongFixture,S::Identity::RuntimeIncarnationId,C::ClockEra
    >(wrongKind,output,scratch);
    assert(wrong.Status==L::OwnedPositionalReencodingStatus::KindMismatch);
    assert(wrong.FirstFailedArgument==0U);
    assert(output[0]==std::byte{0xCCU});
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<1>(scratch).has_value());

    const auto overflow=L::EncodeOwnedDiagnosticArgumentsCbor<
        StrongFixture,S::Identity::RuntimeIncarnationId,C::ClockEra
    >(record.Arguments,std::span<std::byte>(output).first(2U),scratch);
    assert(overflow.Status==L::OwnedPositionalReencodingStatus::EncoderRejected);
    assert(overflow.Encoding.BytesWritten==0U);
    assert(output[0]==std::byte{0xCCU});
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<1>(scratch).has_value());

    L::OwnedDiagnosticArguments<2U,12U> wrongCount{};
    const auto count=L::EncodeOwnedDiagnosticArgumentsJson<
        StrongFixture,S::Identity::RuntimeIncarnationId,C::ClockEra
    >(wrongCount,output,scratch);
    assert(count.Status==L::OwnedPositionalReencodingStatus::CountMismatch);

    using Tiny = L::BasicLogRecord<
        L::AttemptSequenceWidth::Bits16,
        2U,
        11U
    >;
    Tiny insufficient{};
    assert((
        L::AssembleLogRecordSource<StrongFixture>(
            insufficient,
            L::Severity::Info,
            device,
            S::Identity::RuntimeIncarnationId{7U},
            attempt,
            temporal,
            incarnationWire,
            eraWire
        ) == L::LogRecordAssemblyStatus::ArgumentBytesExceeded
    ));
    assert(!insufficient.Attempt.IsValid());
    assert(insufficient.Arguments.Count() == 0U);
    assert(insufficient.Arguments.BytesUsed() == 0U);

    // Clock's zero/unknown era and System's zero incarnation are valid
    // diagnostic *facts*, not valid application root identity assertions.
    L::OwnedDiagnosticArguments<2U, 12U> unknownFacts{};
    assert(unknownFacts.AddRuntimeIncarnationId(
        S::Identity::RuntimeIncarnationId{}
    ) == L::OwnedArgumentResult::Added);
    assert(unknownFacts.AddClockEra(C::ClockEra{}) ==
           L::OwnedArgumentResult::Added);
    assert(unknownFacts.BytesUsed() == 12U);
}
