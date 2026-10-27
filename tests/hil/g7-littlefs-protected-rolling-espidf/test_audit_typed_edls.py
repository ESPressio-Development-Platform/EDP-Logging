"""Offline independent typed EDLS parser negatives using test-only CBOR fixtures."""
import tempfile
import unittest
from pathlib import Path

from edp_logging.archive import SegmentHeader, encode_segment_header
from edp_logging.framing import FrameCodec, encode_frame
from audit_typed_extracted_edls import audit,DEVICE,TYPE_IDENTIFIER,ARGS,FINGERPRINT

def encoded(value):
    if isinstance(value,int):
        if value<24:return bytes([value])
        if value<256:return bytes((24,value))
        if value<65536:return bytes((25,))+value.to_bytes(2,"big")
        if value<2**32:return bytes((26,))+value.to_bytes(4,"big")
        return bytes((27,))+value.to_bytes(8,"big")
    if value is None:return b"\xf6"
    if isinstance(value,bytes):
        head=encoded(len(value))
        return bytes((head[0]|0x40,))+head[1:]+value
    if isinstance(value,list):
        head=encoded(len(value))
        return bytes((head[0]|0x80,))+head[1:]+b"".join(map(encoded,value))
    if isinstance(value,dict):
        head=encoded(len(value))
        return bytes((head[0]|0xA0,))+head[1:]+b"".join(
            encoded(k)+encoded(v) for k,v in sorted(value.items()))
    raise TypeError("unhandled fixture CBOR type")

REMOVE=object()

def typed(generation,*,type_id=TYPE_IDENTIFIER,fingerprint=None, field_overrides=None):
    fields={
        0:[1,0,5],1:3,2:DEVICE,3:7,4:generation,5:16,
        6:123456+generation,8:0,9:0,10:4294967295,
        11:[1,0],12:list(FINGERPRINT if fingerprint is None else fingerprint),
        13:ARGS,14:0
    }
    if field_overrides:
        for key,value in field_overrides.items():
            if value is REMOVE: fields.pop(key,None)
            else: fields[key]=value
    return encoded([1,type_id,fields])

class TypedEDLSOfflineAuditTests(unittest.TestCase):
    def setUp(self):
        t=tempfile.TemporaryDirectory()
        self.addCleanup(t.cleanup)
        self.root=Path(t.name)
        (self.root/"seed.txt").write_bytes(b"EDP1")
        for slot,gen in ((0,4),(2,3)):
            self.put(slot,gen)
    def put(self,slot,gen,*,type_id=TYPE_IDENTIFIER,
            fingerprint=None,field_overrides=None):
        header=encode_segment_header(SegmentHeader(
            slot=slot,generation=gen,incarnation=7,
            device_identifier=DEVICE))
        data=encode_frame(FrameCodec.Cbor,
            typed(gen,type_id=type_id,fingerprint=fingerprint,field_overrides=field_overrides),
            max_payload=1024)
        (self.root/f"slot{slot}").write_bytes(header+data)
    def test_expected_window(self):
        self.assertEqual(audit(self.root),(3,4))
    def test_missing_protected(self):
        (self.root/"slot2").unlink()
        with self.assertRaises(ValueError):audit(self.root)
    def test_unretired_oldest(self):
        (self.root/"slot1").write_bytes(b"x")
        with self.assertRaises(ValueError):audit(self.root)
    def test_corrupt_header(self):
        p=self.root/"slot0";b=bytearray(p.read_bytes());b[36]^=1;p.write_bytes(b)
        with self.assertRaises(ValueError):audit(self.root)
    def test_corrupt_frame(self):
        p=self.root/"slot2";b=bytearray(p.read_bytes());b[-8]^=1;p.write_bytes(b)
        with self.assertRaises(ValueError):audit(self.root)
    def test_wrong_record_type(self):
        self.put(2,3,type_id=b"Z"*8)
        with self.assertRaises(ValueError):audit(self.root)
    def test_mismatched_generated_schema(self):
        self.put(2,3,fingerprint=[89]*16)
        with self.assertRaises(ValueError):audit(self.root)
    def test_truncated(self):
        p=self.root/"slot0";p.write_bytes(p.read_bytes()[:-1])
        with self.assertRaises(ValueError):audit(self.root)
    def test_wrong_generation(self):
        self.put(2,5)
        with self.assertRaises(ValueError):audit(self.root)
    def test_false_device(self):
        p=self.root/"slot0";b=bytearray(p.read_bytes());b[16]^=1;p.write_bytes(b)
        with self.assertRaises(ValueError):audit(self.root)

if __name__=="__main__":
    unittest.main()
