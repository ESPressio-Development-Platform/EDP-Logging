#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/LogRecordCoreFieldSerialisation.hpp>

namespace L=ESPressio::Logging;
namespace C=ESPressio::Clock;
namespace S=ESPressio::Serialisation;

template<class Codec,class T,class Check>
void RoundTrip(const T& original, Check verify) {
    std::array<std::byte,256U> buffer{};
    const auto measure=S::Measure<Codec>(original);
    assert(measure.IsSuccessful());
    assert(measure.RequiredBytes>0U && measure.RequiredBytes<=buffer.size());
    const auto encoded=S::Serialise<Codec>(original,buffer);
    assert(encoded.IsSuccessful());
    assert(encoded.BytesWritten==measure.RequiredBytes);
    T decoded{};
    const auto result=S::Deserialise<Codec>(
        std::span<const std::byte>(buffer.data(),encoded.BytesWritten),decoded
    );
    assert(result.IsSuccessful());
    verify(original,decoded);
}
template<class T,class Check>
void Both(const T& original,Check verify) {
    RoundTrip<S::Cbor>(original,verify);
    RoundTrip<S::Json>(original,verify);
}

void TestMessageIdentity() {
    const L::LogMessageIdentifier original{13U,65U,900000U};
    Both(original,[](const auto& a,const auto& b){
        assert(a.Domain==b.Domain && a.SubDomain==b.SubDomain && a.String==b.String);
    });
    for(std::uint32_t reserved=0;reserved<3U;++reserved) {
        L::LogMessageIdentifier invalid{13U,65U,reserved};
        assert(!S::Measure<S::Cbor>(invalid).IsSuccessful());
        assert(!S::Measure<S::Json>(invalid).IsSuccessful());
    }
    for(std::uint32_t reserved=0;reserved<3U;++reserved) {
        const std::array<std::uint32_t,3U> wire{13U,65U,reserved};
        L::LogMessageIdentifier destination{};
        using Adapter=ESPressio::Bounded::TypeConversionAdapter<
            std::array<std::uint32_t,3U>, L::LogMessageIdentifier>;
        assert(!Adapter::IsSuccessful(Adapter::Convert(wire,destination)));
    }
}

template<L::AttemptSequenceWidth Width>
void TestAttempt(const typename L::AttemptSequence<Width>::Storage number) {
    using Ticket=L::AttemptSequence<Width>;
    const auto original=Ticket::FromValidatedValue(number);
    Both(original,[](const Ticket& a,const Ticket& b){assert(a.Value()==b.Value());});
    assert(!S::Measure<S::Cbor>(Ticket{}).IsSuccessful());
    assert(!S::Measure<S::Json>(Ticket{}).IsSuccessful());
    Ticket output{};
    using Adapter=ESPressio::Bounded::TypeConversionAdapter<typename Ticket::Storage,Ticket>;
    const auto zero=typename Ticket::Storage{0U};
    assert(!Adapter::IsSuccessful(Adapter::Convert(zero,output)));
}

void TestClock() {
    Both(C::MonotonicTimestamp::FromNanoseconds(0x0123456789ABCDEFULL),
        [](const auto& a,const auto& b){assert(a.Nanoseconds()==b.Nanoseconds());});
    Both(C::SynchronizedTimestamp::FromNanoseconds(0U),
        [](const auto& a,const auto& b){assert(a.Nanoseconds()==b.Nanoseconds());});
    Both(C::SynchronizationUncertainty::FromNanoseconds(12345U),
        [](const auto& a,const auto& b){assert(a.Nanoseconds()==b.Nanoseconds());});
    Both(C::ClockEra::FromValue(7U),
        [](const auto& a,const auto& b){assert(a.Value()==b.Value());});
    Both(C::SynchronizationState::Synchronized,
        [](const auto& a,const auto& b){assert(a==b);});
    Both(L::Severity::Info,[](auto a,auto b){assert(a==b);});
}

void TestMalformedCoreWire() {
    // Canonical CBOR array [13, 65, 2] has an invalid reserved
    // Localisation string number; do not accept it as a typed record ID.
    const std::array<std::byte,5U> badMessage{
        std::byte{0x83U},std::byte{0x0dU},std::byte{0x18U},
        std::byte{0x41U},std::byte{0x02U}
    };
    L::LogMessageIdentifier id{};
    assert(!S::Deserialise<S::Cbor>(std::span<const std::byte>(badMessage),id)
        .IsSuccessful());
    using Ticket=L::AttemptSequence<L::AttemptSequenceWidth::Bits32>;
    Ticket ticket{};
    const std::array<std::byte,1U> zeroTicket{std::byte{0U}};
    assert(!S::Deserialise<S::Cbor>(std::span<const std::byte>(zeroTicket),ticket)
        .IsSuccessful());
    const std::array<std::byte,1U> zeroJson{std::byte{'0'}};
    assert(!S::Deserialise<S::Json>(std::span<const std::byte>(zeroJson),ticket)
        .IsSuccessful());
}

int main() {
    TestMalformedCoreWire();
    TestMessageIdentity();
    TestAttempt<L::AttemptSequenceWidth::Bits16>(0xBABEU);
    TestAttempt<L::AttemptSequenceWidth::Bits32>(0xABCDE123U);
    TestAttempt<L::AttemptSequenceWidth::Bits64>(0x1122334455667788ULL);
    TestClock();
}
