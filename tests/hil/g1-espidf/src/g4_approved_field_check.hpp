#pragma once
#include <array>
#include <cstddef>
#include <span>
#include <logging/LogRecordApprovedFieldsSerialisation.hpp>

inline bool CheckG4ApprovedFieldsOnDevice() noexcept {
    namespace L=ESPressio::Logging;
    namespace C=ESPressio::Clock;
    namespace S=ESPressio::Serialisation;
    const L::LogTemporalProvenance input{
        C::ClockCorrelationProjectionStatus::Correlated,
        C::TemporalQualityAssessment::Reliable
    };
    std::array<std::byte,32U> buffer{};
    L::LogTemporalProvenance output{};
    const auto cbor=S::Serialise<S::Cbor>(input,buffer);
    if(!cbor.IsSuccessful() || cbor.BytesWritten!=3U) return false;
    if(!S::Deserialise<S::Cbor>(
        std::span<const std::byte>(buffer.data(),cbor.BytesWritten),
        output).IsSuccessful() || output.Projection!=input.Projection ||
        output.Quality!=input.Quality) return false;
    const auto json=S::Serialise<S::Json>(input,buffer);
    if(!json.IsSuccessful()) return false;
    if(!S::Deserialise<S::Json>(
        std::span<const std::byte>(buffer.data(),json.BytesWritten),
        output).IsSuccessful() || output.Projection!=input.Projection ||
        output.Quality!=input.Quality) return false;
    const L::LogRecordFacts valid{};
    const auto zero=S::Serialise<S::Cbor>(valid,buffer);
    if(!zero.IsSuccessful() || zero.BytesWritten!=1U ||
       buffer[0]!=std::byte{0U}) return false;
    return !S::Measure<S::Cbor>(L::LogRecordFacts{1U}).IsSuccessful();
}
