"""Extended physical workload receipts and actual compiler configuration checks."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from audit_batch_worker_media import receipt
from monitor_edls_provider_profile import PATTERNS


class ExtendedWorkerTests(unittest.TestCase):
    def test_extended_receipt_keeps_its_own_ticket_bound(self):
        """The6000-record workload permits6161, but still rejects6162."""
        text = ("EDP_WORKER|ORIGINAL|ticket=6161|monotonic_ns=1\n"
                "EDP_WORKER|PASS|scheduled_records=6000|producers=2|target_rps=100|"
                "elapsed_us=60000000|ack_segment=99\n")
        self.assertEqual(receipt(text), (99, {6161: 1}))
        for bad in (text.replace("ticket=6161", "ticket=6162"),
                    text.replace("scheduled_records=6000", "scheduled_records=2000"),
                    text.replace("scheduled_records=6000", "scheduled_records=3000")):
            with self.assertRaises(ValueError):
                receipt(bad)

    def test_extended_monitor_requires_count_and_unchanged_configuration(self):
        """A short workload or altered CPU/dwell cannot satisfy the extended gate."""
        text = ("EDP_WORKER|PASS|scheduled_records=6000|producers=2|target_rps=100|"
                "elapsed_us=60000000|scheduled_dwell_ns=500000000|"
                "application_optimisation=o2|cpu_mhz=160|ack_segment=99")
        pattern = PATTERNS["batch-worker-500-o2-6000"]
        self.assertIsNotNone(pattern.search(text))
        for bad in (text.replace("6000", "2000"), text.replace("=160", "=240"),
                    text.replace("=o2", "=og"), text.replace("500000000", "100000000")):
            self.assertIsNone(pattern.search(bad))

    def check_command(self, extended_argument, schedule_flag):
        """Execute the real verifier against a finite isolated command database."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments = ["compiler", "-O2", "-DEDP_TEST_BATCH_WORKER",
                         "-DEDP_WORKER_DWELL_NS=500000000LL",
                         "-DEDP_WORKER_OPTIMISED_BUILD=1"]
            if schedule_flag:
                arguments.append("-DEDP_WORKER_RECORDS_PER_PRODUCER=3000U")
            (root / "commands.json").write_text(json.dumps([
                {"file": "src/main.cpp", "arguments": arguments}]))
            (root / "sdkconfig").write_text(
                "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y\n"
                "CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL=2\n"
                "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160\n")
            command = [sys.executable, str(Path(__file__).with_name(
                "verify_worker_compile_profile.py")),
                "--database", str(root / "commands.json"),
                "--sdkconfig", str(root / "sdkconfig"), "--output", str(root / "proof.json")]
            if extended_argument:
                command += ["--records-per-producer", "3000"]
            result = subprocess.run(command, capture_output=True, text=True, timeout=5)
            proof = root / "proof.json"
            return result.returncode, json.loads(proof.read_text()) if proof.exists() else None

    def test_actual_extended_command_is_verified(self):
        """The matching extended command records3000 sources per producer."""
        status, proof = self.check_command(True, True)
        self.assertEqual(status, 0)
        self.assertEqual(proof["records_per_producer"], 3000)

    def test_wrong_actual_schedule_does_not_get_a_proof(self):
        """Both directions of command/profile mismatch fail before hardware."""
        for argument, flag in ((True, False), (False, True)):
            status, proof = self.check_command(argument, flag)
            self.assertNotEqual(status, 0)
            self.assertIsNone(proof)


if __name__ == "__main__":
    unittest.main()
