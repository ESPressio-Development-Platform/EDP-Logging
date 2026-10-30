"""Typed managed-demo receipt gates; no physical device is used by these tests."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import monitor_edls_provider_profile as monitor


class TypedWorkerMonitorTests(unittest.TestCase):
    def test_requires_every_durable_retirement_and_retention_field(self):
        """A weaker count or volatile marker cannot qualify the typed example."""
        text = ("EDP_TYPED_WORKER_DEMO|PASS|incarnation=1|records=9|durable=9|retired=9|"
                "ack_segment=3|retained_records=5|dwell_ns=100000000")
        pattern = monitor.PATTERNS["typed-managed-batch-demo"]
        self.assertIsNotNone(pattern.fullmatch(text))
        for bad in (text.replace("durable=9", "durable=8"),
                    text.replace("retired=9", "retired=8"),
                    text.replace("records=9", "records=8"),
                    text.replace("incarnation=1", "incarnation=0"),
                    text.replace("|PASS|", "|RAM|"),
                    text.replace("retained_records=5", "retained_records=4")):
            self.assertIsNone(pattern.fullmatch(bad))

    def test_typed_failure_terminates_monitor(self):
        """An explicit application failure ends the host gate promptly."""
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
                return b"EDP_TYPED_WORKER_DEMO|FAIL|stage=8\n"
        with tempfile.TemporaryDirectory() as directory, patch.dict(
                os.environ, {"EDP_PROFILE_KIND": "typed-managed-batch-demo"}), \
                patch.object(monitor, "control"), patch.object(monitor.serial, "Serial", Uart):
            self.assertEqual(monitor.main(str(Path(directory) / "receipt")), 1)

    def test_unknown_kind_fails_before_any_appliance_operation(self):
        """The shell dispatcher rejects unsupported example kinds at preflight."""
        environment = dict(os.environ, EDP_PROFILE_AGENT_ID="unit-test-no-hardware",
                           EDP_PROFILE_EVIDENCE_DIR="/nonexistent/test")
        result = subprocess.run(
            ["bash", str(Path(__file__).with_name("run_batch_demo_hil.sh")),
             "arduino-ide", "unknown"], env=environment, text=True,
            capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unsupported demo kind", result.stderr)


if __name__ == "__main__":
    unittest.main()
