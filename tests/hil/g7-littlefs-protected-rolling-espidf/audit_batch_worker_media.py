#!/usr/bin/env python3
"""Actual sealed media and independently captured source coordinates; no repair."""
from pathlib import Path
import re
from edp_logging.archive import parse_sealed_segment
from edp_logging.record_structure import inspect_typed_envelope
from edp_logging.framing import FrameCodec
from audit_typed_extracted_edls import verify_record, DEVICE

PASS = re.compile(r"EDP_WORKER\|PASS\|scheduled_records=(2000|6000)\|producers=2\|target_rps=100\|[^\n]*\|ack_segment=(\d+)$", re.M)
ORIGINAL = re.compile(r"^EDP_WORKER\|ORIGINAL\|ticket=(\d+)\|monotonic_ns=(\d+)$", re.M)

def receipt(text):
    matches=PASS.findall(text)
    if len(matches)!=1 or int(matches[0][1])<2:
        raise ValueError("missing or ambiguous final worker receipt")
    pairs=ORIGINAL.findall(text)
    coordinates={int(ticket):int(tick) for ticket,tick in pairs}
    if not pairs or len(coordinates)!=len(pairs):
        raise ValueError("missing or duplicate original source coordinate")
    if any(not 1<=ticket<=int(matches[0][0])+161 or tick<=0 for ticket,tick in coordinates.items()):
        raise ValueError("invalid bounded original source")
    return int(matches[0][1]),coordinates

def audit(root: Path, transcript: str):
    ack,coordinates=receipt(transcript)
    expected={f"slot{(g-1)%3}":g for g in (ack-1,ack)}
    if not root.is_dir() or root.is_symlink() or {p.name for p in root.iterdir()}!={"seed.txt",*expected}:
        raise ValueError("wrong physical worker file set")
    seed=root/"seed.txt"
    if seed.is_symlink() or not seed.is_file() or seed.read_bytes()!=b"EDP1":
        raise ValueError("invalid original provisioning seed")
    seen=set()
    for name,generation in expected.items():
        path=root/name
        if path.is_symlink() or not path.is_file():
            raise ValueError("invalid physical segment")
        header,frames=parse_sealed_segment(path.read_bytes(),max_payload=1024,
            max_records=64,max_segment_bytes=16440)
        if (header.slot,header.generation,header.incarnation,header.device_identifier)!=(
            (generation-1)%3,generation,7,DEVICE):
            raise ValueError("wrong protected identity/generation")
        if not 1<=len(frames)<=64:
            raise ValueError("invalid partial/full segment count")
        for frame in frames:
            envelope=inspect_typed_envelope(frame.payload,FrameCodec.Cbor)
            ticket=envelope.fields[4]
            if ticket not in coordinates or ticket in seen:
                raise ValueError("unknown or duplicate retained source record")
            verify_record(frame,ticket,monotonic_ns=coordinates[ticket])
            seen.add(ticket)
    if seen!=set(coordinates):
        raise ValueError("missing retained original record")
    return ack,len(seen)

if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument("directory",type=Path)
    parser.add_argument("transcript",type=Path)
    args=parser.parse_args()
    try:
        ack,records=audit(args.directory,args.transcript.read_text("utf-8"))
    except (OSError,ValueError,KeyError,TypeError) as exc:
        print(f"OFFLINE_BATCH_WORKER_MEDIA=FAIL reason={exc}")
        raise SystemExit(1)
    print(f"OFFLINE_BATCH_WORKER_MEDIA=PASS ack={ack} original_records={records} fields=15")
