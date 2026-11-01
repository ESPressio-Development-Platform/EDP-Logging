# Compiler and conditional compilation

This implementation requires C++20. EDP_LOG_WHEN_ENABLED provides a compile-time threshold gate and should eliminate disabled argument evaluation entirely. Application and test build variants must select the same canonical EDP owner dependencies and SDK configuration when comparing code size or timing.

## Boundaries

G11 site-level and whole-application compiler proofs are separate. Logging opt-in headers introduce dependencies only when imported. No platform-wide enabled logging severity or unbounded global queue is implied.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
