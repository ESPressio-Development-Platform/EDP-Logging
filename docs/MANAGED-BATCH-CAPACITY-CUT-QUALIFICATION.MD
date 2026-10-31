# Managed batch capacity-refusal and power-cut qualification

This native fixture reuses the existing source/representation Memory pools, transactional queue, managed coordinator, canonical Clock, typed CBOR delivery and strict bounded batch/archive. It adds no production API or storage provider. All versions remain0.1.0.

Select esp32dev-batch-worker-500-o2-capacitycut and relay stage managed_capacity. The actual compiler-profile gate retains applicationO2, assertions,160MHz and500ms dwell. The existing write-activity mode remains separately selectable.

After the ordinary pressure phase,128 original source records are strictly acknowledged in two64-record segments. The same worker emits all original occurrence coordinates before capacity pressure. Bootstrap commits the existing armed test-session flag, then calls the existing padding helper with its small-record standalone trial disabled. Only unprotected test files consume the remaining LittleFS reserve. The fixture requires actual allocation refusal and measures no more than8192bytes free before the next batch attempt.

The same producers and worker then begin a new100records/s scheduled phase. A real candidate replacement must fail, the coordinator must report sink-service failure, producers must stop and all execution must become quiescent. The batch must retain a nonzero bounded pending record count in RecoveryRequired, with zero new acknowledged records; actual NVSACK remains2 and total durable completions128. Replacement call telemetry proves the candidate was actually attempted. The failed owner retains its RAM and queued leases until the physical cut. It does not retry, replay sources, destroy unconfirmed ownership or invent a Memory teardown.

The existing conservative VFS provider currently maps unsuccessful native writes/commits to FileReplaceStatus::IoFailure12. The report preserves that actual status; it does not relabel it NoSpace9. The free-space sample is from immediately before candidate generation. The experiment tests refusal under observed physical exhaustion.

The existing GPIO17 controller cuts only after this complete refusal witness, independently checks OFF/ON and the hold interval, and begins a fresh serial epoch. Native reset diagnostics remain diagnostic; POR_CONFIRMED and EXTERNALLY_VERIFIED_POWER_CUT are distinct classifications.

Fresh Bootstrap uses the existing strict archive Recover before starting any new Memory runtime or worker. It checks the128 protected records, reclaims only test padding through the shared existing helper, and checks the same protected records and NVSACK2 again. Padding removal never removes a protected archive path. Failed or partial candidate reconciliation remains the archive owner's responsibility.

The independent raw auditor selects --capacity-cut / EDP_WORKER_AUDIT_POWER_CUT=capacity. It requires a unique valid refusal witness with pending count1..64, unsuccessful native replacement,128 durable completions, ACK2 and at most8192 pre-attempt free bytes. All128 original coordinates must precede that witness. The later recovered receipt must be unique. It then independently compares all15 fields and original occurrence coordinates in actual retained media and checks the official NVS page/entry/blob CRCs. Wrong stage, mixed modes, altered original time, missing original records and invalid capacity claims refuse.

This is a finite near-full failure and recovery experiment. The cut occurs after refusal, not at a claimed exact flash/GC instruction. It does not establish indefinite throughput, flash service life or generic FileStorage interruption atomicity.

Execution status: host100558-2f942103 PASS123. Native100438-e1dcfe78 PASS with actual applicationO2/assertion/160MHz/500ms compile proof. Physical plus independent raw audit **20261031T100726-066996bc PASS**, completed31October10:15:48CET. Padding allocation refused atindex45 with nativeIoFailure12; measured4096bytes free. The real subsequent batch replacement also returnedIoFailure12. The batch retained52 pending records, zero newACK,128 durable completions and NVSACK2 after actual execution quiescence.

ControllerOFF8.243seconds; native1/ROM1 POWERON_RESET yields **POR_CONFIRMED**. Recovery retained all128 original full15-field records, including exact independent pre-cut coordinates. Test-only padding was removed only after protected-history recovery, and the raw file set contains only the seed and two protected segments. Official NVSACK2/incarnation7/page/entry/blob CRCs PASS. Final targetOFF independently verified.

FactorySHA256 4dea856440f5aa555cf3498f0ab10839f9a23651fb57d430e6f941591d8d4bf3. LittleFS5d9bdf81e179c90e291c09e61df9848175d71ae19cc77d300ea4e56dd8a0a80d. NVSef36c0142a4b9bd9e9da74f9a900112c9c0999ddea9fbe5b7960be1910903a61. Private evidence remains on RPI400 in private-managed-capacity-cut-20261031-01.
