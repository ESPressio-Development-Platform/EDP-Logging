#!/usr/bin/env python3
"""Offline EDLS evidence-audit negatives (no target, no flash)."""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from edp_logging.archive import SegmentHeader, encode_segment_header
from edp_logging.framing import FrameCodec, encode_frame
from audit_extracted_edls import audit


class IndependentEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp=TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        (self.root/"seed.txt").write_bytes(b"EDP1")
        for slot, generation in ((0, 4), (2, 3)):
            header=encode_segment_header(SegmentHeader(
                slot=slot,
                generation=generation,
                device_identifier=b"\xA6"*16,
                incarnation=generation
            ))
            frame=encode_frame(
                FrameCodec.Cbor,
                bytes((0x83,0x01,0x02,0x03)),
                max_payload=32
            )
            (self.root/f"slot{slot}").write_bytes(header+frame)

    def test_complete_immutable_window(self):
        self.assertEqual(audit(self.root),(3,4))

    def test_missing_protected_record(self):
        (self.root/"slot2").unlink()
        with self.assertRaisesRegex(ValueError,"file set"):
            audit(self.root)

    def test_unretired_oldest_is_not_claimed_rolled(self):
        (self.root/"slot1").write_bytes(b"unexpected")
        with self.assertRaisesRegex(ValueError,"file set"):
            audit(self.root)

    def test_truncated_record(self):
        (self.root/"slot0").write_bytes(
            (self.root/"slot0").read_bytes()[:50]
        )
        with self.assertRaisesRegex(ValueError,"size"):
            audit(self.root)

    def test_corrupted_header_crc(self):
        data=bytearray((self.root/"slot2").read_bytes())
        data[36]^=0x01
        (self.root/"slot2").write_bytes(data)
        with self.assertRaisesRegex(ValueError,"CRC32C"):
            audit(self.root)

    def test_corrupted_frame_crc(self):
        data=bytearray((self.root/"slot2").read_bytes())
        data[55]^=0x40
        (self.root/"slot2").write_bytes(data)
        with self.assertRaisesRegex(ValueError,"EDLG"):
            audit(self.root)

    def test_mismatched_device_identifier(self):
        data=bytearray((self.root/"slot0").read_bytes())
        data[18]^=0x01
        (self.root/"slot0").write_bytes(data)
        with self.assertRaises(ValueError):
            audit(self.root)

    def test_invalid_seed(self):
        (self.root/"seed.txt").write_bytes(b"UNKNOWN")
        with self.assertRaisesRegex(ValueError,"seed"):
            audit(self.root)


if __name__=="__main__":
    unittest.main()
