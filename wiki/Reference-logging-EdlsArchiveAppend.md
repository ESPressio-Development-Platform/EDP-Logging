# EdlsArchiveAppend — checked existing-slot FileStorage append

Header src/logging/EdlsArchiveAppend.hpp is opt-in. Enum
EdlsAppendStatus distinguishes ProviderAccepted, InvalidFrame,
SegmentUnclean, SegmentCapacityExceeded, ChangedDuringPreflight,
ProviderRejected. EdlsAppendOutcome carries Status, Native
FileAppendStatus and OfferedBytes. The template method
AppendEdlgToCleanEdlsSlot<TMaximumPayloadBytes,TMaximumSlotBytes,
TStorage>(storage,path,expectedSlot,framedBytes) validates EDLG
CRC/framing and scans/validates the full EDLS slot through the existing
read-only inspector before invoking EDP-Persistence AppendFile once.

Caller supplies exclusive FileStorage ownership during scan+append;
neither rechecking file size nor a successful native append proves
power-loss atomicity. No automatic successor allocation, rotation,
manifest signing or historical immutability exists yet.
