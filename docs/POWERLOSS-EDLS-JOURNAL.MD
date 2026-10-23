# Bounded power-loss-atomic EDLS historical journal

## Scope and owned dependencies

The optional header logging/PowerLossEdlsJournal.hpp is a bounded historical
record-retention adapter, not a new flash-storage provider. It composes the
existing logging/PowerLossEdlsSnapshots.hpp owner, reusing canonical EDLG/EDLS
checksums, bounded parsing, and postcommit native readback.

Application Bootstrap selects a concrete EDP-Persistence::KeyValueStorage and
owns exclusive access to its unique keys. The provider must advertise both
RetentionLevel::PowerLoss and InterruptionAtomicity::PowerLoss for individual
key-value writes. Existing ESP-IDF NvsKeyValueStorage meets those declared
guarantees. SPIFFS VfsFileStorage does not and cannot be substituted. No
Logging-local raw-flash writer, worker, queue, allocator, Clock, mutex or
Mesh dependency is created.

## Explicit retention protocol

For TSlots = N + 1, the independent atomic values retain the N latest
successfully committed generations, one complete EDLG per EDLS key.
The extra key is sacrificial while the next generation is prepared.

1. Inspect all keys through the existing snapshot owner. Validate EDLG CRC,
   EDLS checksum, identity, unique nonzero generation, and canonical cyclic
   physical-key order. Protected generations must be complete and contiguous.
2. If the journal has not filled, select an empty key. When full, only the
   key immediately after newest, holding an already-retired oldest
   generation, is permitted as the write target.
3. Write the successor to that unprotected key using the existing provider;
   none of the N protected keys is modified by the pending write.
4. Acknowledge Committed only after successful native StoreValue, exact
   independent readback and full-window ordering verification.
5. Completion of native atomic key commit advances the logical retention
   window. A power cut after commit but before caller ACK may expose a valid
   newer, not-yet-acknowledged generation after reboot. That generation
   participates in retention, possibly retiring an older record before an
   external observer received the ACK. This is an unavoidable
   acknowledgement gap, not silent media loss.

## Invariants and failure handling

- Each key contains one at-most-512-byte value: 40-byte EDLS header plus
  one complete checksummed EDLG frame. The current snapshot owner limits
  TSlots to 2..8 and therefore retains just 1..7 records. This is NOT
  a high-capacity or unbounded production archive.
- Bounded stack scratch and fixed-slot scans; no new dynamic allocation.
  Application Bootstrap serializes all operations across the entire key
  namespace.
- A failed StoreValue may damage the unprotected key; all retained records
  remain readable when their keys are clean. Damaged sacrificial key means
  Degraded, permits read-only recovery, and blocks all further append.
- Missing protected generation, damaged protected key, duplicate generation,
  inconsistent key order, mixed device identities, unknown provider state
  and generation exhaustion all fail closed without recovery-time mutation.
- RecoverRetained refuses any generation outside the configured retention
  window, even if an older retired generation remains physically present.
- Atomicity is per-native-key StoreValue, NOT a cross-key transaction.
  The N+1 design requires no additional manifest for bounded retention,
  because each generation is independently fully atomic, and the extra
  slot protects all N still-retained records during an append.

## Limits and production follow-up

Production scale still requires a suitably bounded and wear-managed high-
capacity EDLS journal or provider: independent commit and reclamation phases,
possibly grouped records per segment, realistic write-amplification/endurance
budget, variable record sizes and streaming, qualified provider operation
semantics and full G1-G15 application orchestration. An unrelated generic
Persistence provider must live under the owning EDP-Persistence domain,
not as hidden Logging-local flash code. NVS flash wear, repeated power
interruption phases and retention under exhaustion still need validation.

## Verification

The established tests/PowerLossEdlsSnapshotsTests.cpp and existing
tests/run-powerloss-edls-snapshots-tests.sh/CI exercise rotating
generations across 3 keys with 2 retained; CRC readback; retirement
rejection; failed write damaging the sacrificial key while protected
records remain readable; fail-closed mutation; duplicate identity/key/
generation and weak-provider compile-fail contracts.

Physical native ESP-IDF qualification uses the dedicated
tests/hil/g7-nvs-journal-espidf/ fixture with user-qualified RPI400
GPIO17 hard-VDD interruption. Exact test run, persisted evidence and
limitations are recorded independently from this normative contract.


## 2026-10-23 actual user-qualified GPIO17 VDD interruption

The dedicated historical-journal ESP-IDF firmware built/linked against
pinned EDP-Persistence-ESP-IDF NVS on RPI400. Build detached run
20261023T125036-aaf02c2a PASS exit0. Independent write-flash/verify
followed a private 2,097,152-byte device flash backup, SHA256
62ad67dd63b5fe288617731fa8f0e0a9b3e6b1a3857ea2cb1bf17b41c4a8ea60.
Test firmware image: 255984 bytes, SHA256
890a5c3ebc707867ec3a9d32f46e201969dd2d6fc0bedc149dcdb024ecf5c27a.

Real GPIO17 power-removal HIL 20261023T125615-eded7957 PASS exit0,
12:56:15–13:00:05 CEST: last host-visible durable ACK before cut command
was generation 7, relay triggered at commit attempt 8. After true power
restoration, ESP_RST_POWERON (1) reported; native NVS/EDLS inspection
was Clean, oldest retained generation 18, newest 19, count 2.
Both generation 18 and 19 EDLG frames independently passed CRC and
payload-to-generation match. Host independently confirmed a second
boot and newest recovered >= last pre-command ACK. Controller finished
POWER=OFF GPIO17=HIGH.

This proves real MCU electrical interruption DURING an NVS append
campaign, NOT interruption within a specifically instrumented
native nvs_commit. The MCU can complete more transactions between
UART host command and physical disconnection, legitimately moving
the bounded retention window beyond last host-observed ACK.
One successful physical test is not exhaustive interruption-phase
or flash-wear certification.

Private backup, raw UART and firmware image remain only on RPI400
under /srv/espressio/scratch/gpt6-logging-journal-20261023-1240/
private-hil-journal-powerloss-20261023/ and durable run
/srv/espressio/runs/20261023T125615-eded7957/. No private
binary image or raw serial evidence is uploaded to GitHub.
