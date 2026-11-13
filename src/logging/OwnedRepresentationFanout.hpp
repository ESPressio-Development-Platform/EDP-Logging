#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include <ESPressio_Memory.hpp>
#include <ESPressio_Platform.hpp>

#include "EdlgFrame.hpp"
#include "RecordProduction.hpp"

namespace ESPressio::Logging {

    /// Outcome reported by the application-selected encoder.
    enum class RepresentationEncodeStatus : std::uint8_t {
        Succeeded = 0U,
        Rejected = 1U,
        ProviderFailure = 2U
    };

    /// Aggregate outcome for the bounded encode, submit and release operation.
    enum class OwnedRepresentationStatus : std::uint8_t {
        SubmittedAndReleased = 0U,
        PoolCapacityUnavailable = 1U,
        PoolUnavailable = 2U,
        PoolProviderFailure = 3U,
        OutputLeaseOccupied = 4U,
        EncoderRejected = 5U,
        EncoderProviderFailure = 6U,
        EncoderLengthInvalid = 7U,
        SubmissionNotAccepted = 8U,
        MemoryReclaimUnconfirmed = 9U
    };

    /// EDP-Memory-pool object containing one complete encoded representation.
    /// Storage and lifetime are provided by a *Bootstrap-configured EDP-Memory
    /// ObjectPool*, not a hidden or independently allocated Logging pool.
    /// @tparam TMaximumPayloadBytes Compile-time capacity of the owned encoded payload.
    template<std::size_t TMaximumPayloadBytes>
    struct OwnedEncodedPayload final {
        static_assert(TMaximumPayloadBytes > 0U);
        /// Writable storage owned by the Memory pool lease.
        std::array<std::byte, TMaximumPayloadBytes> Bytes{};
        /// Number of bytes initialized by the last successful encoding.
        std::size_t Used{0U};
    };

    /// Encoder outcome and bounded output length.
    struct RepresentationEncodeResult final {
        /// Result of the encoder callback.
        RepresentationEncodeStatus Status{RepresentationEncodeStatus::ProviderFailure};
        /// Encoded byte count on success, checked against the offered span.
        std::size_t Written{0U};
    };

    /// Encodes once for a selected codec and attempts one to eight sinks independently.
    /// No accepted submission is mistaken for Persistence durability, UART
    /// transmission, or a destination's eventual consumption.
    /// @tparam TSinkCount Number of statically selected synchronous sinks.
    template<std::size_t TSinkCount>
    struct OwnedRepresentationOutcome final {
        static_assert(TSinkCount >= 1U && TSinkCount <= 8U);
        /// Aggregate status after the attempted phases.
        OwnedRepresentationStatus Status{OwnedRepresentationStatus::PoolUnavailable};
        /// Native Memory acquisition outcome.
        ESPressio::Memory::ObjectPoolAcquisitionResult Acquisition{
            ESPressio::Memory::ObjectPoolAcquisitionResult::NotInitialized
        };
        /// Native release outcome when release is attempted.
        ESPressio::Memory::ObjectPoolLeaseReleaseResult Release{
            ESPressio::Memory::ObjectPoolLeaseReleaseResult::AlreadyEmpty
        };
        /// Encoded byte count after a successful bounded encoder result.
        std::size_t EncodedBytes{0U};
        /// Number of encoder invocations in this operation.
        std::size_t EncoderInvocations{0U};
        /// Number of sinks invoked in this operation.
        std::size_t SinkInvocations{0U};
        /// Number of sinks that reported acceptance.
        std::size_t Accepted{0U};
        /// Per-sink results in invocation order, meaningful through SinkInvocations.
        std::array<RecordSinkSubmission, TSinkCount> Submissions{};
    };

    /// Finite, pooled per-representation fanout, invoked during a single
    /// source-owned coordinator callback.
    ///
    /// The encoder is the application-selected EDP-Serialisation adapter:
    /// it receives immutable source and caller-owned bounded output storage.
    /// This component DOES NOT implement CBOR/JSON LogRecord wire fields.
    /// TCodec is selected at application Composition; call once for every
    /// distinct requested representation profile, NOT once per sink.
    ///
    /// One sink is a valid deployment composition; a fanout does not
    /// require a second sink purely for bookkeeping.
    /// Every sink must synchronously make its *own* bounded copy/ownership
    /// transfer of the borrowed encoded payload before returning Accepted.
    /// EDP-Serial::BoundedEdlgFrameQueue is one suitable copying sink. A
    /// callback retaining the borrowed span or original record violates
    /// the contract. Sinks can independently reject. No unbounded retry,
    /// implicit output transaction, or false partial-delivery rollback.
    ///
    /// Caller MUST keep output lease alive, even after an uncertain
    /// EDP-Memory release, and explicitly resolve ownership at Bootstrap.
    /// No source Queue mutex is held by this function. The only acquisition
    /// wait policy is NoWait, irrespective of sink selection.
    /// @tparam TCodec Codec supplied to every selected sink.
    /// @tparam TMaximumPayloadBytes Byte capacity of the Memory-owned output object.
    /// @tparam TPool Bootstrap-owned Memory pool for this exact payload type.
    /// @tparam TRecord Borrowed source record type.
    /// @tparam TEncoder Nonthrowing encoder writing into the offered output span.
    /// @tparam TSinks Ordered nonthrowing callbacks that accept or refuse the borrowed bytes.
    template<
        EdlgCodec TCodec,
        std::size_t TMaximumPayloadBytes,
        class TPool, class TRecord, class TEncoder, class... TSinks
    >
    [[nodiscard]] OwnedRepresentationOutcome<sizeof...(TSinks)>
    EncodeOnceAndSubmitToSinks(
        TPool& pool,
        typename TPool::LeaseType& lease,
        const TRecord& record,
        TEncoder& encoder,
        TSinks&... sinks
    ) noexcept {
        constexpr std::size_t Count = sizeof...(TSinks);
        static_assert(TCodec == EdlgCodec::Cbor || TCodec == EdlgCodec::Json);
        static_assert(Count >= 1U && Count <= 8U);
        static_assert(std::is_same_v<
            typename TPool::ObjectType, OwnedEncodedPayload<TMaximumPayloadBytes>
        >, "EDP-Memory pool must own exactly this representation's bounded payload");
        static_assert(noexcept(encoder(record, std::declval<std::span<std::byte>>())));
        static_assert(std::is_same_v<
            decltype(encoder(record, std::declval<std::span<std::byte>>())),
            RepresentationEncodeResult
        >);

        OwnedRepresentationOutcome<Count> outcome{};
        outcome.Submissions.fill(RecordSinkSubmission::ProviderFailure);
        if (!lease.IsEmpty()) {
            outcome.Status = OwnedRepresentationStatus::OutputLeaseOccupied;
            return outcome;
        }
        outcome.Acquisition = pool.Acquire(
            lease,
            ESPressio::Platform::Synchronization::WaitTimeout::NoWait()
        );
        if (outcome.Acquisition != ESPressio::Memory::ObjectPoolAcquisitionResult::Succeeded) {
            switch (outcome.Acquisition) {
                case ESPressio::Memory::ObjectPoolAcquisitionResult::CapacityUnavailable:
                case ESPressio::Memory::ObjectPoolAcquisitionResult::TimedOut:
                    outcome.Status = OwnedRepresentationStatus::PoolCapacityUnavailable;
                    break;
                case ESPressio::Memory::ObjectPoolAcquisitionResult::NotInitialized:
                case ESPressio::Memory::ObjectPoolAcquisitionResult::TopologyUnavailable:
                    outcome.Status = OwnedRepresentationStatus::PoolUnavailable;
                    break;
                case ESPressio::Memory::ObjectPoolAcquisitionResult::OutputLeaseOccupied:
                    outcome.Status = OwnedRepresentationStatus::OutputLeaseOccupied;
                    break;
                default:
                    outcome.Status = OwnedRepresentationStatus::PoolProviderFailure;
                    break;
            }
            return outcome;
        }
        auto* payload = lease.Get();
        if (payload == nullptr) {
            outcome.Status = OwnedRepresentationStatus::PoolProviderFailure;
            // Uncertain acquired ownership remains in the caller's lease.
            return outcome;
        }
        const auto encoded = encoder(
            record, std::span<std::byte>(payload->Bytes.data(), payload->Bytes.size())
        );
        outcome.EncoderInvocations = 1U;
        if (encoded.Status == RepresentationEncodeStatus::Succeeded &&
            encoded.Written <= payload->Bytes.size()) {
            payload->Used = encoded.Written;
            outcome.EncodedBytes = encoded.Written;
            const auto view = std::span<const std::byte>(
                payload->Bytes.data(),
                payload->Used
            );
            std::size_t index = 0U;
            const auto submit = [&](auto& sink) noexcept {
                static_assert(noexcept(sink(
                    TCodec,
                    view
                )));
                static_assert(std::is_same_v<
                    decltype(sink(
                        TCodec,
                        view
                    )),
                    RecordSinkSubmission
                >);
                const auto status = sink(
                    TCodec,
                    view
                );
                outcome.Submissions[index++] = status;
                ++outcome.SinkInvocations;
                outcome.Accepted += status == RecordSinkSubmission::Accepted ? 1U : 0U;
            };
            (submit(sinks), ...);
            outcome.Status = outcome.Accepted == Count
                ? OwnedRepresentationStatus::SubmittedAndReleased
                : OwnedRepresentationStatus::SubmissionNotAccepted;
        } else if (encoded.Status == RepresentationEncodeStatus::Succeeded) {
            outcome.Status = OwnedRepresentationStatus::EncoderLengthInvalid;
        } else {
            outcome.Status = encoded.Status == RepresentationEncodeStatus::Rejected
                ? OwnedRepresentationStatus::EncoderRejected
                : OwnedRepresentationStatus::EncoderProviderFailure;
        }
        outcome.Release = lease.Release();
        if (outcome.Release != ESPressio::Memory::ObjectPoolLeaseReleaseResult::Released) {
            outcome.Status = OwnedRepresentationStatus::MemoryReclaimUnconfirmed;
        }
        return outcome;
    }

} // namespace ESPressio::Logging
