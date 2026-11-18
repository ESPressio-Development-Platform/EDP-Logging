# Bounded EDLS FileStorage archive inspection (partial G6/G7)

The optional [EdlsArchiveInspection.hpp](../src/logging/EdlsArchiveInspection.hpp)
inspects existing EDLS/EDLG fixed slots via EDP-Persistence's **abstract
FileStorage**. It introduces no native filesystem or new byte encoding.
Bootstrap supplies the already validated 2–64 FilePathView slots, a stable
readable FileStorage provider, exclusive mutation control, and a per-frame
payload bound of 1–4096 bytes.

The scanner calls the existing Logging EDLS/EDLG CRC readers, stopping at the
first torn/invalid frame and preserving only the contiguous verified prefix.
An invalid header, unreadable slot, wrong physical slot, duplicate generation
or mixed 16-byte DeviceIdentifier FAILS CLOSED as Ambiguous. Generation
exhaustion forbids wrap. NewestSlot and NextGeneration are READ-ONLY
observations and **never authorise a write or durable generation allocation**.
A missing path and a zero-byte file both report `Empty`. Publication
profiles that require a physically absent path check absence separately.
A valid header followed by a damaged tail can still supply generation
evidence. Generation gaps do not prevent the read-only scanner from
calculating a unique next generation. Protected rolling and strict
acknowledgement enforce their own continuity and cleanliness rules.
The application must prevent concurrent mutation during inspection.

This is NOT the production EDLS append/rotate writer or a trustworthy archival
manifest. No external-power-cut, retention, auth, signing or multi-segment
mutation guarantee is made. The FileStorage owner provides the real mutation
atomicity and PowerLoss guarantees when its concrete binding qualifies.

Tests exercise clean and corrupt EDLS/EDLG slots, verified prefix, duplicate
generations, mixed device identity, generation wrap and provider failures
under GCC and Clang. Versions remain 0.1.0.


## V2 strongly typed read outcome (1 November 2026)

The internal `Detail::ReadExactEdlsBytes` now returns an explicit
`EdlsExactReadResult`: `Succeeded=0`, `ProviderFailure=1`,
`IncompleteRead=2`. A provider that reports success but transfers fewer
bytes is not a success and remains fail-closed. Archive scanning and the
successor/rotation readback still reject either failure; no original
integrity check, protected-history provenance or byte requirement is
weakened. `EdlsSlotCondition` and `EdlsArchiveCondition` now explicitly
preserve their previous numeric enumerators. The focused native test verifies
exact, short and provider-rejected outcomes separately. These are C++ source
result contracts, **not** changes to the EDLS on-storage encoding.
