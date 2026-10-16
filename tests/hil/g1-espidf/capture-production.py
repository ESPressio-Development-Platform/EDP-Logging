#!/usr/bin/env python3
"""Backwards-compatible HIL entrypoint: use current, strict native owner capture."""
from pathlib import Path
import runpy

runpy.run_path(
    str(Path(__file__).with_name("capture-serial-persistence.py")),
    run_name="__main__"
)
