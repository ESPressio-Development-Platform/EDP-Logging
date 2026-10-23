#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <GeneratedLocalisationContract.hpp>
#include <logging/OwnedDiagnosticReplay.hpp>

namespace {

    namespace L = ESPressio::Logging;
    namespace Serialisation = ESPressio::Serialisation;
    using GeneratedSchema =
        Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 3U>;

    static_assert(L::DiagnosticSchemaMatches<
        GeneratedSchema, std::uint16_t, bool
    >());

    void TestGeneratedScalarReplayFacade() {
        // Source storage is owned: no caller span or borrowed argument
        // may outlive the original site call.
        L::OwnedDiagnosticArguments<2U, 16U> source{};
        assert(source.AddScalar<std::uint16_t>(300U) ==
            L::OwnedArgumentResult::Added);
        assert(source.AddScalar<bool>(true) ==
            L::OwnedArgumentResult::Added);
        assert(source.Count() == 2U && source.BytesUsed() == 3U);

        std::array<std::byte, 64U> output{};
        const auto cbor = L::EncodeOwnedDiagnosticArguments<
            GeneratedSchema, Serialisation::Cbor, std::uint16_t, bool
        >(output, source);
        assert(cbor.IsSuccessful());
        assert(cbor.BytesWritten == 5U);
        const std::array<std::uint8_t, 5U> expected{
            0x82U, 0x19U, 0x01U, 0x2CU, 0xF5U
        };
        for (std::size_t i = 0U; i < expected.size(); ++i) {
            assert(std::to_integer<std::uint8_t>(output[i]) == expected[i]);
        }

        const auto json = L::EncodeOwnedDiagnosticArguments<
            GeneratedSchema, Serialisation::Json, std::uint16_t, bool
        >(output, source);
        assert(json.IsSuccessful());
        assert(std::string_view(
            reinterpret_cast<const char*>(output.data()),
            json.BytesWritten
        ) == "[300,true]");

        // An absent source MUST NOT be turned into a successful
        // empty positional array.
        L::OwnedDiagnosticArguments<2U, 16U> absent{};
        const auto missing = L::EncodeOwnedDiagnosticArguments<
            GeneratedSchema, Serialisation::Cbor, std::uint16_t, bool
        >(output, absent);
        assert(!missing.IsSuccessful());
        assert(missing.Status == L::OwnedArgumentReplayStatus::CountMismatch);
        assert(missing.BytesWritten == 0U);

        // The private source descriptor cannot be silently reinterpreted as
        // a different generated argument kind even when byte sizes overlap.
        L::OwnedDiagnosticArguments<2U, 16U> wrongKind{};
        assert(wrongKind.AddScalar<bool>(true) ==
            L::OwnedArgumentResult::Added);
        assert(wrongKind.AddScalar<std::uint16_t>(300U) ==
            L::OwnedArgumentResult::Added);
        const auto rejected = L::EncodeOwnedDiagnosticArguments<
            GeneratedSchema, Serialisation::Cbor, std::uint16_t, bool
        >(output, wrongKind);
        assert(!rejected.IsSuccessful());
        assert(rejected.Status ==
            L::OwnedArgumentReplayStatus::SourceDescriptorRejected);
        assert(rejected.BytesWritten == 0U);

        // The codec's own bounded output contract is authoritative.
        std::array<std::byte, 1U> tooSmall{};
        const auto insufficient = L::EncodeOwnedDiagnosticArguments<
            GeneratedSchema, Serialisation::Cbor, std::uint16_t, bool
        >(tooSmall, source);
        assert(!insufficient.IsSuccessful());
        assert(insufficient.Status ==
            L::OwnedArgumentReplayStatus::EncoderRejected);
        assert(insufficient.BytesWritten == 0U);
    }

}

int main() {
    TestGeneratedScalarReplayFacade();
}
