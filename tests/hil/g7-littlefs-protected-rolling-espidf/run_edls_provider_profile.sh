#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_PROFILE_AGENT_ID:?RPI400 lock owner required}"
EVIDENCE="${EDP_PROFILE_EVIDENCE_DIR:?unique private evidence path required}"
PROJECT="$(cd "$(dirname "$0")" && pwd)"
BUILD_ENV="${EDP_PROFILE_BUILD_ENV:-esp32dev-typed-strict-profile}"
case "$BUILD_ENV" in
    esp32dev-typed-strict-profile) export EDP_PROFILE_KIND=single-record ;;
    esp32dev-sealed-batch) export EDP_PROFILE_KIND=sealed-batch ;;
    esp32dev-sealed-batch-64) export EDP_PROFILE_KIND=sealed-batch-64 ;;
    esp32dev-batch-worker) export EDP_PROFILE_KIND=batch-worker ;;
    *) echo "Unsupported profile build: $BUILD_ENV" >&2; exit 2 ;;
esac
IMAGE="$PROJECT/.pio/build/$BUILD_ENV/firmware.factory.bin"
SEED="$PROJECT/.pio/littlefs-seeded.img"
BACKUP_POLICY="${EDP_PROFILE_BACKUP_POLICY:-full}"
case "$BACKUP_POLICY" in
    full|disposable) ;;
    *) echo "Unsupported backup policy: $BACKUP_POLICY" >&2; exit 2 ;;
esac
CONTROL=/srv/espressio/bin/esp32-integrated
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test ! -e "$EVIDENCE"
test -s "$IMAGE"
test "$(stat -c %s "$SEED")" -eq 589824
grep -qx CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192 "$PROJECT/sdkconfig.$BUILD_ENV"
mkdir -p "$EVIDENCE"
sha256sum "$IMAGE" "$SEED" >"$EVIDENCE/test-images.sha256"
"$CONTROL" status
if test "$BACKUP_POLICY" = full; then
    "$CONTROL" esptool read-flash 0x0 0x200000 "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.log" 2>&1
    test "$(stat -c %s "$EVIDENCE/backup-2mb.bin")" -eq 2097152
    sha256sum "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.sha256"
    echo FULL_CHIP_BACKUP=PASS
else
    # Opt-in only when the operator has authorized disposable target firmware.
    echo FULL_CHIP_BACKUP=WAIVED_DISPOSABLE_TARGET
fi
"$CONTROL" esptool write-flash 0x0 "$IMAGE" >"$EVIDENCE/flash-firmware.log" 2>&1
"$CONTROL" esptool verify-flash 0x0 "$IMAGE" >"$EVIDENCE/verify-firmware.log" 2>&1
echo FIRMWARE_VERIFIED=PASS
"$CONTROL" esptool write-flash 0x170000 "$SEED" >"$EVIDENCE/flash-littlefs.log" 2>&1
"$CONTROL" esptool verify-flash 0x170000 "$SEED" >"$EVIDENCE/verify-littlefs.log" 2>&1
echo SEEDED_LITTLEFS_VERIFIED=PASS
"$CONTROL" hold-reset
python3 -u "$PROJECT/monitor_edls_provider_profile.py" "$EVIDENCE/monitor"
"$CONTROL" status
if test "$EDP_PROFILE_KIND" = batch-worker; then
    grep -Fq 'EDP_WORKER|PASS|scheduled_records=2000|producers=2|target_rps=100|' "$EVIDENCE/monitor/serial.txt"
    grep -Fq 'EDP_WORKER|PRESSURE|attempts=160|published=128|deadline_refused=32|ack_records=0' "$EVIDENCE/monitor/serial.txt"
    grep -Fq 'EDP_WORKER|IDLE|partial_records=1|subsequent_input=0|dwell_ns=100000000|' "$EVIDENCE/monitor/serial.txt"
elif test "$EDP_PROFILE_KIND" = sealed-batch-64; then
    grep -Fq 'EDP_BATCH|PASS|records=256|ack_segment=4|window=3,4|first=129|last=256|' "$EVIDENCE/monitor/serial.txt"
elif test "$EDP_PROFILE_KIND" = sealed-batch; then
    grep -Fq 'EDP_BATCH|PASS|records=256|ack_segment=16|window=15,16|first=225|last=256|' "$EVIDENCE/monitor/serial.txt"
else
    grep -Fq 'EDP_PROFILE|PASS|count=256|ack=256|window=255,256' "$EVIDENCE/monitor/serial.txt"
fi
echo PROFILE_PHYSICAL_RUNTIME=PASS
