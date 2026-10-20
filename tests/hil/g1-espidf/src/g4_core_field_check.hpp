#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <logging/LogRecordCoreFieldSerialisation.hpp>

namespace EDPLoggingCoreHil {
    namespace L = ESPressio::Logging;
    namespace C = ESPressio::Clock;
    namespace S = ESPressio::Serialisation;

    template<class Codec,class T,class Verify>
    [[nodiscard]] inline bool RoundTrip(const T& source,Verify verify) noexcept {
        std::array<std::byte,128U> buffer{};
        const auto measured=S::Measure<Codec>(source);
        if(!measured.IsSuccessful() || measured.RequiredBytes>buffer.size())
            return false;
        const auto result=S::Serialise<Codec>(source,buffer);
        if(!result.IsSuccessful() || result.BytesWritten!=measured.RequiredBytes)
            return false;
        T decoded{};
        if(!S::Deserialise<Codec>(
            std::span<const std::byte>(buffer.data(),result.BytesWritten),
            decoded).IsSuccessful()) return false;
        return verify(source,decoded);
    }

    template<class T,class Verify>
    [[nodiscard]] inline bool Both(const T& value,Verify verify) noexcept {
        return RoundTrip<S::Cbor>(value,verify) && RoundTrip<S::Json>(value,verify);
    }

    [[nodiscard]] inline bool VerifyOnDevice() noexcept {
        const L::LogMessageIdentifier id{12U,42U,300U};
        if(!Both(id,[](const auto& a,const auto& b){
            return a.Domain==b.Domain && a.SubDomain==b.SubDomain && a.String==b.String;
        })) return false;
        if(S::Measure<S::Cbor>(L::LogMessageIdentifier{12U,42U,2U}).IsSuccessful())
            return false;
        if(!Both(L::AttemptSequence<L::AttemptSequenceWidth::Bits16>
                ::FromValidatedValue(54321U),
                [](const auto& a,const auto& b){return a.Value()==b.Value();}))
            return false;
        if(!Both(L::AttemptSequence<L::AttemptSequenceWidth::Bits32>
                ::FromValidatedValue(0xABCDEF12U),
                [](const auto& a,const auto& b){return a.Value()==b.Value();}))
            return false;
        if(!Both(L::AttemptSequence<L::AttemptSequenceWidth::Bits64>
                ::FromValidatedValue(0x1122334455667788ULL),
                [](const auto& a,const auto& b){return a.Value()==b.Value();}))
            return false;
        if(S::Measure<S::Cbor>(L::AttemptSequence<L::AttemptSequenceWidth::Bits32>{})
                .IsSuccessful()) return false;
        if(!Both(C::MonotonicTimestamp::FromNanoseconds(123456U),
                 [](const auto& a,const auto& b){return a.Nanoseconds()==b.Nanoseconds();}))
            return false;
        if(!Both(C::SynchronizedTimestamp::FromNanoseconds(0U),
                 [](const auto& a,const auto& b){return a.Nanoseconds()==b.Nanoseconds();}))
            return false;
        if(!Both(C::SynchronizationUncertainty::FromNanoseconds(40U),
                 [](const auto& a,const auto& b){return a.Nanoseconds()==b.Nanoseconds();}))
            return false;
        if(!Both(C::ClockEra::FromValue(15U),
                 [](const auto& a,const auto& b){return a.Value()==b.Value();}))
            return false;
        if(!Both(C::SynchronizationState::Synchronized,
                 [](const auto& a,const auto& b){return a==b;}))
            return false;
        if(!Both(L::Severity::Info,[](const auto& a,const auto& b){return a==b;}))
            return false;
        return true;
    }
}
