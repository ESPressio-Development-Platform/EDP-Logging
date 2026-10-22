#!/usr/bin/env python3
"""Validate the five actual G4 whole-record stack high-water observations.

This is *application HIL* evidence; it does not certify production stacks.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

PHASES = ("projection", "cbor_encode", "cbor_decode", "json_encode", "json_decode")
PATTERN = re.compile(
    r"EDP_LOGGING_HIL\|G4_STACK_PHASE\|([^|\r\n]+)\|"
    r"minimum_remaining_bytes=(\d+)"
)
MINIMUM_ACCEPTABLE_MARGIN_BYTES = 2048


def validate(data: str) -> tuple[int, list[tuple[str, int]]]:
    observations = [(phase, int(value)) for phase, value in PATTERN.findall(data)]
    if tuple(phase for phase, _ in observations) != PHASES:
        raise ValueError(
            f"Expected exactly five phases {PHASES}, observed {observations}"
        )
    values = [n for _, n in observations]
    if any(a < b for a, b in zip(values, values[1:])):
        raise ValueError("Minimum remaining stack unexpectedly increased")
    minimum = min(values)
    if minimum < MINIMUM_ACCEPTABLE_MARGIN_BYTES:
        raise ValueError(
            f"Whole-record fixture stack margin {minimum} B "
            f"is below {MINIMUM_ACCEPTABLE_MARGIN_BYTES} B"
        )
    return minimum, observations


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify-g4-stack-phases.py CAPTURE_LOG", file=sys.stderr)
        return 2
    try:
        minimum, phases = validate(
            Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
        )
    except (OSError, ValueError) as exc:
        print(f"G4_STACK_PHASES_FAIL: {exc}", file=sys.stderr)
        return 1
    print(f"G4_STACK_PHASES_PASS minimum_margin_bytes={minimum} phases={phases}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
