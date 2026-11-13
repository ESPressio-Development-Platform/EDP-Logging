#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <tuple>

#include <GeneratedLocalisationContract.hpp>
#include <logging/OwnedDiagnosticPositionalReencoding.hpp>

namespace {

    namespace L = ESPressio::Logging;
    using GeneratedSchema =
        Fixture::Diagnostic::DiagnosticMessageSchema<1U, 0U, 3U>;

    static_assert(L::IsDiagnosticSchemaMatching<
        GeneratedSchema, std::uint16_t, bool
    >());

    void TestGeneratedScalarPositionalReencoding() {
        // Source storage is owned: no caller span or borrowed argument
        // may outlive the original site call.
        L::OwnedDiagnosticArguments<2U, 16U> source{};
        assert(source.AddScalar<std::uint16_t>(300U) ==
            L::OwnedArgumentResult::Added);
        assert(source.AddScalar<bool>(true) ==
            L::OwnedArgumentResult::Added);
        assert(source.Count() == 2U && source.BytesUsed() == 3U);

        std::array<std::byte, 64U> output{};
        L::OwnedPositionalScratch<std::uint16_t, bool> scratch{};
        const auto cbor = L::EncodeOwnedDiagnosticArgumentsCbor<GeneratedSchema>(
            source,
            output,
            scratch
        );
        assert(cbor.IsSuccessful());
        assert(!std::get<0U>(scratch).has_value());
        assert(!std::get<1U>(scratch).has_value());
        assert(cbor.Encoding.BytesWritten == 5U);
        const std::array<std::uint8_t, 5U> expected{
            0x82U, 0x19U, 0x01U, 0x2CU, 0xF5U
        };
        for (std::size_t i = 0U; i < expected.size(); ++i) {
            assert(std::to_integer<std::uint8_t>(output[i]) == expected[i]);
        }

        const auto json = L::EncodeOwnedDiagnosticArgumentsJson<GeneratedSchema>(
            source,
            output,
            scratch
        );
        assert(json.IsSuccessful());
        assert(!std::get<0U>(scratch).has_value());
        assert(!std::get<1U>(scratch).has_value());
        assert(std::string_view(
            reinterpret_cast<const char*>(output.data()),
            json.Encoding.BytesWritten
        ) == "[300,true]");

        // An absent source MUST NOT be turned into a successful
        // empty positional array.
        L::OwnedDiagnosticArguments<2U, 16U> absent{};
        const auto missing = L::EncodeOwnedDiagnosticArgumentsCbor<GeneratedSchema>(
            absent,
            output,
            scratch
        );
        assert(!missing.IsSuccessful());
        assert(!std::get<0U>(scratch).has_value());
        assert(!std::get<1U>(scratch).has_value());
        assert(missing.Status == L::OwnedPositionalReencodingStatus::CountMismatch);
        assert(missing.FirstFailedArgument == 0U);
        assert(missing.Encoding.BytesWritten == 0U);

        // The private source descriptor cannot be silently reinterpreted as
        // a different generated argument kind even when byte sizes overlap.
        L::OwnedDiagnosticArguments<2U, 16U> wrongKind{};
        assert(wrongKind.AddScalar<bool>(true) ==
            L::OwnedArgumentResult::Added);
        assert(wrongKind.AddScalar<std::uint16_t>(300U) ==
            L::OwnedArgumentResult::Added);
        const auto rejected = L::EncodeOwnedDiagnosticArgumentsCbor<GeneratedSchema>(
            wrongKind,
            output,
            scratch
        );
        assert(!rejected.IsSuccessful());
        assert(!std::get<0U>(scratch).has_value());
        assert(!std::get<1U>(scratch).has_value());
        assert(rejected.Status ==
            L::OwnedPositionalReencodingStatus::KindMismatch);
        assert(rejected.FirstFailedArgument == 0U);
        assert(rejected.Encoding.BytesWritten == 0U);

        // The codec's own bounded output contract is authoritative.
        std::array<std::byte, 1U> tooSmall{};
        const auto insufficient = L::EncodeOwnedDiagnosticArgumentsCbor<GeneratedSchema>(
            source,
            tooSmall,
            scratch
        );
        assert(!insufficient.IsSuccessful());
        assert(!std::get<0U>(scratch).has_value());
        assert(!std::get<1U>(scratch).has_value());
        assert(insufficient.Status ==
            L::OwnedPositionalReencodingStatus::EncoderRejected);
        assert(insufficient.Encoding.BytesWritten == 0U);
    }

}

int main() {
    TestGeneratedScalarPositionalReencoding();
}
