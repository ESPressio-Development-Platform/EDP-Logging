#!/usr/bin/env bash
# Compile two whole ESP-IDF WROOM test applications from the SAME owner/dependency
# snapshot, with ONLY the test-site preprocessor selector changing (0 versus 1).
# This is a test fixture, not a production Logging configuration switch.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
: "${EDP_LOGGING_DEPENDENCY_ROOT:?Set the pinned EDP dependency directory}"
: "${G11_REPORT_DIR:?Set a NEW result directory; existing runs must not be overwritten}"
test -d "$EDP_LOGGING_DEPENDENCY_ROOT/EDP-Logging/src"
test -f "$ROOT/tests/hil/g1-espidf/src/main.cpp"
test ! -e "$G11_REPORT_DIR"
mkdir -p "$G11_REPORT_DIR"
REPORT="$(cd "$G11_REPORT_DIR" && pwd -P)"
DEP_ROOT="$(cd "$EDP_LOGGING_DEPENDENCY_ROOT" && pwd -P)"
if [[ -n "${EDP_LOGGING_G11_PLATFORMIO_CORE_DIR:-}" ]]; then
    export PLATFORMIO_CORE_DIR="$EDP_LOGGING_G11_PLATFORMIO_CORE_DIR"
fi
PIO="${EDP_LOGGING_G11_PIO_BIN:-}"
if [[ -z "$PIO" ]]; then
    PIO="$(command -v pio || true)"
fi
if [[ -z "$PIO" && -n "${PLATFORMIO_CORE_DIR:-}" ]]; then
    PIO="$PLATFORMIO_CORE_DIR/penv/bin/pio"
fi
test -x "$PIO" || { echo 'G11_WHOLE_APP|PIO_NOT_FOUND' >&2; exit 2; }
SIZE="${EDP_LOGGING_G11_SIZE_BIN:-}"
if [[ -z "$SIZE" ]]; then
    SIZE="$(command -v xtensa-esp32-elf-size || true)"
fi
if [[ -z "$SIZE" ]]; then
    SIZE="$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages" -maxdepth 4 -type f -name xtensa-esp32-elf-size -print -quit 2>/dev/null || true)"
fi
test -x "$SIZE" || { echo 'G11_WHOLE_APP|XTENSA_SIZE_NOT_FOUND' >&2; exit 2; }
"$PIO" --version > "$REPORT/pio-version.txt"
"$SIZE" --version > "$REPORT/size-version.txt"
sha256sum "$ROOT/tests/hil/g1-espidf/src/main.cpp" > "$REPORT/owner-main.sha256"
# A caller must provide one coherent pinned owner closure; DO NOT silently
# permit PlatformIO to download a different transitive default branch.
for owner in EDP-Logging EDP-Clock EDP-System EDP-Memory EDP-Threading EDP-Serialisation EDP-Localisation EDP-Platform EDP-Platform-ESP-IDF EDP-Platform-FreeRTOS EDP-Platform-Portable EDP-Persistence EDP-Persistence-ESP-IDF EDP-Serial EDP-Serial-ESP-IDF EDP-BoundedTopology EDP-BoundedTypes; do
    test -d "$DEP_ROOT/$owner/src" || { echo "G11_WHOLE_APP|OWNER_MISSING|$owner" >&2; exit 3; }
    printf '%s|%s\n' "$owner" "$(realpath "$DEP_ROOT/$owner")" >> "$REPORT/owner-paths.txt"
done
test "$(realpath "$DEP_ROOT/EDP-Logging")" = "$ROOT" || {
    echo 'G11_WHOLE_APP|OWNER_MUST_MATCH_SOURCE_ROOT' >&2; exit 3;
}
for mode in off on; do
    selected=0
    if [[ "$mode" == on ]]; then selected=1; fi
    DEST="$REPORT/$mode"
    mkdir -p "$DEST/app"
    # Copy fixtures without local build outputs, generated sdkconfig or dirty
    # platform state. Never mutate the source under test.
    tar --exclude='.pio' --exclude='sdkconfig.esp32dev' \
        -C "$ROOT/tests/hil/g1-espidf" -cf - . | tar -C "$DEST/app" -xf -
    # The source imports the real generated Localisation schema. A fresh
    # isolated G11 source tree must generate its own deterministic owner
    # fixture before C++ compilation, rather than assume previous build output.
    python3 "$DEST/app/generate_g4_root_fixture.py" \
        "$DEP_ROOT/EDP-Localisation" "$DEST/app/src/generated"
    test -s "$DEST/app/src/generated/GeneratedLocalisationContract.hpp"
    sha256sum "$DEST/app/src/generated/GeneratedLocalisationContract.hpp" \
        > "$DEST/generated-contract.sha256"
    printf '    -DEDP_LOGGING_G11_APP_SITE_ENABLED=%s\n' "$selected" >> "$DEST/app/platformio.ini"
    test "$(sha256sum "$ROOT/tests/hil/g1-espidf/src/main.cpp" | cut -d' ' -f1)" = \
         "$(sha256sum "$DEST/app/src/main.cpp" | cut -d' ' -f1)"
    printf 'G11_WHOLE_APP|%s|BUILD_START\n' "$mode"
    (
        cd "$DEST/app"
        EDP_LOGGING_DEPENDENCY_ROOT="$DEP_ROOT" "$PIO" run -e esp32dev \
            > "$DEST/build.log" 2>&1
    ) || {
        tail -100 "$DEST/build.log" >&2
        echo "G11_WHOLE_APP|$mode|BUILD_FAIL" >&2
        exit 4
    }
    ELF="$DEST/app/.pio/build/esp32dev/firmware.elf"
    BIN="$DEST/app/.pio/build/esp32dev/firmware.factory.bin"
    test -s "$ELF" && test -s "$BIN"
    "$SIZE" "$ELF" > "$DEST/size.txt"
    "$SIZE" -A "$ELF" > "$DEST/sections.txt"
    sha256sum "$ELF" "$BIN" "$DEST/app/sdkconfig.esp32dev" > "$DEST/digests.txt"
    echo "G11_WHOLE_APP|$mode|LINK_PASS"
done
python3 - "$REPORT" <<'PY'
from pathlib import Path
import hashlib
import json
import sys
r = Path(sys.argv[1])
off = r / "off" / "app"
on = r / "on" / "app"
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def sizes(mode):
    v = (r / mode / "size.txt").read_text().splitlines()[-1].split()
    return dict(zip(("text", "data", "bss", "dec"), map(int, v[:4])))
for rel in ("src/main.cpp", "sdkconfig.defaults", "partitions.csv",
            "src/generated/GeneratedLocalisationContract.hpp"):
    assert digest(off / rel) == digest(on / rel), rel
config0 = (off / "platformio.ini").read_text()
config1 = (on / "platformio.ini").read_text()
f0 = "    -DEDP_LOGGING_G11_APP_SITE_ENABLED=0\n"
f1 = "    -DEDP_LOGGING_G11_APP_SITE_ENABLED=1\n"
assert config0.count(f0) == config1.count(f1) == 1
assert config0.replace(f0, "") == config1.replace(f1, "")
sdk = {}
for mode, app in (("off", off), ("on", on)):
    cfg = (app / "sdkconfig.esp32dev").read_text()
    expected_stack = next(line for line in (app / "sdkconfig.defaults").read_text().splitlines()
                          if line.startswith("CONFIG_ESP_MAIN_TASK_STACK_SIZE="))
    assert expected_stack in cfg, "build must retain the source fixture's explicit stack budget"
    sdk[mode] = digest(app / "sdkconfig.esp32dev")
assert sdk["off"] == sdk["on"], "generated SDK configuration diverged"
measure = {mode: sizes(mode) for mode in ("off", "on")}
result = {
    "qualification": "G11 test-app Off/On same-source linking (not product validation)",
    "selector": {"off": 0, "on": 1},
    "source_main_sha256": digest(off / "src/main.cpp"),
    "source_platformio_baseline_sha256": hashlib.sha256(config0.replace(f0, "").encode()).hexdigest(),
    "sdkconfig_sha256": sdk["off"],
    "owner_paths_sha256": digest(r / "owner-paths.txt"),
    "pio_version": (r / "pio-version.txt").read_text().strip(),
    "size_version": (r / "size-version.txt").read_text().splitlines()[0],
    "size": measure,
    "delta_on_minus_off": {key: measure["on"][key]-measure["off"][key] for key in measure["off"]},
    "images": {
        mode: {key: digest(r / mode / "app" / ".pio" / "build" / "esp32dev" / file)
            for key, file in (("elf", "firmware.elf"), ("factory", "firmware.factory.bin"))}
        for mode in ("off", "on")
    },
}
(r / "manifest.json").write_text(json.dumps(result, sort_keys=True, indent=2) + "\n")
print("G11_WHOLE_APP|BOTH_VARIANTS_LINKED|PASS")
print("G11_WHOLE_APP|manifest_sha256=" + digest(r / "manifest.json"))
print("G11_WHOLE_APP|size_delta=" + str(result["delta_on_minus_off"]))
PY
