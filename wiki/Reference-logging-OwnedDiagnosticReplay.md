# Reference — `src/logging/OwnedDiagnosticReplay.hpp`

**Classification:** internal source-compatible replay facade. [Reviewed source](https://github.com/ESPressio-Development-Platform/EDP-Logging/blob/73e39775eb0d4bd5398e479ae931aa42c47ab22a/src/logging/OwnedDiagnosticReplay.hpp).

This convenience overload uses the same [owner-qualified positional replay path](Reference-logging-OwnedDiagnosticPositionalReencoding) as the explicit-scratch overload. Its local stack scratch is sized by the caller's generated argument types.

| `OwnedArgumentReplayStatus` | Meaning |
| --- | --- |
| `Succeeded` (0) | The owner codec produced the complete positional value. |
| `CountMismatch` (1) | The source count differs from the exact generated argument list. |
| `SourceDescriptorRejected` (2) | Kind, availability or width checks failed while reconstructing a source position. |
| `SourceScalarRejected` (3) | The owner-qualified conversion rejected the source value. The historical status name applies to supported strong and bounded types as well. |
| `EncoderRejected` (4) | The canonical codec refused the representation. |

`OwnedArgumentReplayResult::Status` defaults to CountMismatch. `BytesWritten` records the codec's reported encoded size on success and is zero otherwise. `IsSuccessful()` tests for Succeeded. Consult the owner encoder's failure guarantees for destination bytes.

`EncodeOwnedDiagnosticArguments<TGeneratedMessageSchema, TCodec, TValues..., N, B, TByteOperationsProvider>(destination, source)` borrows the caller's destination span and an immutable owned source arena. `TGeneratedMessageSchema` supplies the ordered generated message contract. `TCodec` must be precisely EDP-Serialisation Cbor or Json. `TValues...` supplies the exact ordered value types and determines the local `OwnedPositionalScratch` layout. `N` and `B` are the source's descriptor count and byte capacity, and `TByteOperationsProvider` selects its borrowed Memory byte-operations provider. The facade delegates exactly once to the selected positional codec and maps its typed status. It retains no references after the call.

The call is synchronous and uses fixed local storage. Choose externally provisioned positional scratch where large signatures would exceed the selected worker's stack budget. Schema compatibility, historical translation provenance and sink delivery are separate contracts.
