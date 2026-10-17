
## Per-incarnation bounded ticket evidence (2026-10-17)

`--validate-record --expected-root-type-hex HEX16 --analyse-attempts`
selects bounded device/incarnation ticket observations; options
`--max-analysis-records` and `--max-analysis-groups` limit memory
and return typed errors when exhausted. A reported unseen interval
between observed tickets is only a gap in the forensic capture,
not proof of loss or its cause. A matching user-provided root pin
does not authenticate producer identity or historical schema.
