# Physical ESP32 checkpoint and native Serial qualification

Status: three separate real ESP32-D0WD runs passed on 2026-10-16;
only a partial G7/G12 test-specific binding, NOT the final production sink.

Bootstrap selects existing EDP-Persistence-ESP-IDF NVS provider, bound
through EDP-Persistence KeyValueStorage and the EDP-Memory ByteOperations
abstraction, and EDP-Serial-ESP-IDF native UART FIFO writer under the
existing EDP-Serial bounded frame queue with EDP-Threading arbitration.
No Logging-to-Serial or Logging-to-Mesh link.

NVS first boot writes an exact EDLG frame and commits with NVS.
Second boot after actual esp_restart independently reads through the
generic Logging KeyValue checkpoint bridge, validates CRC and framing,
deletes the test namespace key and confirms ordinary Logging Memory
teardown. Then EDP-Serial transmits an actual raw 24-byte binary
EDLG frame to /dev/serial0 (not a printed hex frame). Host confirms
CRC, codec and payload using existing canonical decoder.

Each of three independent GPIO reset cycles: 11 of 11 app and host
milestones passed, zero FAIL, verified EDLG binary frame TRUE.
Full original flash (4,194,304 bytes) was privately saved before
flashing and restored with independent whole-chip digest matching
BEFORE original normal reboot, then lock released per Policy handoff.

The output and persisted input are TEST-ONLY tuple data. This is
restart recovery, NOT interrupted external power-loss recovery, nor a
rolling EDLS archive, published governed LogRecord schema, or source
authenticity proof. Exact current evidence:
tests/hil/g1-espidf/VALIDATION.MD and docs/EDLG-KEYVALUE-CHECKPOINT.MD.
