# Record production and bounded single-sink drain (partial G1/G8/G10)

This feature-only, header-only implementation introduces RecordProduction.hpp.
This generic producer API is NOT a governed canonical LogRecord. The permanent
EDP-System LogRecord TypeIdentifier has not been issued and a test-only positional
CBOR array must never be decoded as a governed production LogRecord.

## Dependency direction

Logging consumes EDP-Clock strong temporal values and a Bootstrap-injected
CaptureCoherent() contract. EDP-Mesh is neither a dependency nor a prerequisite.
A local monotonic-only Clock observation is valid, with no synchronized coordinate
and an invalid ClockEra. In mesh-enabled applications the upstream Clock/Mesh
integration owns its reference-coherent time capture, not Logging.

ProduceRecordUntil uses an EDP-Threading sequencer mutex to issue a non-wrapping
attempt ticket and capture exactly one coherent Clock sample BEFORE capacity
waiting. Issued tickets remain consumed, including rejected admissions. The
owned ticket and temporal values go into the EDP-Memory record constructor.
Bounded queue-before-Memory admission and transactional FIFO Commit are reused,
not reimplemented. Caller retains scratch/reservation on release uncertainty.

DrainOneRecord exclusively claims the oldest FIFO record, borrows the native
EDP-Memory-owned object for a noexcept sink submission, then explicitly retires
the record. Accepted, Rejected, Partial and ProviderFailure are distinguished
from confirmed Memory reclamation. Sink acceptance does not prove flush,
physical receipt or durability. A failed reclamation leaves the RAII claim
caller-owned; Bootstrap must not tear down Memory prematurely.

## Limits and tests

Full universal canonical LogRecord fields 0..14, published generated schema,
multi-sink ownership, concrete worker, EDP-Serial/Persistence bindings, power-cut
retention/recovery and the EDP-System governed root identity remain OPEN.

Native GCC and Clang tests reuse actual Memory ObjectPool, Threading wake,
BoundedTopology transactional FIFO and local Clock. Target ESP32 HIL additionally
serialises a TEST-ONLY positional array [attempt, monotonic, value] through
EDP-Serialisation, frames with canonical EDLG/CRC, emits UART proof and confirms
source lease reclamation. This does not imply full G1-G15 qualification.
