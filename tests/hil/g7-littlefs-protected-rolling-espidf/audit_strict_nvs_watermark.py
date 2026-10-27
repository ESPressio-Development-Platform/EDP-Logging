#!/usr/bin/env python3
"""Read-only independent ESP-IDF NVS media audit of EDLS PowerLoss ACK.

Must be run on the independently extracted *physical* 0x9000/0x6000
partition or full 2MB backup, never on simulated firmware telemetry.
Reuses Espressif's installed NVS parser for actual page/entry metadata and
the existing EDP-Logging EDLS header/CRC decoder. No flash mutation.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

from edp_logging.archive import parse_segment_header


def audit(media: bytes, namespace: str, expected_ack: int,
          expected_incarnation: int, nvs_parser_dir: Path) -> None:
    sys.path.insert(0, str(nvs_parser_dir))
    from nvs_parser import NVS_Partition

    if len(media)==2097152:
        media=media[0x9000:0xF000]
    if len(media)!=0x6000:
        raise ValueError(f"NVS partition wrong size {len(media)}")

    part=NVS_Partition("physical-nvs",bytearray(media))
    entries=[]
    for page in part.pages:
        if page.is_empty:
            continue
        h=page.header
        if h["status"] not in ("Active","Full"):
            raise ValueError(f"unclean NVS page status: {h}")
        if h["crc"]["original"]!=h["crc"]["computed"]:
            raise ValueError("NVS page header CRC failed")
        for entry in page.entries:
            if entry.state!="Written":
                continue
            c=entry.metadata["crc"]
            if c["original"]!=c["computed"]:
                raise ValueError("NVS entry metadata CRC failed")
            if entry.metadata["span"]>1 and c["data_original"]!=c["data_computed"]:
                raise ValueError("NVS blob-data CRC failed")
            entries.append(entry)

    mappings=[e for e in entries if e.metadata["namespace"]==0 and
              e.key==namespace and e.metadata["type"]=="uint8_t"]
    if len(mappings)!=1:
        raise ValueError(f"NVS namespace {namespace!r} not uniquely present")
    ns=mappings[0].data["value"]
    key_entries=[e for e in entries if
                 e.metadata["namespace"]==ns and e.key=="edlsack"]
    indices=[e for e in key_entries if e.metadata["type"]=="blob_index"]
    chunks=[e for e in key_entries if e.metadata["type"]=="blob_data"]
    # The factory/partition fixture stores exactly one active 40B latest ACK
    # as one data chunk + one blob index. Multiple live values are ambiguous.
    if len(indices)!=1 or len(chunks)!=1:
        raise ValueError(
            f"expected one live NVS blob index and chunk, "
            f"found indices={len(indices)} chunks={len(chunks)}")
    idx=indices[0]
    chunk=chunks[0]
    if idx.data["size"]!=40 or idx.data["chunk_count"]!=1 or (
        chunk.data["size"]!=40 or len(chunk.children)!=2
    ):
        raise ValueError("NVS EDLS watermark blob size or chunk structure invalid")
    payload=b"".join(bytes(child.raw) for child in chunk.children)[:40]
    h=parse_segment_header(payload)
    if (h.slot,h.generation,h.incarnation,h.device_identifier)!=(
        0,expected_ack+1,expected_incarnation,bytes([0xA6])*16
    ):
        raise ValueError(
            f"NVS watermark semantic mismatch: {h}, "
            f"expected ack={expected_ack} inc={expected_incarnation}")
    print(
        f"OFFLINE_REAL_NVS_WATERMARK_AUDIT=PASS "
        f"ack={h.generation-1} last_incarnation={h.incarnation} "
        f"ns={namespace} page_crc=PASS entry_crc=PASS blob_crc=PASS"
    )


def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument("image",type=Path)
    ap.add_argument("--namespace",required=True)
    ap.add_argument("--ack",type=int,required=True)
    ap.add_argument("--incarnation",type=int,required=True)
    ap.add_argument("--nvs-parser-dir",type=Path,required=True)
    args=ap.parse_args()
    audit(args.image.read_bytes(),args.namespace,args.ack,args.incarnation,
          args.nvs_parser_dir)

if __name__=="__main__":
    try:
        main()
    except (ValueError,OSError) as exc:
        print(f"OFFLINE_REAL_NVS_WATERMARK_AUDIT=FAIL {exc}",file=sys.stderr)
        raise SystemExit(1)
