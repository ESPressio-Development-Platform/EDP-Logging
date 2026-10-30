
## Per-incarnation bounded ticket evidence (2026-10-17)

`--validate-record --expected-root-type-hex HEX16 --analyse-attempts`
selects bounded device/incarnation ticket observations; options
`--max-analysis-records` and `--max-analysis-groups` limit memory
and return typed errors when exhausted. A reported unseen interval
between observed tickets is only a gap in the forensic capture,
not proof of loss or its cause. A matching user-provided root pin
does not authenticate producer identity or historical schema.

## Cross-device time comparisons

`--validate-record --expected-root-type-hex HEX16
--compare-record-times` performs finite, explicit pairwise comparisons
using the existing EDP-Clock uncertainty-aware evidence rules.
`--max-comparison-pairs` bounds work/output and refuses a partial
summary on overflow. Incomparable means no trusted common era/
synchronization projection; Overlaps means the supported intervals
overlap, not an arbitrary total-order tie breaker. Neither outcome
permits UTC inference or lost-message conclusions.


Sealed batch inspection: archive.SegmentHeader adds profile=0. parse_sealed_segment checks bounded full body, count, length and whole CRC via existing framing reader; default1MiB/64-record limit. CLI --archive validates seal before any records are emitted, even with --forensic. archive_recovery.inspect_slot never promotes a sealed prefix. NVS evidence remains necessary for ACK.
