# G7 EDLS torn-oldest-slot relay recovery

This is a SECOND, deliberately destructive physical HIL phase on the same
RPI400-integrated ESP32-WROOM-32 used by
`../g7-power-cut-espidf`. The user has qualified GPIO17 relay interruption as
genuine power loss by confirming that the wiring prevents back-powering.
Both stages use the approved `esp32-integrated` daemon and a real
`EDP-Persistence-ESP-IDF::VfsFileStorage` SPIFFS backend; no platform-native
operations are imported into production EDP-Logging.

## Required predecessor and safety

Run the first fixture successfully first. That established two full
EDLS segments with identity 0x9A: slot0/gen1 and slot1/gen2, recorded
as `20261023T110910-d66bc60e`. The first phase left its pre-test 2 MiB
full-flash backup and original hardware logs in RPI400's private scratch.

This second phase uses the exact same correct **2 MiB physical flash**
partition map. It does not format, seed or repair SPIFFS, and it will refuse
to proceed unless the original two segments are independently re-verified.
It uses a separate test-only NVS namespace `edptorn26` to record its phase.

The test deliberately replaces **only the oldest** slot (slot0) with a
valid checksum-protected generation-3 EDLS header and **eight bytes of
an incomplete EDLG frame**. This simulates a torn generation rollover
between native FileStorage mutation and logical successor completion.
The `EDP_TORN|PARTIAL_PUBLISHED` marker is emitted AFTER this
partial write returns successfully; the relay then cuts the real
ESP32 3.3V supply. **This particular cut is between two logical
write phases, not necessarily while ReplaceFile itself is in flight.**

After restoring VDD the test requires an actual `ESP_RST_POWERON`,
verifies the oldest slot is `DamagedTail` with generation 3 and no full
frames, verifies the untouched slot1 still contains generation 2 with
one CRC-valid frame, then invokes the real
`PublishEdlsOldestEligibleSuccessor` API. It must refuse to rotate
the ambiguous/damaged topology, and independent readback must show that
this refusal did not modify the protected slot.

## Build and execution

On the exclusively locked RPI400 (following Policy V2):

```sh
export EDP_LOGGING_DEPENDENCY_ROOT=/path/to/pinned/EDP-sibling-repos
cd "$EDP_LOGGING_DEPENDENCY_ROOT/EDP-Logging/tests/hil/g7-torn-slot-espidf"
pio run -e esp32dev

export EDP_LOGGING_AGENT_ID=<current-RPI400-lock-owner>
export EDP_LOGGING_EVIDENCE_DIR=/absolute/unique/test-evidence-directory
bash run_relay_cut_test.sh
```

The helper uses the approved controller to flash+independently verify, hold
reset while the UART monitor starts, cut the relay, restore power, and
observe the read-only recovery outcome. Both runner and monitor have
fail-safe relay-OFF cleanup. No script should erase/format NVS or SPIFFS
automatically, nor assume `/dev/serial0` implies powered hardware.

## Interpretation

A passing result confirms a real power-loss reboot in a deliberately
created *partial EDLS generation* state, with the previously committed
newest generation intact and further rotation refusing to proceed.
It is strong crash-recovery evidence, but does not provide a universal
PowerLoss-atomicity capability for SPIFFS `VfsFileStorage` (which still
truthfully advertises `InterruptionAtomicity::None`). Neither this
fixture nor its predecessor changes any library version from `0.1.0`.


## Observed physical results — 2026-10-23

The dedicated source is compiled with the pinned PioArduino/ESP-IDF 5.5.5
and EDP-* dependency topology. Build **PASS exit 0** recorded as
`20261023T112430-efc2ed76`. Physical first attempt
`20261023T113004-4ef09f88` returned PASS as an appliance run, but the
corrected, authoritative fully checked relay/HIL receipt is
**`20261023T113223-4de652e4`**, PASS exit 0.

The corrected run independently flashed and verified the image before
opening the UART; GPIO17 was commanded OFF after receipt of
`EDP_TORN|PARTIAL_PUBLISHED|slot=0|gen=3|bytes=48`, remained OFF for
**2.3925 seconds**, and was then commanded ON. A distinct second boot
was observed, `ESP_RST_POWERON` (reset reason 1), without any firmware
return from the intentionally incomplete publication before cut.
Recovery scanned physical SPIFFS and observed a valid generation-3
header with damaged/incomplete frame tail in slot0, while the untouched
slot1 generation 2 remained Clean. Real
`PublishEdlsOldestEligibleSuccessor` refused that ambiguous topology,
and subsequent readback verified slot1 still Clean and slot0 damaged.
The explicit firmware marker was
`EDP_TORN|PASS|PROTECTED_GENERATION_SURVIVED_AND_FAILED_CLOSED`.
The controller left relay power OFF after the test.

This is genuine power-loss *between two logical writes* and fail-closed
archive recovery, **not** a proof that SPIFFS `ReplaceFile` is atomic
under power loss or that arbitrary in-flight replacement operations
preserve all prior content. The original complete 2,097,152-byte
pre-test chip image remains backed up privately on RPI400, SHA-256
`e5e87e86d9096752bf304edc98fbeb5b999aafbe20a49033a22c96b65b466a97`.
Do not publish that binary into Git. This HIL does not alter production
EDP-Logging behaviour or any 0.1.0 version identifier.
