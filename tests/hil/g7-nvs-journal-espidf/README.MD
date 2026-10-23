# ESP32 NVS historical EDLS journal hard-power qualification

This dedicated ESP-IDF application composes the established
PowerLossEdlsSnapshots owner through PowerLossEdlsJournal<3,256,64>
and the REAL EDP-Persistence-ESP-IDF NvsKeyValueStorage provider.
Three unique independent NVS keys in namespace edpjhist26 preserve
the TWO latest complete EDLS/EDLG generations during a pending append.
A separate phase marker lives in edpjphase26. No other NVS namespaces
are mutated by the application. No automatic NVS erase, format or repair.

The user explicitly qualified the RPI400 GPIO17 relay-controlled ESP32
3.3V supply as genuine power removal without parasitic back-power.
Only the policy-approved esp32-integrated controller is used. The
runner preserves a private 2-MiB original device flash backup, builds
and independently verifies native PIOArduino/ESP-IDF firmware, holds
reset until UART capture, then switches power OFF during an active
NVS append campaign, waits, and restores power. The application
requires ESP_RST_POWERON on reboot, scans all three slots, confirms
a Clean two-record retained window, replays BOTH generation-specific
checksummed EDLG payloads, and refuses invalid/archive-corrupt data.

The host monitor tracks the latest pre-cut visible ACK, requires the
newest recovered generation to be at least that ACK, requires distinct
physical second boot and the terminal success marker, and always
powers the ESP32 OFF on completion. Because host, UART and relay have
finite latency, it does NOT prove the flash cut landed inside any
particular nvs_commit instruction.

The scope is deliberately bounded: 3 physical keys, 2 logically
retained individual records, each <=512 bytes including EDLS header.
An older record is retired at completed native commitment of a
successor even if the host does not receive that ACK before cut.
80 synthetic commits is a testing-only workload, NOT a recommended
production flash wear policy. This does NOT qualify a large archive,
long-term endurance, all NVS cut phases or product G1-G15 readiness.

To run, follow Policy/main/V2 exclusive RPI400 lock requirements.
With the pinned EDP_LOGGING_DEPENDENCY_ROOT, use a unique private
EDP_LOGGING_EVIDENCE_DIR and the actual lock-owner agent ID in
EDP_LOGGING_AGENT_ID; invoke run_relay_cut_test.sh through the
durable detached RPI400 runner. Preserve run-id receipts and SHA256
of the private complete backup. Never commit firmware binaries,
flash contents or raw UART capture to GitHub.


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
