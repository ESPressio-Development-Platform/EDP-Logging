# PowerLossEdlsJournal

[Public header](../src/logging/PowerLossEdlsJournal.hpp) adapts the established PowerLossEdlsSnapshots and EDP-Persistence KeyValueStorage provider into N+1 atomic key slots for N individually replayable, still-retained generations. With 2..8 existing physical slots, 1..7 complete EDLG frames (each within 512-byte EDLS key) are retained. Append never overwrites a protected key. Replay validates record CRC, identity, generation, slot order and retention bounds. Failed writes may damage only sacrificial space; recovery of protected generations remains read-only, while further mutation fails closed. The actual production capacity and flash-wear strategy remain open. See [normative architecture](../docs/POWERLOSS-EDLS-JOURNAL.MD) and [ESP32 fixture](../tests/hil/g7-nvs-journal-espidf/README.MD).

Physical RPI400 ESP32 GPIO17 hard-power-cut journal HIL
20261023T125615-eded7957 PASS: ESP_RST_POWERON, both retained
generations 18 and 19 independently CRC-verified after power
restoration, with bounded two-record history. See normative
docs/POWERLOSS-EDLS-JOURNAL.MD for precise limitations.
