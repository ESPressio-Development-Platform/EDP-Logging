"""Stream bounded EDP-Logging JSONL forensic evidence into conservative CSV.

This is deliberately a host-side *presentation* adapter. It neither changes
LogRecord wire contracts nor strengthens the source evidence's provenance.
Run: python -m edp_logging.export_csv [path|-] [--max-line-bytes N]
"""
from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Sequence, TextIO

# Exact stable columns. Never silently omit an unknown diagnostic field.
COLUMNS = (
    "kind", "offset", "length", "codec", "payloadLength", "forensicCandidate",
    "crc32c", "slot", "generation", "deviceIdentifierHex", "incarnation",
    "attemptSequence", "attemptSequenceWidth", "sequenceWidth",
    "monotonicNanoseconds", "synchronizedNanoseconds",
    "clockEra", "uncertaintyNanoseconds", "messageIdentifier",
    "typeIdentifierHex", "fieldIdentifiers", "parameterSchemaInterpreted",
    "logRecordCandidate", "rootTypeMatchedSuppliedIdentity", "rootTypeGoverned",
    "schemaArtifactSha256PinMatched", "historicalContractFamilyFingerprint",
    "historicalManifestTrustedPinMatched", "parameterKinds", "argumentCount",
    "recordError", "recordFactError", "schemaError", "historicalTemplateError",
    "status", "message", "observedDistinctTickets", "duplicateObservations",
    "firstObservedTicket", "lastObservedTicket", "unseenTicketIntervals",
    "partialCapturePossible", "lossCauseInferred", "relationship",
    "leftOffset", "rightOffset", "certainTotalOrder", "civilUtcInferred",
    "unobservedGapLossInferred", "historicalTemplate",
    "historicalTemplateSupplyingLanguage", "historicalTemplateFallbackUsed",
    "historicalTemplateInterpolated",
)


class CsvEvidenceError(ValueError):
    """Malformed or unbounded input evidence; never silently skip a bad line."""


def _safe_cell(value: object) -> str:
    if value is None:
        return ""
    if type(value) is bool:
        return "true" if value else "false"
    if type(value) is int:
        return str(value)
    if type(value) is float:
        raise CsvEvidenceError("floating-point evidence requires an explicitly typed decoder")
    if type(value) in (dict, list):
        text = json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    elif type(value) is str:
        text = value
    else:
        raise CsvEvidenceError("unsupported evidence value type")
    # CSV quoting does not prevent spreadsheet formula execution.
    if text.lstrip().startswith(("=", "+", "-", "@")) or text[:1] in ("\t", "\r", "\n"):
        text = "'" + text
    return text


def export_stream(
    input_stream: TextIO,
    output_stream: TextIO,
    *,
    max_line_bytes: int = 65536,
    max_rows: int = 100000,
) -> int:
    """Stream a fixed-schema CSV, returning the number of evidence rows.

    A failure after prior emitted lines is a failed partial CSV stream, never
    a committed export. Consumers MUST require successful process exit status.
    """
    if max_line_bytes < 1 or max_rows < 1:
        raise CsvEvidenceError("finite input bounds must be positive")
    writer = csv.DictWriter(output_stream, fieldnames=COLUMNS, lineterminator="\n")
    writer.writeheader()
    count = 0
    while True:
        # Bounded readline prevents allocating arbitrary unterminated input.
        line = input_stream.readline(max_line_bytes + 1)
        if not line:
            break
        line_number = count + 1
        if len(line.encode("utf-8")) > max_line_bytes or (
            len(line) == max_line_bytes + 1 and not line.endswith("\n")
        ):
            raise CsvEvidenceError(f"JSONL line {line_number} exceeds byte bound")
        if not line.strip():
            raise CsvEvidenceError(f"JSONL line {line_number} is blank")
        if count >= max_rows:
            raise CsvEvidenceError("finite CSV row ceiling exceeded")

        def unique_fields(items: list[tuple[str, object]]) -> dict[str, object]:
            result: dict[str, object] = {}
            for key, item in items:
                if key in result:
                    raise CsvEvidenceError(f"JSONL line {line_number} has duplicate key {key!r}")
                result[key] = item
            return result

        try:
            value = json.loads(line, object_pairs_hook=unique_fields)
        except (json.JSONDecodeError, UnicodeError) as error:
            raise CsvEvidenceError(f"invalid JSONL line {line_number}: {error}") from error
        if not isinstance(value, dict) or not isinstance(value.get("kind"), str) or not value["kind"]:
            raise CsvEvidenceError(f"JSONL line {line_number} lacks a string kind")
        extras = set(value) - set(COLUMNS)
        if extras:
            raise CsvEvidenceError(f"JSONL line {line_number} has unrecognised evidence fields: {sorted(extras)}")
        row = {key: _safe_cell(value[key]) for key in COLUMNS if key in value}
        writer.writerow(row)
        count += 1
    return count


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Export conservative, fixed-schema EDP-Logging forensic JSONL as CSV"
    )
    parser.add_argument("path", type=Path, nargs="?", default=Path("-"),
                        help="JSONL input path, or '-' for a pipe/standard input")
    parser.add_argument("--max-line-bytes", type=int, default=65536)
    parser.add_argument("--max-rows", type=int, default=100000)
    options = parser.parse_args(argv)
    try:
        if options.path == Path("-"):
            export_stream(sys.stdin, sys.stdout, max_line_bytes=options.max_line_bytes,
                          max_rows=options.max_rows)
        else:
            with options.path.open("r", encoding="utf-8", newline="") as source:
                export_stream(source, sys.stdout, max_line_bytes=options.max_line_bytes,
                              max_rows=options.max_rows)
    except (CsvEvidenceError, OSError, UnicodeError) as error:
        print(f"EDP_LOGGING_CSV_ERROR: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
