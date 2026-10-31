"""Managed cut media must match independently captured original source coordinates."""
import tempfile
import unittest
from pathlib import Path
from audit_batch_worker_media import audit, receipt
from test_audit_batch_worker_media import segment

class ManagedCutMediaTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / "seed.txt").write_bytes(b"EDP1")
        (self.root / "slot0").write_bytes(segment(0, 1, range(1, 65)))
        (self.root / "slot1").write_bytes(segment(1, 2, range(65, 129)))
        self.original = "".join(
            f"EDP_WORKER|ORIGINAL|ticket={ticket}|monotonic_ns={1000000000+ticket}\n"
            for ticket in range(1, 129))
        self.trigger = "EDP_BND|SEALED_WRITE_ACTIVITY|gen=3|nvs_ack=2\n"
        self.done = "EDP_WORKER|POWER_CUT_RECOVERED|ack_segment=2|records=128\n"
        self.text = self.original + self.trigger + self.done

    def test_capacity_cut_uses_same_independent_original_field_audit(self):
        text = self.text.replace(self.trigger,
            "EDP_BND|MANAGED_CAPACITY_SAFE|ack=2|pending_records=50|native_replace=12|durable=128|free_bytes=4096\n")
        self.assertEqual(audit(self.root, text, False, True), (2, 128))
        (self.root / "slot1").write_bytes(
            segment(1, 2, range(65, 129), {99: {6: 1000000100}}))
        with self.assertRaises(ValueError):
            audit(self.root, text, False, True)

    def test_capacity_refusal_must_prove_retained_ram_and_old_durability(self):
        marker = "EDP_BND|MANAGED_CAPACITY_SAFE|ack=2|pending_records=50|native_replace=12|durable=128|free_bytes=4096\n"
        for old, new in (("ack=2", "ack=3"), ("pending_records=50", "pending_records=0"),
                         ("pending_records=50", "pending_records=65"),
                         ("native_replace=12", "native_replace=0"),
                         ("durable=128", "durable=129"), ("free_bytes=4096", "free_bytes=8193")):
            with self.subTest(field=old), self.assertRaises(ValueError):
                receipt(self.original + marker.replace(old, new) + self.done, False, True)

    def test_cut_modes_cannot_borrow_other_stage_evidence(self):
        with self.assertRaises(ValueError):
            receipt(self.text, False, True)
        with self.assertRaises(ValueError):
            receipt(self.text, True, True)

    def test_all_128_original_pressure_records_survive(self):
        self.assertEqual(audit(self.root, self.text, True), (2, 128))

    def test_new_valid_crc_cannot_change_original_occurrence(self):
        (self.root / "slot1").write_bytes(
            segment(1, 2, range(65, 129), {99: {6: 1000000100}}))
        with self.assertRaises(ValueError):
            audit(self.root, self.text, True)

    def test_after_cut_reconstruction_is_not_original_evidence(self):
        with self.assertRaises(ValueError):
            receipt(self.trigger + self.original + self.done, True)

    def test_missing_ambiguous_wrong_count_or_missing_activity_refused(self):
        for text in ("", self.text + self.done,
                     self.text.replace("records=128", "records=127"),
                     self.original + self.done,
                     self.original + self.done + self.trigger,
                     self.text + "EDP_WORKER|PASS|scheduled_records=2000|producers=2|target_rps=100|ack_segment=2\n"):
            with self.subTest(text=text[-100:]), self.assertRaises(ValueError):
                receipt(text, True)

    def test_invalid_raw_audit_mode_refused_before_hardware(self):
        import os, subprocess
        env = dict(os.environ, EDP_PROFILE_AGENT_ID="unit-no-hardware",
                   EDP_PROFILE_EVIDENCE_DIR="/nonexistent/unit-no-hardware",
                   EDP_WORKER_AUDIT_POWER_CUT="invalid")
        result = subprocess.run(
            ["bash", str(Path(__file__).with_name("run_batch_worker_raw_audit.sh"))],
            env=env, text=True, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unsupported worker raw audit mode", result.stderr)

    def test_missing_duplicate_or_unknown_original_source_refused(self):
        for text in (
            self.text.replace(self.original.splitlines(keepends=True)[0], ""),
            self.text.replace("ticket=128|", "ticket=127|"),
            self.text.replace("ticket=128|", "ticket=161|"),
            self.text.replace("monotonic_ns=1000000128", "monotonic_ns=0")):
            with self.assertRaises(ValueError):
                receipt(text, True)

    def test_unreconciled_spare_and_missing_retained_original_refused(self):
        (self.root / "slot2").write_bytes(segment(2, 3, [129]))
        with self.assertRaises(ValueError):
            audit(self.root, self.text, True)
        (self.root / "slot2").unlink()
        (self.root / "slot1").write_bytes(segment(1, 2, range(65, 128)))
        with self.assertRaises(ValueError):
            audit(self.root, self.text, True)

if __name__ == "__main__":
    unittest.main()
