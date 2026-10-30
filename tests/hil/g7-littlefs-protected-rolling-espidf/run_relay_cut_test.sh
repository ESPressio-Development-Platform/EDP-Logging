#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_LITTLEFS_AGENT_ID:?Exclusive RPI400 lock-owner ID required}"
EVIDENCE="${EDP_LITTLEFS_EVIDENCE_DIR:?Unique private absolute evidence path required}"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
PROJECT="$ROOT/tests/hil/g7-littlefs-protected-rolling-espidf"
BUILD_ENV="${EDP_LFS_BUILD_ENV:-esp32dev}"
STAGE="${EDP_LFS_CUT_STAGE:-reclaim}"
case "$STAGE:$BUILD_ENV" in
    lowspace:esp32dev-lowspace|lowspace_reclaim:esp32dev-lowspace-reclaim|write:esp32dev|reclaim:esp32dev|typed:esp32dev-typed|midwrite:esp32dev-midwrite|strictack:esp32dev-strictack|typed_ack_gap:esp32dev-typed-strict|typed_ack_gap:esp32dev-sealed-batch-cut|typed_ack_gap:esp32dev-sealed-batch-64-cut) ;;
    *) echo "Invalid stage/build pairing: $STAGE:$BUILD_ENV" >&2; exit 2 ;;
esac
IMAGE="$PROJECT/.pio/build/$BUILD_ENV/firmware.factory.bin"
FS_IMAGE="$PROJECT/.pio/littlefs-seeded.img"
# The prior native first boot failed with a 3584-byte main-task overflow.
# Never flash this fixture if its actual ESP-IDF config is not the tested
# explicit application-edge stack budget, independently revalidated below.
if ! grep -qx 'CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192' "$PROJECT/sdkconfig.$BUILD_ENV"; then
    echo 'EDLS Native HIL refused: expected explicit 8192-byte main task stack' >&2
    exit 3
fi
CONTROL=/srv/espressio/bin/esp32-integrated
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test -s "$IMAGE"
test -s "$FS_IMAGE"
test "$(stat -c %s "$FS_IMAGE")" -eq 589824
test ! -e "$EVIDENCE"
mkdir -p "$EVIDENCE"
sha256sum "$IMAGE" "$FS_IMAGE" > "$EVIDENCE/test-images.sha256"
"$CONTROL" status
/srv/espressio/bin/rpi400-status flash 'LittleFS bounded' 'full chip backup then explicit test provisioning'
"$CONTROL" esptool read-flash 0x0 0x200000 "$EVIDENCE/backup-2mb.bin" > "$EVIDENCE/backup.log" 2>&1
test "$(stat -c %s "$EVIDENCE/backup-2mb.bin")" -eq 2097152
sha256sum "$EVIDENCE/backup-2mb.bin" > "$EVIDENCE/backup.sha256"
echo FULL_CHIP_BACKUP=PASS
"$CONTROL" esptool write-flash 0x0 "$IMAGE" > "$EVIDENCE/flash-firmware.log" 2>&1
"$CONTROL" esptool verify-flash 0x0 "$IMAGE" > "$EVIDENCE/verify-firmware.log" 2>&1
echo FIRMWARE_VERIFIED=PASS
# EXPLICIT TEST PROVISIONING: only this dedicated LittleFS partition;
# full-device backup already completed. Production mount NEVER autoformats.
"$CONTROL" esptool write-flash 0x170000 "$FS_IMAGE" > "$EVIDENCE/flash-littlefs.log" 2>&1
"$CONTROL" esptool verify-flash 0x170000 "$FS_IMAGE" > "$EVIDENCE/verify-littlefs.log" 2>&1
echo DEDICATED_LITTLEFS_IMAGE_VERIFIED=PASS
"$CONTROL" hold-reset
/srv/espressio/bin/rpi400-status serial 'LittleFS bounded' 'GPIO17 hard power recovery'
set +e
python3 -u "$PROJECT/relay_cut_monitor.py" "$EVIDENCE/monitor"
rc=$?
set -e
"$CONTROL" status
echo "LITTLEFS_MONITOR_EXIT=$rc"
test "$rc" -eq 0
if test "$STAGE" = typed_ack_gap; then
    grep -Fq 'EDP_BND|TYPED_ACK_GAP|gen=3|nvs_ack=2|candidate_complete=1' "$EVIDENCE/monitor/serial.txt" || exit 9
    grep -Fq 'EDP_EDLS|TYPED_STRICT_ACK_REPLAY_VERIFIED|all_15_fields=1|ack=4|window=3,4' "$EVIDENCE/monitor/serial.txt" || exit 9
fi
if test "$STAGE" = strictack; then
    grep -Fq 'EDP_EDLS|STRICT_NVS_RECOVERED|ack=4|window=3,4' "$EVIDENCE/monitor/serial.txt" || exit 9
    grep -Fq 'EDP_BND|STRICT_CANDIDATE_START|attempt=1' "$EVIDENCE/monitor/serial.txt" || exit 9
fi
if test "$STAGE" = midwrite; then
    grep -Fq 'EDP_BND|REPLAY_VERIFIED|TWO_PROTECTED_AFTER_MIDWRITE_CUT' "$EVIDENCE/monitor/serial.txt" || exit 9
    grep -Fq 'EDP_BND|MIDWRITE_START|attempt=1' "$EVIDENCE/monitor/serial.txt" || exit 9
fi
if test "$STAGE" = typed; then
    grep -Fq 'EDP_EDLS|TYPED_REPLAY_VERIFIED|all_15_fields=1|window=3,4'         "$EVIDENCE/monitor/serial.txt" || {
        echo 'Typed 15-field postcut archive verification missing' >&2
        exit 9
    }
    grep -Fq 'EDP_EDLS|TYPED_SUCCESSOR_PENDING|gen=3'         "$EVIDENCE/monitor/serial.txt" || {
        echo 'Typed full source pre-cut successor missing' >&2
        exit 9
    }
fi
echo LITTLEFS_BOUNDED_PHYSICAL_EVALUATION=PASS
