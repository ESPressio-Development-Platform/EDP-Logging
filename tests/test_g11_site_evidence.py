#!/usr/bin/env python3
"""G11 off/on UART marker acceptance and rejection unit tests."""
from pathlib import Path
import importlib.util
import sys
import unittest

SITE = Path(__file__).parent / "hil" / "g1-espidf" / "g11_site_evidence.py"
spec = importlib.util.spec_from_file_location("g11_site_evidence", SITE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
validate = module.validate_site_markers


def site(mode, clock=None, identity=None, arguments=None, result="PASS"):
    counters = [mode if value is None else value
                for value in (clock, identity, arguments)]
    return ("EDP_LOGGING_HIL|G11_WHOLE_FIRMWARE_SITE|"
            f"selected={mode}|clock={counters[0]}|identity={counters[1]}|"
            f"arguments={counters[2]}|result={result}")


class SiteEvidenceTests(unittest.TestCase):
    def test_off_never_evaluates(self):
        self.assertTrue(validate([site(0)], 0))
        self.assertFalse(validate([site(0, clock=1)], 0))
        self.assertFalse(validate([site(0, identity=1)], 0))
        self.assertFalse(validate([site(0, arguments=1)], 0))

    def test_on_evaluates_every_source_exactly_once(self):
        self.assertTrue(validate([site(1)], 1))
        for parameter in ("clock", "identity", "arguments"):
            self.assertFalse(validate([site(1, **{parameter: 2})], 1))
            self.assertFalse(validate([site(1, **{parameter: 0})], 1))

    def test_selection_and_result_qualifiers(self):
        self.assertFalse(validate([site(0)], 1))
        self.assertFalse(validate([site(1)], 0))
        self.assertFalse(validate([site(1, result="FAIL")], 1))
        self.assertFalse(validate([site(0)], 2))
        self.assertFalse(validate([site(0)], "0"))
        self.assertFalse(validate([], 0))

    def test_strict_format_and_unique_occurrence(self):
        good = site(1)
        self.assertTrue(validate(["boot", good, "test complete"], 1))
        self.assertFalse(validate([good, good], 1))
        self.assertTrue(validate([good, good], 1, 2))
        self.assertFalse(validate([good, good, good], 1, 2))
        self.assertFalse(validate([good, site(0)], 1, 2))
        self.assertFalse(validate([good + "|unexpected=1"], 1))
        self.assertFalse(validate([good.replace("selected=1", "selected=01")], 1))
        self.assertFalse(validate([good.replace("arguments=1", "arguments=-1")], 1))
        self.assertFalse(validate([good.replace("|clock=1", "|CLOCK=1")], 1))


if __name__ == "__main__":
    unittest.main(verbosity=2)
