#pragma once

#include <types/TypeIdentifier.hpp>

namespace ESPressio::Logging {

    /// Universal identity declaration for the canonical structured LogRecord.
    ///
    /// Allocated by the authorised ESPressio Development Platform TypeAuthority:
    /// Authority 0x000001, local Type 0x0000000001. Canonical bytes are
    /// exactly 00 00 01 00 00 00 00 01, and MUST NEVER be reassigned.
    ///
    /// This is intentionally IDENTITY-ONLY at the present implementation gate.
    /// The governed 15-field, Fields 0..14, type-safe payload/argument schema
    /// and Serialisation envelope have NOT yet been implemented. No fabricated
    /// empty FieldSet or production-constructible record is introduced to
    /// misrepresent that qualification status.
    struct LogRecord final {

        /// Permanent System-owned Type identity: not a message StringIdentifier.
        inline static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0x00U, 0x01U,
                0x00U, 0x00U, 0x00U, 0x00U, 0x01U
            }
        };

        /// Until all 15 locked fields are bound to this semantic root, callers
        /// cannot accidentally construct an invalid production LogRecord.
        LogRecord() = delete;

    };

} // namespace ESPressio::Logging
