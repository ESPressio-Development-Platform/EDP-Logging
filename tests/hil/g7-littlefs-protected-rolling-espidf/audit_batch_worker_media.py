#!/usr/bin/env python3
"""Actual sealed media and independently captured source coordinates; no repair."""
from pathlib import Path
import re
from edp_logging.archive import parse_sealed_segment
from edp_logging.record_structure import inspect_typed_envelope
from edp_logging.framing import FrameCodec
from audit_typed_extracted_edls import verify_record, DEVICE

PASS = re.compile(r"EDP_WORKER\|PASS\|scheduled_records=(2000|6000|30000)\|producers=2\|target_rps=100\|[^\n]*\|ack_segment=(\d+)$", re.M)
ORIGINAL = re.compile(r"^EDP_WORKER\|ORIGINAL\|ticket=(\d+)\|monotonic_ns=(\d+)$", re.M)

CUT_PASS = re.compile(r"^EDP_WORKER\|POWER_CUT_RECOVERED\|ack_segment=2\|records=128$", re.M)

CAPACITY_SAFE = re.compile(
    r"^EDP_BND\|MANAGED_CAPACITY_SAFE\|ack=2\|pending_records=([1-9][0-9]*)"
    r"\|native_replace=(9|12)\|durable=128\|free_bytes=([0-9]+)$", re.M)

def receipt(text, power_cut=False, capacity_cut=False):
    if power_cut and capacity_cut:
        raise ValueError("ambiguous cut audit mode")
    if power_cut or capacity_cut:
        if len(CUT_PASS.findall(text)) != 1 or "EDP_WORKER|PASS|" in text:
            raise ValueError("missing or ambiguous managed-cut receipt")
        if capacity_cut:
            witnesses = list(CAPACITY_SAFE.finditer(text))
            if (len(witnesses) != 1 or int(witnesses[0].group(1)) > 64 or
                    int(witnesses[0].group(3)) > 8192 or
                    "EDP_BND|SEALED_WRITE_ACTIVITY|" in text):
                raise ValueError("unqualified capacity refusal evidence")
        pairs = ORIGINAL.findall(text)
        coordinates = {int(ticket): int(tick) for ticket, tick in pairs}
        trigger = text.find("EDP_BND|MANAGED_CAPACITY_SAFE|ack=2" if capacity_cut else
                            "EDP_BND|SEALED_WRITE_ACTIVITY|gen=3|nvs_ack=2")
        if len(pairs) != 128 or len(coordinates) != 128 or trigger < 0 or CUT_PASS.search(text).start() <= trigger:
            raise ValueError("missing original managed source evidence")
        if any(not 1 <= ticket <= 160 or tick <= 0 for ticket, tick in coordinates.items()):
            raise ValueError("invalid original managed pressure source")
        if len(ORIGINAL.findall(text[:trigger])) != 128:
            raise ValueError("source evidence was not captured before cut")
        return 2, coordinates
    matches=PASS.findall(text)
    if len(matches)!=1 or int(matches[0][1])<2:
        raise ValueError("missing or ambiguous final worker receipt")
    if int(matches[0][0]) == 30000 and "|latency_scope=last_6000|latency_samples=6000|" not in PASS.search(text).group(0):
        raise ValueError("long profile must declare bounded quantile scope")
    pairs=ORIGINAL.findall(text)
    coordinates={int(ticket):int(tick) for ticket,tick in pairs}
    if not pairs or len(coordinates)!=len(pairs):
        raise ValueError("missing or duplicate original source coordinate")
    if any(not 1<=ticket<=int(matches[0][0])+161 or tick<=0 for ticket,tick in coordinates.items()):
        raise ValueError("invalid bounded original source")
    return int(matches[0][1]),coordinates

def audit(root: Path, transcript: str, power_cut=False, capacity_cut=False):
    ack,coordinates=receipt(transcript, power_cut, capacity_cut)
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
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--power-cut", action="store_true")
    modes.add_argument("--capacity-cut", action="store_true")
    args=parser.parse_args()
    try:
        ack,records=audit(args.directory,args.transcript.read_text("utf-8"),args.power_cut,args.capacity_cut)
    except (OSError,ValueError,KeyError,TypeError) as exc:
        print(f"OFFLINE_BATCH_WORKER_MEDIA=FAIL reason={exc}")
        raise SystemExit(1)
    print(f"OFFLINE_BATCH_WORKER_MEDIA=PASS ack={ack} original_records={records} fields=15")
