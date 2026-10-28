#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_LOAD_AGENT_ID:?RPI400 lock owner required}"
EVIDENCE="${EDP_LOAD_EVIDENCE_DIR:?unique private evidence path required}"
PROJECT="$(cd "$(dirname "$0")" && pwd)"
IMAGE="$PROJECT/.pio/build/esp32dev-typed-strict-load/firmware.factory.bin"
SEED="$PROJECT/.pio/littlefs-seeded.img"
CONTROL=/srv/espressio/bin/esp32-integrated
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test ! -e "$EVIDENCE"
test -s "$IMAGE"
test "$(stat -c %s "$SEED")" -eq 589824
grep -qx CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192 "$PROJECT/sdkconfig.esp32dev-typed-strict-load"
mkdir -p "$EVIDENCE"
sha256sum "$IMAGE" "$SEED" >"$EVIDENCE/test-images.sha256"
"$CONTROL" status
"$CONTROL" esptool read-flash 0x0 0x200000 "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.log" 2>&1
test "$(stat -c %s "$EVIDENCE/backup-2mb.bin")" -eq 2097152
sha256sum "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.sha256"
echo FULL_CHIP_BACKUP=PASS
"$CONTROL" esptool write-flash 0x0 "$IMAGE" >"$EVIDENCE/flash-firmware.log" 2>&1
"$CONTROL" esptool verify-flash 0x0 "$IMAGE" >"$EVIDENCE/verify-firmware.log" 2>&1
echo FIRMWARE_VERIFIED=PASS
"$CONTROL" esptool write-flash 0x170000 "$SEED" >"$EVIDENCE/flash-littlefs.log" 2>&1
"$CONTROL" esptool verify-flash 0x170000 "$SEED" >"$EVIDENCE/verify-littlefs.log" 2>&1
echo SEEDED_LITTLEFS_VERIFIED=PASS
"$CONTROL" hold-reset
python3 -u "$PROJECT/monitor_typed_load.py" "$EVIDENCE/monitor"
"$CONTROL" status
grep -Fq 'EDP_LOAD|PASS|count=256|ack=256|window=255,256' "$EVIDENCE/monitor/serial.txt"
echo LOAD_PHYSICAL_RUNTIME=PASS
