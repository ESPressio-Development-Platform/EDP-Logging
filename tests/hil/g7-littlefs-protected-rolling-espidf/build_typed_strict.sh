#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPENDENCY_ROOT="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$HERE/../../../.." && pwd)}"
LOCALISATION="$DEPENDENCY_ROOT/EDP-Localisation"
BUILD_ENV="${EDP_LFS_BUILD_ENV:-esp32dev-typed-strict}"
case "$BUILD_ENV" in
    esp32dev-batch-worker-500-o2-30000-80|esp32dev-batch-worker-500-o2-30000-dual|esp32dev-batch-worker-500-o2-30000-wide|esp32dev-batch-worker-500-o2-30000|esp32dev-sealed-batch-64-retirecut|esp32dev-batch-worker-500-o2-capacitycut|esp32dev-batch-worker-500-o2-writecut|esp32dev-sealed-batch-64-writecut|esp32dev-typed-strict|esp32dev-sealed-batch|esp32dev-sealed-batch-cut|esp32dev-sealed-batch-64|esp32dev-sealed-batch-64-cut|esp32dev-batch-worker|esp32dev-batch-worker-500|esp32dev-batch-worker-500-o2|esp32dev-batch-worker-500-o2-6000) ;;
    *) echo "Unsupported typed strict build: $BUILD_ENV" >&2; exit 2 ;;
esac
test -d "$LOCALISATION/tools" || { echo "Missing EDP-Localisation owner" >&2; exit 2; }
python3 "$HERE/generate_typed_fixture.py" \
  --localisation "$LOCALISATION" \
  --output "$HERE/src/generated"
cd "$HERE"
"${EDP_PIO_COMMAND:-pio}" run -e "$BUILD_ENV" -j "${EDP_PIO_JOBS:-2}"

if test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-80 || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-dual || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-wide || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000 || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-capacitycut || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-writecut || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2 || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-6000; then
    RECORDS_PER_PRODUCER=1000
    SCHEDULE_RPS=100
    if test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-80 || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-dual || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-wide || test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000; then RECORDS_PER_PRODUCER=15000; fi
    if test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-6000; then RECORDS_PER_PRODUCER=3000; fi
    if test "$BUILD_ENV" = esp32dev-batch-worker-500-o2-30000-80; then SCHEDULE_RPS=80; fi
    "${EDP_PIO_COMMAND:-pio}" run -e "$BUILD_ENV" -t compiledb -j "${EDP_PIO_JOBS:-2}"
    python3 "$HERE/verify_worker_compile_profile.py" \
        --records-per-producer "$RECORDS_PER_PRODUCER" \
        --schedule-rps "$SCHEDULE_RPS" \
        --database "$HERE/compile_commands.json" \
        --sdkconfig "$HERE/sdkconfig.$BUILD_ENV" \
        --output "$HERE/.pio/build/$BUILD_ENV/verified-compile-profile.json"
fi
