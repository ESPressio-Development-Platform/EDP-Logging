#!/usr/bin/env python3
"""Independent offline raw-LittleFS audit of every typed EDLS record.

Reuses qualified EDLS/EDLG/TypedEnvelope parsers. No UART trust, new CBOR
schema decoder, filesystem repair, or flash mutation.
"""
from __future__ import annotations

from io import BytesIO
from pathlib import Path
import sys
from edp_logging.archive import parse_segment_header, parse_sealed_segment
from edp_logging.framing import Frame, FrameCodec, scan_stream
from edp_logging.record_structure import inspect_typed_envelope

EXPECTED = {"slot0": (0,4), "slot2": (2,3)}
TYPE_IDENTIFIER = bytes((0,0,1,0,0,0,0,1))
DEVICE = bytes((0xA6,))*16
ARGS = [0x01020304,0x0102030405060708]
# Independent golden owner fixture, never learned from media under audit.
FINGERPRINT = [197,113,70,141,242,104,131,221,93,63,240,70,7,164,83,169]
EXPECTED_FIELDS = set(range(15)) - {7}

def verify_record(packet: Frame, ticket: int) -> tuple[int, ...]:
    if packet.codec != FrameCodec.Cbor or packet.recovered_candidate:
        raise ValueError("noncanonical or recovered EDLG frame")
    envelope=inspect_typed_envelope(packet.payload,FrameCodec.Cbor)
    if envelope.type_identifier!=TYPE_IDENTIFIER:
        raise ValueError("permanent EDP System TypeIdentifier mismatch")
    fields=envelope.fields
    # Fifteen bindings; absent optional synchronized timestamp omits Field7.
    if set(fields)!=EXPECTED_FIELDS:
        raise ValueError("invalid mandatory/optional LogRecord FieldIds")
    if fields[0]!=[1,0,5] or fields[1]!=3:
        raise ValueError("owner-generated message or warning severity mismatch")
    if fields[2]!=DEVICE or fields[3]!=7:
        raise ValueError("source DeviceIdentifier or incarnation mismatch")
    if fields[4]!=ticket or fields[5]!=16:
        raise ValueError("attempt ticket or sequence width mismatch")
    if fields[6]!=123456+ticket or fields[8]!=0 or fields[9]!=0:
        raise ValueError("monotonic or absent synchronized-clock evidence mismatch")
    if fields[10]!=4294967295 or fields[11]!=[1,0]:
        raise ValueError("owner clock uncertainty or provenance mismatch")
    if fields[13]!=ARGS or fields[14]!=0:
        raise ValueError("exact typed positional values or reserved facts mismatch")
    fp=fields[12]
    if type(fp) is not list or fp!=FINGERPRINT:
        raise ValueError("actual generated diagnostic fingerprint mismatch")
    return tuple(fp)

def audit(root: Path, expected=EXPECTED, *, records_per_segment: int=1) -> tuple[int,int]:
    if records_per_segment not in (1,16,64):
        raise ValueError("unsupported qualified fixture record count")
    if not root.is_dir() or root.is_symlink():
        raise ValueError("missing independently extracted filesystem")
    if set(p.name for p in root.iterdir()) != {"seed.txt",*expected}:
        raise ValueError("wrong typed EDLS finite physical slot set")
    seed=root/"seed.txt"
    if seed.is_symlink() or not seed.is_file() or seed.read_bytes()!=b"EDP1":
        raise ValueError("wrong explicit test provisioning seed")
    fingerprints: set[tuple[int, ...]] = set()
    for filename,(slot,generation) in expected.items():
        path=root/filename
        if not path.is_file() or path.is_symlink():
            raise ValueError("retained typed archive file invalid")
        data=path.read_bytes()
        if records_per_segment>1:
            header,frames=parse_sealed_segment(
                data,max_payload=1024,max_records=records_per_segment,
                max_segment_bytes=records_per_segment*256+56)
            if len(frames)!=records_per_segment:
                raise ValueError("incomplete sealed fixture batch")
        else:
            if not 65<=len(data)<=1200:
                raise ValueError("typed EDLS segment size invalid")
            header=parse_segment_header(data[:40])
            frames=tuple(scan_stream(BytesIO(data[40:]),max_payload=1024))
            if header.profile!=0 or len(frames)!=1 or not isinstance(frames[0],Frame):
                raise ValueError("missing complete CRC-valid canonical EDLG frame")
        if (header.slot,header.generation,header.incarnation)!=(
            slot,generation,7
        ) or header.device_identifier!=DEVICE:
            raise ValueError("typed EDLS header identity or generation mismatch")
        for index,packet in enumerate(frames):
            ticket=(generation-1)*records_per_segment+index+1
            fingerprints.add(verify_record(packet,ticket))
    if len(fingerprints)!=1:
        raise ValueError("diagnostic schema changed within physical archive")
    generations=sorted(g for (_,g) in expected.values())
    return (generations[0],generations[-1])

if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument("directory",type=Path)
    parser.add_argument("--older",type=int,default=3)
    parser.add_argument("--newer",type=int,default=4)
    parser.add_argument("--records-per-segment",type=int,choices=(1,16,64),default=1)
    args=parser.parse_args()
    if args.older<1 or args.newer!=args.older+1 or args.newer%3!=1:
        raise SystemExit("require consecutive generations with newest physical slot0")
    expected={"slot0":(0,args.newer),"slot2":(2,args.older)}
    try:
        oldest,newest=audit(args.directory,expected,records_per_segment=args.records_per_segment)
    except (ValueError,OSError) as exc:
        print(f"OFFLINE_TYPED_EDLS_AUDIT=FAIL reason={exc}",flush=True)
        raise SystemExit(1)
    first=(oldest-1)*args.records_per_segment+1
    last=newest*args.records_per_segment
    print(f"OFFLINE_TYPED_EDLS_AUDIT=PASS oldest={oldest} newest={newest} fields=15 "
          f"records_per_segment={args.records_per_segment} first={first} last={last}",flush=True)
