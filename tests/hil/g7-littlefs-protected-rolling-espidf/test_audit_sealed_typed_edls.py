"""Sealed physical-audit negatives: every original typed record is required."""
import tempfile
import unittest
from pathlib import Path

from edp_logging.archive import SegmentHeader, encode_segment_header
from edp_logging.framing import FrameCodec, crc32c, encode_frame
from audit_typed_extracted_edls import audit, DEVICE
from test_audit_typed_edls import typed

def sealed(slot, generation, *, count=None, records_per_segment=16,
           replace_ticket=None, override=None):
    if count is None:
        count=records_per_segment
    header=encode_segment_header(SegmentHeader(
        slot=slot,generation=generation,incarnation=7,device_identifier=DEVICE,profile=1))
    frames=[]
    for index in range(count):
        ticket=(generation-1)*records_per_segment+index+1
        fields=override if ticket==replace_ticket else None
        frames.append(encode_frame(FrameCodec.Cbor,typed(ticket,field_overrides=fields)))
    body=b"".join(frames)
    prefix=header+body+b"EDLC"+count.to_bytes(4,"little")+len(body).to_bytes(4,"little")
    return prefix+crc32c(prefix).to_bytes(4,"little")

class SealedTypedAuditTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        (self.root/"seed.txt").write_bytes(b"EDP1")
        for slot,generation in ((0,4),(2,3)):
            (self.root/f"slot{slot}").write_bytes(sealed(slot,generation))
    def check(self):
        return audit(self.root,records_per_segment=16)
    def test_complete_sixteen_original_records(self):
        self.assertEqual(self.check(),(3,4))
    def test_every_record_ticket_and_source_clock_checked(self):
        for ticket in range(33,49):
            for fields in ({4:ticket+1},{6:123456+ticket+1}):
                with self.subTest(ticket=ticket,fields=fields):
                    (self.root/"slot2").write_bytes(sealed(
                        2,3,replace_ticket=ticket,override=fields))
                    with self.assertRaises(ValueError): self.check()
    def test_every_field_has_independent_expected_value(self):
        overrides={0:[1,0,6],1:2,2:b"Z"*16,3:8,4:34,5:32,6:1,
                   7:123,8:1,9:1,10:0,11:[0,0],12:[0]*16,
                   13:[1,2],14:1}
        for field,value in overrides.items():
            with self.subTest(field=field):
                (self.root/"slot2").write_bytes(sealed(
                    2,3,replace_ticket=33,override={field:value}))
                with self.assertRaises(ValueError): self.check()
    def test_valid_seal_but_short_batch_rejected(self):
        (self.root/"slot2").write_bytes(sealed(2,3,count=15))
        with self.assertRaises(ValueError): self.check()
    def test_complete_frame_suffix_loss_rejected(self):
        data=sealed(2,3)
        first=encode_frame(FrameCodec.Cbor,typed(33))
        (self.root/"slot2").write_bytes(data[:40+15*len(first)])
        with self.assertRaises(ValueError): self.check()
    def test_unsupported_count_and_symlink_seed_rejected(self):
        with self.assertRaises(ValueError): audit(self.root,records_per_segment=2)
        (self.root/"seed.txt").unlink()
        target=self.root.parent/(self.root.name+"-seed")
        target.write_bytes(b"EDP1")
        self.addCleanup(target.unlink)
        (self.root/"seed.txt").symlink_to(target)
        with self.assertRaises(ValueError): self.check()

class SixtyFourRecordAuditTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        (self.root/"seed.txt").write_bytes(b"EDP1")
        for slot,generation in ((0,4),(2,3)):
            (self.root/f"slot{slot}").write_bytes(
                sealed(slot,generation,records_per_segment=64))
    def check(self):
        return audit(self.root,records_per_segment=64)
    def test_exact_two_complete_large_segments(self):
        self.assertEqual(self.check(),(3,4))
    def test_every_original_large_batch_record_checked(self):
        for ticket in range(129,193):
            with self.subTest(ticket=ticket):
                (self.root/"slot2").write_bytes(sealed(
                    2,3,records_per_segment=64,replace_ticket=ticket,
                    override={4:ticket+1}))
                with self.assertRaises(ValueError):self.check()
    def test_large_profile_rejects_short_complete_seal(self):
        (self.root/"slot2").write_bytes(sealed(
            2,3,records_per_segment=64,count=63))
        with self.assertRaises(ValueError):self.check()
    def test_profile_cannot_silently_reinterpret_record_tickets(self):
        with self.assertRaises(ValueError):audit(self.root,records_per_segment=16)
        (self.root/"slot0").write_bytes(sealed(0,4))
        with self.assertRaises(ValueError):self.check()

if __name__=="__main__":
    unittest.main()
