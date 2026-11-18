#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include <serialisation/Utf8Validation.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>
#include <memory/ByteOperationsContract.hpp>
#include <identity/DeviceIdentifier.hpp>
#include <identity/RuntimeIncarnationId.hpp>
#include <types/TypeIdentifier.hpp>
#include <types/FieldIdentifier.hpp>

namespace ESPressio::Clock {

    class ClockEra;

} // ESPressio::Clock

namespace ESPressio::Logging {

    /// Internal in-memory diagnostic argument kinds; NOT wire tag allocations.
    enum class OwnedArgumentKind : std::uint8_t {
        Boolean = 0U,
        Signed8 = 1U,
        Signed16 = 2U,
        Signed32 = 3U,
        Signed64 = 4U,
        Unsigned8 = 5U,
        Unsigned16 = 6U,
        Unsigned32 = 7U,
        Unsigned64 = 8U,
        Float32 = 9U,
        Float64 = 10U,
        Utf8 = 11U,
        Bytes = 12U,
        TypeIdentifier = 13U,
        DeviceIdentifier = 14U,
        FieldIdentifier = 15U,
        RuntimeIncarnationId = 16U,
        ClockEra = 17U
    };


    /// Capacity, data-validation and type qualification result of one append.
    enum class OwnedArgumentResult : std::uint8_t {
        Added = 0U,
        CountExceeded = 1U,
        BytesExceeded = 2U,
        InvalidUtf8 = 3U,
        UnsupportedType = 4U
    };


    /// Status of obtaining one borrowed view/descriptor from owned storage.
    enum class OwnedArgumentAccessResult : std::uint8_t {
        Available = 0U,
        OutOfRange = 1U
    };


    /// Finite descriptor into the source record's own byte arena.
    struct OwnedArgumentDescriptor final {

        // Strongly classified source storage slice.

        /// Source-side kind; EDP-Serialisation supplies actual wire tags.
        OwnedArgumentKind Kind{OwnedArgumentKind::Bytes};

        /// Offset into this record's private bounded argument byte array.
        std::uint16_t Offset{0U};

        /// Length of the argument's fully owned byte representation.
        std::uint16_t Size{0U};

    };


    /// Private bounded source-side heterogeneous argument capture arena.
    ///
    /// This is not an EDLG/EDLS wire format. Scalar representations are
    /// architecture-native in this PRIVATE arena; only EDP-Serialisation may
    /// produce public canonical CBOR/JSON. No external caller buffer remains
    /// referenced after a successful Add...() operation.
    ///
    /// @tparam TMaximumArguments Maximum total captured argument count.
    /// @tparam TMaximumBytes Maximum aggregate captured data bytes.
    /// @tparam TByteOperationsProvider Stateless EDP-Memory ByteOperations
    /// provider chosen by the owning application; default portable.
    template<
        std::size_t TMaximumArguments = 8U,
        std::size_t TMaximumBytes = 128U,
        class TByteOperationsProvider =
            ESPressio::Platform::Portable::Memory::ByteOperationsProvider
    >
    class OwnedDiagnosticArguments final {

        static_assert(
            TMaximumArguments >= 1U && TMaximumArguments <= 64U,
            "Owned argument count must be in the finite range 1..64"
        );
        static_assert(
            TMaximumBytes >= 1U && TMaximumBytes <= 4096U,
            "Owned argument byte capacity must be in the finite range 1..4096"
        );

        // A compile-time-selected Memory provider performs all byte copies,
        // introducing no per-record object, pointer, or dynamic ownership.
        static_assert(
            sizeof(ESPressio::Memory::Detail::ByteOperationsProviderTraits<
                TByteOperationsProvider
            >) > 0U,
            "Argument copying requires a valid EDP-Memory ByteOperations provider"
        );
        static_assert(
            std::is_empty_v<TByteOperationsProvider> &&
            std::is_nothrow_default_constructible_v<TByteOperationsProvider>,
            "Argument copying requires a stateless, noexcept Memory provider"
        );


    private:

        // Exact source-owned argument descriptor and byte capacities.

        /// One physical descriptor slot per admitted argument.
        std::array<OwnedArgumentDescriptor, TMaximumArguments> _items{};

        /// Captured byte arena with no external pointer lifetime.
        std::array<std::byte, TMaximumBytes> _data{};

        /// Committed argument count, never exceeding capacity.
        std::size_t _count{0U};

        /// Committed byte count, never exceeding capacity.
        std::size_t _used{0U};


        // UTF-8 validation and transactional append helpers.

        /// Atomically captures a complete native argument byte slice.
        /// @param kind Source-side kind of the argument.
        /// @param data Borrowed span valid only during the call.
        /// @returns Typed capacity/status result with no partial append.
        [[nodiscard]] OwnedArgumentResult Store(
            OwnedArgumentKind kind,
            std::span<const std::byte> data
        ) noexcept {
            if (_count == TMaximumArguments) {
                return OwnedArgumentResult::CountExceeded;
            }
            if (data.size() > TMaximumBytes - _used) {
                return OwnedArgumentResult::BytesExceeded;
            }

            // The Memory owner copies the complete preflight-qualified
            // range; zero bytes require no native pointer dereference.
            // No ownership/descriptor state is changed until copy returns.
            if (!data.empty()) {
                TByteOperationsProvider{}.CopyBytes(
                    _data.data() + _used,
                    data.data(),
                    data.size()
                );
            }

            _items[_count] = {
                kind,
                static_cast<std::uint16_t>(_used),
                static_cast<std::uint16_t>(data.size())
            };
            _used += data.size();
            ++_count;

            return OwnedArgumentResult::Added;
        }


    public:

        // Lifetime and finite-capacity observations.

        /// Creates a zero-argument owner with all byte/descriptor slots internal.
        constexpr OwnedDiagnosticArguments() noexcept = default;

        /// Returns successfully captured argument count.
        [[nodiscard]] constexpr std::size_t Count() const noexcept {
            return _count;
        }

        /// Returns successfully captured aggregate byte count.
        [[nodiscard]] constexpr std::size_t BytesUsed() const noexcept {
            return _used;
        }

        /// Returns compile-time byte capacity.
        [[nodiscard]] static constexpr std::size_t ByteCapacity() noexcept {
            return TMaximumBytes;
        }

        /// Returns compile-time argument capacity.
        [[nodiscard]] static constexpr std::size_t ArgumentCapacity() noexcept {
            return TMaximumArguments;
        }


        // Checked read-only borrowing access for in-memory owners.

        /// Reads one source-owned descriptor into caller output, if present.
        /// @param index Argument index.
        /// @param output Unchanged on failure.
        /// @returns Available or OutOfRange.
        [[nodiscard]] constexpr OwnedArgumentAccessResult GetDescriptor(
            std::size_t index,
            OwnedArgumentDescriptor& output
        ) const noexcept {
            if (index >= _count) {
                return OwnedArgumentAccessResult::OutOfRange;
            }

            output = _items[index];
            return OwnedArgumentAccessResult::Available;
        }

        /// Borrows bytes strictly inside this owner's lifetime.
        /// @param index Argument index.
        /// @param output Unchanged on failure; never becomes a durable/wire view.
        /// @returns Available or OutOfRange.
        [[nodiscard]] constexpr OwnedArgumentAccessResult GetArgumentBytes(
            std::size_t index,
            std::span<const std::byte>& output
        ) const noexcept {
            if (index >= _count) {
                return OwnedArgumentAccessResult::OutOfRange;
            }

            const auto& descriptor = _items[index];
            output = std::span<const std::byte>(
                _data.data() + descriptor.Offset,
                descriptor.Size
            );

            return OwnedArgumentAccessResult::Available;
        }


        // Typed, exact-width scalar and finite variable-length data capture.

        /// Copies an exact-width arithmetic source scalar into private storage.
        /// These bytes are PRIVATE host representation, not a wire encoding.
        /// @tparam TValue One owner-approved exact-width scalar Type.
        /// @param value Source scalar to capture immediately.
        /// @returns Typed count/byte/type qualification result.
        template<class TValue>
        [[nodiscard]] OwnedArgumentResult AddScalar(
            TValue value
        ) noexcept {
            using Value = std::remove_cv_t<TValue>;

            constexpr auto kind = []() consteval {
                if constexpr (std::is_same_v<Value, bool>) {
                    return OwnedArgumentKind::Boolean;
                } else if constexpr (std::is_same_v<Value, std::int8_t>) {
                    return OwnedArgumentKind::Signed8;
                } else if constexpr (std::is_same_v<Value, std::int16_t>) {
                    return OwnedArgumentKind::Signed16;
                } else if constexpr (std::is_same_v<Value, std::int32_t>) {
                    return OwnedArgumentKind::Signed32;
                } else if constexpr (std::is_same_v<Value, std::int64_t>) {
                    return OwnedArgumentKind::Signed64;
                } else if constexpr (std::is_same_v<Value, std::uint8_t>) {
                    return OwnedArgumentKind::Unsigned8;
                } else if constexpr (std::is_same_v<Value, std::uint16_t>) {
                    return OwnedArgumentKind::Unsigned16;
                } else if constexpr (std::is_same_v<Value, std::uint32_t>) {
                    return OwnedArgumentKind::Unsigned32;
                } else if constexpr (std::is_same_v<Value, std::uint64_t>) {
                    return OwnedArgumentKind::Unsigned64;
                } else if constexpr (std::is_same_v<Value, float>) {
                    return OwnedArgumentKind::Float32;
                } else if constexpr (std::is_same_v<Value, double>) {
                    return OwnedArgumentKind::Float64;
                } else {
                    return OwnedArgumentKind::Bytes;
                }
            }();

            static_assert(
                std::is_same_v<Value, bool> ||
                std::is_same_v<Value, std::int8_t> ||
                std::is_same_v<Value, std::uint8_t> ||
                std::is_same_v<Value, std::int16_t> ||
                std::is_same_v<Value, std::uint16_t> ||
                std::is_same_v<Value, std::int32_t> ||
                std::is_same_v<Value, std::uint32_t> ||
                std::is_same_v<Value, std::int64_t> ||
                std::is_same_v<Value, std::uint64_t> ||
                std::is_same_v<Value, float> ||
                std::is_same_v<Value, double>,
                "Only exact-width arithmetic scalars are supported; use owner-certified adapters"
            );

            const auto data =
                std::bit_cast<std::array<std::byte, sizeof(Value)>>(value);
            return Store(
                kind,
                data
            );
        }

        /// Validates strict canonical UTF-8 (including rejection of embedded NUL)
        /// using the sole EDP-Serialisation validator, then copies all bytes.
        /// Supply arbitrary NUL-bearing binary data using AddBytes and an
        /// appropriately approved generated diagnostic argument schema.
        /// @param text Borrowed UTF-8 input.
        /// @returns Added or typed malformed/capacity failure.
        [[nodiscard]] OwnedArgumentResult AddUtf8(
            std::string_view text
        ) noexcept {
            if (!ESPressio::Serialisation::ValidateUtf8(
                text.data(), text.size()
            ).IsSuccessful()) {
                return OwnedArgumentResult::InvalidUtf8;
            }

            const auto bytes =
                std::as_bytes(std::span(text.data(), text.size()));
            return Store(
                OwnedArgumentKind::Utf8,
                bytes
            );
        }

        /// Copies an arbitrary binary argument into private memory.
        /// @param bytes Borrowed binary input.
        /// @returns Typed append/capacity result.
        [[nodiscard]] OwnedArgumentResult AddBytes(
            std::span<const std::byte> bytes
        ) noexcept {
            return Store(
                OwnedArgumentKind::Bytes,
                bytes
            );
        }

        /// Copies an issued EDP-System TypeIdentifier's canonical eight bytes.
        /// @param identifier Issued strong Type identity.
        /// @returns Added or UnsupportedType/finite-capacity result.
        [[nodiscard]] OwnedArgumentResult AddTypeIdentifier(
            const System::TypeIdentifier& identifier
        ) noexcept {
            if (!identifier.IsValid()) {
                return OwnedArgumentResult::UnsupportedType;
            }

            const auto& bytes = identifier.Bytes();
            std::array<std::byte, System::TypeIdentifier::Size> copy{};
            for (std::size_t index = 0U; index < copy.size(); ++index) {
                copy[index] = static_cast<std::byte>(bytes[index]);
            }

            return Store(
                OwnedArgumentKind::TypeIdentifier,
                copy
            );
        }

        /// Captures a governed System FieldIdentifier as one private owned byte.
        /// Field 0 is a valid Type-local identifier. This is never a wire tag.
        /// @param identifier Strong one-byte Field identity.
        /// @returns Added or finite-capacity failure.
        [[nodiscard]] OwnedArgumentResult AddFieldIdentifier(
            System::FieldIdentifier identifier
        ) noexcept {
            const std::array<std::byte, 1U> bytes{
                static_cast<std::byte>(identifier.Value())
            };
            return Store(
                OwnedArgumentKind::FieldIdentifier,
                bytes
            );
        }


        /// Copies a valid strong 16-byte System DeviceIdentifier.
        /// @param identifier Issued source device identity.
        /// @returns Added or UnsupportedType/finite-capacity result.
        [[nodiscard]] OwnedArgumentResult AddDeviceIdentifier(
            const System::Identity::DeviceIdentifier& identifier
        ) noexcept {
            if (!identifier.IsValid()) {
                return OwnedArgumentResult::UnsupportedType;
            }

            const auto& bytes = identifier.Bytes();
            std::array<std::byte, System::Identity::DeviceIdentifier::Size> copy{};
            for (std::size_t index = 0U; index < copy.size(); ++index) {
                copy[index] = static_cast<std::byte>(bytes[index]);
            }

            return Store(
                OwnedArgumentKind::DeviceIdentifier,
                copy
            );
        }


        /// Captures an exact 32-bit EDP-System incarnation identity as source-only bytes.
        /// The zero sentinel remains representable for diagnostic observations:
        /// this does NOT make it a valid emitting LogRecord root incarnation.
        /// @param identifier Owner-issued semantic incarnation value.
        /// @returns Added or a typed finite-capacity refusal.
        [[nodiscard]] OwnedArgumentResult AddRuntimeIncarnationId(
            const System::Identity::RuntimeIncarnationId& identifier
        ) noexcept {
            const std::uint32_t value = identifier.Value();
            const auto bytes =
                std::bit_cast<std::array<std::byte, sizeof(value)>>(value);

            return Store(
                OwnedArgumentKind::RuntimeIncarnationId,
                bytes
            );
        }


        /// Captures an exact 64-bit EDP-Clock era identity as source-only bytes.
        /// This overload is instantiated only when the optional ClockEra owner
        /// is explicitly selected; no Clock header is imported by the base arena.
        /// The zero/unavailable era remains a representable diagnostic value.
        /// @tparam TEra Exact owning EDP-Clock ClockEra strong value Type.
        /// @param era Coordinator-issued or zero/unavailable era value.
        /// @returns Added or a typed finite-capacity refusal.
        template<class TEra>
        [[nodiscard]] OwnedArgumentResult AddClockEra(
            const TEra& era
        ) noexcept {
            static_assert(
                std::is_same_v<
                    std::remove_cv_t<TEra>,
                    ESPressio::Clock::ClockEra
                >,
                "Only the existing owner-issued EDP-Clock ClockEra is allowed"
            );

            const std::uint64_t value = era.Value();
            const auto bytes =
                std::bit_cast<std::array<std::byte, sizeof(value)>>(value);

            return Store(
                OwnedArgumentKind::ClockEra,
                bytes
            );
        }

    };

} // ESPressio::Logging
