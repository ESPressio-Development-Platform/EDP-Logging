# Reference — `src/logging/EdlsArchiveAppend.hpp`

**Classification:** PUBLIC OPT-IN checked existing-slot append, with application-composed **EDP-Persistence FileStorage** direct contract. [Exact source](../src/logging/EdlsArchiveAppend.hpp). It includes the existing Logging EDLG strict reader and EDLS read-only inspector; no new FileStorage, heap, writer task, mutex or archive-retention policy is created.

## Public `EdlsAppendStatus` enum

**ProviderAccepted (0)** means the selected FileStorage accepted one append under its own actual profile; **InvalidFrame (1)** means a claimed EDLG input failed exact validation or payload bounds; **SegmentUnclean (2)** means the existing EDLS physical slot was not completely CRC-clean; **SegmentCapacityExceeded (3)** means the fully framed new bytes exceed the configured physical slot bound; **ChangedDuringPreflight (4)** means the independent file-size read did not agree with the previously verified extent; **ProviderRejected (5)** means native append rejected the mutation. None says bytes survived physical power interruption.

## Public `EdlsAppendOutcome` members

`Status` is the semantic result, defaulting to ProviderRejected; `Native : EDP-Persistence::FileAppendStatus` preserves native accepted/refused state when mutation was attempted, otherwise defaults to NotReady; `OfferedBytes : size_t` counts exactly the attempted framed bytes, zero when no mutation was offered. This is a value-only outcome, not a queued future, leased buffer, physical commit certificate or terminal archive watermark.

## `AppendEdlgToCleanEdlsSlot<TMaximumPayloadBytes,TMaximumSlotBytes,TStorage>`

**`TMaximumPayloadBytes`** is a strictly positive compile-time bound up to 4096 on an already encoded EDLG payload. **`TMaximumSlotBytes`** is the physical fixed file ceiling, at least a 40-byte EDLS header and 20-byte minimal EDLG frame. **`TStorage`** is the caller-selected abstract EDP-Persistence FileStorage provider; its compile-time contract must satisfy `HasAppendOperation<TStorage>`. These template arguments statically constrain the borrowed storage, scratch/work and legal operation selection; they do not allocate a new provider.

Runtime parameters: `storage` is a borrowed **exclusive application-owned** FileStorage instance; `path` is the physical slot location; `expectedSlot` is the canonical path/ordinal mapping expected by EDLS inspection; `framedBytes` is a borrowed complete EDLG frame and may be used by native AppendFile only during this synchronous operation. The application must hold exclusive writer coordination for the entire scan → independent size recheck → append. Without that exclusion, even matching file sizes cannot exclude concurrent mutation.

The method checks the finite maximum, parses the **complete** EDLG frame including CRC, rejects trailing bytes beyond the one verified frame, then calls the existing `InspectEdlsSlot` and demands `EdlsSlotCondition::Clean`. It computes the existing verified prefix, bounds-checks the total, rechecks actual FileStorage size, and only then invokes **one** native `AppendFile`. It preserves the native provider result and attempted byte count. Failed preflight performs **no** native write. A native failure may have partially modified bytes according to the concrete provider's separately documented failure/interruption semantics; no rollback or immutable-successor guarantee is fabricated here.

## Application-visible limits

This helper intentionally performs **non-rotating** mutation of an existing clean slot. It does not allocate a new generation, preserve protected predecessors, seal a complete multi-frame archive, or record a strict NVS/KeyValueStorage watermark. For crash-safe protected finite retention on LittleFS, applications must instead use the independently qualified **immutable successor + strict acknowledgement** owner in [Protected EDLS Rolling](Reference-logging-EdlsArchiveRotation). One accepted AppendFile is not automatically a durable acknowledgement or write-atomicity proof.

Tests include the existing strict archive-inspection/append host fixtures and bounded native FileStorage profiles. See the [normative archive append contract](../docs/EDLS-ARCHIVE-APPEND.MD) and [G1–G15 evidence](../docs/ACCEPTANCE-G1-G15.MD), retaining real negative provider and interrupted-replacement results.
