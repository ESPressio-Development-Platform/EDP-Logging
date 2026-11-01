# Private implementation invariants

Finite byte arenas, source descriptors, checked EDLS frames, protected predecessor slots, and segment candidate generations have narrowly scoped lifetimes. Scratch and borrowed spans are always caller-owned or bounded by the queue/worker slot.

## Boundaries

Never bypass the original CRC, full-frame verification, strict NVS acknowledgment, bounded deadline, or cleanup ordering. Plain FileStorage does not guarantee interrupted-write durability; retention is controlled by the application.

See [Reference Index](Reference-Index) for source-by-source declarations and [Tooling Reference](Tooling-Reference) for maintained decoder and evidence tools.
