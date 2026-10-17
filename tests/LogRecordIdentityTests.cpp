#include <array>
#include <cstdint>
#include <type_traits>

#include <ESPressio_System.hpp>
#include <logging/LogRecord.hpp>

namespace S = ESPressio::System;
namespace L = ESPressio::Logging;

static_assert(S::IdentifiedType<L::LogRecord>);
static_assert(std::is_same_v<decltype(L::LogRecord::Identifier),
                              const S::TypeIdentifier>);
static_assert(S::TypeIdentifierOf<L::LogRecord> ==
              L::LogRecord::Identifier);
static_assert(L::LogRecord::Identifier.IsValid());
static_assert(L::LogRecord::Identifier.Authority().IsValid());
static_assert(L::LogRecord::Identifier.LocalIdentifier().IsValid());

static_assert(S::TypeIdentifierOf<L::LogRecord>.Bytes() ==
              S::TypeIdentifier::Storage{
                0x00U,0x00U,0x01U,0x00U,0x00U,0x00U,0x00U,0x01U
              });
static_assert(S::TypeIdentifierOf<L::LogRecord>.Authority().Bytes() ==
              S::TypeAuthorityIdentifier::Storage{0x00U,0x00U,0x01U});
static_assert(S::TypeIdentifierOf<L::LogRecord>.LocalIdentifier().Bytes() ==
              S::TypeLocalIdentifier::Storage{
                0x00U,0x00U,0x00U,0x00U,0x01U
              });

// Guard against accidentally claiming a complete canonical 15-field record.
static_assert(!S::SchemaType<L::LogRecord>);
static_assert(!std::is_default_constructible_v<L::LogRecord>);

int main() { return 0; }
