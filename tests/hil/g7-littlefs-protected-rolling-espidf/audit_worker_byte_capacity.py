#!/usr/bin/env python3
"""Verify native successor-boundary peaks and independently extracted sink bytes."""
import argparse
from pathlib import Path
import re

WITNESS = re.compile(r"^EDP_BYTE_CAPACITY\|PASS\|limit=([0-9]+)\|peak=([0-9]+)$", re.M)

def verify_capacity(transcript, limit, final_bytes):
    """Require one exact native witness consistent with the independent media."""
    matches = WITNESS.findall(transcript)
    if len(matches) != 1 or transcript.count("EDP_BYTE_CAPACITY|") != 1:
        raise ValueError("missing or ambiguous byte-capacity witness")
    observed_limit, peak = map(int, matches[0])
    if limit <= 0 or observed_limit != limit or not 40 < final_bytes <= peak <= limit:
        raise ValueError("byte capacity or observed peak violated")
    if "|HALT|" in transcript:
        raise ValueError("native failure cannot qualify capacity")
    return peak

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("transcript", type=Path)
    parser.add_argument("--limit", required=True, type=int)
    args = parser.parse_args()
    from audit_batch_worker_media import audit
    text = args.transcript.read_text("utf-8")
    audit(args.root, text)
    # Existing media audit proves the exact file set and parses the ACK generation;
    # the separate official NVS audit proves the 40-byte committed ACK value.
    total = 40 + sum(path.stat().st_size for path in args.root.glob("slot*"))
    peak = verify_capacity(text, args.limit, total)
    print(f"WORKER_BYTE_CAPACITY_AUDIT=PASS limit={args.limit} peak={peak} final={total}")

if __name__ == "__main__":
    main()
