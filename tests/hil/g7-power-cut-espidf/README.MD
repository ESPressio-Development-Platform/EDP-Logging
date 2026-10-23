# G7/G12 relay-interruption EDLS retention — integrated ESP32

This deliberately destructive **test-only** firmware runs on the RPI400-integrated ESP32-WROOM-32 with a 2 MiB flash chip, relay-switched 3.3V supply and UART `/dev/serial0`. It does **not** change any EDP-Logging production source or assert a new FileStorage capability.

## Authority and safety

- The user confirmed on 2026-10-23 that the GPIO17/relay wiring electrically isolates the MCU from parasitic back-powering; treat an OFF transition under this fixture as real VDD interruption. No independent voltmeter trace was recorded.
- Follow `Policy/main/V2/GITHUB_ACTIONS.MD`, `Policy/docs/RPI400_INTEGRATED_ESP32_POWER.MD` and current appliance lock/LED/OLED rules. Only `/srv/espressio/bin/esp32-integrated` controls GPIO17. Never drive it with `gpioset`.
- This fixture **changes the firmware and partition table** of the integrated target and can expose SPIFFS filesystem loss. Take a complete flash backup first if pre-existing content matters. Do not use this script against other user hardware.
- The physical flash is 2 MiB, not the inherited HIL's 4 MiB partition model. `partitions.csv` ends exactly at offset `0x200000`, with app `0x10000..0x100000` and SPIFFS `0x100000..0x200000`. Check the binary partition table before flashing.
- SPIFFS may be formatted on the first dedicated test initialisation only when the private NVS `armed` key is absent. On recovery after an armed cut, automatic filesystem formatting and repair are explicitly disabled.
- Two independent, checksum-verified EDLS segments are written as `power0` generation 1 and `power1` generation 2. Their content is fixed and cannot be borrowed from another running source. A separate 96 KiB native FileStorage `ReplaceFile` targets `powerlarge`. The relay is commanded OFF immediately after `EDP_POWERLOSS|REPLACE_ENTER`, held OFF for over one second, then ON.
- The recovery firmware demands `esp_reset_reason() == ESP_RST_POWERON` and inspects the two original EDLS slots without rewriting, formatting or repairing them. Any ambiguous/damaged old segment produces failure.

## Build and run on RPI400

Use clean sibling source pins for EDP-Logging, EDP-Persistence, EDP-Persistence-ESP-IDF, EDP-Serialisation, EDP-Memory, EDP-Platform and other existing source dependencies.

```bash
export EDP_LOGGING_DEPENDENCY_ROOT=/absolute/path/to/sibling/EDP-repos
cd "$EDP_LOGGING_DEPENDENCY_ROOT/EDP-Logging/tests/hil/g7-power-cut-espidf"
pio run -e esp32dev

# Run only after the agent holds the RPI400 appliance lock,
# with unique evidence path and an approved full-device backup.
export EDP_LOGGING_AGENT_ID=<current-lock-agent-id>
export EDP_LOGGING_EVIDENCE_DIR=/absolute/unique/scratch/evidence-directory
bash run_relay_cut_test.sh
```

The monitor opens the UART only after the approved power-on preflight. The helper flashes through the guarded ROM downloader, independently verifies the complete factory image, holds reset until UART capture is ready, and calls the approved power-off/power-on daemon. Both the monitor and the shell runner have unconditional relay OFF cleanup. It requires a second hardware boot, no successful write-return marker, a POWERON reset, and a clean EDLS archive. A single test run is deliberately non-idempotent: the private NVS `armed` flag and archive remain available as evidence.

## Actual 2026-10-23 physical results

- Flash readback of the entire pre-test device **PASS**: `20261023T110541-ebe4137c`, 2,097,152 bytes; SHA-256 `e5e87e86d9096752bf304edc98fbeb5b999aafbe20a49033a22c96b65b466a97`. Retained on RPI400 under the private test scratch directory, **not** in Git.
- Dedicated firmware build `20261023T110644-c27fdd1c` **PASS exit 0** (PioArduino/ESP-IDF 5.5.5, same pinned toolchain as earlier Logging HIL); executable factory image size 280,320 bytes. Binary partition range checks confirmed no file extends above the physical 2 MiB flash.
- Native flash + independent esptool image verification + one relay cut + hardware recovery: **PASS exit 0**, detached run `20261023T110910-d66bc60e`. After fresh archive generation 1 and 2, the device emitted `REPLACE_ENTER` and the relay controller reported `POWER=OFF GPIO17=HIGH`. It remained OFF for 2.37 seconds before re-energisation. No `REPLACE_RETURN` was observed. Reboot reported `RESET_REASON|value=1` (ESP-IDF `ESP_RST_POWERON`). Both prior slots remained `Clean`, generations 1 and 2, and archive condition `Unambiguous`. The interrupted `powerlarge` file was reported as zero bytes. The final `PASS|PRIOR_EDLS_SURVIVED_RELAY_CYCLE` marker was observed and the controller left the MCU powered OFF.
- Full raw UART capture, Relay ON/OFF timestamps, image hashes, flash/write/verify logs and test exit status are retained under `/srv/espressio/scratch/gpt6-logging-power-20261023-1048/g7-relay-powercut-observation-20261023` and `/srv/espressio/runs/20261023T110910-d66bc60e`.

## Interpretation and outstanding gates

This is credible **physical power-loss retention evidence** under the user-qualified wiring, not merely `esp_restart()` or a GPIO EN reset. It establishes that two previously committed EDLS generations survived the tested real power loss during a separate native VFS file operation. It does **not** qualify every possible interruption instant, a torn overwrite of an EDLS slot itself, filesystem atomicity, or the guaranteed `PowerLoss` retention capability of `EDP-Persistence-ESP-IDF::VfsFileStorage`; that provider still advertises `InterruptionAtomicity::None` and `FailurePreservation::MayModify` truthfully. Do not change those property claims on the basis of one empirical pass.

Issue #12 is unblocked **for hardware control**, but the stricter data-integrity and PowerLoss-provider guarantees remain open. Continue with rollover-cut/torn-EDLS-tail recovery, archive fail-closed and repeated timings, then complete remaining Logging G1–G15 gates. Maintain every library at version `0.1.0`.
