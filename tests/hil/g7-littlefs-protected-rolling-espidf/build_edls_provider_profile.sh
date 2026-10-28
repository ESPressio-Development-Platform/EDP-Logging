#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPENDENCY_ROOT="${EDP_LOGGING_DEPENDENCY_ROOT:-$(cd "$HERE/../../../.." && pwd)}"
LOCALISATION="$DEPENDENCY_ROOT/EDP-Localisation"
test -d "$LOCALISATION/tools" || { echo "Missing EDP-Localisation owner" >&2; exit 2; }
python3 "$HERE/generate_typed_fixture.py" \
  --localisation "$LOCALISATION" \
  --output "$HERE/src/generated"
cd "$HERE"
"${EDP_PIO_COMMAND:-pio}" run -e esp32dev-typed-strict-profile
