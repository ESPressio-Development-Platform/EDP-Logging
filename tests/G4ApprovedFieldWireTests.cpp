#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <ESPressio_Serialisation.hpp>
#include <logging/LogRecordApprovedFieldsSerialisation.hpp>

namespace L = ESPressio::Logging;
namespace S = ESPressio::Serialisation;
namespace C = ESPressio::Clock;

template<class Codec, class Value>
void VerifyRoundTrip(const Value& original) {
    std::array<std::byte, 512U> bytes{};
    const auto measured=S::Measure<Codec>(original);
    assert(measured.IsSuccessful());
    assert(measured.RequiredBytes>0U && measured.RequiredBytes<=bytes.size());
    const auto written=S::Serialise<Codec>(original,bytes);
    assert(written.IsSuccessful());
    assert(written.BytesWritten==measured.RequiredBytes);
    Value decoded{};
    const auto result=S::Deserialise<Codec>(
        std::span<const std::byte>(bytes.data(),written.BytesWritten),decoded
    );
    assert(result.IsSuccessful());
    if constexpr (std::is_same_v<Value,L::LogTemporalProvenance>) {
        assert(original.Projection==decoded.Projection);
        assert(original.Quality==decoded.Quality);
    } else {
        assert(original.Value==decoded.Value);
    }
}

void TestApprovedValues() {
    for(std::uint8_t p=0U;p<=3U;++p) {
        for(std::uint8_t q=0U;q<=2U;++q) {
            L::LogTemporalProvenance record{
                static_cast<C::ClockCorrelationProjectionStatus>(p),
                static_cast<C::TemporalQualityAssessment>(q)
            };
            assert(L::IsApprovedProvenance(record));
            VerifyRoundTrip<S::Cbor>(record);
            VerifyRoundTrip<S::Json>(record);
        }
    }
    VerifyRoundTrip<S::Cbor>(L::LogRecordFacts{});
    VerifyRoundTrip<S::Json>(L::LogRecordFacts{});
}

template<class Codec, class Value, std::size_t N>
void RejectDecode(const std::array<std::uint8_t,N>& bytes,Value& destination) {
    const auto outcome=S::Deserialise<Codec>(std::as_bytes(std::span(bytes)),destination);
    assert(!outcome.IsSuccessful());
}

void TestReservedAndMalformed() {
    L::LogTemporalProvenance invalidProjection{
        static_cast<C::ClockCorrelationProjectionStatus>(4U),
        C::TemporalQualityAssessment::Unreliable
    };
    L::LogTemporalProvenance invalidQuality{
        C::ClockCorrelationProjectionStatus::Correlated,
        static_cast<C::TemporalQualityAssessment>(3U)
    };
    assert(!L::IsApprovedProvenance(invalidProjection));
    assert(!L::IsApprovedProvenance(invalidQuality));
    assert(!S::Measure<S::Cbor>(invalidProjection).IsSuccessful());
    assert(!S::Measure<S::Json>(invalidProjection).IsSuccessful());
    assert(!S::Measure<S::Cbor>(invalidQuality).IsSuccessful());
    assert(!S::Measure<S::Json>(invalidQuality).IsSuccessful());

    L::LogRecordFacts invalidFacts{1U};
    assert(!S::Measure<S::Cbor>(invalidFacts).IsSuccessful());
    assert(!S::Measure<S::Json>(invalidFacts).IsSuccessful());

    L::LogTemporalProvenance dest{};
    RejectDecode<S::Cbor>(std::array<std::uint8_t,3U>{0x82U,4U,0U},dest);
    RejectDecode<S::Cbor>(std::array<std::uint8_t,3U>{0x82U,0U,3U},dest);
    RejectDecode<S::Cbor>(std::array<std::uint8_t,2U>{0x81U,0U},dest);
    RejectDecode<S::Cbor>(std::array<std::uint8_t,4U>{0x83U,0U,0U,0U},dest);

    L::LogRecordFacts facts{};
    RejectDecode<S::Cbor>(std::array<std::uint8_t,1U>{1U},facts);
    RejectDecode<S::Cbor>(std::array<std::uint8_t,2U>{0x18U,0xffU},facts);
    assert(facts.Value==0U);
}

void TestCanonicalBytes() {
    const L::LogTemporalProvenance pair{
        C::ClockCorrelationProjectionStatus::Correlated,
        C::TemporalQualityAssessment::Unreliable
    };
    std::array<std::byte,32U> bytes{};
    auto b=S::Serialise<S::Cbor>(pair,bytes);
    assert(b.IsSuccessful() && b.BytesWritten==3U);
    assert(bytes[0]==std::byte{0x82U});
    assert(bytes[1]==std::byte{0U});
    assert(bytes[2]==std::byte{0U});
    b=S::Serialise<S::Json>(pair,bytes);
    assert(b.IsSuccessful() && b.BytesWritten==5U);
    assert(bytes[0]==std::byte{'['} && bytes[1]==std::byte{'0'});
    assert(bytes[2]==std::byte{','} && bytes[3]==std::byte{'0'});
    assert(bytes[4]==std::byte{']'});
    b=S::Serialise<S::Cbor>(L::LogRecordFacts{},bytes);
    assert(b.IsSuccessful() && b.BytesWritten==1U && bytes[0]==std::byte{0U});
    b=S::Serialise<S::Json>(L::LogRecordFacts{},bytes);
    assert(b.IsSuccessful() && b.BytesWritten==1U && bytes[0]==std::byte{'0'});
}
int main() {
    TestApprovedValues();
    TestReservedAndMalformed();
    TestCanonicalBytes();
}
