#!/usr/bin/env bash
set -Eeuo pipefail
OWNER="${EDP_PROFILE_AGENT_ID:?RPI400 lock owner required}"
EVIDENCE="${EDP_PROFILE_EVIDENCE_DIR:?Existing private physical-run evidence required}"
KIND="${EDP_BATCH_AUDIT_KIND:?profile, power-cut, profile64 or power-cut64 required}"
PROJECT="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$PROJECT/../../.." && pwd)"
case "$KIND" in
    profile)
        OLDER=15; NEWER=16; RECORDS=16
        RECEIPT="EDP_BATCH|PASS|records=256|ack_segment=16|window=15,16|first=225|last=256|"
        ;;
    power-cut)
        OLDER=3; NEWER=4; RECORDS=16
        RECEIPT="EDP_BATCH|REPLAY|ack_segment=4|records_per_segment=16|first=33|last=64"
        ;;
    profile64)
        OLDER=3; NEWER=4; RECORDS=64
        RECEIPT="EDP_BATCH|PASS|records=256|ack_segment=4|window=3,4|first=129|last=256|"
        ;;
    power-cut64)
        OLDER=3; NEWER=4; RECORDS=64
        RECEIPT="EDP_BATCH|REPLAY|ack_segment=4|records_per_segment=64|first=129|last=256"
        ;;
    *) echo "Unsupported batch audit: $KIND" >&2; exit 2 ;;
esac
CONTROL=/srv/espressio/bin/esp32-integrated
trap '"$CONTROL" power-off >/dev/null 2>&1 || true' EXIT
/srv/espressio/bin/espressio-agent-lock owns "$OWNER" >/dev/null
grep -Fq "$RECEIPT" "$EVIDENCE/monitor/serial.txt"
test ! -e "$EVIDENCE/postbatch-littlefs.img"
test ! -e "$EVIDENCE/postbatch-nvs.img"
test ! -e "$EVIDENCE/extracted"
"$CONTROL" esptool read-flash 0x170000 0x90000 "$EVIDENCE/postbatch-littlefs.img" >"$EVIDENCE/read-littlefs.log" 2>&1
"$CONTROL" esptool read-flash 0x9000 0x6000 "$EVIDENCE/postbatch-nvs.img" >"$EVIDENCE/read-nvs.log" 2>&1
test "$(stat -c %s "$EVIDENCE/postbatch-littlefs.img")" -eq 589824
test "$(stat -c %s "$EVIDENCE/postbatch-nvs.img")" -eq 24576
sha256sum "$EVIDENCE/postbatch-littlefs.img" "$EVIDENCE/postbatch-nvs.img" | tee "$EVIDENCE/postbatch-media.sha256"
/srv/espressio/cache/platformio/penv/bin/littlefs-python extract --block-size 4096 \
    "$EVIDENCE/postbatch-littlefs.img" "$EVIDENCE/extracted" >"$EVIDENCE/extract.log" 2>&1
PYTHONPATH="$ROOT/tools" python3 "$PROJECT/audit_typed_extracted_edls.py" \
    "$EVIDENCE/extracted" --older "$OLDER" --newer "$NEWER" --records-per-segment "$RECORDS"
PYTHONPATH="$ROOT/tools" python3 "$PROJECT/audit_strict_nvs_watermark.py" \
    "$EVIDENCE/postbatch-nvs.img" --namespace batchack --ack "$NEWER" --incarnation 7 \
    --nvs-parser-dir /srv/espressio/cache/platformio/packages/framework-espidf/components/nvs_flash/nvs_partition_tool
echo SEALED_BATCH_PHYSICAL_RAW_AUDIT=PASS
