#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "OwnedDiagnosticPositionalReencoding.hpp"

namespace ESPressio::Logging {

    // Retained source-compatible facade for the original scalar replay API.
    // The single owner-qualified rehydration path is now used for all Types.
    enum class OwnedArgumentReplayStatus : std::uint8_t {
        Succeeded = 0U,
        CountMismatch = 1U,
        SourceDescriptorRejected = 2U,
        SourceScalarRejected = 3U,
        EncoderRejected = 4U
    };

    struct OwnedArgumentReplayResult final {
        OwnedArgumentReplayStatus Status{OwnedArgumentReplayStatus::CountMismatch};
        std::size_t BytesWritten{0U};
        [[nodiscard]] constexpr bool IsSuccessful() const noexcept {
            return Status == OwnedArgumentReplayStatus::Succeeded;
        }
    };

    // Compatibility API creates one fixed-size stack scratch matching exactly
    // the caller's compile-time Types. New call sites should use the explicit
    // caller-owned scratch overload in OwnedDiagnosticPositionalReencoding.hpp.
    template<
        class TGeneratedMessageSchema, class TCodec,
        class... TValues, std::size_t N, std::size_t B
    >
    [[nodiscard]] OwnedArgumentReplayResult EncodeOwnedDiagnosticArguments(
        std::span<std::byte> destination,
        const OwnedDiagnosticArguments<N,B>& source
    ) noexcept {
        static_assert(
            std::is_same_v<TCodec,ESPressio::Serialisation::Cbor> ||
            std::is_same_v<TCodec,ESPressio::Serialisation::Json>,
            "Unsupported owner positional codec");
        OwnedPositionalScratch<TValues...> scratch{};
        const auto result=[&]() noexcept {
            if constexpr (std::is_same_v<TCodec,ESPressio::Serialisation::Cbor>)
                return EncodeOwnedDiagnosticArgumentsCbor<
                    TGeneratedMessageSchema,TValues...>(source,destination,scratch);
            else
                return EncodeOwnedDiagnosticArgumentsJson<
                    TGeneratedMessageSchema,TValues...>(source,destination,scratch);
        }();

        const auto status=[&]() constexpr noexcept {
            switch (result.Status) {
                case OwnedPositionalReencodingStatus::Succeeded:
                    return OwnedArgumentReplayStatus::Succeeded;
                case OwnedPositionalReencodingStatus::CountMismatch:
                    return OwnedArgumentReplayStatus::CountMismatch;
                case OwnedPositionalReencodingStatus::OwnerValueRejected:
                    return OwnedArgumentReplayStatus::SourceScalarRejected;
                case OwnedPositionalReencodingStatus::EncoderRejected:
                    return OwnedArgumentReplayStatus::EncoderRejected;
                case OwnedPositionalReencodingStatus::KindMismatch:
                case OwnedPositionalReencodingStatus::SourceUnavailable:
                case OwnedPositionalReencodingStatus::ValueWidthMismatch:
                    return OwnedArgumentReplayStatus::SourceDescriptorRejected;
            }
            return OwnedArgumentReplayStatus::SourceDescriptorRejected;
        }();
        return {
            status,
            result.IsSuccessful() ? result.Encoding.BytesWritten : 0U
        };
    }
} // namespace ESPressio::Logging
