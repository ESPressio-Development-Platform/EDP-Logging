# Build and test qualification

The [19 November release assessment](../docs/ACCEPTANCE-G1-G15.MD#current-release-assessment--19-november-2026)
supersedes the historical open-work statements below. Current qualification
passed all 68 Logging host runners,40 Python tests,eight consuming Serial
cases and all12 demo builds. The current retained-claim and owned-async
WROOM executions, final five-minute workload and independent raw-media
receipts are recorded there with exact executable revisions and scope.
Declaration/reference and compiler-control reconciliation is complete
for this selected release. No hosted Actions execution is claimed.

Native GCC and Clang suites include negative compile cases, canonical wire, Memory/Threading integration, queue lifetime, EDLS capacity/rolling/recovery and Python media audits. Additional ESP-IDF/Arduino firmware builds and RPI400 relay-controlled hard power cuts are separate qualification dimensions.

## Boundaries

Use exact detached run receipts and the source commit recorded in the [qualification ledger](../docs/LOGGING-QUALIFICATION-STATUS.MD). Do not mark a build, flash, cut or durability proof PASS without its actual receipt.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.

## 9 November 2026 — additional minimal core demonstration

The newly maintained `demos/FramingAndInstrumentation/` family contains separate Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF applications, all using the **existing C++20-only** `ESPressio_Logging.hpp` umbrella (no EDP concrete dependencies). It checks compile-time severity gating including zero evaluated work when disabled; one single-context `AttemptSequence` allocation; EDLG bounded framed JSON payload; strict borrowed `ReadEdlgFrame` view; and CRC rejection of a corrupted frame. The JSON bytes `{}` **are not** the generated 15-field diagnostic LogRecord: no translation, CBOR, serial transport, archive persistence or NVS ACK is claimed. See [minimal demo contract](../demos/FramingAndInstrumentation/README.MD). Each target must actually be compiled before qualification; a checked-in source tree or successful compile alone never proves firmware executed on an ESP32.
## 12 November 2026 — current Logging main post-merge verification

Published `EDP-Logging/main` code-equivalent baseline `9fea51b0bdebcd8a4206628b1e1def81ff620381`, branch version `0.1.0` with no tag. Host/native job `20261112T092223-3f2f828e` PASSED **83/83**: 34 GCC + 34 Clang Logging tests, eight optional downstream Serial/Serial-ESP-IDF feature host tests, and all three demo families compiled/linked for both PlatformIO Arduino-ESP32 and ESP-IDF, plus Python. Exact final Python docstring/CLI description changes passed 40 tests. The separate 9–10 Nov and earlier physical WROOM finite-rate and genuine relay PowerLoss receipts remain distinct from these build-only 12 Nov results.

Current [qualification status](../docs/LOGGING-QUALIFICATION-STATUS.MD), [acceptance matrix](../docs/ACCEPTANCE-G1-G15.MD) and [Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-09/2026-11-09-09-29-EDP-Logging-Dependency-First-Reintegration.MD) distinguish exact tested profiles, historical negatives, the then-pending V2 source member audit and optional providers. The RPI400 lock was absent and MCU power OFF at final verification.

## Maintained workflow dependency selection

Host foundation, optional ClockEra and ESP-IDF firmware workflows use `main` for EDP-System, EDP-Localisation, EDP-Serialisation, EDP-Memory, EDP-Threading and EDP-Clock where each job selects those owners. The previous workflow refs for Memory, Threading and Localisation resolved to the same commits at this checkpoint. System, Serialisation and Clock advanced only documentation or ignore rules relative to the previous refs. The native fixture's specific ESP-IDF Platform commit remains an application-profile selection. The optional Serial abstract and ESP-IDF companion checkouts also use their verified `main` refs.

The existing RPI400 host and native receipts apply to their recorded checkout topology. The workflow changes here correct branch selection. Hosted workflow execution is tracked independently.

The [G12 integration ledger](../docs/SERIAL-OUTPUT-INTEGRATION.MD) records the RPI400 physical UART fixture and companion post-main runs. The G1 workflow generates its diagnostic schema with the existing Localisation tool before compiling the fixture. The host fan-out fixture uses the Portable SpinLock and the native fixture uses the ESP-IDF SpinLock.


The public demo manifests select the PIOArduino ESP32 platform and C++20.
See the [Arduino-ESP32 framing manifest](../demos/FramingAndInstrumentation/PlatformIO_Arduino/platformio.ini)
for the qualified pin and build flags. The native workflow runs all eight
PlatformIO demo combinations. RPI400 also compiles the four Arduino IDE
sketches using the Arduino CLI and ESP32 core.


## Reproducing the selected Arduino profile

The four PlatformIO Arduino demo manifests explicitly select
Arduino-ESP32 3.3.12 and its ESP-IDF 5.5 library bundle alongside the
recorded PIOArduino platform revision. The framing recipe uses the same
pins as the two managed demo families. Preserve both package selections
when reproducing the qualified build.

Concurrent RPI400 native and demo builds use distinct
PLATFORMIO_BUILD_CACHE_DIR locations. A shared SCons cache replacement
failure was observed during the first final sweep. The isolated-cache
retry and its exact run IDs are recorded in the current Policy handoff.

## Owned asynchronous delivery example

[OwnedAsyncSink](../demos/OwnedAsyncSink/README.MD) adds the fourth
three-target family. Its common Bootstrap selects one existing Memory
payload slot and Threading Dedicated Worker. The Worker checks its copy
after source mutation; Bootstrap verifies terminal delivery, exact lease
release and runtime teardown. All three targets built on source
db0bdf797756709b51ed8c926af4f6a09671c4fa. The native WROOM execution
20261119T080249-d436698d passed with result 0 and verified restoration
of the original full flash image. The native workflow covers eight
PlatformIO combinations; RPI400 also built all four Arduino IDE sketches.
See the demo README for exact receipts and [async ownership](../docs/ASYNC-COPYING-TASK-SINK.MD)
for the provider-wait and failure-preservation contracts.

## GitHub-resolved PlatformIO demos

Current demo manifests fetch library dependencies through GitHub lib_deps
URLs. The source/profile migration and exact isolated-build receipts are
tracked in the [Policy handoff](https://github.com/ESPressio-Development-Platform/Policy/blob/main/Handoff/2026-11-19/2026-11-19-09-25-Logging-Estate-Demo-GitHub-Dependencies.MD).
These recipe builds are separate from earlier physical UART/archive
qualification. No fresh hardware execution is implied.

## GitHub URL demo recipe qualification — 19 November2026

All eight Logging PlatformIO recipes passed in isolated inputs containing
demo assets and no local production src tree or sibling repositories.
Arduino framing passed in20261119T094437-9c5c966d before that larger
matrix's later async include-collision failure. Corrected async,
configurable and typed Arduino cases passed in20261119T095314-6b9b7bcc.
All four native cases passed in20261119T094437-f381dc01; the shared typed
hook preparation also passed20261119T095733-c35b053f.

The package-aware selector preserves the intended ESP-IDF provider for
overlapping header names; dependency sources remain installed GitHub
packages. Current manifest bytes match the tested copies, and each case
has an actual linked firmware image. Installed Git revisions are retained
in the case receipts. Private dependencies use the authorized GitHub
transport cache on RPI400; normal developers need repository access in Git.
No global Git settings or demo credentials were introduced.

These are build/recipe qualifications. The earlier independent UART,
archive, finite-load and relay PowerLoss receipts retain their original
source/profile scope; no new MCU flashing occurred in this migration.

## Complete root README application — 19 November 2026

The initial [progressive consumer walkthrough](../README.MD#getting-started) at
commit e113d4898d19d0fd6c8a821d54f5c01519f371ea showed all five project files for actual governed source capture and typed
JSON console output under PIOArduino. It includes the GitHub-only manifest,
owner-generated diagnostic schema, committed identity metadata, canonical
Clock composition, fixed resource capacities, severity sites and exact
error disposition. The [profile contract](../docs/BASIC-CONSOLE-INTEGRATION.MD)
distinguishes synchronous console acceptance from delivery or durable logs.

Final RPI400 run **20261119T115428-a557d627 PASS** verifies:

- Extraction of the exact README manifest, schema hook, two application
  headers and Arduino main, followed by actual ESP32 firmware compilation
  and ELF/binary linkage. Tested file bytes match the published source blocks.
- Both GCC and Clang execution of the same application against real EDP
  headers/providers with scripted native NVS/MAC/console services and a
  deterministic Clock-compatible test timebase.
- Eleven cases per compiler: initial identity, existing incarnation advance,
  corrupt/zero/exhausted counter, commit/readback failure, NVS init/open
  failure, MAC failure and partial console acceptance.
- Exact JSON envelope and field checks: Info/Warning, attempts1/2,
  arguments[1]/[2], message1/0/3, nonzero incarnation, canonical time,
  unsynchronized provenance, fingerprint and reserved record facts.
  Optional synchronized field7 is correctly absent.
- README relative links, direct discovery of every current docs page,
  whitespace and unchanged runtime/package/demo/test source.

Earlier receipts remain accurate: 20261119T114630-05d1df5f failed while
narrowing the source bundle's language configuration, and
20261119T114751-05dedd6e rejected an incorrect tutorial SynchronizedReading
constructor. Both defects were corrected in the application example.
20261119T114937-7056aecd passed the initial complete firmware;
20261119T115146-9e289250 passed initial exact README firmware extraction;
20261119T115248-66003b3d passed the initial scripted application cases.
The final run supersedes their example bytes and strengthens JSON assertions.

This work did not upload firmware, execute the application on a physical
device, measure UART throughput or requalify durable log archives. NVS is
used for the runtime incarnation, not as a log sink.

## Authored Localisation inputs — 19 November 2026

The current README replaces the source-construction Python hook with all
nine raw JSON files for the Platform/application Localisation bundles.
Together with platformio.ini and the three unchanged C++ files, all thirteen
authored project files are shown. Dependency installation is followed by the
existing Localisation CLI validate, compile and verify-generated commands;
headers and language packs are compiler outputs. The firmware consumes the
contract header, without loading language packs at runtime.

Run **20261119T120547-4d23f145 PASS** verified the exact raw source files,
standard owner CLI sequence, language-pack output and complete ESP32
PIOArduino firmware compile/link. Run **20261119T120731-30474162 PASS**
verified fresh README/file byte equality, all local documentation links and
docs discovery, unchanged runtime/package/demo/test source and eleven real-EDP
scripted application cases each under GCC and Clang, including exact record
field behavior for the newly generated schema. Installed Logging runtime
headers were also byte-compared with the candidate source.

Initial run 20261119T120436-3c0d55a8 failed the owner's integrity validation
because the displayed pretty-printed Platform files had a different byte
digest from the old compact bundle. The application manifest now binds the
exact UTF-8/LF files shown, and the tutorial explains that editing/reformatting
the bundle requires an updated matching digest. No library defect or runtime
source change was involved. Earlier receipts retain their original scope.
No firmware was uploaded or physically executed for this correction.
