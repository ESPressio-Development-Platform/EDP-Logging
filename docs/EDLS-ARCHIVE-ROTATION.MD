# G6/G7 finite EDLS oldest-eligible rotation

**Source:** `src/logging/EdlsArchiveRotation.hpp`.

**Status:** Optional bounded EDP-Persistence FileStorage composition; **not** a
power-cut-safe journal or release-qualified archive. The existing
`PublishEdlsSuccessorIntoEmptySlot` remains unchanged as the conservative
never-overwrite option.

## Approved D13 lifecycle contract

Bootstrap provides a static table of **2–64** unique, independent EDLS
FileStorage paths, a 16-byte canonical DeviceIdentifier, nonzero
RuntimeIncarnationId, a checksum-valid complete EDLG frame, a maximum payload
size of 1–4096 bytes, a finite segment size and **exclusive mutation ownership**
from scan through independent readback. It selects a FileStorage provider
through its existing EDP-Persistence composition. EDP-Logging introduces
no concrete filesystem SDK, mutex, queue, allocator, task scheduler or clock.

`PublishEdlsOldestEligibleSuccessor<Slots,PayloadMaximum,SlotMaximum>(...)`
scans every known slot with `InspectEdlsArchive` and validates the
source frame, identity, generation and bounded capacity *before* mutation.
When an empty slot exists, the existing guarded empty-slot publication
routine remains authoritative. Once **all** slots are occupied, reclamation
is permitted only when **every** slot is CRC-clean, each generation is unique,
the device identities agree, and the 64-bit next generation has not
exhausted. It chooses the lowest generation except the newest; it refuses
any damaged tail, invalid header, unreadable slot, duplicate generation or
identity conflict. Selection is based on finite canonical generation
order, **never** wall-clock timestamps.

It checks the candidate's FileStorage GetFileSize a second time, prepares
the standard 40-byte EDLS header plus complete bounded EDLG frame in
local fixed-size scratch, and performs exactly one `ReplaceFile` call on the
oldest slot, never on the previously newest slot. It then reopens and
independently validates the old newest generation (header identity, version,
CRC, frame count, entire contiguous verified extent), verifies the replaced
slot's new generation/header/frame CRC/size and independently compares
every newly read frame byte against the expected output, in 64-byte
bounded read chunks. The result retains the raw EDP-Persistence
`FileReplaceStatus`, chosen and protected slots, no-wrap generation
number, and explicit `PriorNewestVerified` fact.

## Failure/retention truth

`EdlsRollingStatus` distinguishes initial Empty-slot publication,
oldest-eligible replacement, invalid frame/identity, archive ambiguity,
capacity and generation limits, changed candidate preflight,
native provider refusal, unconfirmed readback, unconfirmed prior
newest, and no reclaimable candidate. A damaged replaced slot
following a torn write is **not** automatically erased/retried: its
next archive scan refuses a destructive repair until an approved
separate recovery procedure determines whether the remaining clean
history is sufficient. No false "complete rotation" status follows a
partial/native provider failure.

**No result from this API alone means fsync, flush-to-media,
interruption atomicity, remote receipt, physical retained history or
externally CUT-power survival.** Existing
EDP-Persistence-ESP-IDF VFS adapters may explicitly declare
`FailurePreservation::MayModify` and
`InterruptionAtomicity::None`. A fully checksum-valid normal readback
cannot substitute for those provider guarantees. The application must
choose a stronger independently verified storage contract for a
PowerLoss-qualified deployment; a true external power switch and
observed subsequent recovery remain separate G7/G15 tests.

## Verification and portability

The expanded `EdlsArchiveInspectionTests.cpp` uses an actual complete
two-slot fake FileStorage lifecycle over **64 generations**, validating
that each old newest remains identical and readable, exact no-wrap
generation sequences, corrupt source and mismatching identity refusal,
faulted native Replace, truncated successful replacement,
fail-closed subsequent recovery, unreadable/duplicate/wrapped
generations and explicit per-slot facts.

`RealVfsArchiveSuccessorTests.cpp` additionally composes the actual
`EDP-Persistence-ESP-IDF::VfsFileStorage` owner on a host OS directory,
publishes ten successive rotating generations, and reopens fresh
concrete provider objects to verify both physical files at every
generation, then injects an actual OS-file corrupted tail and proves
no automatic destructive retry.

The existing ESP-IDF/PioArduino WROOM HIL
`tests/hil/g1-espidf/src/main.cpp` now includes the optional
`EdlsArchiveRotation.hpp` in *application composition only*, exercises
three rotations on real SPIFFS, checks the preceding newest CRC/identity
through fresh VFS handles, refuses post-corruption destructive
rotation, and emits
`EDP_LOGGING_HIL|PASS|NATIVE_EDLS_VFS_ROLLING_GUARD`.
The serial capture validator requires this **20th** mandatory marker.
The physical proof is valid only once real device flash and capture
receipts are recorded; do not infer a HIL pass from this source or
from a successful cross-compilation.

The entire G6/G7/G8/G9 product gate also requires a qualified archival
schema/issuer manifest, clear retention/wear selection, torn-tail
recovery, application-level final delivery and a controlled external
power-interruption test. None are claimed complete here.


## 2026-10-23 native SPIFFS EDLS rolling qualification

Real integrated RPI400 ESP32-WROOM-32 PIOArduino/ESP-IDF firmware built
successfully under detached run `20261023T082753-c258aae5` (**PASS, exit0**),
after owner-generated Localisation schema fixture
`20261023T082731-6210621f` (**PASS, exit0**). Exact combined
`firmware.factory.bin` **467,200 bytes**, SHA-256
`354e98347121bbce2a4c006b50b6ca1cab342547b9770eb4110b263d8ea155b8`.

RPI400 detached integrated-device campaign
**`20261023T083422-cc50dcba` PASS exit0**: performed actual
`esp32-integrated bootloader`, `esptool write-flash 0x0` and a **separate**
`esptool verify-flash` with matching digest, followed by **three
independent reset/UART full HIL cycles**. Every cycle reported
`EDP_LOGGING_NATIVE_HIL_SUMMARY|markers=20/20|fail=0|binary_edlg_verified=True|resource_snapshot_verified=True|g11_site_verified=True`.
The new mandatory `NATIVE_EDLS_VFS_ROLLING_GUARD` passed in every
cycle, as did the previous native EDLS successor, G10 real asynchronous
Threading worker, binary EDLG CRC and shutdown/teardown markers.

**Actual measured whole-test** free8 heap before/after/min:
277,100 / 273,580 / 268,656 bytes; app-main stack margin:
7,544 bytes; existing DedicatedCoordinator stack margin:
5,036 bytes. These are **not** isolated Logging/rotation stack or
product-worst-case budgets. The raw serial trace/firmware binary,
flash/verify log and SHA manifests remain only on the private RPI400
under `/srv/espressio/scratch/gpt6-logging-20261023-0755-gates/edls-rolling-physical-20261023`,
with durable detached run history under
`/srv/espressio/runs/20261023T083422-cc50dcba/`.
They must not be committed/published.

All controlled resets and on-device readbacks are **normal-operation
recovery evidence**, **not** uncontrolled external power-cut durability.
No original pre-test firmware backup or restoration was performed in
this G6/G7 campaign: the integrated device was already dedicated to
Logging HIL firmware before it started.

## 2026-10-23 — physical GPIO17 power-loss evidence (bounded #12 gate)

RPI400 now uses its V2-authorised relay to switch the integrated WROOM's
3.3V supply. The user explicitly confirmed no parasitic back-powering
through other connections. This qualifies the OFF/ON operation as a true
electrical power interruption, unlike `esp_restart()` or reset-only HIL.
The dedicated `tests/hil/g7-power-cut-espidf` fixture uses a corrected
2 MiB flash partition topology, genuine
`EDP-Persistence-ESP-IDF::VfsFileStorage` (SPIFFS), two fully committed
EDLS segments, a separate 96 KiB interrupted native replacement, and a
strictly nonformatting read-only restart inspection. It retains an exact
full-device pretest flash backup.

RPI400 detached physical receipt `20261023T110910-d66bc60e` is
**PASS exit 0**: image write/independent verify; relay OFF after
`REPLACE_ENTER`; 2.37s off; power ON; `ESP_RST_POWERON`; EDLS slot0
clean generation 1 and slot1 clean generation 2; archive unambiguous;
the interrupted separate file reported zero bytes; no
`REPLACE_RETURN` marker before/after the cut. The relay was left OFF.

This demonstrates **prior committed EDLS retention in that actual cut**.
It does not make `VfsFileStorage` interruption-atomic, nor prove all
timings or interrupted writes to the EDLS slot itself. The provider still
correctly advertises `FileInterruptionAtomicity::None`,
`AppendInterruptionAtomicity::None` and `FailurePreservation::MayModify`.
The previously hardware-blocked part of #12 is now resolved, but
permanent PowerLoss-product assurance requires further rollover-cut and
provider-contract qualification.

## 2026-10-23 — second physical relay-cut: torn EDLS successor

The first GPIO17 power-cut HIL established prior-generation retention
during a separate native SPIFFS replacement. A **distinct** second
fixture, `tests/hil/g7-torn-slot-espidf`, then simulated a torn EDLS
oldest-slot successor: a valid generation-3 header followed by only
8 bytes of an incomplete EDLG frame. The file write returned before
the host cut physical ESP32 power for 2.3925 seconds. On restored power
ESP-IDF reported `ESP_RST_POWERON` (1); physical readback classified
the generation-3 slot `DamagedTail`, retained generation 2 as clean
with one checksum-valid frame, and the real
`PublishEdlsOldestEligibleSuccessor` refused to rotate that ambiguous
topology, leaving the protected slot unchanged. RPI400 durable test
`20261023T113223-4de652e4` **PASS exit0** with raw private evidence.

The cut occurred **after** the incomplete ReplaceFile returned and
before any subsequent publication. Do not claim this proves interrupted
ReplaceFile atomicity, universal recovery or a power-loss-safe VFS
capability. See the fixture README for the exact phase boundary.
All version identifiers remain `0.1.0`.
