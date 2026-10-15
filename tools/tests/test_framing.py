from __future__ import annotations

import io
import unittest

from edp_logging.archive import SegmentHeaderError, parse_segment_header
from edp_logging.framing import (
    Frame, FrameCodec, FrameIssue, FrameStatus, crc32c, encode_frame, scan_stream,
)


class FramingTests(unittest.TestCase):

    def test_crc32c_known_vector(self) -> None:
        self.assertEqual(crc32c(b"123456789"), 0xE3069283)

    def test_valid_frame_and_exact_offset(self) -> None:
        framed = encode_frame(FrameCodec.Cbor, b"\x83\x01\x48")
        self.assertEqual(len(framed), 23)
        events = list(scan_stream(io.BytesIO(framed), base_offset=40))
        self.assertEqual(len(events), 1)
        self.assertIsInstance(events[0], Frame)
        self.assertEqual(events[0].offset, 40)
        self.assertFalse(events[0].recovered_candidate)
        self.assertEqual(events[0].payload, b"\x83\x01\x48")

    def test_truncated_and_corrupt_candidate_rejected(self) -> None:
        framed = bytearray(encode_frame(FrameCodec.Cbor, b"test"))
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed[:-1])))[0].status,
            FrameStatus.TruncatedFrame
        )
        framed[16] ^= 1
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed)))[0].status,
            FrameStatus.CrcMismatch
        )

    def test_invalid_flags_and_bounded_length(self) -> None:
        framed = bytearray(encode_frame(FrameCodec.Json, b"{}"))
        framed[6] = 1
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed)))[0].status,
            FrameStatus.UnsupportedFlags
        )
        framed[6] = 0
        framed[8:12] = (0xFFFFFFFF).to_bytes(4, "little")
        self.assertEqual(
            list(scan_stream(io.BytesIO(framed), max_payload=256))[0].status,
            FrameStatus.PayloadTooLarge
        )

    def test_forensic_resync_preserves_damage(self) -> None:
        first = encode_frame(FrameCodec.Cbor, b"first")
        second = encode_frame(FrameCodec.Cbor, b"second")
        damaged = first[:18] + bytes([first[18] ^ 1]) + first[19:]
        data = damaged + b"garbage" + second
        result = list(scan_stream(io.BytesIO(data), forensic=True, resync_limit=100))
        accepted = [x for x in result if isinstance(x, Frame)]
        self.assertEqual(len(accepted), 1)
        self.assertEqual(accepted[0].payload, b"second")
        self.assertTrue(accepted[0].recovered_candidate)
        self.assertEqual(accepted[0].offset, len(damaged) + 7)
        self.assertTrue(any(isinstance(x, FrameIssue) for x in result))

    def test_resync_budget_enforced(self) -> None:
        output = list(scan_stream(
            io.BytesIO(b"q" * 100),
            forensic=True,
            resync_limit=4,
        ))
        self.assertEqual(output[-1].status, FrameStatus.ResynchronizationLimit)

    def test_edls_header_crc_and_identity(self) -> None:
        header = bytearray(40)
        header[:4] = b"EDLS"
        header[4] = 1
        header[6:8] = (2).to_bytes(2, "little")
        header[8:16] = (7).to_bytes(8, "little")
        header[16:32] = bytes(range(16))
        header[32:36] = (9).to_bytes(4, "little")
        header[36:40] = crc32c(header[:36]).to_bytes(4, "little")
        parsed = parse_segment_header(bytes(header))
        self.assertEqual((parsed.slot, parsed.generation, parsed.incarnation), (2, 7, 9))
        header[16] ^= 0x40
        with self.assertRaises(SegmentHeaderError):
            parse_segment_header(bytes(header))


if __name__ == "__main__":
    unittest.main()
