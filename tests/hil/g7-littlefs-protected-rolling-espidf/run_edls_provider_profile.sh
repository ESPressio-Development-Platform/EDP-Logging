#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_PROFILE_AGENT_ID:?RPI400 lock owner required}"
EVIDENCE="${EDP_PROFILE_EVIDENCE_DIR:?unique private evidence path required}"
PROJECT="$(cd "$(dirname "$0")" && pwd)"
BUILD_ENV="${EDP_PROFILE_BUILD_ENV:-esp32dev-typed-strict-profile}"
EDP_PROFILE_WIDE_QUEUE=0
case "$BUILD_ENV" in
    esp32dev-typed-strict-profile) export EDP_PROFILE_KIND=single-record ;;
    esp32dev-sealed-batch) export EDP_PROFILE_KIND=sealed-batch ;;
    esp32dev-sealed-batch-64) export EDP_PROFILE_KIND=sealed-batch-64 ;;
    esp32dev-batch-worker) export EDP_PROFILE_KIND=batch-worker ;;
    esp32dev-batch-worker-500) export EDP_PROFILE_KIND=batch-worker-500 ;;
    esp32dev-batch-worker-500-o2) export EDP_PROFILE_KIND=batch-worker-500-o2 ;;
    esp32dev-batch-worker-500-o2-6000) export EDP_PROFILE_KIND=batch-worker-500-o2-6000 ;;
    esp32dev-batch-worker-500-o2-30000) export EDP_PROFILE_KIND=batch-worker-500-o2-30000 ;;
    esp32dev-batch-worker-500-o2-30000-wide) export EDP_PROFILE_KIND=batch-worker-500-o2-30000 EDP_PROFILE_WIDE_QUEUE=1 ;;
    esp32dev-batch-worker-500-o2-30000-dual) export EDP_PROFILE_KIND=batch-worker-500-o2-30000 EDP_PROFILE_WIDE_QUEUE=2 ;;
    esp32dev-batch-worker-500-o2-30000-80) export EDP_PROFILE_KIND=batch-worker-500-o2-30000-80 EDP_PROFILE_WIDE_QUEUE=1 ;;
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
if test "$EDP_PROFILE_KIND" = batch-worker-500-o2 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-6000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then
    grep -Fq '"status": "Verified"' "$PROJECT/.pio/build/$BUILD_ENV/verified-compile-profile.json"
fi
mkdir -p "$EVIDENCE"
sha256sum "$IMAGE" "$SEED" >"$EVIDENCE/test-images.sha256"
"$CONTROL" status
"$CONTROL" bootloader
flash_held() {
    "$CONTROL" with-power -- /srv/espressio/cache/platformio/penv/bin/esptool \
        --chip esp32 --port /dev/serial0 --before no-reset --after no-reset "$@"
}
if test "$BACKUP_POLICY" = full; then
    flash_held read-flash 0x0 0x200000 "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.log" 2>&1
    test "$(stat -c %s "$EVIDENCE/backup-2mb.bin")" -eq 2097152
    sha256sum "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.sha256"
    echo FULL_CHIP_BACKUP=PASS
else
    # Opt-in only when the operator has authorized disposable target firmware.
    echo FULL_CHIP_BACKUP=WAIVED_DISPOSABLE_TARGET
fi
flash_held write-flash 0x0 "$IMAGE" >"$EVIDENCE/flash-firmware.log" 2>&1
flash_held verify-flash 0x0 "$IMAGE" >"$EVIDENCE/verify-firmware.log" 2>&1
echo FIRMWARE_VERIFIED=PASS
flash_held write-flash 0x170000 "$SEED" >"$EVIDENCE/flash-littlefs.log" 2>&1
flash_held verify-flash 0x170000 "$SEED" >"$EVIDENCE/verify-littlefs.log" 2>&1
echo SEEDED_LITTLEFS_VERIFIED=PASS
"$CONTROL" hold-reset
python3 -u "$PROJECT/monitor_edls_provider_profile.py" "$EVIDENCE/monitor"
"$CONTROL" status
if test "$EDP_PROFILE_KIND" = batch-worker || test "$EDP_PROFILE_KIND" = batch-worker-500 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-6000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then
    EXPECTED_SCHEDULED_RECORDS=2000
    if test "$EDP_PROFILE_KIND" = batch-worker-500-o2-6000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then EXPECTED_SCHEDULED_RECORDS=6000; fi
    if test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then EXPECTED_SCHEDULED_RECORDS=30000; fi
    EXPECTED_RPS=100
    if test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then EXPECTED_RPS=80; fi
    grep -Fq "EDP_WORKER|PASS|scheduled_records=$EXPECTED_SCHEDULED_RECORDS|producers=2|target_rps=$EXPECTED_RPS|" "$EVIDENCE/monitor/serial.txt"
    case "$EDP_PROFILE_WIDE_QUEUE" in
        1) grep -Fq 'EDP_WORKER|PRIME|attempts=128|published=128|deadline_refused=0|ack_records=0|queue_slots=252' "$EVIDENCE/monitor/serial.txt" ;;
        2) grep -Fq 'EDP_WORKER|PRIME|attempts=128|published=128|deadline_refused=0|ack_records=0|queue_slots=496' "$EVIDENCE/monitor/serial.txt" ;;
        0) grep -Fq 'EDP_WORKER|PRESSURE|attempts=160|published=128|deadline_refused=32|ack_records=0' "$EVIDENCE/monitor/serial.txt" ;;
    esac
    if test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then
        EXPECTED_SCHEDULED_RECORDS=30000
        grep -Fq "EDP_WORKER|PASS|scheduled_records=30000|producers=2|target_rps=$EXPECTED_RPS|" "$EVIDENCE/monitor/serial.txt"
        grep -Fq 'EDP_WORKER|ALL_SCHEDULED_LATENCY|records=30000|' "$EVIDENCE/monitor/serial.txt"
    fi
    EXPECTED_DWELL_NS=100000000
    if test "$EDP_PROFILE_KIND" = batch-worker-500 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-6000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000 || test "$EDP_PROFILE_KIND" = batch-worker-500-o2-30000-80; then EXPECTED_DWELL_NS=500000000; fi
    grep -Fq "EDP_WORKER|IDLE|partial_records=1|subsequent_input=0|dwell_ns=$EXPECTED_DWELL_NS|" "$EVIDENCE/monitor/serial.txt"
    grep -Fq "|scheduled_dwell_ns=$EXPECTED_DWELL_NS|" "$EVIDENCE/monitor/serial.txt"
elif test "$EDP_PROFILE_KIND" = sealed-batch-64; then
    grep -Fq 'EDP_BATCH|PASS|records=256|ack_segment=4|window=3,4|first=129|last=256|' "$EVIDENCE/monitor/serial.txt"
elif test "$EDP_PROFILE_KIND" = sealed-batch; then
    grep -Fq 'EDP_BATCH|PASS|records=256|ack_segment=16|window=15,16|first=225|last=256|' "$EVIDENCE/monitor/serial.txt"
else
    grep -Fq 'EDP_PROFILE|PASS|count=256|ack=256|window=255,256' "$EVIDENCE/monitor/serial.txt"
fi
echo PROFILE_PHYSICAL_RUNTIME=PASS
