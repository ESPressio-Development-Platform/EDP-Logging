# Forensic JSONL-to-CSV export (partial Logging G8/G9)

**Scope:** Host-only, standard-library Python 3. No MCU changes, on-wire
format changes, new Logging task, Mesh dependency or trusted-schema claim.

The command `python3 -m edp_logging.export_csv` converts existing
`edp_logging` JSON Lines events into a fixed-column CSV suitable for
offline inspection. It preserves *reported evidence*, including frame
errors, record identity, device/incarnation, ticket observations,
uncertainty-aware temporal relationships and historical template fields.
It does not infer lost messages, UTC, issuer identity or chronology when
the source evidence does not establish those things.

## Examples

From an existing saved capture:

```bash
cd EDP-Logging/tools
python3 -m edp_logging capture.edlg --forensic --validate-record \
  --expected-root-type-hex 0000010000000001 > capture.jsonl
python3 -m edp_logging.export_csv capture.jsonl > capture.csv
```

From a binary-only producer stream, with explicit pipe-failure handling:

```bash
cd EDP-Logging/tools
set -o pipefail
cat capture.edlg | python3 -u -m edp_logging - --forensic \
  --validate-record --expected-root-type-hex 0000010000000001 |
  python3 -u -m edp_logging.export_csv - > capture.csv
```

The root ID is **an equality pin supplied by the caller**, not
authentication. When the input contains damaged frames the source
decoder exits nonzero even if some forensic candidates were recovered;
a shell script MUST respect `pipefail` and regard a partially written
CSV as an **unsuccessful export**, not durable evidence. A production
application should write to a temporary file and only rename it
after all programs in the pipeline exit successfully.

`--max-line-bytes` (default 65536) bounds each actual input read in
characters and UTF-8 bytes; an unterminated oversized JSONL line is
rejected **before arbitrary buffering**. `--max-rows` (default
100000) sets a finite record ceiling. Exceeding either limit,
malformed JSON, duplicate object keys, unknown field names and
unexpected floating-point evidence values yield nonzero exit.
Neither incomplete nor unknown fields are silently skipped.

CSV cells use standard quoting, with a leading apostrophe for
user-controlled strings beginning with a spreadsheet formula sigil
(including sigils preceded by leading whitespace). Booleans are
rendered `true`/`false`; structured JSON values are encoded as
compact JSON cells, preserving the boundaries of arrays such as
`unseenTicketIntervals`. Unreported fields are blank, **never
synthesised**. The fixed column set intentionally requires a reviewed
update if the source CLI begins emitting new evidence properties.

## Qualification

`PYTHONPATH=tools python3 -m unittest tools.tests.test_export_csv`
tests bounded reads, malformed/duplicate/unknown fields, formula
injection handling, chronological uncertainty *as carried in source
events*, and nonseekable stdin. This standalone exporter does not
establish full historical Localisation trust, a complete LogRecord
typed serializer, or on-device PowerLoss qualification.
