"""Five-minute receipts retain finite tickets and explicit bounded percentile scope."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from audit_batch_worker_media import audit, receipt
from monitor_edls_provider_profile import PATTERNS
from test_audit_batch_worker_media import segment

class LongWorkerProfileTests(unittest.TestCase):
    def text(self, ticket=30161):
        return (
            f"EDP_WORKER|ORIGINAL|ticket={ticket}|monotonic_ns={1000000000+ticket}\n"
            "EDP_WORKER|PASS|scheduled_records=30000|producers=2|target_rps=100|"
            "elapsed_us=300000000|latency_scope=last_6000|latency_samples=6000|"
            "p50_us=800000|p95_us=1100000|p99_us=1300000|scheduled_dwell_ns=500000000|"
            "application_optimisation=o2|cpu_mhz=160|ack_segment=601\n")

    def test_exact_long_profile_and_last_window_are_explicit(self):
        text=self.text()
        self.assertEqual(receipt(text), (601, {30161: 1000030161}))
        self.assertIsNotNone(PATTERNS["batch-worker-500-o2-30000"].search(text))
        for bad in (text.replace("last_6000", "all_scheduled"),
                    text.replace("latency_samples=6000", "latency_samples=30000"),
                    text.replace("scheduled_records=30000", "scheduled_records=6000"),
                    text.replace("cpu_mhz=160", "cpu_mhz=240"),
                    text.replace("application_optimisation=o2", "application_optimisation=og")):
            self.assertIsNone(PATTERNS["batch-worker-500-o2-30000"].search(bad))

    def test_80rps_receipt_is_disjoint_from_100rps(self):
        original=self.text()
        reduced=original.replace("target_rps=100","target_rps=80").replace(
            "elapsed_us=300000000","elapsed_us=375000000")
        self.assertEqual(receipt(reduced), (601, {30161: 1000030161}))
        self.assertIsNotNone(PATTERNS["batch-worker-500-o2-30000-80"].search(reduced))
        self.assertIsNone(PATTERNS["batch-worker-500-o2-30000"].search(reduced))
        self.assertIsNone(PATTERNS["batch-worker-500-o2-30000-80"].search(original))
        for bad in (reduced.replace("last_6000","all_scheduled"),
                    reduced.replace("latency_samples=6000","latency_samples=30000"),
                    reduced.replace("target_rps=80","target_rps=79"),
                    reduced.replace("cpu_mhz=160","cpu_mhz=240")):
            self.assertIsNone(PATTERNS["batch-worker-500-o2-30000-80"].search(bad))

    def test_duplicate_identical_final_original_receipt_is_refused(self):
        original = self.text()
        for repeated_ns in ("1000030161", "1000030162"):
            duplicated = original.replace(
                "EDP_WORKER|PASS|",
                "EDP_WORKER|ORIGINAL|ticket=30161|monotonic_ns=" +
                repeated_ns + "\nEDP_WORKER|PASS|",
                1
            )
            with self.assertRaises(ValueError):
                receipt(duplicated)

    def test_ticket_horizon_and_false_all_run_percentiles_refused(self):
        for bad in (self.text(30162), self.text().replace("last_6000", "all_scheduled"),
                    self.text().replace("latency_samples=6000", "latency_samples=30000"),
                    "|latency_scope=last_6000|latency_samples=6000|\n" + self.text().replace("last_6000", "all_scheduled")):
            with self.assertRaises(ValueError):
                receipt(bad)

    def test_long_retained_media_still_checks_original_occurrence(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            (root/"seed.txt").write_bytes(b"EDP1")
            (root/"slot2").write_bytes(segment(2,600,[30160]))
            (root/"slot0").write_bytes(segment(0,601,[30161]))
            text=("EDP_WORKER|ORIGINAL|ticket=30160|monotonic_ns=1000030160\n"+self.text())
            self.assertEqual(audit(root,text),(601,2))
            (root/"slot0").write_bytes(segment(0,601,[30161],{30161:{6:1000030162}}))
            with self.assertRaises(ValueError):
                audit(root,text)

    def check_compile(self, selected, actual, *, selected_rate=100, actual_rate=100):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            args=["compiler","-O2","-DEDP_TEST_BATCH_WORKER",
                  "-DEDP_WORKER_DWELL_NS=500000000LL","-DEDP_WORKER_OPTIMISED_BUILD=1",
                  f"-DEDP_WORKER_RECORDS_PER_PRODUCER={actual}U"]
            if actual_rate != 100:
                args.append(f"-DEDP_WORKER_SCHEDULE_RPS={actual_rate}U")
            (root/"commands.json").write_text(json.dumps([{"file":"src/main.cpp","arguments":args}]))
            (root/"sdkconfig").write_text("CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y\n"
                "CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL=2\nCONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160\n")
            result=subprocess.run([sys.executable,str(Path(__file__).with_name("verify_worker_compile_profile.py")),
                "--records-per-producer",str(selected),"--schedule-rps",str(selected_rate),
                "--database",str(root/"commands.json"),
                "--sdkconfig",str(root/"sdkconfig"),"--output",str(root/"proof.json")],
                text=True,capture_output=True,timeout=5)
            return result.returncode, json.loads((root/"proof.json").read_text()) if (root/"proof.json").exists() else None

    def test_actual_15000_per_producer_flag_is_qualified(self):
        code,proof=self.check_compile(15000,15000)
        self.assertEqual(code,0)
        self.assertEqual(proof["records_per_producer"],15000)

    def test_actual_80rps_build_must_include_exact_rate_flag(self):
        for selected,actual,valid in ((80,80,True),(80,100,False),(100,80,False),
                                      (100,100,True)):
            with self.subTest(selected=selected,actual=actual):
                code,proof=self.check_compile(15000,15000,selected_rate=selected,actual_rate=actual)
                self.assertEqual(code==0,valid)
                self.assertEqual(proof["schedule_rps"],selected) if valid else self.assertIsNone(proof)

    def test_long_and_short_actual_commands_cannot_be_mislabelled(self):
        for selected,actual in ((15000,3000),(3000,15000),(1000,15000)):
            code,proof=self.check_compile(selected,actual)
            self.assertNotEqual(code,0)
            self.assertIsNone(proof)

if __name__=="__main__":
    unittest.main()
