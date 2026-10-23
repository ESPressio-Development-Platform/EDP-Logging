#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PROJECT="$ROOT/tests/hil/g7-nvs-journal-espidf"
IMAGE="$PROJECT/.pio/build/esp32dev/firmware.factory.bin"
OWNER="${EDP_LOGGING_AGENT_ID:?Current RPI400 exclusive lock owner is required}"
OUTPUT="${EDP_LOGGING_EVIDENCE_DIR:?Use a unique private absolute evidence directory}"
CONTROL="/srv/espressio/bin/esp32-integrated"
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
 /srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test -s "$IMAGE"
test ! -e "$OUTPUT"
mkdir -p "$OUTPUT"
echo "SOURCE_HEAD=$(git -C "$ROOT" rev-parse HEAD)"
echo "FIRMWARE_IMAGE_BYTES=$(stat -c %s "$IMAGE")"
sha256sum "$IMAGE" >"$OUTPUT/firmware.sha256"
"$CONTROL" status
/srv/espressio/bin/rpi400-status flash 'Logging NVS PowerLoss' 'backup and verify'
# Private 2-MiB pretest snapshot; never commit/upload its bytes.
"$CONTROL" esptool read-flash 0x0 0x200000 "$OUTPUT/pretest-flash-2mb.bin" >"$OUTPUT/backup.log" 2>&1
test "$(stat -c %s "$OUTPUT/pretest-flash-2mb.bin")" -eq 2097152
sha256sum "$OUTPUT/pretest-flash-2mb.bin" >"$OUTPUT/pretest-flash.sha256"
echo PRETEST_FLASH_BACKUP=PASS
"$CONTROL" esptool write-flash 0x0 "$IMAGE" >"$OUTPUT/flash.log" 2>&1
"$CONTROL" esptool verify-flash 0x0 "$IMAGE" >"$OUTPUT/verify.log" 2>&1
echo FIRMWARE_FLASH_VERIFY=PASS
"$CONTROL" hold-reset
/srv/espressio/bin/rpi400-status serial 'Logging NVS PowerLoss' 'cut during commit campaign'
set +e
python3 -u "$PROJECT/relay_cut_monitor.py" "$OUTPUT/monitor"
rc=$?
set -e
echo "NVS_POWERLOSS_MONITOR_EXIT=$rc"
"$CONTROL" status
test "$rc" -eq 0
echo NVS_POWERLOSS_QUALIFICATION=PASS
