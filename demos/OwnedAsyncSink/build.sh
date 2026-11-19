#!/usr/bin/env bash
set -Eeuo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LOGGING="$(cd "$HERE/../.." && pwd)"
DEPENDENCIES="$(dirname "$LOGGING")"
TARGET="${1:-all}"
PIO="${EDP_PIO_COMMAND:-pio}"
ARDUINO="${EDP_ARDUINO_CLI_COMMAND:-arduino-cli}"
JOBS="${EDP_DEMO_JOBS:-2}"
case "$TARGET" in
    all|espidf|platformio-arduino|arduino-ide) ;;
    *) echo "Unknown target: $TARGET" >&2; exit 2 ;;
esac
if test "$TARGET" = all || test "$TARGET" = espidf; then
    "$PIO" run --project-dir "$HERE/PlatformIO_ESP-IDF" -e esp32dev -j "$JOBS"
fi
if test "$TARGET" = all || test "$TARGET" = platformio-arduino; then
    "$PIO" run --project-dir "$HERE/PlatformIO_Arduino" -e esp32dev -j "$JOBS"
fi
if test "$TARGET" = all || test "$TARGET" = arduino-ide; then
owners=(Platform-ESP-IDF Platform-FreeRTOS Memory Platform-Portable Platform System BoundedTopology Clock Threading)
for owner in "${owners[@]}"; do
    test -d "$DEPENDENCIES/EDP-$owner/src" || { echo "Missing EDP-$owner" >&2; exit 2; }
done
    EXTRA="-std=gnu++20 -O2 -I$LOGGING/src"
    for owner in "${owners[@]}"; do
        EXTRA="$EXTRA -I$DEPENDENCIES/EDP-$owner/src"
    done
    "$ARDUINO" compile --fqbn esp32:esp32:esp32 \
        --jobs "$JOBS" \
        --build-path "$HERE/Arduino_IDE/.build" \
        --build-property "compiler.cpp.extra_flags=$EXTRA" \
        "$HERE/Arduino_IDE"
fi
