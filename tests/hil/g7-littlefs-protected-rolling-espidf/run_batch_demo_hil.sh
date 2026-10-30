#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_PROFILE_AGENT_ID:?held RPI400 owner required}"
EVIDENCE="${EDP_PROFILE_EVIDENCE_DIR:?unique private evidence directory required}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
DEMO_KIND="${2:-container}"
case "$DEMO_KIND" in
    container) DEMO_NAME=ConfigurableBoundedEdlsBatch; PROFILE_KIND=configurable-batch-demo ;;
    typed-managed) DEMO_NAME=TypedManagedBatchWorker; PROFILE_KIND=typed-managed-batch-demo ;;
    *) echo "Unsupported demo kind: $DEMO_KIND" >&2; exit 2 ;;
esac
DEMO="$ROOT/demos/$DEMO_NAME"
TARGET="${1:?espidf, platformio-arduino or arduino-ide}"
case "$TARGET" in
    espidf) IMAGE="$DEMO/PlatformIO_ESP-IDF/.pio/build/esp32dev/firmware.factory.bin" ;;
    platformio-arduino) IMAGE="$DEMO/PlatformIO_Arduino/.pio/build/esp32dev/firmware.factory.bin" ;;
    arduino-ide) IMAGE="$DEMO/Arduino_IDE/.build/Arduino_IDE.ino.merged.bin" ;;
    *) echo "Unsupported target: $TARGET" >&2; exit 2 ;;
esac
BACKUP_POLICY="${EDP_PROFILE_BACKUP_POLICY:-full}"
case "$BACKUP_POLICY" in full|disposable) ;; *) echo "Invalid backup policy" >&2; exit 2 ;; esac
CONTROL=/srv/espressio/bin/esp32-integrated
ESPTOOL=/srv/espressio/cache/platformio/penv/bin/esptool
SEED="$HERE/.pio/littlefs-seeded.img"
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test ! -e "$EVIDENCE"
test -s "$IMAGE"
test "$(stat -c %s "$SEED")" -eq 589824
mkdir -p "$EVIDENCE"
sha256sum "$IMAGE" "$SEED" >"$EVIDENCE/test-images.sha256"

# Reuse the appliance's public with-power command without its esptool command's
# automatic final reset. Remain in ROM/stub throughout independent verification:
# application startup must not modify the NVS/filesystem bytes being checked.
# GPIO17 and reset pins remain exclusively owned by the appliance controller.
"$CONTROL" bootloader
flash_held() {
    "$CONTROL" with-power -- "$ESPTOOL" --chip esp32 --port /dev/serial0 \
        --before no-reset --after no-reset "$@"
}
if test "$BACKUP_POLICY" = full; then
    flash_held read-flash 0x0 0x200000 "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.log" 2>&1
    test "$(stat -c %s "$EVIDENCE/backup-2mb.bin")" -eq 2097152
    sha256sum "$EVIDENCE/backup-2mb.bin" >"$EVIDENCE/backup.sha256"
    echo FULL_CHIP_BACKUP=PASS
else
    # Explicit opt-in only when the user has authorised disposable firmware.
    echo FULL_CHIP_BACKUP=WAIVED_DISPOSABLE_TARGET
fi
flash_held write-flash 0x0 "$IMAGE" >"$EVIDENCE/flash-firmware.log" 2>&1
flash_held verify-flash 0x0 "$IMAGE" >"$EVIDENCE/verify-firmware.log" 2>&1
flash_held write-flash 0x170000 "$SEED" >"$EVIDENCE/flash-littlefs.log" 2>&1
flash_held verify-flash 0x170000 "$SEED" >"$EVIDENCE/verify-littlefs.log" 2>&1
echo INDEPENDENT_IMAGE_AND_FILESYSTEM_VERIFICATION_BEFORE_BOOT=PASS
for boot in 1 2; do
    "$CONTROL" hold-reset
    EDP_PROFILE_KIND="$PROFILE_KIND" python3 -u "$HERE/monitor_edls_provider_profile.py" "$EVIDENCE/boot$boot"
done
python3 - "$EVIDENCE" "$DEMO_KIND" <<'PY'
from pathlib import Path
import re
import sys
root = Path(sys.argv[1])
receipts = []
for boot in (1, 2):
    text = (root / f"boot{boot}/serial.txt").read_text()
    if sys.argv[2] == "typed-managed":
        if text.count("EDP_TYPED_WORKER_DEMO|RAM|published=9|durable=0") != 1 or \
                text.count("EDP_TYPED_WORKER_DEMO|RAM_RETIRED|retired=9|durable=8") != 1:
            raise SystemExit("DEMO_CROSS_BOOT=FAIL missing distinct volatile/retired evidence")
        matches = re.findall(
            r"^EDP_TYPED_WORKER_DEMO\|PASS\|incarnation=([1-9][0-9]*)\|records=9"
            r"\|durable=9\|retired=9\|ack_segment=([1-9][0-9]*)"
            r"\|retained_records=5\|dwell_ns=([0-9]+)$", text, re.M)
        matches = [(incarnation, dwell, ack) for incarnation, ack, dwell in matches]
    else:
        matches = re.findall(
            r"^EDP_BATCH_DEMO\|PASS\|incarnation=([1-9][0-9]*)\|dwell_ns=([0-9]+)"
            r"\|full_records=4\|idle_records=1\|ack_segment=([1-9][0-9]*)$", text, re.M)
    if len(matches) != 1:
        raise SystemExit("DEMO_CROSS_BOOT=FAIL invalid receipt count")
    receipts.append(tuple(map(int, matches[0])))
first, second = receipts
expected_first_ack = 3 if sys.argv[2] == "typed-managed" else 2
if first[0] != 1 or second[0] != 2 or first[2] != expected_first_ack or second[2] != 2 * expected_first_ack or first[1] != second[1]:
    raise SystemExit(f"DEMO_CROSS_BOOT=FAIL {receipts}")
print(f"DEMO_CROSS_BOOT=PASS incarnation=1,2 ack={first[2]},{second[2]} dwell_ns={first[1]}")
PY
echo BATCH_DEMO_PHYSICAL_RUNTIME=PASS
