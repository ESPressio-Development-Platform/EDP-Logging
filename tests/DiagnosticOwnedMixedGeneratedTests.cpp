#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#include <GeneratedLocalisationContract.hpp>
#include <logging/DiagnosticArgumentEncoding.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/OwnedDiagnosticPositionalReencoding.hpp>

namespace L=ESPressio::Logging;
namespace S=ESPressio::System;
namespace C=ESPressio::Clock;
using Schema=Fixture::Diagnostic::DiagnosticMessageSchema<1U,0U,4U>;
using Device=S::Identity::DeviceIdentifier;
static_assert(L::IsDiagnosticSchemaMatching<Schema,Device,bool,std::uint16_t,float,double>());

int main() {
    Device::Storage id{};
    id[0]=0xA5U; id[15]=0x3AU;
    Device source{id};
    bool active=true;
    std::uint16_t count=513U;
    float scalar=1.5F;
    double fraction=-3.25;
    const auto temporal=L::QualifiedLogTime{
        C::MonotonicTimestamp::FromNanoseconds(808U),
        C::SynchronizedTimestamp{},
        C::SynchronizationUncertainty::Maximum(),
        C::ClockEra{},
        C::SynchronizationState::NeverSynchronized,
        L::LogTemporalEvidenceStatus::Unavailable
    };
    const auto ticket=L::AttemptSequence<L::AttemptSequenceWidth::Bits16>::
        FromValidatedValue(9U);
    using Record=L::BasicLogRecord<L::AttemptSequenceWidth::Bits16,5U,31U>;
    Record record{};
    assert((L::AssembleLogRecordSource<Schema>(
        record,L::Severity::Info,source,
        S::Identity::RuntimeIncarnationId{2U},
        ticket,temporal,source,active,count,scalar,fraction
    )==L::LogRecordAssemblyStatus::Prepared));
    assert(record.Arguments.Count()==5U);
    assert(record.Arguments.BytesUsed()==31U);
    assert(L::ValidateLogRecord(record)==L::LogRecordValidation::Valid);

    std::array<std::byte,256U> direct{};
    std::array<std::byte,256U> stored{};
    const auto cb=L::EncodeDiagnosticArgumentsCbor<Schema>(
        direct,source,active,count,scalar,fraction);
    assert(cb.IsSuccessful() && cb.BytesWritten>0U);
    const auto expectedCbor=direct;
    const auto js=L::EncodeDiagnosticArgumentsJson<Schema>(
        direct,source,active,count,scalar,fraction);
    assert(js.IsSuccessful() && js.BytesWritten>0U);
    const auto expectedJson=direct;

    // No borrowed source remains after assembly; mutate every input.
    id.fill(0U);
    source=Device{id};
    active=false;
    count=0U;
    scalar=0.0F;
    fraction=0.0;

    L::OwnedPositionalScratch<Device,bool,std::uint16_t,float,double> scratch{};
    const auto ownedCbor=L::EncodeOwnedDiagnosticArgumentsCbor<
        Schema,Device,bool,std::uint16_t,float,double>(
        record.Arguments,stored,scratch);
    assert(ownedCbor.IsSuccessful() &&
           ownedCbor.Encoding.BytesWritten==cb.BytesWritten);
    for (std::size_t i=0U;i<cb.BytesWritten;++i)
        assert(stored[i]==expectedCbor[i]);
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<4>(scratch).has_value());

    const auto ownedJson=L::EncodeOwnedDiagnosticArgumentsJson<
        Schema,Device,bool,std::uint16_t,float,double>(
        record.Arguments,stored,scratch);
    assert(ownedJson.IsSuccessful() &&
           ownedJson.Encoding.BytesWritten==js.BytesWritten);
    for (std::size_t i=0U;i<js.BytesWritten;++i)
        assert(stored[i]==expectedJson[i]);
    assert(!std::get<0>(scratch).has_value());
    assert(!std::get<4>(scratch).has_value());

    L::OwnedDiagnosticArguments<5U,31U> missing{};
    const auto refused=L::EncodeOwnedDiagnosticArgumentsCbor<
        Schema,Device,bool,std::uint16_t,float,double>(
        missing,stored,scratch);
    assert(refused.Status==L::OwnedPositionalReencodingStatus::CountMismatch);
}
