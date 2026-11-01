# Owned diagnostic byte-copy composition — consuming and expansion contracts

**Branch:** `feature/logging-v1`, version `0.1.0`, 1 November 2026.

## Consuming developers

`logging/OwnedDiagnosticArguments.hpp` is an **opt-in** bounded diagnostic source owner; the standalone `ESPressio_Logging.hpp` umbrella remains limited to its lightweight foundation and has no imported EDP-Memory runtime/SDK/Platform concrete.

The arena stores exactly the caller-specified bounded argument count and byte capacity. It commits an argument descriptor only after count/size checks and after successfully copying all source bytes into its internal array. It does not retain borrowed input pointers. General source copying delegates to the **existing `EDP-Memory::ByteOperations` `CopyBytes`** operation rather than a parallel manual loop, using the stateless portable C++ provider by default and allowing an application to select a different stateless, `noexcept`, nothrow-constructible provider as the third template parameter:

```cpp
using Source = ESPressio::Logging::OwnedDiagnosticArguments<8U,128U>;
// Or: OwnedDiagnosticArguments<8U,128U,ApplicationStatelessByteOperations>
```

No pointer/provider state, additional heap allocation or extra per-record capacity is retained. This optional header directly requires the EDP-System, EDP-Serialisation, EDP-Memory, EDP-Platform-Portable and associated dependency graph; consumers must supply corresponding include paths or declared library dependencies. The selected provider is also propagated through the existing positional CBOR/JSON rehydration/replay seam, so it is not discarded at the output boundary.

**Source compatibility:** `AddBytes`, identity and strong-argument *mutators* are runtime operations, not constexpr. Accessors and immutable capacity measurements retain constexpr. Strongly typed result outcomes, exact rejection ordering, source-kind discrimination, byte payload, capacity limit and zero-byte semantics are unchanged; invalid candidates do not call CopyBytes or mutate committed descriptor counts. `AddUtf8` retains strict canonical UTF-8 enforcement, including U+0000 rejection. `AddBytes` is for binary values and only a generated schema with a byte-valued argument may subsequently encode those bytes.

## Library expansion

The selected `EDP-Memory` public `ByteOperations` contract already exposes `CopyBytes(void*,const void*,size_t) const noexcept`; no new Memory interface or platform-specific operation was introduced. A provider must have the exact trait-prescribed Composition capability, copy/move/fill/compare signatures, no mutable instance state and a nothrow default constructor. This design follows the existing `EDP-BoundedTypes` stateless byte-operations provider pattern while retaining the in-place Logging-specific descriptor and type-kind ownership.

Do not introduce private `memcpy`, direct copy loops, retained provider pointers, hidden runtime allocation or alternative encoders. Source/canonical boundaries remain distinct. Test the positive provider-selection path, preflight no-copy rejection, exact bytes, ownership after caller mutation, strict CBOR/JSON rehydration and the existing generated/physical profile whenever the Memory copy contract changes. This work does **not** expand arbitrary native FileStorage power-loss guarantees or issue a flash endurance prediction.
