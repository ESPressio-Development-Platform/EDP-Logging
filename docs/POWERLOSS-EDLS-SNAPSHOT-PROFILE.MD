# PowerLoss-atomic KeyValueStorage-backed EDLS snapshot profile (partial G7)

## Authority and scope

`logging/PowerLossEdlsSnapshots.hpp` is an opt-in, **two-to-eight-key
bounded generation-snapshot** profile. It composes **only** the existing
`EDP-Persistence::KeyValueStorage` abstract capability, rejecting
non-PowerLoss retention and interruption-atomicity properties at compile
time. No EDP-Mesh, direct ESP-IDF NVS API, alternative flash driver,
worker, queue, allocator, synchronization object or Clock is introduced
into Logging. Application Bootstrap chooses and exclusively owns the
provider, its namespace, and the unique key names.

The qualified reference binding is
`EDP-Persistence-ESP-IDF::NvsKeyValueStorage` whose declared per-key
maximum is **512 bytes**, `RetentionLevel::PowerLoss`, and
`KeyValueInterruptionAtomicity::PowerLoss`. It also declares
`KeyValueFailurePreservation::MayModify`: unsuccessful writes may
damage the target key. The implementation therefore always writes the
**inactive key** and never destroys the previous newest key while
preparing a new candidate. No extra commit selector is necessary:
the complete CRC-validated generation carried inside each key is the
authority, and recovery selects the largest unambiguous generation.

## Contract and explicit limits

* Each key stores one exact **40-byte canonical EDLS header** plus one
  complete **20-byte-header/trailer EDLG frame**. The independent EDLG
  CRC and EDLS header checksum must pass before candidate selection.
* The maximum snapshot is **512 bytes**, with static cap and observed
  provider maximum both enforced. The specific source template may
  choose smaller buffer/payload limits; no dynamic allocation.
* The current **retention policy is one acknowledged latest generation**.
  An older generation is explicitly retired when the newer generation is
  acknowledged. This is NOT a history-preserving multi-record EDLS
  file archive. Do not apply this profile to unbounded production logs.
* The next generation is written into an empty or oldest **non-newest**
  slot. The owner must serialize all calls; no unrelated writers may
  use the reserved key names. Generation numbers never wrap.
* Admission reports `Committed` only after the provider successfully
  completed `StoreValue`, plus an independently read back exact
  generation/identity/EDLG frame byte-for-byte. The provider's
  PowerLoss atomicity is an explicit precondition for durable
  acknowledgement and is not inferred from CRC or readback alone.
* `Inspect` and `RecoverNewest` distinguish Empty, Clean, Degraded,
  Ambiguous and ProviderFailure. One damaged inactive key does not
  prevent recovery of the still-CRC-valid protected newest key; it
  **does** prevent further mutation until separate owner-reviewed
  remediation. Conflicting clean identity/generation fails closed.
* A StoreValue failure may damage its inactive key; the previously
  protected newest key remains physically separate. The caller must
  not acknowledge failed/unconfirmed writes. Recovery can present
  complete unacknowledged new generations if they were actually
  committed before the cut; this is permitted by the contract.
* This reference profile is **not** a new Persistence provider. It
  cannot upgrade `VfsFileStorage` (SPIFFS) or other non-atomic
  FileStorage implementations. Full historical EDLS requires a
  different space/wear/transaction policy (e.g. qualified journal or
  power-loss-capable file owner), and its physical tests are still open.

## Verification

Host `tests/run-powerloss-edls-snapshots-tests.sh` exercises two-key
rotations, one-record EDLG recovery, source corruption, identity mismatch,
provider read failure, intentionally torn inactive key following failed
StoreValue, fail-closed mutation and recovery of the protected generation.
It uses a contract-compliant fake implementation (not the native ESP-IDF
NVS driver). This establishes bounded logic, **not yet native NVS cut
during key commit**. Genuine hard-power tests must run on GPIO17
user-qualified relay fixture and verify the specific NVS provider's
acknowledged-generation retention and real `ESP_RST_POWERON`.
