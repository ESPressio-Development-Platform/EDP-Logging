# Actual PowerLoss NVS-backed EDLS latest-generation test (G7 / #12)

## Scope

This isolated PIOArduino/ESP-IDF HIL application uses
`logging/PowerLossEdlsSnapshots.hpp` with the **real**
EDP-Persistence-ESP-IDF NvsKeyValueStorage owner, whose static
KeyValueStorage Properties advertise PowerLoss retention, PowerLoss
interruption atomicity and maximum 512-byte values. The user confirms
the RPI400 GPIO17 relay electrically disconnects all ESP32 VDD without
parasitic back-power. The host never touches relay GPIO directly.

The fixture creates two small CRC-validated EDLS generation keys in
namespace `edpksnap26`, then exercises a short bounded succession of
NVS StoreValue commits and exposes exact `ACK` markers only after
native commit **and independent complete readback**. The UART monitor
records the greatest generation acknowledged BEFORE turning the relay
OFF; the restarted device must report `ESP_RST_POWERON`, recover a
checksum-valid EDLS/EDLG latest generation, and report a generation at
least as new as that greatest host-observed ACK. Recovery of a newer
never-acknowledged-but-actually-committed candidate is permitted.

This design DOES NOT guarantee the host's GPIO relay command lands
inside the specific microsecond window of `nvs_commit()` without
independent flash-controller instrumentation. The hardware campaign
validates an asynchronous power interruption amidst a native commit
campaign and the explicit acknowledged-generation recovery contract.
Do not claim every possible torn NVS flash phase is covered.

## Resource, retention and wear constraints

* Two NVS keys, app-owned namespace, **one most-recent acknowledged
  generation** retained by policy; earlier generations explicitly
  retired as successors are acknowledged. This is NOT a historical
  multi-record EDLS archive or general SPIFFS FileStorage replacement.
* On-device per-key blob capacity 512 bytes including EDLS header and
  EDLG frame, no dynamic log buffers beyond existing provider internals.
* The artificial 80-commit maximum is a bounded proof workload, not
  an appropriate production flash write rate.
* All test state is confined to separate `edpdsnap26` (phase) and
  `edpksnap26` (snapshot) namespaces; no application secrets are
  read or logged. The flash image, UART raw bytes and firmware binary
  belong in an RPI400-private evidence directory only.
* No NVS erase/reformat/reinitialize recovery is performed on failure,
  and an unexpected nonempty key/phase is not silently overwritten.

## Running

Acquire RPI400 exclusive lock per Policy/main/V2, and assemble pinned
EDP-* repos under one `EDP_LOGGING_DEPENDENCY_ROOT`. Select a unique
private evidence path, set the invoking lock-owner ID, then run the
approved durable detached runner wrapping `run_relay_cut_test.sh`.

The runner preserves an independent full **2 MiB** pretest device
flash image, flashes and independently verifies the firmware image,
holds reset until UART capture is established, then cuts and restores
VDD using the Policy-owned `esp32-integrated` daemon. It records
pre-cut last ACK, recovered generation, power-on reset reason and
PASS/FAIL. A trap leaves the integrated ESP32 powered **OFF**.
Do not commit backups, NVS contents, or UART captures to GitHub.

This separate HIL fixture does not change EDP-Persistence's advertised
contracts, Logging main branch or any 0.1.0 version. The full
historical archive and wear-managed PowerLoss provider remain open.


## Verified native GPIO17 electrical cut — 2026-10-23

The user confirms there are no MCU back-power paths through peripherals
or UART. PIOArduino/ESP-IDF native application with actual
`EDP-Persistence-ESP-IDF::NvsKeyValueStorage` and newly implemented
`PowerLossEdlsSnapshots` built successfully: first full compiler
`20261023T121636-02df8343` PASS, final hardening build
`20261023T122234-20da2538` PASS exit0.

RPI400 run **`20261023T122318-b1b4e020` PASS exit0**:
exact 2,097,152-byte pretest flash image privately backed up
SHA-256 `7b0d4c1aa6ec8cb45cf4fd763eb4a682d63f85c5b18c8d90a1df867738224d3c`.
Final factory image SHA-256
`c92eb3ac95a0ecb1f39f5ce175feb47efeb2e6d465ede79f65059b2793c1a4f6`,
image bytes `254960`. Native flash write and independent esptool
verification PASS.

First native boot established two independently committed, readback-
verified EDLS/EDLG generations. The host observed acknowledged generation
**5** before initiating the relay cut during the continuing NVS commit
campaign. MCU continued committing briefly during command and relay
latency, and after power restoration reported `ESP_RST_POWERON` (1)
and independently recovered the **CRC-verified generation 29** as Clean
(`status=1`), 24-byte EDLG frame. The host checked that recovered
generation was >= the largest pre-cut host-observed ACK and emitted
`HOST|CHECK_SECOND_BOOT=True|LAST_ACKED=5|RECOVERED=29|PASS=True`.
Target relay was left **OFF**.

The physical interruption is genuinely from removal of MCU supply,
but relay/host latency prevents claiming the command landed inside a
particular `nvs_commit()` instruction. It validates the bounded
latest-generation NVS recovery contract against ONE asynchronous
cut during a real commit campaign, rather than every flash-writer
interruption phase. Raw UART/flash/backups remain private on RPI400:
`/srv/espressio/scratch/gpt6-durability-20261023-1205/physical-nvs-cut-20261023-first`
and durable run
`/srv/espressio/runs/20261023T122318-b1b4e020/`.
No backup or NVS data is stored in GitHub.

The fixture intentionally does not autoformat, repair, or erase NVS
after a failed or uncertain read. This is **not yet** qualification of
a bounded multi-record historical journal, flash wear budget, or
production Logging release.
