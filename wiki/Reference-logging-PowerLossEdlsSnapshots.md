# PowerLossEdlsSnapshots — bounded latest-generation profile

**Source:** [PowerLossEdlsSnapshots.hpp](../src/logging/PowerLossEdlsSnapshots.hpp)

This optional Logging adapter uses existing EDP-Persistence
`KeyValueStorage` keys with mandatory compile-time `PowerLoss`
retention **and** interruption atomicity. The ESP-IDF NVS provider has
both properties but limits each complete value to 512 bytes.

The adapter maintains two to eight CRC-checked EDLS generations,
avoids overwriting the newest, and only acknowledges writes after a
successful provider commit and byte-for-byte readback. It explicitly
retains only the **newest acknowledged generation**, not all historical
records. A corrupted inactive key degrades recovery and blocks further
writes; it does not silently trigger destructive repair.

This is the first small-record durability profile, **not** a drop-in
replacement for rolling multi-record SPIFFS archives. The complete
guarantees and HIL limits are in
[POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD](../docs/POWERLOSS-EDLS-SNAPSHOT-PROFILE.MD).


## Real ESP32 verification

The native ESP-IDF NVS-backed two-key profile passed a genuine
GPIO17-controlled supply interruption during repeated atomic-key
commits. After reboot with `ESP_RST_POWERON`, recovered generation
29 was CRC clean and at least as new as the host-visible last
acknowledgement 5. Durable run `20261023T122318-b1b4e020`
PASS exit0. See the HIL README for phase boundaries, private evidence,
and strict caveats about the single cut and 512-byte/latest-only
limits.
