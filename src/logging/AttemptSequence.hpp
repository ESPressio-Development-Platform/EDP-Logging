#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace ESPressio::Logging {

    /// Compile-time storage selection for a unique instrumentation attempt within one incarnation.
    enum class AttemptSequenceWidth : std::uint8_t {
        Bits16 = 16,
        Bits32 = 32,
        Bits64 = 64
    };

    /// Selected width for applications that do not explicitly choose another capacity.
    inline constexpr AttemptSequenceWidth DefaultAttemptSequenceWidth =
        AttemptSequenceWidth::Bits32;

    /// Outcome of allocating a ticket; exhaustion never wraps or silently reuses a value.
    enum class AttemptSequenceAllocationResult : std::uint8_t {
        Issued = 0,
        OutputOccupied = 1,
        SequenceExhausted = 2
    };

    namespace Detail {

        /// Maps the statically configured bit count to its exact unsigned storage type.
        /// @tparam TWidth Application-selected per-incarnation bit width.
        template<AttemptSequenceWidth TWidth>
        struct AttemptSequenceStorage final {

            static_assert(
                TWidth == AttemptSequenceWidth::Bits16 ||
                TWidth == AttemptSequenceWidth::Bits32 ||
                TWidth == AttemptSequenceWidth::Bits64,
                "Attempt sequence must select Bits16, Bits32 or Bits64 at compile time"
            );

            using Type = std::conditional_t<
                TWidth == AttemptSequenceWidth::Bits16,
                std::uint16_t,
                std::conditional_t<
                    TWidth == AttemptSequenceWidth::Bits32,
                    std::uint32_t,
                    std::uint64_t
                >
            >;
        };

    } // ESPressio::Logging::Detail

    /// Strong, compact per-incarnation instrumentation attempt identity.
    ///
    /// Zero is permanently invalid. Native width changes only through Bootstrap's compile-time
    /// type selection, never at runtime; portable CBOR/JSON encoders supply width metadata.
    /// @tparam TWidth Selected storage width.
    template<AttemptSequenceWidth TWidth = DefaultAttemptSequenceWidth>
    class AttemptSequence final {
    public:

        /// Exact unsigned backing type.
        using Storage = typename Detail::AttemptSequenceStorage<TWidth>::Type;

        /// Physical memory bit-width selected for this incarnation.
        static constexpr std::uint8_t WidthBits = static_cast<std::uint8_t>(TWidth);

    private:

        /// Unassigned (zero) or allocated attempt identity.
        Storage _value{0U};

        /// Constructs an already validated ticket produced by a counter.
        explicit constexpr AttemptSequence(Storage value) noexcept : _value(value) {}

    public:

        /// Constructs an invalid/unassigned output slot.
        constexpr AttemptSequence() noexcept = default;

        /// Restores a caller-validated ticket; zero remains the invalid/unassigned value.
        static constexpr AttemptSequence FromValidatedValue(Storage value) noexcept {
            return AttemptSequence(value);
        }

        /// Returns the exact unsigned identity.
        [[nodiscard]] constexpr Storage Value() const noexcept {
            return _value;
        }

        /// Indicates that the ticket has been assigned.
        [[nodiscard]] constexpr bool IsValid() const noexcept {
            return _value != 0U;
        }
    };

    /// Source of non-reused instrumentation attempt tickets within ONE RuntimeIncarnationId.
    ///
    /// It deliberately does NOT claim internal thread safety or use atomics; 64-bit atomics
    /// are not assured lock-free on all ESP32 targets. The owning Logging runtime MUST call
    /// Issue() under its configured Threading-coordinated exclusive producer sequencer.
    /// There is intentionally no public reset/rewind operation.
    /// @tparam TWidth Statically selected per-incarnation ticket storage width.
    template<AttemptSequenceWidth TWidth = DefaultAttemptSequenceWidth>
    class AttemptSequenceCounter final {
    public:

        /// Exact counter/ticket storage Type.
        using Storage = typename AttemptSequence<TWidth>::Storage;

        /// Largest available ticket within the selected runtime incarnation.
        static constexpr Storage MaximumValue = std::numeric_limits<Storage>::max();

    private:

        /// Last issued nonzero value, or zero before first issue.
        Storage _lastIssued{0U};

    public:

        /// Starts a new incarnation's counter in the never-issued state.
        constexpr AttemptSequenceCounter() noexcept = default;

        AttemptSequenceCounter(const AttemptSequenceCounter&) = delete;
        AttemptSequenceCounter& operator =(const AttemptSequenceCounter&) = delete;
        AttemptSequenceCounter(AttemptSequenceCounter&&) = delete;
        AttemptSequenceCounter& operator =(AttemptSequenceCounter&&) = delete;

        /// Allocates exactly one ticket; rejected admissions still retain their consumed ticket.
        ///
        /// Caller MUST hold the owning producer sequencer's external exclusive coordination
        /// across the entire Issue() call, including output validation and update.
        /// @param output Invalid ticket slot populated only on Issued.
        [[nodiscard]] constexpr AttemptSequenceAllocationResult Issue(
            AttemptSequence<TWidth>& output
        ) noexcept {
            if (output.IsValid()) {
                return AttemptSequenceAllocationResult::OutputOccupied;
            }

            if (_lastIssued == MaximumValue) {
                return AttemptSequenceAllocationResult::SequenceExhausted;
            }

            ++_lastIssued;
            output = AttemptSequence<TWidth>::FromValidatedValue(_lastIssued);
            return AttemptSequenceAllocationResult::Issued;
        }

        /// Returns the last ticket issued (zero only before any attempt was allocated).
        [[nodiscard]] constexpr Storage LastIssued() const noexcept {
            return _lastIssued;
        }

        /// Indicates that the counter cannot allocate another distinct ticket.
        [[nodiscard]] constexpr bool IsExhausted() const noexcept {
            return _lastIssued == MaximumValue;
        }

    };

    static_assert(sizeof(AttemptSequence<AttemptSequenceWidth::Bits16>) == 2U);
    static_assert(sizeof(AttemptSequence<AttemptSequenceWidth::Bits32>) == 4U);
    static_assert(sizeof(AttemptSequence<AttemptSequenceWidth::Bits64>) == 8U);

} // ESPressio::Logging
