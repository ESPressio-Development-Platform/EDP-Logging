# Tooling Reference

The maintained Python standard-library offline toolchain is separate from the ESP32 runtime. It parses, validates, filters and exports complete EDLG/EDLS records and canonical diagnostic source evidence; it does not bypass CRC, typed source qualification or the storage provider.

## Source modules

- `tools/edp_logging/__init__.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/__main__.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/analysis.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/archive.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/archive_recovery.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/cli.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/export_csv.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/framing.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/historical_schema.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/record_facts.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/record_structure.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.
- `tools/edp_logging/schema.py`: maintained host-side helper under the Logging tooling package; inspect its tests and executable CLI for exact arguments and error semantics.

## Qualification and failure boundaries

Use the [Tools reference](Tools-edp-logging) and [Host decoder](Host-Decoder) for operator workflows. A verified decoder only establishes integrity of the bytes actually presented, not that a native Flash provider committed those bytes before a power loss.
