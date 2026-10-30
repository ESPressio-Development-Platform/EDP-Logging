"""Control tests for finite demo receipts; these are not physical executions."""
from pathlib import Path
import os
import tempfile
import unittest
from unittest.mock import patch

import monitor_edls_provider_profile as monitor


class DemoMonitorTests(unittest.TestCase):
    def test_requires_complete_nonzero_receipt(self):
        text = ("EDP_BATCH_DEMO|PASS|incarnation=1|dwell_ns=100000000|"
                "full_records=4|idle_records=1|ack_segment=2")
        pattern = monitor.PATTERNS["configurable-batch-demo"]
        self.assertIsNotNone(pattern.fullmatch(text))
        for bad in (
            text.replace("incarnation=1", "incarnation=0"),
            text.replace("ack_segment=2", "ack_segment=0"),
            text.replace("full_records=4", "full_records=3"),
            text.replace("idle_records=1", "idle_records=0"),
            text.replace("|PASS|", "|FAIL|"),
            text + "|unexpected=1",
        ):
            with self.subTest(receipt=bad):
                self.assertIsNone(pattern.fullmatch(bad))

    def exercise_monitor(self, line):
        class Uart:
            def __init__(self, *args, **kwargs):
                pass
            def __enter__(self):
                return self
            def __exit__(self, *args):
                return False
            def reset_input_buffer(self):
                pass
            def readline(self):
                return line.encode("utf-8") + b"\n"

        with tempfile.TemporaryDirectory() as directory, \
                patch.dict(os.environ, {"EDP_PROFILE_KIND": "configurable-batch-demo"}), \
                patch.object(monitor, "control") as control, \
                patch.object(monitor.serial, "Serial", Uart):
            result = monitor.main(str(Path(directory) / "receipt"))
            self.assertEqual(control.call_args_list[0].args, ("power-on",))
            self.assertEqual(control.call_args_list[1].args, ("release",))
            return result

    def test_failure_returns_immediately(self):
        self.assertEqual(self.exercise_monitor("EDP_BATCH_DEMO|FAIL|result=4"), 1)

    def test_complete_receipt_returns_success(self):
        self.assertEqual(self.exercise_monitor(
            "EDP_BATCH_DEMO|PASS|incarnation=2|dwell_ns=0|"
            "full_records=4|idle_records=1|ack_segment=4"), 0)


if __name__ == "__main__":
    unittest.main()
