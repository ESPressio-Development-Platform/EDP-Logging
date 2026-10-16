# EDP-Logging host G5 historical diagnostic bridge

Optional offline `edp_logging.historical_schema.load_archived_diagnostic_schema` and CLI flags `--historical-archive`, `--contract-family-fingerprint`, optional `--contract-family-digest` and external `--trusted-archive-manifest-sha256`. Uses EDP-Localisation's existing historical family verifier/EDPL parser, then Logging's existing generated diagnostic schema parser. It never derives the translation-sensitive historical family from Field12's translation-independent diagnostic schema fingerprint.

Outputs retain integrity vs independent manifest provenance distinction; untrusted archives do NOT count as trusted source identity. The governed root TypeIdentifier remains **unallocated** and G4 remains open; this is host forensic argument interpretation, not authentic LogRecord verification or an embedded runtime decoder.

[Detailed G5 host bridge contract](../docs/HISTORICAL-DIAGNOSTIC-SCHEMA-BRIDGE.MD) • [Host decoder](../docs/HOST-DECODER.MD).

Optional `--historical-language` reports the selected **raw archived message template**, original supplying/fallback language and `historicalTemplateInterpolated=false`. Its `--max-historical-template-bytes` bound is checked by the existing Localisation grammar owner. Typed value formatting/redaction and full LogRecord authority remain out of scope.
