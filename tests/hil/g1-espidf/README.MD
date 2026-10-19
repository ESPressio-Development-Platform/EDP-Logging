# EDP-Logging G1 physical ESP32-WROOM / ESP-IDF qualification

This opt-in test exercises the partial G1 transactional queue-before-Memory
admission on a physical ESP32, using real EDP-Memory object pool leases,
EDP-Threading bounded wake and ordinary mutex providers, platform-specific
FreeRTOS Mutex/Signal concretes selected by application Bootstrap, and a
single canonical EDP-Clock timebase. It is NOT a complete LogRecord producer,
worker, sink or durable archive.

## Authoritative dependencies

Read Policy/main/V2 and the current Logging living handoff. All EDP package
versions remain 0.1.0. Ignore deprecated ESPressio-* repositories. Supply
a source-pinned EDP_LOGGING_DEPENDENCY_ROOT with EDP-Logging, EDP-Memory,
EDP-Threading, EDP-Clock, EDP-System, EDP-BoundedTopology, EDP-BoundedTypes,
EDP-Platform, EDP-Platform-FreeRTOS and EDP-Platform-Portable siblings.
PlatformIO intentionally uses direct pinned source include roots rather than
allowing library metadata to pull unrelated main branch revisions. The
PioArduino platform commit is d41d8383706dc146fd91a0e1d3c59a915a4d493e,
with ESP-IDF 5.5.5 and C++20.

## Build

From this directory, with matching approved cached PlatformIO toolchain:

    export EDP_LOGGING_DEPENDENCY_ROOT=/absolute/path/to/pinned/siblings
    export PLATFORMIO_CORE_DIR=/absolute/path/to/platformio/cache
    pio run -e esp32dev

## Physical execution and expected results

The RPI400-integrated ESP32 reset/download pins are controlled by the
esp32-integrated helper; USB-UART DTR/RTS assumptions do not apply.
A tester MUST inspect board security/flash size, back up the complete flash,
verify it, then flash the correct bootloader, partition table and firmware
using the integrated helper. Collect actual 115200 baud Serial after reset.

    EDP_LOGGING_HIL|START|G1_WROOM_REAL_EDP_PROVIDERS
    EDP_LOGGING_HIL|CLOCK|CANONICAL_1MHZ
    EDP_LOGGING_HIL|PASS|FIFO_MEMORY_OWNERSHIP
    EDP_LOGGING_HIL|PASS|DUAL_CORE_PRODUCER_WAKE
    EDP_LOGGING_HIL|PASS|DRAIN_SLOT_ISOLATION
    EDP_LOGGING_HIL|PASS|SHUTDOWN_AND_TEARDOWN
    EDP_LOGGING_HIL|RESULT|PASS

Any FAIL marker or missing expected marker is NOT physical PASS. Restore the
original flash image after testing unless the user explicitly authorises
permanent replacement. Original flash images may contain secrets: retain
only in secured appliance recovery storage and never publish/commit them.
Record genuine HIL run IDs, firmware/source SHA, observed serial, backup and
actual restoration evidence in the living Policy handoff. Do not confuse
build success, simulation, physical operation and proven power-loss durability.

## Extended HIL source-to-UART case

This application also tests RecordProduction.hpp using a standalone Clock
provider with no synchronized coordinate and no Mesh. The Memory-owned
record carries a unique attempt ticket and a pre-admission Clock reading,
then a bounded sink encodes test-only CBOR with EDP-Serialisation and writes
the EDLG/CRC frame to real UART. capture-production.py independently verifies
all nine expected markers and the actual frame CRC. Tested on physical
ESP32-D0WD, 3 independent reset runs. Not a canonical governed LogRecord or
durability certification. Detailed proof in VALIDATION.MD.

## Current NVS and actual EDP-Serial native UART hardware validation

Current firmware adds application-composed EDP-Persistence-ESP-IDF
NvsKeyValueStorage, EDP-Platform-Portable EDP-Memory ByteOperations,
and EDP-Serial-ESP-IDF native UART0 FIFO through EDP-Serial's existing
bounded EDLG frame queue. It does not import either concrete provider
from Logging's own headers and it needs no Mesh.

A fresh whole-chip original backup and SHA-256 verification are mandatory
before flashing. UART observer is attached while ESP32 remains in
ROM-download mode; after reset, boot 1 stores a test-only framed CBOR
value to the app's NVS namespace and restarts the MCU. Boot 2 must
recover and checksum-check it and delete the key, then run the previous
dual-core G1 source tests and send an actual raw binary UART EDLG frame.

Run current capture-serial-persistence.py (old capture-production.py
forwards to it). Exactly 11 expected markers, no failures and a
host-decoded CRC-valid binary frame are required. Three hardware
cycles PASS; see VALIDATION.MD. Restoring and independently verifying
all 4 MiB original firmware BEFORE final reboot is mandatory.
NVS restart readback cannot stand in for an external power cut;
the canonical LogRecord root remains unissued.

## Extra 2026-10-16 physical three-sink Memory test

The app now publishes one additional native pooled Record and reports
MULTI_SINK_NATIVE_RECLAMATION after composing three synchronous sinks
that return Accepted, Rejected, and ProviderFailure. The existing
real EDP-Memory ConsumerClaim is shared, no sink is called twice,
the aggregate outcome is Partial/FailedAndReclaimed, and native
pool live-instance count returns to zero. Current capture script
expects 12/12 lifecycle markers as well as actual NVS recovery
and EDP-Serial raw wire CRC verification. This still does not
constitute a multi-task worker/fork/durable LogRecord pipeline.

## 2026-10-16 disposable SPIFFS VFS owner proof

New application-only test partitions.csv isolates a **temporary**
edpfs SPIFFS partition (0x210000, 1984 KiB) from the factory app.
The HIL app imports existing EDP-Persistence-ESP-IDF VfsFileStorage
and ESP-IDF SPIFFS **ONLY at application Bootstrap**, never inside
Logging source. TestConcreteEdlsVfsSuccessor publishes generation 1
into an empty physical file slot, reopens the native FileStorage,
publishes generation 2 into the other empty slot, proves safe refusal
to overwrite either slot, injects actual VFS file corruption and
proves the first generation's CRC prefix remains clean. Files are
cleaned and the mount closed. Host serial capture expects 13 app
and host markers, including NATIVE_EDLS_VFS_SUCCESSOR_GUARD.

THREE physical ESP32 campaigns succeeded 13/13 each; complete
original 4 MiB flash was backed up immediately before HIL and
restored with independent full-chip digest matching before normal
original firmware reboot. See VALIDATION.MD for all source pins,
evidence and limits. This is NOT externally interrupted POWERLOSS,
atomic filesystem replace, destructive generation rotation, or
final canonical LogRecord certification.


## G11 compiled-out source site test

The new test-only EDP_LOGGING_G11_APP_SITE_ENABLED switch is 0 by default and 1 for an enabled Info source site. Build with [dual-image comparator](../../run-g11-whole-firmware-comparison.sh) using pinned EDP_LOGGING_DEPENDENCY_ROOT and a fresh G11_REPORT_DIR; optional EDP_LOGGING_G11_PIO_BIN, EDP_LOGGING_G11_SIZE_BIN and EDP_LOGGING_G11_PLATFORMIO_CORE_DIR select installed tools. This builds and hashes linked images; it does not flash. The actual UART capture sets EDP_LOGGING_G11_APP_SITE_ENABLED=0/1 in its **host** environment and requires two correctly selected site observations, one on each side of the expected NVS restart, in addition to the unchanged 18 functional markers. See [historical HIL record](VALIDATION.MD).
