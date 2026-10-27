"""Independent host regressions for protected pair after live candidate cuts."""
import tempfile
import unittest
from pathlib import Path

from edp_logging.archive import SegmentHeader,encode_segment_header
from edp_logging.framing import FrameCodec,encode_frame
from audit_midwrite_extracted_edls import audit

class MidwritePhysicalAuditTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        (self.root/"seed.txt").write_bytes(b"EDP1")
        for slot in (0,1):
            header=encode_segment_header(SegmentHeader(
                slot=slot,generation=slot+1,incarnation=3,
                device_identifier=bytes([0xA6])*16))
            frame=encode_frame(FrameCodec.Cbor,
                bytes([0x83,0x01,0x02,0x03]),max_payload=32)
            (self.root/f"slot{slot}").write_bytes(header+frame)

    def test_exact_protected_pair_no_candidate(self):
        self.assertEqual(audit(self.root)[:2],(1,2))
    def test_torn_candidate_never_counts_as_acknowledged(self):
        (self.root/"slot2").write_bytes(bytes([0xAB])*1024)
        oldest,newest,candidate=audit(self.root)
        self.assertEqual((oldest,newest),(1,2))
        self.assertTrue(candidate.startswith("1024B"))
    def test_corrupt_acked_pair_must_fail(self):
        p=self.root/"slot1"
        buf=bytearray(p.read_bytes())
        buf[-1]^=0xFF
        p.write_bytes(buf)
        with self.assertRaises(ValueError):
            audit(self.root)
    def test_unexpected_file_rejected(self):
        (self.root/"slot3").write_bytes(b"?")
        with self.assertRaises(ValueError):
            audit(self.root)
    def test_candidate_not_a_regular_file(self):
        (self.root/"slot2").mkdir()
        with self.assertRaises(ValueError):
            audit(self.root)
    def test_invalid_prewrite_incarnation(self):
        p=self.root/"slot0"
        buf=bytearray(p.read_bytes())
        buf[32]^=1
        p.write_bytes(buf)
        with self.assertRaises(ValueError):
            audit(self.root)
