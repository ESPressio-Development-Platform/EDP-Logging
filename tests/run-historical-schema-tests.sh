#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCALISATION="${EDP_LOCALISATION_SOURCE_DIR:-$ROOT/../EDP-Localisation}"
test -f "$LOCALISATION/tools/edp_localisation/historical_archive.py" || {
    echo "historical schema bridge requires the EDP-Localisation archival owner" >&2
    exit 2
}
PYTHONPATH="$ROOT/tools:$LOCALISATION/tools:$LOCALISATION/tools/tests${PYTHONPATH:+:$PYTHONPATH}" \
    python3 "$ROOT/tests/optional/ArchivedSchemaTests.py"
echo "Optional Localisation historical diagnostic schema bridge: PASS"
