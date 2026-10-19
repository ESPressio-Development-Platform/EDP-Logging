#!/usr/bin/env python3
"""Strict, finite site-evaluation assertion for the test-only G11 WROOM gate.

The marker is intentionally NOT counted among the 18 canonical HIL markers.
"""
from pathlib import Path
import re
import sys

PREFIX = "EDP_LOGGING_HIL|G11_WHOLE_FIRMWARE_SITE|"
FORM = re.compile(
    r"^EDP_LOGGING_HIL\|G11_WHOLE_FIRMWARE_SITE\|"
    r"selected=([01])\|clock=([0-9]+)\|identity=([0-9]+)\|"
    r"arguments=([0-9]+)\|result=(PASS|FAIL)$"
)


def validate_site_markers(lines, expected_mode, expected_occurrences=1):
    """Accept one assertion per observed bootstrap; NVS-restart HIL has two."""
    if type(expected_mode) is not int or expected_mode not in (0, 1):
        return False
    candidates = [line.strip() for line in lines if PREFIX in line]
    if expected_occurrences not in (1, 2) or len(candidates) != expected_occurrences:
        return False
    for candidate in candidates:
        match = FORM.fullmatch(candidate)
        if match is None:
            return False
        mode, clock, identity, arguments, outcome = match.groups()
        if not (
            int(mode) == expected_mode
            and all(int(value) == expected_mode for value in (clock, identity, arguments))
            and outcome == "PASS"
        ):
            return False
    return True


def main():
    """Read one independently captured transcript and enforce a 0/1 mode."""
    if len(sys.argv) not in (3, 4) or sys.argv[1] not in ("0", "1") or (len(sys.argv)==4 and sys.argv[3] not in ("1","2")):
        print("G11_SITE_EVIDENCE|INVALID_ARGS", file=sys.stderr)
        return 2
    lines = Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace").splitlines()
    passed = validate_site_markers(lines, int(sys.argv[1]), int(sys.argv[3]) if len(sys.argv)==4 else 1)
    print(f"G11_SITE_EVIDENCE|mode={sys.argv[1]}|result={'PASS' if passed else 'FAIL'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
