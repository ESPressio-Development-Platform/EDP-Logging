# logging/OwnedDiagnosticPositionalReencoding.hpp

**Classification:** optional PUBLIC COMPOSITION API (generated-source to canonical positional serialization), with internal private reconstruction helpers. **No** EDP-Logging umbrella dependency is added.

**OwnedPositionalReencodingStatus** is a compact typed result vocabulary: `Succeeded`, `CountMismatch`, `KindMismatch`, `SourceUnavailable`, `ValueWidthMismatch`, `OwnerValueRejected`, and `EncoderRejected`. None is a wire error tag. `OwnedPositionalReencodingOutcome` stores the high-level status, first failed positional index and unchanged owner `EDP-Serialisation::SerialisationResult`; `IsSuccessful()` demands both successful stages.

**OwnedPositionalScratch<TValues...>** is a caller-owned, compile-time sized tuple of optional exact typed values. Bootstrap must account for its entire finite storage footprint and lifecycle. Both public encoders **clear all scratch entries on success and failure**; no source span survives their return. There is no hidden heap, Thread, captured callback, asynchronous borrowing or platform SDK reference.

**`EncodeOwnedDiagnosticArgumentsCbor<Schema,TValues...>(source,destination,scratch)`** and its JSON counterpart are synchronous optional output operations. Preconditions: the schema must be an actual generated EDP-Localisation declared message, and its kinds/count must match the parameter pack at compile time. The source must be an EDP-Logging private bounded argument arena with the exact same count/position kinds and native widths. Owner values must pass the associated System/Clock/BoundedTypes constructors or assignments. On success each position is reconstructed by value and handed to the existing EDP-Serialisation positional writer; the native arena kind numbers are **never** emitted as CBOR tags. On failure the status and position identify the first source refusal, or the typed owner serialisation result records the encoder refusal; caller output mutation remains subject to EDP-Serialisation's own transactional writer contract.

**Private helpers:** `Detail::LoadOwnedNativeScalar<T>` reconstructs exact native private arithmetic bits using `std::bit_cast`; it has no wire authority. `Detail::LoadOwnedPosition<T>` validates count/descriptors and applies value-specific owner conversions. `Detail::EncodeOwnedImpl<Codec,Schema,TValues...>` checks the generated schema, owns the deterministic positional sequence and clears caller scratch. These are PRIVATE IMPLEMENTATION facilities, not generally substitutable codecs. Supported branches are exact arithmetic (bool 0/1), bounded String/Bytes, System FieldIdentifier, RuntimeIncarnationId, TypeIdentifier and DeviceIdentifier, and optional ClockEra. The returned `EncoderRejected` does not promise end-to-end UART/Persistence durability.

**Tests:** `DiagnosticOwnedStrongGeneratedTests.cpp`, `DiagnosticBoundedGeneratedContractTests.cpp`, `DiagnosticOwnedMixedGeneratedTests.cpp`, executed by their owner generated-schema runners with GCC, Clang and UBSan. Do not infer completed canonical fifteen-field LogRecord root emission, historical decoder, asynchronous sink fork/join or genuine physical PowerLoss from this helper.

## G3/G5 replay runner coverage correction (2026-10-23)

The generated-contract runner now builds **and executes** both the
owner scalar replay compatibility facade test and the heterogeneous
generated-schema rehydration test; previously the scalar test source
was absent and the mixed-type test was only compiled. The new facade
fixture validates direct CBOR/JSON equivalence and typed negative
outcomes without any external argument borrowing or new wire tags.
See the normative owned-positional-reencoding document.
