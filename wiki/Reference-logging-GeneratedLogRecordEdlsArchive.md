# Reference — `src/logging/GeneratedLogRecordEdlsArchive.hpp`

**Classification:** public opt-in governed-record archival adapter. [Reviewed source](../src/logging/GeneratedLogRecordEdlsArchive.hpp).

This adapter uses the [generated wire projection](Reference-logging-GeneratedLogRecordWireProjection), EDP-Serialisation CBOR TypedEnvelope, the canonical EDLG writer and the existing protected rolling EDLS successor writer. Application Bootstrap owns the FileStorage provider and exclusive archive access.

| `GeneratedEdlsPublishStatus` | Meaning |
| --- | --- |
| `Published` (0) | The successor was published within capacity or published and the predecessor retired. |
| `SourceIdentityMismatch` (1) | The expected incarnation is zero or the record's device/incarnation differs from the archive's supplied identity. |
| `ProjectionRejected` (2) | The generated projection rejected the record or positional value. |
| `CborRejected` (3) | Measure or encode failed, or the encoded byte count differed from the measured size. |
| `CborCapacityExceeded` (4) | The measured payload exceeds the selected maximum or the supplied CBOR scratch. |
| `FramingRejected` (5) | The EDLG writer rejected framing of the encoded payload. |
| `ArchiveRejected` (6) | The existing rolling successor operation did not return either publication-success status. |

`GeneratedEdlsPublishOutcome<N>` reports `Status` (default ProjectionRejected), `Projection` (the exact generated projection outcome), `Archive` (the exact rolling successor outcome for `N` physical paths), `CborBytes` (zero until successful CBOR encoding) and `EdlgBytes` (zero until successful framing). `IsPublished()` tests Published. A later failure can leave earlier successful stage counts and scratch contents visible. Archive mutation and preservation follow the reported successor result and selected FileStorage contract.

## Operations and arguments

`PublishGeneratedLogRecordToProtectedEdlsWithScratch` takes a borrowed complete source `record`, writable generated `projection`, positional scratch, CBOR and EDLG byte scratch, the selected `storage`, ordered file `paths`, the expected `deviceIdentifier` and `runtimeIncarnation`, and writable `archiveScratch`. The buffers must satisfy the underlying stage capacities. The CBOR and EDLG spans must be disjoint while framing. Once framing succeeds, the CBOR buffer may be reused as archive scratch if it has the required space and is disjoint from the live EDLG frame.

- `TMessageSchema` and `TParameters...` select the exact generated Localisation message and ordered value types.
- `TWidth` selects the attempt ticket width. The projection and record must have the same governed root TypeIdentifier.
- `TMaximumArguments` and `TArgumentBytes` match the bounded source arena.
- `TProtectedSlots` selects retained slots and `TPhysicalSlots` must equal that count plus one for the successor reserve.
- `TMaximumPayload` is 1 through 4096 bytes. `TMaximumSegment` must accommodate the 40-byte header, maximum payload and 20-byte EDLG envelope.
- `TStorage` supplies the existing application-selected FileStorage contract.

The function checks source identity, projects the record, measures and encodes CBOR, frames the exact bytes and calls the rolling successor owner. Refusals before the archive stage leave FileStorage untouched. The provider and all borrowed buffers must remain live for the ordinary-context synchronous call. Bootstrap serializes writers, establishes paths and selects retention. Publication does not provide the separate KeyValueStorage watermark of the [strict acknowledged archive](../docs/EDLS-ACKNOWLEDGED-RECOVERY.MD).

`PublishGeneratedLogRecordToProtectedEdls` uses the same algorithm and template constraints with local archive scratch of `EdlsHeaderBytes + TMaximumPayload + 20` bytes. Use WithScratch to place this bounded buffer in application-owned storage on constrained workers.
