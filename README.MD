# EDP-Logging

Bounded, structured diagnostic logging for the [ESPressio Development Platform](https://github.com/ESPressio-Development-Platform).

**Version:** `0.1.0` · **Language:** C++20 · **License:** Apache-2.0

EDP-Logging provides deterministic, application-composed diagnostic record capture, bounded producer/consumer delivery, portable record framing and finite durable log archives. The application chooses resource limits, concrete providers, storage placement and retention capacity. The library does not introduce an allocator, task scheduler, filesystem or platform clock of its own.

## Contents

- [Capabilities](#capabilities)
- [Getting started](#getting-started)
- [Application composition](#application-composition)
- [Dependencies](#dependencies)
- [Durability and resource boundaries](#durability-and-resource-boundaries)
- [Documentation](#documentation)
- [Examples and testing](#examples-and-testing)
- [Compatibility and limitations](#compatibility-and-limitations)
- [Changelog](./CHANGELOG.md)

## Capabilities

| Area | Provided functionality |
| --- | --- |
| Diagnostic sites | Six emitted severity levels, compile-time threshold selection and optional evaluation-free disabled instrumentation |
| Record identity | Nonwrapping attempt sequences, typed source identities and a governed 15-field generated record model |
| Source capture | Schema-checked, bounded owned arguments, optional coherent Clock evidence and canonical CBOR/JSON adapters |
| Concurrent delivery | Finite queue admission and lease ownership, producer wake coordination, a dedicated worker and bounded sink fan-out |
| Local framing | EDLG record frames, EDLS immutable segments, strict decoding and inspection |
| Persistence | Application-configured finite logical byte capacity, sealed batch publication, protected rolling generations, strict acknowledgement through a separately qualified KeyValueStorage provider |
| Forensics | Offline decoding, chronology inspection and CSV export; explicit errors for unsupported or uncertain data |

The minimal umbrella header stays lightweight; more extensive facilities are **opt-in**, and require the application to compose their dependencies explicitly. Logging remains usable without Mesh and without a synchronized network clock.

## Getting started

This complete **PIOArduino / Arduino-ESP32** application emits two real,
schema-checked LogRecords to the board's serial console. It captures the
device/runtime identity, canonical local time, severity, attempt sequence and
an owned argument, then serializes the full **15-field typed JSON envelope**.
It uses synchronous console output so that a first integration needs no
worker, record queue or filesystem.

Every project file is shown below. Start with an empty project directory;
you do not need a Logging checkout, sibling repositories or demo assets.

### 1. Create the project and select dependencies

Create `platformio.ini`, `prepare_schema.py`, `include/AppIdentity.hpp`,
`include/BasicLogger.hpp` and `src/main.cpp` at the paths used below.

Save this as **`platformio.ini`**:

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32.git#d41d8383706dc146fd91a0e1d3c59a915a4d493e
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_compat_mode = off
lib_ldf_mode = chain+
extra_scripts = pre:prepare_schema.py

platform_packages =
    framework-arduinoespressif32 @ https://github.com/espressif/arduino-esp32/archive/refs/tags/3.3.12.zip
    framework-arduinoespressif32-libs @ https://github.com/espressif/esp32-arduino-lib-builder/releases/download/idf-release_v5.5/esp32-arduino-libs-idf-release_v5.5-b774170f-v3.zip

build_unflags = -std=gnu++11 -std=gnu++14 -std=gnu++17
build_flags = -std=gnu++20

lib_deps =
    https://github.com/ESPressio-Development-Platform/EDP-Logging.git#main
    https://github.com/ESPressio-Development-Platform/EDP-System.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Clock.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Memory.git#main
    https://github.com/ESPressio-Development-Platform/EDP-BoundedTopology.git#main
    https://github.com/ESPressio-Development-Platform/EDP-BoundedTypes.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Localisation.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Serialisation.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Platform.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Platform-Portable.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Persistence.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Persistence-ESP-IDF.git#main
    https://github.com/ESPressio-Development-Platform/EDP-Platform-ESP-IDF.git#87f9a71918be0ac606b5acc2479204fbe45b77db
    https://github.com/ESPressio-Development-Platform/EDP-Security.git#mesh_v1
```

This selects an ESP32-WROOM-class `esp32dev` board and the qualified
PIOArduino/Arduino-ESP32 bundle. Change the board and port to match your
hardware. Library URLs use `main` for the current 0.1.0 API; pin reviewed
commit SHAs for reproducible application builds.

The native Platform provider remains at an explicit qualified revision
because the required timebase is not yet integrated into that repository's
`main`. Its package metadata also selects Security's `mesh_v1` branch, listed
explicitly above. These are application provider/package-resolution edges;
basic Logging does not acquire a runtime Security or Mesh dependency.
Configure Git access outside this file for any private repositories.

### 2. Generate the application's diagnostic schema

Logging records identify a Localisation message and own arguments matching
its generated signature. Do not invent numeric descriptors or hand-write a
replacement generated header.

Save this as **`prepare_schema.py`**. The build hook reuses Localisation's
complete getting-started source bundle, selects English, and defines message
**1/0/3**, `Startup step {0}`, with one exact **`u32`** argument. It invokes
the owning generator to create the headers under `include/generated`.

```python
"""Generate this application's diagnostic contract through EDP-Localisation."""
from pathlib import Path
import json
import shutil
import sys

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
packages = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
owner = packages / "EDP-Localisation"
sys.path.insert(0, str(owner / "tools"))
from edp_localisation.generator import compile_generated_set

# Reuse the owner's complete source-bundle format, not a test-only generator.
example = owner / "examples/getting-started"
source = project / ".generated/localisation-source"
shutil.copytree(example / "reference-source", source, dirs_exist_ok=True)
for language in ("de", "de-AT"):
    shutil.rmtree(source / language, ignore_errors=True)
manifest_path = source / "manifest.json"
manifest = json.loads(manifest_path.read_text())
manifest["supportedLanguages"] = ["en-GB"]
manifest_path.write_text(json.dumps(manifest))

catalogue_path = source / "strings_catalogue.json"
catalogue = json.loads(catalogue_path.read_text())
message = catalogue["domains"][0]["subDomains"][0]["strings"][0]
message["parameters"] = [
    {"index": 0, "status": "active", "type": "u32", "symbol": "Step"}
]
catalogue_path.write_text(json.dumps(catalogue))
language_path = source / "en-GB/language.json"
language = json.loads(language_path.read_text())
language["displayNames"] = {}
language_path.write_text(json.dumps(language))
strings_path = source / "en-GB/strings.json"
strings = json.loads(strings_path.read_text())
strings["domains"]["1"]["subDomains"]["0"]["strings"]["3"] = "Startup step {0}"
strings_path.write_text(json.dumps(strings))

compile_generated_set(
    source,
    example / "platform-localisation",
    [source / "schema-inventory.json"],
    project / "include/generated",
    "App::Diagnostic",
)
```

The application source is the copied catalogue and the explicit edits above;
the generated descriptor carries its diagnostic fingerprint. Nothing is
loaded from a filesystem at runtime. The JSON record preserves message
identity and the numeric argument; it does not automatically render a
translated sentence.

### 3. Establish device and runtime identity

Each record needs a stable device identity and a durably advanced runtime
incarnation, so sequence 1 after a reboot cannot be mistaken for the previous
boot's sequence 1. This is identity metadata, not durable log storage.

Save this as **`include/AppIdentity.hpp`**. It reuses the existing NVS
KeyValueStorage, Memory byte operations and CBOR codec. A missing counter
starts at one; corrupt data, exhaustion, commit failure or failed readback
stops logging. Startup never erases NVS to conceal a failure.

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <esp_mac.h>
#include <ESPressio_System.hpp>
#include <ESPressio_Serialisation.hpp>
#include <ESPressio_Platform_Portable_ByteOperations.hpp>
#include <NvsKeyValueStorage.hpp>

namespace App {

    namespace Identity = ESPressio::System::Identity;
    namespace Persistence = ESPressio::Persistence;
    namespace Codec = ESPressio::Serialisation;

    /// Outcomes of reading this application's existing boot counter.
    enum class CounterRead : std::uint8_t {
        /// A nonzero stored counter was decoded.
        Found = 0U,
        /// No counter exists in this namespace.
        Missing = 1U,
        /// The stored value or provider read was not trustworthy.
        Invalid = 2U
    };

    /// Application identity bootstrap outcomes.
    enum class IdentitySetup : std::uint8_t {
        /// The new identity is committed and read back.
        Ready = 0U,
        /// The native device MAC could not be read.
        HardwareUnavailable = 1U,
        /// Existing metadata was unreadable or invalid.
        StoredCounterInvalid = 2U,
        /// The incarnation cannot advance without wrapping.
        CounterExhausted = 3U,
        /// The next counter was not confirmed durable.
        CommitUnconfirmed = 4U
    };

    /// Dedicated application metadata namespace binding.
    struct IdentityStore final {};
    /// Reuse the existing Memory byte-copy provider.
    using Bytes = ESPressio::Platform::Portable::Memory::ByteOperationsProvider;
    /// Existing committed NVS blob provider; bootstrap owns its lifetime.
    using Ledger = Persistence::EspIdf::NvsKeyValueStorage<IdentityStore, Bytes>;

    /// Reads and validates a complete CBOR counter without guessing on failure.
    [[nodiscard]] inline CounterRead ReadCounter(
        Ledger& ledger,
        std::uint32_t& value
    ) noexcept {
        std::array<std::byte, 8U> encoded{};
        const auto read = ledger.ReadValue(
            Persistence::KeyView::Validate("incarnation").Value,
            {encoded.data(), encoded.size()}
        );
        if (read.Status == Persistence::KeyValueReadStatus::NotFound) {
            return CounterRead::Missing;
        }
        if (read.Status != Persistence::KeyValueReadStatus::Succeeded ||
            !Persistence::HasFact(
                read.Facts,
                Persistence::ReadFact::AvailableDataSizeIsKnown
            ) ||
            read.AvailableDataSize.RawValue != read.BytesTransferred) {
            return CounterRead::Invalid;
        }
        const auto decoded = Codec::Deserialise<Codec::Cbor>(
            std::span<const std::byte>(
                encoded.data(),
                read.BytesTransferred
            ),
            value
        );
        return decoded.IsSuccessful() && value != 0U
            ? CounterRead::Found : CounterRead::Invalid;
    }

    /// Commits a new incarnation before exposing it to any log producer.
    [[nodiscard]] inline IdentitySetup EstablishIdentity(
        Ledger& ledger,
        Identity::DeviceIdentifier& device,
        Identity::RuntimeIncarnationId& incarnation
    ) noexcept {
        std::array<std::uint8_t, 6U> mac{};
        if (esp_read_mac(
                mac.data(),
                ESP_MAC_WIFI_STA
            ) != ESP_OK) {
            return IdentitySetup::HardwareUnavailable;
        }
        // Tutorial identity namespace "EDP-README" followed by the hardware MAC.
        Identity::DeviceIdentifier::Storage raw{
            0x45U, 0x44U, 0x50U, 0x2DU, 0x52U,
            0x45U, 0x41U, 0x44U, 0x4DU, 0x45U
        };
        const Bytes copy{};
        copy.CopyBytes(
            raw.data() + 10U,
            mac.data(),
            mac.size()
        );

        std::uint32_t previous{0U};
        const auto read = ReadCounter(
            ledger,
            previous
        );
        if (read != CounterRead::Found && read != CounterRead::Missing) {
            return IdentitySetup::StoredCounterInvalid;
        }
        if (previous == std::numeric_limits<std::uint32_t>::max()) {
            return IdentitySetup::CounterExhausted;
        }
        const std::uint32_t next = previous + 1U;
        std::array<std::byte, 8U> encoded{};
        const auto written = Codec::Serialise<Codec::Cbor>(
            next,
            encoded
        );
        if (!written.IsSuccessful() ||
            ledger.StoreValue(
                Persistence::KeyView::Validate("incarnation").Value,
                {encoded.data(), written.BytesWritten}
            ) != Persistence::KeyValueStoreStatus::Succeeded) {
            return IdentitySetup::CommitUnconfirmed;
        }
        std::uint32_t observed{0U};
        if (ReadCounter(
                ledger,
                observed
            ) != CounterRead::Found || observed != next) {
            return IdentitySetup::CommitUnconfirmed;
        }
        device = Identity::DeviceIdentifier{raw};
        incarnation = Identity::RuntimeIncarnationId{next};
        return IdentitySetup::Ready;
    }

} // namespace App
```

The hardware MAC plus the tutorial's `EDP-README` prefix creates this
example's device namespace. A product should use its authoritative provisioned
device identity and one application-wide incarnation authority. Do not erase
or reset the `edpreadme/incarnation` metadata while reusing this identity:
that would allow old device/incarnation/sequence combinations to recur.
Use an appropriately provisioned development board for the tutorial.

### 4. Capture, serialize and submit a complete record

Save this as **`include/BasicLogger.hpp`**. This small application adapter
joins the existing Logging facilities; it does not implement a new codec,
allocator, clock or transport. Its record has capacity for one owned
argument and four argument bytes; JSON output has a fixed 1024-byte limit.

```cpp
#pragma once

#include <Arduino.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "AppIdentity.hpp"
#include "generated/GeneratedLocalisationContract.hpp"
#include <ESPressio_Clock.hpp>
#include <ESPressio_Logging.hpp>
#include <logging/LogRecordSourceAssembly.hpp>
#include <logging/GeneratedLogRecordWireProjection.hpp>

namespace App {

    namespace Logging = ESPressio::Logging;
    namespace Clock = ESPressio::Clock;
    /// The owner-generated message 1/0/3 has exactly one uint32 argument.
    using Message = Diagnostic::DiagnosticMessageSchema<1U, 0U, 3U>;
    /// One owned argument and four argument bytes; no queue or dynamic pool.
    using Record = Logging::BasicLogRecord<
        Logging::AttemptSequenceWidth::Bits32,
        1U,
        4U
    >;

    /// Outcomes of synchronous record production and console submission.
    enum class EmitResult : std::uint8_t {
        /// The console accepted all JSON bytes and the newline.
        ConsoleAccepted = 0U,
        /// No unused attempt ticket remains in this incarnation.
        AttemptsExhausted = 1U,
        /// Record assembly rejected identity, time or arguments.
        SourceRejected = 2U,
        /// The generated wire projection rejected the source.
        ProjectionRejected = 3U,
        /// The bounded JSON encoder rejected the record.
        EncodingRejected = 4U,
        /// Some console output was not accepted; do not blindly retry.
        ConsoleIncomplete = 5U
    };

    /// Single-context logger; its identity owner must outlive it.
    class BasicLogger final {

    private:

        // Stable identity and per-incarnation attempt owner.

        /// Authoritative immutable identity established before construction.
        const Identity::SystemIdentity& _identity;
        /// Tickets never wrap; each enabled attempt consumes one ticket.
        Logging::AttemptSequenceCounter<
            Logging::AttemptSequenceWidth::Bits32
        > _attempts{};

    public:

        // Construction and synchronous emission.

        /// Borrows the application's address-stable identity facade.
        explicit BasicLogger(const Identity::SystemIdentity& identity) noexcept
            : _identity(identity) {}

        /// Prevents duplicate ownership of the per-incarnation attempt sequence.
        BasicLogger(const BasicLogger&) = delete;
        /// Prevents replacement of the sequence owner.
        BasicLogger& operator =(const BasicLogger&) = delete;
        /// Keeps the identity/sequence binding address-stable.
        BasicLogger(BasicLogger&&) = delete;
        /// Prevents transfer into another sequence owner.
        BasicLogger& operator =(BasicLogger&&) = delete;

        /// Captures one real record and submits its complete JSON envelope.
        [[nodiscard]] EmitResult Emit(
            Logging::Severity severity,
            std::uint32_t step
        ) noexcept {
            Logging::AttemptSequence<Logging::AttemptSequenceWidth::Bits32> attempt{};
            if (_attempts.Issue(attempt) !=
                Logging::AttemptSequenceAllocationResult::Issued) {
                return EmitResult::AttemptsExhausted;
            }

            // This application has no synchronized clock; report that honestly.
            const Logging::CoherentLogClockSample observation{
                Clock::MonotonicNow(),
                Clock::SynchronizedReading{
                    Clock::SynchronizedTimestamp{},
                    Clock::SynchronizationUncertainty::Maximum(),
                    Clock::SynchronizationState::NeverSynchronized
                },
                Clock::ClockEra{},
                false
            };
            const auto temporal = Logging::QualifyLogClockSample(observation);
            Record record{};
            if (Logging::AssembleLogRecordSource<Message>(
                    record,
                    severity,
                    _identity.Device(),
                    _identity.Incarnation(),
                    attempt,
                    temporal,
                    step
                ) != Logging::LogRecordAssemblyStatus::Prepared) {
                return EmitResult::SourceRejected;
            }

            Logging::GeneratedLogRecordWireProjection<
                Logging::AttemptSequenceWidth::Bits32,
                std::uint32_t
            > wire{};
            Logging::OwnedPositionalScratch<std::uint32_t> positional{};
            if (!Logging::ProjectGeneratedLogRecord<
                    Message,
                    Logging::AttemptSequenceWidth::Bits32,
                    std::uint32_t
                >(
                    record,
                    wire,
                    positional
                ).IsSuccessful()) {
                return EmitResult::ProjectionRejected;
            }
            std::array<std::byte, 1024U> output{};
            const auto encoded = Codec::Serialise<
                Codec::Json,
                Codec::RootProfile::TypedEnvelope
            >(
                wire,
                output
            );
            if (!encoded.IsSuccessful()) {
                return EmitResult::EncodingRejected;
            }
            const auto accepted = Serial.write(
                reinterpret_cast<const std::uint8_t*>(output.data()),
                encoded.BytesWritten
            );
            if (accepted != encoded.BytesWritten || Serial.write('\n') != 1U) {
                return EmitResult::ConsoleIncomplete;
            }
            return EmitResult::ConsoleAccepted;
        }

    };

} // namespace App
```

`ConsoleAccepted` means the Arduino console accepted all JSON bytes and the
newline. It does not prove physical delivery, host receipt or persistence.
A partial write may already have exposed a prefix; the application reports
failure without blindly resubmitting the record.

This adapter is for **one ordinary execution context**. Serial writes can
block according to the selected Arduino core's behavior. Use the existing
bounded queue/coordinator and an appropriate sink when your application
needs concurrent producers or asynchronous delivery.

### 5. Bootstrap and call logging sites

Save this as **`src/main.cpp`**. Bootstrap establishes identity, selects the
existing native timebase, binds the one canonical EDP-Clock and then creates
the logger. The whole logging operation remains inside each lazy severity
gate, including ticket allocation, clock capture and argument evaluation.

```cpp
#include <Arduino.h>
#include <nvs_flash.h>

#include "BasicLogger.hpp"
#include <clock/MonotonicClockProvider.hpp>
#include <timing/EspTimerMonotonicTimebaseProvider.hpp>

/// Application bootstrap: no log producers exist before identity/Clock are ready.
void setup() {
    Serial.begin(115200);

    // Failure is reported on the console; no automatic NVS erase or retry.
    if (nvs_flash_init() != ESP_OK) {
        Serial.println("BOOT FAILED: NVS unavailable");
        return;
    }
    static App::Bytes bytes{};
    static App::Ledger ledger(bytes);
    if (ledger.Open("edpreadme") !=
        ESPressio::Persistence::EspIdf::NvsOpenStatus::Succeeded) {
        Serial.println("BOOT FAILED: identity namespace unavailable");
        return;
    }
    App::Identity::DeviceIdentifier device{};
    App::Identity::RuntimeIncarnationId incarnation{};
    const auto established = App::EstablishIdentity(
        ledger,
        device,
        incarnation
    );
    ledger.Close();
    if (established != App::IdentitySetup::Ready) {
        Serial.println("BOOT FAILED: identity not committed; no records emitted");
        return;
    }
    static const App::Identity::SystemIdentity identity(
        device,
        incarnation
    );

    // Exactly one existing native timebase and one canonical application Clock.
    using Timebase =
        ESPressio::Platform::ESPIDF::Timing::EspTimerMonotonicTimebaseProvider;
    using CanonicalClock = App::Clock::MonotonicClockProvider<Timebase>;
    namespace Composition = ESPressio::System::CompositionFramework;
    using Wiring = Composition::Architecture<
        Composition::Composition<App::Clock::Domain, Timebase, CanonicalClock>,
        Composition::Composition<ESPressio::Memory::Domain, App::Bytes>,
        Composition::Composition<App::Persistence::Domain, App::Ledger>
    >;
    static_assert(Wiring::IsValid);
    static Timebase timebase{};
    static CanonicalClock clock(timebase);
    if (!App::Clock::BindMonotonicClock(clock)) {
        Serial.println("BOOT FAILED: canonical Clock already bound");
        return;
    }
    static App::BasicLogger logger(identity);
    constexpr auto minimum = App::Logging::SeverityThreshold::Info;

    // Keep the entire operation inside the gate: disabled sites do no capture.
    auto result = App::EmitResult::ConsoleAccepted;
    EDP_LOG_WHEN_ENABLED(
        minimum,
        App::Logging::Severity::Debug,
        result = logger.Emit(
            App::Logging::Severity::Debug,
            std::uint32_t{0U}
        )
    );
    if (result != App::EmitResult::ConsoleAccepted) {
        Serial.println("LOG FAILED: debug record; no automatic resubmission");
        return;
    }
    EDP_LOG_WHEN_ENABLED(
        minimum,
        App::Logging::Severity::Info,
        result = logger.Emit(
            App::Logging::Severity::Info,
            std::uint32_t{1U}
        )
    );
    if (result != App::EmitResult::ConsoleAccepted) {
        Serial.println("LOG FAILED: startup record; no automatic resubmission");
        return;
    }
    EDP_LOG_WHEN_ENABLED(
        minimum,
        App::Logging::Severity::Warning,
        result = logger.Emit(
            App::Logging::Severity::Warning,
            std::uint32_t{2U}
        )
    );
    if (result != App::EmitResult::ConsoleAccepted) {
        Serial.println("LOG FAILED: warning record; no automatic resubmission");
        return;
    }
    Serial.println("README LOGGING COMPLETE");
}

/// This finite startup example creates no background producer or logging task.
void loop() {}
```

The selected minimum is `Info`: the Debug site does no record work and
consumes no attempt ticket. Info and Warning produce attempts 1 and 2.
The uint32 argument is 1 for the startup record and 2 for the warning.
Raise the threshold to `Warning` to suppress the Info site, or select `Off`
to compile out all three sites.

There is no synchronized clock in this application. Records retain the
canonical local monotonic coordinate and explicitly unavailable synchronized
evidence; no timestamp is manufactured from `millis()` or an unrelated
counter. Keep the static identity, clock and logger alive for the application
lifetime. This finite example emits only during `setup()`; `loop()` is empty.

### 6. Build, upload and observe the records

Run from the project directory:

```bash
pio run -e esp32dev
pio run -e esp32dev -t upload --upload-port YOUR_BOARD_PORT
pio device monitor --port YOUR_BOARD_PORT --baud 115200
```

Replace `YOUR_BOARD_PORT` with your board's serial port. Reset the board
after opening the monitor if startup output was missed. No filesystem image,
partition CSV, external Python package or `uploadfs` step is needed.

With the default Info threshold and successful bootstrap, expect:

1. One typed JSON record for message **1/0/3**, severity **Info**, attempt
   **1**, positional arguments **[1]**.
2. One typed JSON record for the same message, severity **Warning**, attempt
   **2**, positional arguments **[2]**.
3. `README LOGGING COMPLETE`.

The JSON root has a `$edp` typed-envelope header and a `value` object whose
keys are governed field identifiers. In that object, field `0` is the message
triple, `1` is severity (Info = 2, Warning = 3), `3` is the incarnation,
`4` is the attempt, `6` is local monotonic nanoseconds, and `13` is the
positional argument array. Optional field `7` is omitted because this
application has no synchronized timestamp; that is valid 15-field-schema
output, not a truncated record.

Device bytes, incarnation and monotonic coordinates vary. A normal reboot
advances the incarnation and restarts its attempt sequence. On failure, a
`BOOT FAILED` or `LOG FAILED` console line identifies the stopped stage;
no success summary follows. The text error lines are bootstrap diagnostics,
not fabricated LogRecords.

### 7. Extend the integration deliberately

The example is a complete synchronous console integration. Add facilities
only as your application needs them:

- [Record production and ownership](./docs/RECORD-PRODUCTION-OWNERSHIP.MD),
  [bounded record admission](./docs/BOUNDED-RECORD-ADMISSION.MD) and
  [dedicated coordination](./docs/DEDICATED-COORDINATOR.MD) for queued producers.
- [Typed Managed Batch Worker](./demos/TypedManagedBatchWorker/README.MD)
  for the complete source → worker → generated CBOR → finite acknowledged
  LittleFS/NVS archive application.
- [Basic console integration contract](./docs/BASIC-CONSOLE-INTEGRATION.MD)
  for this walkthrough's lifetime, resource, identity and output boundaries.

The [demo index](./demos/README.MD) also contains framing, configurable
batching and asynchronous owned-sink examples in all three maintained target
layouts. Persistent logging requires its own finite archive capacity,
provider qualification and checked durable acknowledgement.


## Application composition

A full logger is assembled at application Bootstrap, rather than by invoking a global singleton:

1. Select the **generated diagnostic schema**, message/type identities, optional temporal evidence and compile-time record capacities.
2. Configure the **source pool**, transactional queue, deadline/wake primitives and one dedicated Threading coordinator, using the existing EDP-Memory / EDP-Threading / EDP-Clock owners.
3. Choose one or more **typed sinks** and supply their owned, bounded representation buffers.
4. For persistent storage, select actual **FileStorage** and a separately qualified durable **KeyValueStorage** provider, choose an immutable **finite logical byte capacity** before archive initialization, and configure sealed batch size and maximum dwell.
5. Start producers and the worker only after all providers and lifetimes are valid. At shutdown, stop admission, drain or explicitly report outstanding ownership, settle the sink, join tasks and tear down providers in the documented order.

A queue admission or sink acceptance is **not** a durable acknowledgement. A generation is acknowledged only after the required checked publication and durable watermark conditions succeed. Failure or uncertainty is reported explicitly; there is no implicit erase, format, indefinite retry or fabricated recovery.

See [Architecture](./docs/ARCHITECTURE.MD), [Record Production Ownership](./docs/RECORD-PRODUCTION-OWNERSHIP.MD), [Coordinator Sink Service](./docs/COORDINATOR-SINK-SERVICE.MD) and [Strict Acknowledged Recovery](./docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) before integrating a custom sink.

## Dependencies

The **default** `#include <ESPressio_Logging.hpp>` requires the **C++20 standard library only**. The following recursive tree describes opt-in activation; conditional dependencies are *not* imposed merely by installing EDP-Logging:

```text
EDP-Logging [C++20 core; no mandatory EDP library]
├── EDP-Clock [optional: temporal evidence, source records, admission or workers]
│   ├── EDP-System
│   └── EDP-Platform
│       └── EDP-System
├── EDP-Memory [optional: frame writing, owned arguments, pools or archives]
│   ├── EDP-System
│   ├── EDP-Platform
│   │   └── EDP-System
│   └── EDP-BoundedTopology
├── EDP-Threading [optional: admission, queue, coordinator or async sink headers]
│   ├── EDP-System
│   ├── EDP-Platform
│   │   └── EDP-System
│   ├── EDP-Clock
│   │   ├── EDP-System
│   │   └── EDP-Platform
│   │       └── EDP-System
│   ├── EDP-BoundedTopology
│   └── EDP-Memory
│       ├── EDP-System
│       ├── EDP-Platform
│       │   └── EDP-System
│       └── EDP-BoundedTopology
├── EDP-System [optional: source records, identity and Composition]
├── EDP-Platform [optional: admission/queue locks and lifecycle atomics]
│   └── EDP-System
├── EDP-BoundedTopology [optional: transactional record queue]
├── EDP-BoundedTypes [optional: bounded arguments, generated records or batching]
│   ├── EDP-Memory
│   │   ├── EDP-System
│   │   ├── EDP-Platform
│   │   │   └── EDP-System
│   │   └── EDP-BoundedTopology
│   └── EDP-Platform-Portable
│       ├── EDP-Platform
│       │   └── EDP-System
│       └── EDP-Memory
│           ├── EDP-System
│           ├── EDP-Platform
│           │   └── EDP-System
│           └── EDP-BoundedTopology
├── EDP-Serialisation [optional: diagnostic argument codecs or generated record projection]
│   ├── EDP-System
│   ├── EDP-BoundedTypes
│   │   ├── EDP-Memory
│   │   │   ├── EDP-System
│   │   │   ├── EDP-Platform
│   │   │   │   └── EDP-System
│   │   │   └── EDP-BoundedTopology
│   │   └── EDP-Platform-Portable
│   │       ├── EDP-Platform
│   │       │   └── EDP-System
│   │       └── EDP-Memory
│   │           ├── EDP-System
│   │           ├── EDP-Platform
│   │           │   └── EDP-System
│   │           └── EDP-BoundedTopology
│   ├── EDP-BoundedTopology
│   ├── EDP-Memory
│   │   ├── EDP-System
│   │   ├── EDP-Platform
│   │   │   └── EDP-System
│   │   └── EDP-BoundedTopology
│   ├── EDP-Platform-Portable
│   │   ├── EDP-Platform
│   │   │   └── EDP-System
│   │   └── EDP-Memory
│   │       ├── EDP-System
│   │       ├── EDP-Platform
│   │       │   └── EDP-System
│   │       └── EDP-BoundedTopology
│   ├── EDP-Localisation
│   │   ├── EDP-System
│   │   └── EDP-Memory
│   │       ├── EDP-System
│   │       ├── EDP-Platform
│   │       │   └── EDP-System
│   │       └── EDP-BoundedTopology
│   └── EDP-Clock
│       ├── EDP-System
│       └── EDP-Platform
│           └── EDP-System
├── EDP-Localisation [optional: generated diagnostic schema contracts]
│   ├── EDP-System
│   └── EDP-Memory
│       ├── EDP-System
│       ├── EDP-Platform
│       │   └── EDP-System
│       └── EDP-BoundedTopology
├── EDP-Persistence [optional: archive or checkpoint headers]
│   └── EDP-System
└── EDP-Platform-Portable [optional: default owned-argument/archive copy provider]
    ├── EDP-Platform
    │   └── EDP-System
    └── EDP-Memory
        ├── EDP-System
        ├── EDP-Platform
        │   └── EDP-System
        └── EDP-BoundedTopology
```

The Serialisation → Clock edge applies when its ClockEra adapter is imported,
as the whole-record projection does here. The Serialisation → Clock edge applies when its ClockEra adapter is imported,
as the whole-record projection does here. Each selected owner is expanded to its foundations; [the complete conditional and transitive dependency graph](./DEPENDENCY_GRAPH.MD) specifies actual header-level requirements and provider ownership. Declare direct dependencies explicitly in the application. The application chooses concrete ESP-IDF/Arduino/host implementations at Bootstrap; **EDP-Serial** and **EDP-Mesh** are not required by Logging. The optional historical-schema **Python** tooling uses EDP-Localisation's Python tools, not an embedded library dependency.

## Durability and resource boundaries

- **Finite retention:** the application supplies the logical archive byte budget, including the required reserved generations and acknowledgement value, before initialization. Oldest **eligible** generations roll out; still-protected records are not retired before a verified, acknowledged successor exists.
- **Power-loss semantics:** an acknowledged EDLS generation uses checked immutable segments and a separately qualified durable NVS/KeyValueStorage watermark. This does **not** make arbitrary filesystem operations or every FileStorage provider interruption-atomic.
- **Memory and scheduling:** all source/queue/worker/representation limits are bounded and configured for the application. Admission deadlines and drain latency depend on the selected providers, workload and hardware.
- **Flash endurance:** greater application-chosen persistent storage capacity generally reduces reuse frequency and wear, but the library makes **no device lifespan guarantee**. Prefer separate storage for high-volume production diagnostics.
- **Clock semantics:** basic local logging does not require synchronized clocks. Applications that require cross-node precision must separately qualify and supply the relevant Clock/Era evidence.

For normative detail use [Immutable Persistence Byte Capacity](./docs/IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD), [Protected EDLS Rolling](./docs/EDLS-PROTECTED-ROLLING.MD), [PowerLoss Acknowledged Recovery](./docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) and [Production Retention and Endurance](./docs/PRODUCTION-RETENTION-ENDURANCE-REVIEW.MD).

## Documentation

The documents under `./docs` define the detailed implementation contracts. Use this index to locate the relevant owner and integration rules without treating historical test receipts as API guidance.

### Architecture, record schema and producer contracts

- [Conditional Cross-Repository Dependency Baseline](./docs/DEPENDENCY-OWNER-BASELINE.MD)
- [Basic Console Integration](./docs/BASIC-CONSOLE-INTEGRATION.MD)
- [Architecture](./docs/ARCHITECTURE.MD) · [Log Record Identity](./docs/LOG-RECORD-IDENTITY.MD) · [Log Record Source Assembly](./docs/LOG-RECORD-SOURCE-ASSEMBLY.MD)
- [Record Production Ownership](./docs/RECORD-PRODUCTION-OWNERSHIP.MD) · [G4 Generated Whole Record](./docs/G4-GENERATED-WHOLE-RECORD.MD) · [G4 Core Field Adapters](./docs/G4-CORE-FIELD-ADAPTERS.MD)
- [G4 Approved Field Wire](./docs/G4-APPROVED-FIELD-WIRE.MD) · [G4–G10 Owned Root Fan-out](./docs/G4-G10-OWNED-ROOT-FANOUT.MD)
- [Diagnostic Argument Encoding](./docs/DIAGNOSTIC-ARGUMENT-ENCODING.MD) · [Bounded Diagnostic Arguments](./docs/DIAGNOSTIC-BOUNDED-ARGUMENTS.MD) · [Owned Argument Byte Operations](./docs/OWNED-ARGUMENT-BYTE-OPERATIONS.MD)
- [Diagnostic System Identity](./docs/DIAGNOSTIC-SYSTEM-IDENTITY-ARGUMENTS.MD) · [Diagnostic Clock Era](./docs/DIAGNOSTIC-CLOCK-ERA-ARGUMENTS.MD) · [Coherent Temporal Evidence](./docs/COHERENT-TEMPORAL-EVIDENCE.MD)
- [Owned Positional Re-encoding](./docs/OWNED-DIAGNOSTIC-POSITIONAL-REENCODING.MD) · [Owned Representation Fan-out](./docs/OWNED-REPRESENTATION-FANOUT.MD)

### Admission, concurrency and sink lifecycle

- [Bounded Admission](./docs/BOUNDED-ADMISSION.MD) · [Bounded Record Admission](./docs/BOUNDED-RECORD-ADMISSION.MD) · [Bounded Queue Transitions](./docs/BOUNDED-QUEUE-TRANSITIONS.MD)
- [Admission Wake Coordinator](./docs/ADMISSION-WAKE-COORDINATOR.MD) · [Admission Lifecycle](./docs/ADMISSION-LIFECYCLE.MD) · [Atomic Lifecycle Binding](./docs/ADMISSION-LIFECYCLE-ATOMIC-BINDING.MD)
- [Tracked Admission Leases](./docs/TRACKED-ADMISSION-LEASES.MD) · [Transactional Record Queue](./docs/TRANSACTIONAL-RECORD-QUEUE.MD)
- [Dedicated Coordinator](./docs/DEDICATED-COORDINATOR.MD) · [Coordinator Sink Service](./docs/COORDINATOR-SINK-SERVICE.MD) · [Asynchronous Copying Task Sink](./docs/ASYNC-COPYING-TASK-SINK.MD)

### Frames, archives, retention and persistence

- [EDLG KeyValue Checkpoint](./docs/EDLG-KEYVALUE-CHECKPOINT.MD) · [EDLS Archive Inspection](./docs/EDLS-ARCHIVE-INSPECTION.MD) · [EDLS Archive Append](./docs/EDLS-ARCHIVE-APPEND.MD)
- [EDLS Safe Successor](./docs/EDLS-SAFE-SUCCESSOR.MD) · [EDLS Archive Rotation](./docs/EDLS-ARCHIVE-ROTATION.MD) · [EDLS Protected Rolling](./docs/EDLS-PROTECTED-ROLLING.MD)
- [EDLS Acknowledged Recovery](./docs/EDLS-ACKNOWLEDGED-RECOVERY.MD) · [PowerLoss EDLS Journal](./docs/POWERLOSS-EDLS-JOURNAL.MD) · [PowerLoss Snapshot Profile](./docs/POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD)
- [Bounded EDLS Batching](./docs/BOUNDED-EDLS-BATCHING.MD) · [Typed EDLS CBOR Archive](./docs/TYPED-EDLS-CBOR-ARCHIVE.MD) · [Immutable Persistence Capacity](./docs/IMMUTABLE-PERSISTENCE-BYTE-CAPACITY.MD)
- [Strict ACK Same-Transaction Plan Reuse](./docs/EDLS-STRICT-ACK-SAME-TRANSACTION-PLAN-REUSE-2026-10-28.MD) · [Strict ACK Exclusive-Writer Scan Reuse](./docs/EDLS-STRICT-ACK-EXCLUSIVE-WRITER-SCAN-REUSE-2026-10-28.MD) · [Post-Retirement Scan Optimisation](./docs/EDLS-POSTRETIRE-SCAN-OPTIMISATION-2026-10-29.MD)

### Host tools, diagnostics and archival analysis

- [Optional Serial Output and G12 Provider Integration](./docs/SERIAL-OUTPUT-INTEGRATION.MD)
- [Host Decoder](./docs/HOST-DECODER.MD) · [Forensic CSV Export](./docs/FORENSIC-CSV-EXPORT.MD) · [Historical Diagnostic Schema Bridge](./docs/HISTORICAL-DIAGNOSTIC-SCHEMA-BRIDGE.MD)

### Qualification and evidence

- [G1–G15 Acceptance Matrix](./docs/ACCEPTANCE-G1-G15.MD) · [Logging Qualification Status](./docs/LOGGING-QUALIFICATION-STATUS.MD) · [Long Managed Workload Qualification](./docs/LONG-MANAGED-WORKLOAD-QUALIFICATION.MD)
- [Managed Batch Capacity-Cut Qualification](./docs/MANAGED-BATCH-CAPACITY-CUT-QUALIFICATION.MD) · [Managed Batch Write-Cut Qualification](./docs/MANAGED-BATCH-WRITE-CUT-QUALIFICATION.MD)
- [Sealed Batch Retirement-Cut Qualification](./docs/SEALED-BATCH-RETIREMENT-CUT-QUALIFICATION.MD) · [Sealed Batch Write-Activity Qualification](./docs/SEALED-BATCH-WRITE-ACTIVITY-QUALIFICATION.MD)
- [ESP32-WROOM Resource Qualification](./docs/WROOM-RESOURCE-QUALIFICATION.MD) · [256-Record Provider Latency](./docs/EDLS-PUBLISH-256-RECORD-PROVIDER-LATENCY-2026-10-28.MD) · [Production Retention Endurance Review](./docs/PRODUCTION-RETENTION-ENDURANCE-REVIEW.MD)

See also [Dependency Graph](./DEPENDENCY_GRAPH.MD) and the [internal developer Wiki](./wiki/Home.md) for source-level extension and reference material.

## Examples and testing

PlatformIO demo projects declare their library dependencies as GitHub URLs
in lib_deps. Run their pio commands directly with the repository-owned demo
source/assets present; dependency resolution does not require sibling
library checkouts. See the [demo index](./demos/README.MD).


Buildable applications are indexed in [demos/README.MD](./demos/README.MD), with all three target source layouts documented in their own READMEs. The [Framing and Instrumentation](./demos/FramingAndInstrumentation/README.MD) core example has independently passed GCC/Clang execution and both PlatformIO Arduino-ESP32 and ESP-IDF compile/link builds on the approved RPI400; it does not constitute generated-record or persistence qualification. The repository's `tests/` tree contains GCC/Clang host validations, generated-schema and CBOR tests, bounded queue and recovery negatives, and separate physical ESP32 HIL fixtures.

A typical host prerequisite is to provide a **pinned sibling source dependency root** for the opt-in tests:

```sh
export EDP_LOGGING_DEPENDENCY_ROOT=/path/to/pinned/EDP-repositories
CXX=g++ bash tests/run-edls-protected-rolling-tests.sh g++
CXX=clang++ bash tests/run-edls-protected-rolling-tests.sh clang++
```

Physical HIL tests require explicit appliance provisioning, actual connected hardware and their target-specific runners; do not treat a host test pass as physical PowerLoss evidence. The [qualification status](./docs/LOGGING-QUALIFICATION-STATUS.MD) and [acceptance matrix](./docs/ACCEPTANCE-G1-G15.MD) record tested scopes and remaining qualifications.

## Compatibility and limitations

EDP-Logging is at **`0.1.0`** on `main`. Select the matching owner revisions from the [dependency baseline](./docs/DEPENDENCY-OWNER-BASELINE.MD) for the optional APIs you import. The core library is C++20, with integration examples targeting Arduino-ESP32 and ESP-IDF. Optional providers may be unavailable for a particular target and must be qualified independently. The implementation intentionally does **not** promise indefinite log retention, universal 100-records/sec performance, generic power-atomic FileStorage, network clock synchronization, native-memory record dumping or Flash lifespan. Where evidence is conditional or a feature is deferred, the [acceptance matrix](./docs/ACCEPTANCE-G1-G15.MD) is authoritative.

Licensed under [Apache-2.0](./LICENSE).
