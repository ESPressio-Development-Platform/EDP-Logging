#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPENDENCY_ROOT="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$HERE/../../../.." && pwd)}"
LOCALISATION="$DEPENDENCY_ROOT/EDP-Localisation"
BUILD_ENV="${EDP_LFS_BUILD_ENV:-esp32dev-typed-strict}"
case "$BUILD_ENV" in
    esp32dev-typed-strict|esp32dev-sealed-batch|esp32dev-sealed-batch-cut|esp32dev-sealed-batch-64|esp32dev-sealed-batch-64-cut) ;;
    *) echo "Unsupported typed strict build: $BUILD_ENV" >&2; exit 2 ;;
esac
test -d "$LOCALISATION/tools" || { echo "Missing EDP-Localisation owner" >&2; exit 2; }
python3 "$HERE/generate_typed_fixture.py" \
  --localisation "$LOCALISATION" \
  --output "$HERE/src/generated"
cd "$HERE"
"${EDP_PIO_COMMAND:-pio}" run -e "$BUILD_ENV" -j "${EDP_PIO_JOBS:-2}"
