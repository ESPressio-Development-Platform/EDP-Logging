# EDP-Logging Wiki

Status: partial Logging foundation on `feature/logging-v1`, package version 0.1.0.

Implemented: [Severity](Reference-logging-Severity), [Attempt Sequence](Reference-logging-AttemptSequence), the [partial bounded admission bridge](Reference-logging-BoundedAdmission), the [composed fixed-slot wake coordinator](Reference-logging-AdmissionWakeCoordinator), the [finite admission shutdown/drain](Reference-logging-AdmissionLifecycleCoordinator), the [tracked RAII lease and strict drain](Reference-logging-TrackedAdmissionCoordinator), the [optional G2 temporal evidence qualifier](Reference-logging-LogTemporalEvidence), and the [governed scalar/bounded diagnostic argument adapter](Reference-logging-DiagnosticArgumentEncoding), and [Python host framing](Host-Decoder). The [Architecture](../docs/ARCHITECTURE.MD) describes the actual source boundary; the complete approved architecture remains governed by Policy/main.

No functional Logging producer/sinks exist yet. The existing C++20 foundation suites passed GCC and Clang on RPI400 on 2026-10-16; the Python host suite (20 tests including exact-byte schema-pin regression) also passed. Firmware/physical storage and end-to-end runtime validation remain outstanding. The [Host Decoder](Host-Decoder) page describes the source-level schema pin verification boundary.

The [optional historical diagnostic schema host bridge](Tooling-edp-logging-historical-diagnostic) links pinned Localisation archived ContractFamilies to the offline decoder without a new embedded library dependency.

[System Identity generated diagnostic arguments](Reference-logging-SystemIdentityDiagnosticArguments) extend the optional owner-certified G3 path without adding new direct dependencies.
