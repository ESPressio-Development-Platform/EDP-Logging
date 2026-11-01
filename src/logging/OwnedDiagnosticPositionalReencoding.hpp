#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <logging/DiagnosticArgumentEncoding.hpp>
#include <logging/OwnedDiagnosticArguments.hpp>

namespace ESPressio::Logging {

    enum class OwnedPositionalReencodingStatus : std::uint8_t {
        Succeeded = 0U,
        CountMismatch = 1U,
        KindMismatch = 2U,
        SourceUnavailable = 3U,
        ValueWidthMismatch = 4U,
        OwnerValueRejected = 5U,
        EncoderRejected = 6U
    };

    struct OwnedPositionalReencodingOutcome final {
        OwnedPositionalReencodingStatus Status{
            OwnedPositionalReencodingStatus::CountMismatch
        };
        std::size_t FirstFailedArgument{0U};
        ESPressio::Serialisation::SerialisationResult Encoding{};
        [[nodiscard]] bool IsSuccessful() const noexcept {
            return Status == OwnedPositionalReencodingStatus::Succeeded &&
                   Encoding.IsSuccessful();
        }
    };

    // Bootstrap supplies the complete fixed storage, rather than letting
    // Logging allocate a hidden stack/heap tuple of heterogeneous values.
    template<class... TValues>
    using OwnedPositionalScratch = std::tuple<std::optional<TValues>...>;

    namespace Detail {
        template<class TValue, class TByteOperationsProvider>
        [[nodiscard]] TValue LoadOwnedNativeScalar(
            std::span<const std::byte> bytes
        ) noexcept {
            std::array<std::byte, sizeof(TValue)> storage{};
            TByteOperationsProvider{}.CopyBytes(
                storage.data(),
                bytes.data(),
                storage.size()
            );
            return std::bit_cast<TValue>(storage);
        }

        template<class TValue, std::size_t N, std::size_t B,
                 class TByteOperationsProvider>
        [[nodiscard]] OwnedPositionalReencodingStatus LoadOwnedPosition(
            const OwnedDiagnosticArguments<N,B,TByteOperationsProvider>& source,
            std::size_t index,
            std::optional<TValue>& output
        ) noexcept {
            using Value = std::remove_cv_t<TValue>;
            OwnedArgumentDescriptor descriptor{};
            std::span<const std::byte> bytes{};
            if (source.GetDescriptor(index,descriptor) != OwnedArgumentAccessResult::Available ||
                source.GetArgumentBytes(index,bytes) != OwnedArgumentAccessResult::Available) {
                return OwnedPositionalReencodingStatus::SourceUnavailable;
            }
            if (descriptor.Kind != DiagnosticArgumentKindFor<OwnedArgumentKind,Value>()) {
                return OwnedPositionalReencodingStatus::KindMismatch;
            }
            if (bytes.size() != descriptor.Size) {
                return OwnedPositionalReencodingStatus::ValueWidthMismatch;
            }
            if constexpr (std::is_same_v<Value,bool>) {
                if (bytes.size() != 1U)
                    return OwnedPositionalReencodingStatus::ValueWidthMismatch;
                const auto byte=std::to_integer<std::uint8_t>(bytes[0]);
                if (byte > 1U) return OwnedPositionalReencodingStatus::OwnerValueRejected;
                output.emplace(byte != 0U);
            } else if constexpr (std::is_arithmetic_v<Value>) {
                if (bytes.size()!=sizeof(Value))
                    return OwnedPositionalReencodingStatus::ValueWidthMismatch;
                output.emplace(LoadOwnedNativeScalar<Value,TByteOperationsProvider>(bytes));
            } else if constexpr (IsBoundedDiagnosticString<Value>::value) {
                Value next{};
                const auto status=next.Assign(std::string_view(
                    reinterpret_cast<const char*>(bytes.data()),bytes.size()));
                if (status != ESPressio::Bounded::StringAssignmentResult::Succeeded)
                    return OwnedPositionalReencodingStatus::OwnerValueRejected;
                output.emplace(std::move(next));
            } else if constexpr (IsBoundedDiagnosticBytes<Value>::value) {
                Value next{};
                const auto status=next.Assign(
                    reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size());
                if (status != ESPressio::Bounded::BytesAssignmentResult::Succeeded)
                    return OwnedPositionalReencodingStatus::OwnerValueRejected;
                output.emplace(std::move(next));
            } else if constexpr (std::is_same_v<Value,ESPressio::System::FieldIdentifier>) {
                if (bytes.size()!=1U)
                    return OwnedPositionalReencodingStatus::ValueWidthMismatch;
                output.emplace(std::to_integer<std::uint8_t>(bytes[0]));
            } else if constexpr (std::is_same_v<
                Value, ESPressio::System::Identity::RuntimeIncarnationId>) {
                if (bytes.size()!=sizeof(std::uint32_t))
                    return OwnedPositionalReencodingStatus::ValueWidthMismatch;
                output.emplace(LoadOwnedNativeScalar<std::uint32_t,TByteOperationsProvider>(bytes));
            } else if constexpr (IsCertifiedDiagnosticClockEra<Value>::value) {
                if (bytes.size()!=sizeof(std::uint64_t))
                    return OwnedPositionalReencodingStatus::ValueWidthMismatch;
                output.emplace(Value::FromValue(
                    LoadOwnedNativeScalar<std::uint64_t,TByteOperationsProvider>(bytes)));
            } else if constexpr (
                std::is_same_v<Value,ESPressio::System::TypeIdentifier> ||
                std::is_same_v<Value,ESPressio::System::Identity::DeviceIdentifier>
            ) {
                if (bytes.size()!=Value::Size)
                    return OwnedPositionalReencodingStatus::ValueWidthMismatch;
                typename Value::Storage array{};
                TByteOperationsProvider{}.CopyBytes(
                    array.data(),
                    bytes.data(),
                    array.size()
                );
                Value next{array};
                if (!next.IsValid())
                    return OwnedPositionalReencodingStatus::OwnerValueRejected;
                output.emplace(next);
            } else {
                static_assert(std::is_same_v<Value,void>,
                    "Missing EDP owner-certified native source conversion");
            }
            return OwnedPositionalReencodingStatus::Succeeded;
        }
    }

    namespace Detail {
        template<
            bool TJson,
            class TGeneratedSchema,
            class... TValues,
            std::size_t N,
            std::size_t B,
            class TByteOperationsProvider,
            std::size_t... TIndices
        >
        [[nodiscard]] OwnedPositionalReencodingOutcome EncodeOwnedImpl(
            const OwnedDiagnosticArguments<N,B,TByteOperationsProvider>& source,
            std::span<std::byte> destination,
            OwnedPositionalScratch<TValues...>& scratch,
            std::index_sequence<TIndices...>
        ) noexcept {
            static_assert(IsDiagnosticSchemaMatching<TGeneratedSchema,TValues...>(),
                "Owned positions must exactly match the generated Localisation schema");
            static_assert((ESPressio::Serialisation::SerialisableType<TValues> && ...),
                "Rehydrated values must have EDP-Serialisation owner support");

            const auto clear=[&]() noexcept {
                (std::get<TIndices>(scratch).reset(),...);
            };
            clear();
            OwnedPositionalReencodingOutcome outcome{};
            if (source.Count()!=sizeof...(TValues)) {
                outcome.Status=OwnedPositionalReencodingStatus::CountMismatch;
                outcome.FirstFailedArgument=source.Count();
                return outcome;
            }
            outcome.Status=OwnedPositionalReencodingStatus::Succeeded;
            // Static schema order determines the exact owner value Type at
            // each position. The arena's PRIVATE Kind is checked, never
            // serialized as a public CBOR/JSON discriminator.
            (([&]() noexcept {
                if (outcome.Status!=OwnedPositionalReencodingStatus::Succeeded)
                    return;
                outcome.Status=LoadOwnedPosition<TValues>(
                    source,TIndices,std::get<TIndices>(scratch));
                if (outcome.Status!=OwnedPositionalReencodingStatus::Succeeded)
                    outcome.FirstFailedArgument=TIndices;
            }()),...);
            if (outcome.Status!=OwnedPositionalReencodingStatus::Succeeded) {
                clear();
                return outcome;
            }

            if constexpr (TJson) {
                outcome.Encoding=EncodeDiagnosticArgumentsJson<TGeneratedSchema>(
                    destination,(*std::get<TIndices>(scratch))...);
            } else {
                outcome.Encoding=EncodeDiagnosticArgumentsCbor<TGeneratedSchema>(
                    destination,(*std::get<TIndices>(scratch))...);
            }
            clear();
            if (!outcome.Encoding.IsSuccessful())
                outcome.Status=OwnedPositionalReencodingStatus::EncoderRejected;
            return outcome;
        }
    }

    /// Rehydrate a private source arena through its exact GENERATED message
    /// signature, then emit canonical CBOR via EDP-Serialisation. The caller
    /// owns all bounded rehydration scratch and destination storage.
    /// The scratch is cleared on EVERY path, including an encode refusal.
    template<
        class TGeneratedSchema, class... TValues,
        std::size_t N, std::size_t B,
        class TByteOperationsProvider
    >
    [[nodiscard]] OwnedPositionalReencodingOutcome
    EncodeOwnedDiagnosticArgumentsCbor(
        const OwnedDiagnosticArguments<N,B,TByteOperationsProvider>& source,
        std::span<std::byte> destination,
        OwnedPositionalScratch<TValues...>& scratch
    ) noexcept {
        return Detail::EncodeOwnedImpl<false,TGeneratedSchema,TValues...>(
            source,destination,scratch,std::index_sequence_for<TValues...>{});
    }

    /// The same ownership/reconstruction contract using the existing
    /// EDP-Serialisation canonical positional JSON encoder.
    template<
        class TGeneratedSchema, class... TValues,
        std::size_t N, std::size_t B,
        class TByteOperationsProvider
    >
    [[nodiscard]] OwnedPositionalReencodingOutcome
    EncodeOwnedDiagnosticArgumentsJson(
        const OwnedDiagnosticArguments<N,B,TByteOperationsProvider>& source,
        std::span<std::byte> destination,
        OwnedPositionalScratch<TValues...>& scratch
    ) noexcept {
        return Detail::EncodeOwnedImpl<true,TGeneratedSchema,TValues...>(
            source,destination,scratch,std::index_sequence_for<TValues...>{});
    }

} // namespace ESPressio::Logging
