"""Bounded offline forensic CSV export, no trust upgrade or formula execution."""
from __future__ import annotations

import csv
import io
import json
import unittest
from unittest.mock import patch

from edp_logging.export_csv import CsvEvidenceError, COLUMNS, export_stream, main


class ExportCsvTests(unittest.TestCase):
    def test_preserves_evidence_and_explicit_untrusted_values(self) -> None:
        events = [
            {"kind": "frame", "offset": 0, "codec": "Cbor", "rootTypeGoverned": False,
             "deviceIdentifierHex": "AA0011", "attemptSequence": 3,
             "messageIdentifier": [1, 2, 3], "forensicCandidate": True},
            {"kind": "frame-error", "offset": 24, "status": "CrcMismatch"},
            {"kind": "temporal-comparison", "leftOffset": 0, "rightOffset": 48,
             "relationship": "Incomparable", "certainTotalOrder": False,
             "civilUtcInferred": False, "unobservedGapLossInferred": False},
            {"kind": "attempt-evidence", "unseenTicketIntervals": [[2, 2]],
             "lossCauseInferred": False, "partialCapturePossible": True},
        ]
        source = io.StringIO("".join(json.dumps(event)+"\n" for event in events))
        destination = io.StringIO()
        self.assertEqual(export_stream(source, destination), 4)
        data = list(csv.DictReader(io.StringIO(destination.getvalue())))
        self.assertEqual(len(data), 4)
        self.assertEqual(data[0]["rootTypeGoverned"], "false")
        self.assertEqual(data[0]["messageIdentifier"], "[1,2,3]")
        self.assertEqual(data[2]["relationship"], "Incomparable")
        self.assertEqual(data[2]["certainTotalOrder"], "false")
        self.assertEqual(data[3]["unseenTicketIntervals"], "[[2,2]]")
        self.assertEqual(data[3]["lossCauseInferred"], "false")
        self.assertEqual(tuple(data[0].keys()), COLUMNS)

    def test_csv_injection_guards_whitespace_and_formula_prefixes(self) -> None:
        injected = ["=cmd()", "  +SUM(1,2)", "@evil", "-42script", "\t=HYPERLINK(1)", "normal"]
        content = "".join(json.dumps({"kind":"frame-error","message":v})+"\n" for v in injected)
        output = io.StringIO()
        self.assertEqual(export_stream(io.StringIO(content),output),len(injected))
        values = [r["message"] for r in csv.DictReader(io.StringIO(output.getvalue()))]
        self.assertEqual(values, ["'"+v for v in injected[:-1]]+["normal"])

    def test_finite_bounds_fail_not_silently_skip(self) -> None:
        for payload, kwargs in (
            ('{"kind":"frame"}\n', {"max_line_bytes":2}),
            ('{"kind":"frame"}\n{"kind":"frame"}\n', {"max_rows":1}),
            ('{}\n', {}),
            ('null\n', {}),
            ('{"kind":9}\n', {}),
            ('not-json\n', {}),
            ('\n', {}),
            ('{"kind":"frame","uncertaintyNanoseconds":1.25}\n', {}),
            ('{"kind":"frame","kind":"frame-error"}\n', {}),
            ('{"kind":"frame","unknownEvidence":"important"}\n', {}),
        ):
            with self.subTest(payload=payload,kwargs=kwargs):
                with self.assertRaises(CsvEvidenceError):
                    export_stream(io.StringIO(payload),io.StringIO(),**kwargs)
        with self.assertRaises(CsvEvidenceError):
            export_stream(io.StringIO(),io.StringIO(),max_rows=0)

    def test_read_call_is_bounded_for_malicious_endless_line(self) -> None:
        class NonSeekable:
            calls = 0
            def readline(self, maximum: int) -> str:
                self.calls += 1
                self.maximum = maximum
                return "x" * maximum
        stream = NonSeekable()
        with self.assertRaisesRegex(CsvEvidenceError, "exceeds byte bound"):
            export_stream(stream, io.StringIO(), max_line_bytes=64)
        self.assertEqual(stream.calls,1)
        self.assertEqual(stream.maximum,65)

    def test_cli_processes_stream_without_closing_stdin(self) -> None:
        input_stream=io.StringIO('{"kind":"frame","offset":0}\n')
        output_stream=io.StringIO()
        with patch("sys.stdin",input_stream), patch("sys.stdout",output_stream):
            self.assertEqual(main(["-","--max-rows","3"]),0)
        self.assertFalse(input_stream.closed)
        self.assertEqual(list(csv.DictReader(io.StringIO(output_stream.getvalue())))[0]["offset"],"0")


    def test_cli_transaction_does_not_replace_destination_on_bad_input(self) -> None:
        import contextlib
        import tempfile
        from pathlib import Path

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "events.jsonl"
            committed = root / "analysis.csv"
            committed.write_text("PREVIOUS_COMMITTED_EXPORT\n", encoding="utf-8")
            source.write_text('{"kind":"frame","offset":0}\ninvalid-json\n', encoding="utf-8")
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main([str(source), "--output", str(committed)]), 1)
            self.assertEqual(committed.read_text(encoding="utf-8"),
                             "PREVIOUS_COMMITTED_EXPORT\n")
            self.assertEqual(list(root.glob(".analysis.csv.*.tmp")), [])

            source.write_text('{"kind":"frame","offset":0}\n', encoding="utf-8")
            self.assertEqual(main([str(source), "--output", str(committed)]), 0)
            with committed.open("r", encoding="utf-8", newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["offset"], "0")
            self.assertEqual(list(root.glob(".analysis.csv.*.tmp")), [])

            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main([str(source), "--output", str(source)]), 1)
            self.assertEqual(source.read_text(encoding="utf-8"),
                             '{"kind":"frame","offset":0}\n')

    def test_cli_transaction_stream_failure_and_capacity_preserve_old_file(self) -> None:
        import contextlib
        import tempfile
        from pathlib import Path

        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "evidence.csv"
            target.write_text("OLD\n", encoding="utf-8")
            stdin = io.StringIO('{"kind":"frame","offset":1}\n{"kind":"frame","offset":2}\n')
            with patch("sys.stdin", stdin):
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(main(["-", "--output", str(target), "--max-rows", "1"]), 1)
            self.assertFalse(stdin.closed)
            self.assertEqual(target.read_text(encoding="utf-8"), "OLD\n")
            self.assertEqual(list(Path(directory).glob(".evidence.csv.*.tmp")), [])

            stdin = io.StringIO('{"kind":"frame","offset":1}\n')
            with patch("sys.stdin", stdin):
                self.assertEqual(main(["-", "--output", str(target)]), 0)
            with target.open("r", encoding="utf-8", newline="") as stream:
                self.assertEqual(next(csv.DictReader(stream))["offset"], "1")
            self.assertFalse(stdin.closed)

if __name__ == "__main__":
    unittest.main()
