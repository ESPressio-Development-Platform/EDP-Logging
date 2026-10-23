#!/usr/bin/env bash
set -Eeuo pipefail

LOGGING_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PROJECT="$LOGGING_ROOT/tests/hil/g7-torn-slot-espidf"
IMAGE="$PROJECT/.pio/build/esp32dev/firmware.factory.bin"
OWNER="${EDP_LOGGING_AGENT_ID:?Set the current appliance lock owner ID}"
CONTROL="/srv/espressio/bin/esp32-integrated"
RUN_DIR="${EDP_LOGGING_EVIDENCE_DIR:?Set a unique absolute HIL evidence path}"
# Preserve a guaranteed fail-safe relay power-off on all script exits.
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT

# Enforce exclusive appliance ownership and no write outside dedicated HIL.
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test -s "$IMAGE"
test ! -e "$RUN_DIR"
mkdir -p "$RUN_DIR"
stat -c 'FIRMWARE_IMAGE_BYTES=%s' "$IMAGE"
sha256sum "$IMAGE" > "$RUN_DIR/firmware.sha256"

"$CONTROL" status
/srv/espressio/bin/rpi400-status flash 'G7 torn EDLS slot' 'flash+independent verify'
"$CONTROL" esptool write-flash 0x0 "$IMAGE" > "$RUN_DIR/flash.log" 2>&1 || {
  tail -50 "$RUN_DIR/flash.log"
  exit 21
}
"$CONTROL" esptool verify-flash 0x0 "$IMAGE" > "$RUN_DIR/verify.log" 2>&1 || {
  tail -50 "$RUN_DIR/verify.log"
  exit 22
}
echo G7_RELAY_FLASH_AND_VERIFY=PASS

# Wrapper resets after esptool; hold EN until monitor opens UART.
"$CONTROL" hold-reset
/srv/espressio/bin/rpi400-status serial 'G7 torn EDLS slot' 'torn EDLS phase crash'
set +e
python3 -u "$PROJECT/relay_cut_monitor.py" "$RUN_DIR/monitor"
status=$?
set -e
echo "G7_RELAY_MONITOR_EXIT=$status"
"$CONTROL" status
if [[ $status -ne 0 ]]; then
  echo G7_RELAY_TEST=FAIL
  exit "$status"
fi
echo G7_TORN_SLOT_POWER_RECOVERY=PASS
echo 'G7_POWERLOSS_ATOMICITY_QUALIFIED=NO'
