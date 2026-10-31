#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_PROFILE_AGENT_ID:?RPI400 lock owner required}"
EVIDENCE="${EDP_PROFILE_EVIDENCE_DIR:?existing unique worker evidence required}"
PROJECT="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$PROJECT/../../.." && pwd)"
AUDIT_POWER_CUT="${EDP_WORKER_AUDIT_POWER_CUT:-0}"
case "$AUDIT_POWER_CUT" in
    0|1|capacity) ;;
    *) echo "Unsupported worker raw audit mode" >&2; exit 2 ;;
esac
AUDIT_ARGS=()
if test "$AUDIT_POWER_CUT" = 1; then AUDIT_ARGS+=(--power-cut); fi
if test "$AUDIT_POWER_CUT" = capacity; then AUDIT_ARGS+=(--capacity-cut); fi
CONTROL=/srv/espressio/bin/esp32-integrated
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
test ! -e "$EVIDENCE/worker-littlefs.img"
test ! -e "$EVIDENCE/worker-nvs.img"
test ! -e "$EVIDENCE/extracted"
ACK="$(PYTHONPATH="$ROOT/tools" python3 - "$PROJECT" "$EVIDENCE/monitor/serial.txt" "$AUDIT_POWER_CUT" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from audit_batch_worker_media import receipt
ack,_=receipt(Path(sys.argv[2]).read_text("utf-8"),sys.argv[3]=="1",sys.argv[3]=="capacity")
print(ack)
PY
)"
"$CONTROL" esptool read-flash 0x170000 0x90000 "$EVIDENCE/worker-littlefs.img" >"$EVIDENCE/read-littlefs.log" 2>&1
"$CONTROL" esptool read-flash 0x9000 0x6000 "$EVIDENCE/worker-nvs.img" >"$EVIDENCE/read-nvs.log" 2>&1
test "$(stat -c %s "$EVIDENCE/worker-littlefs.img")" -eq 589824
test "$(stat -c %s "$EVIDENCE/worker-nvs.img")" -eq 24576
sha256sum "$EVIDENCE/worker-littlefs.img" "$EVIDENCE/worker-nvs.img" | tee "$EVIDENCE/worker-media.sha256"
/srv/espressio/cache/platformio/penv/bin/littlefs-python extract --block-size 4096 \
    "$EVIDENCE/worker-littlefs.img" "$EVIDENCE/extracted" >"$EVIDENCE/extract.log" 2>&1
PYTHONPATH="$ROOT/tools" python3 "$PROJECT/audit_batch_worker_media.py" \
    "$EVIDENCE/extracted" "$EVIDENCE/monitor/serial.txt" "${AUDIT_ARGS[@]}"
PYTHONPATH="$ROOT/tools" python3 "$PROJECT/audit_strict_nvs_watermark.py" \
    "$EVIDENCE/worker-nvs.img" --namespace batchack --ack "$ACK" --incarnation 7 \
    --nvs-parser-dir /srv/espressio/cache/platformio/packages/framework-espidf/components/nvs_flash/nvs_partition_tool
echo BATCH_WORKER_PHYSICAL_RAW_AUDIT=PASS
