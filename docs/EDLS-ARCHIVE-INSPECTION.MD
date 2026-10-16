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
A damaged tail is never appended behind implicitly; concurrency mutation
while scanning must be prevented by the application.

This is NOT the production EDLS append/rotate writer or a trustworthy archival
manifest. No external-power-cut, retention, auth, signing or multi-segment
mutation guarantee is made. The FileStorage owner provides the real mutation
atomicity and PowerLoss guarantees when its concrete binding qualifies.

Tests exercise clean and corrupt EDLS/EDLG slots, verified prefix, duplicate
generations, mixed device identity, generation wrap and provider failures
under GCC and Clang. Versions remain 0.1.0.
