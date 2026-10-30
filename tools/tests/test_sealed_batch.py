"""Sealed batch golden parity, suffix loss and bounded offline inspection."""
import tempfile
import unittest
from pathlib import Path

from edp_logging.archive import (
    SegmentHeader, SegmentHeaderError, encode_segment_header,
    parse_segment_header, parse_sealed_segment,
)
from edp_logging.archive_recovery import inspect_slot, SlotState
from edp_logging.framing import FrameCodec, crc32c, encode_frame


def sealed(records=3):
    """Independent golden construction from the documented EDLS/EDLC layout."""
    header = encode_segment_header(SegmentHeader(0, 7, b"\xa6"*16, 9, 1))
    frame = encode_frame(FrameCodec.Cbor, b"\x82\x01\x02")
    body = frame * records
    prefix = header + body + b"EDLC" + records.to_bytes(4, "little") + len(body).to_bytes(4, "little")
    return prefix + crc32c(prefix).to_bytes(4, "little")


class SealedBatchTests(unittest.TestCase):
    def test_complete_batch_and_header_profile(self):
        data = sealed()
        header, frames = parse_sealed_segment(data)
        self.assertEqual(header.profile, 1)
        self.assertEqual(parse_segment_header(data[:40]), header)
        self.assertEqual(len(frames), 3)
        self.assertEqual([f.payload for f in frames], [b"\x82\x01\x02"]*3)

    def test_all_truncated_prefixes_fail_closed(self):
        data = sealed()
        for end in range(len(data)):
            with self.subTest(end=end), self.assertRaises(SegmentHeaderError):
                parse_sealed_segment(data[:end])

    def test_complete_record_suffix_loss_and_valid_crc_wrong_count(self):
        data = sealed()
        # Remove one complete framed record while retaining the original seal.
        lost = data[:40+46] + data[-16:]
        with self.assertRaises(SegmentHeaderError):
            parse_sealed_segment(lost)
        # Recompute all checksums around a wrong count to test count validation itself.
        wrong = bytearray(data)
        wrong[-12:-8] = (2).to_bytes(4, "little")
        wrong[-4:] = crc32c(wrong[:-4]).to_bytes(4, "little")
        with self.assertRaises(SegmentHeaderError):
            parse_sealed_segment(bytes(wrong))

    def test_bounds_and_no_partial_slot_acceptance(self):
        data = sealed()
        for kwargs in ({"max_records": 2}, {"max_segment_bytes": 76}, {"max_payload": 2}):
            with self.assertRaises(SegmentHeaderError):
                parse_sealed_segment(data, **kwargs)
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder, "slot0")
            path.write_bytes(data)
            self.assertEqual(inspect_slot(path, 0).verified_frames, 3)
            path.write_bytes(data[:40+46])  # complete, CRC-valid two-record prefix
            result = inspect_slot(path, 0)
            self.assertEqual(result.state, SlotState.DamagedTail)
            self.assertEqual(result.verified_frames, 0)

    def test_cli_never_promotes_an_unsealed_prefix(self):
        import contextlib
        import io
        import json
        from edp_logging.cli import main
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder, "segment")
            path.write_bytes(sealed())
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(main([str(path), "--archive"]), 0)
            rows = [json.loads(line) for line in output.getvalue().splitlines()]
            self.assertTrue(rows[0]["sealed"])
            self.assertEqual(sum(r["kind"] == "frame" for r in rows), 3)
            path.write_bytes(sealed()[:40+46])
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(main([str(path), "--archive", "--forensic"]), 2)
            self.assertNotIn('"kind": "frame"', output.getvalue())

    def test_unknown_profile_and_corruption(self):
        data = bytearray(sealed())
        for offset in range(len(data)):
            changed = data.copy()
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(SegmentHeaderError):
                parse_sealed_segment(bytes(changed))
        with self.assertRaises(SegmentHeaderError):
            encode_segment_header(SegmentHeader(0, 1, b"\xa6"*16, 7, 2))


if __name__ == "__main__":
    unittest.main()
