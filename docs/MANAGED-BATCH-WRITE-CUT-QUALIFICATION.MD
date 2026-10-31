# Managed batch write-activity power-cut qualification

This native fixture extends the existing generated full-record worker test. It reuses the same bounded Memory source/representation pools, transactional queue, one DedicatedCoordinator, canonical Clock, typed encoder/delivery, bounded batch and strict LittleFS/NVS archive. It introduces no production API or provider. All package versions remain0.1.0.

## Experiment

Select esp32dev-batch-worker-500-o2-writecut with the existing build_typed_strict.sh. The build runner independently checks actual O2 compiler arguments, enabled assertions,160MHz and500ms dwell. The existing normal O2/500ms environment remains selectable and is built as a regression prerequisite.

Two managed producers perform the existing pressure phase:128 successful volatile source publications and32 bounded deadline refusals before worker start. The existing worker encodes and durably commits two64-record segments. As each consumed-once durable completion is recorded, that same worker prints the original attempt ticket and occurrence coordinate from its tagged test evidence. All128 coordinates leave the device before the next phase starts. They are evidence, not another work transport.

Bootstrap commits the existing test-session armed flag, then releases the two50Hz producers into the new scheduled phase. At the first candidate3 replacement, the same test-only forwarding adapter used by the sealed64 write-activity fixture repeats real replacements of the unacknowledged spare for a finite window. Both protected batches and NVSACK2 remain unchanged. The controller alone commands and verifies supply OFF/ON and interval, discards stale pre-cut UART bytes, and classifies actual ROM/native reset diagnostics.

The interruption may lose volatile sources after the first128. They have no durable completion. The test does not replay or acknowledge them after reboot.

## Recovery and original-source proof

A fresh boot requires the armed session and strict NVSACK2. Before any new Memory runtime or worker starts, the existing archive Recover reconciles only the unacknowledged spare. Native code checks the full retained frame counts, identity, generation, seal/record CRCs and canonical reconstruction. Its reconstructed occurrence coordinates are structural checks only; it emits no new ORIGINAL evidence.

The independent raw audit explicitly selects --power-cut / EDP_WORKER_AUDIT_POWER_CUT=1. It requires128 unique bounded original coordinates appearing before the activity marker and one later recovered receipt, rejects ordinary completion markers mixed into the transcript, and reads actual LittleFS/NVS partitions. The exact two retained sealed files must contain precisely those128 records. Every field, including each original occurrence coordinate, is compared independently. Espressif's NVS parser verifies ACK2 and page/entry/blob CRCs. A newly valid frame/segment CRC cannot hide altered source time.

The fixture does not weaken the ordinary workload receipt path. Unknown audit modes fail before hardware; absent/duplicate/or post-trigger source evidence, wrong original time, extra spare, missing original and ambiguous completion receipts are negative cases.

## Limits

This is a finite managed-path write-activity-window experiment. UART/controller latency does not identify an interrupted flash opcode or metadata-GC phase. It is not an endurance certificate or an indefinite throughput result, and it does not upgrade generic FileStorage interruption atomicity. No old firmware restoration is required for the user's authorised disposable target; complete new image/filesystem verification before boot remains mandatory.

## Execution status

Corrected host20261031T004853-abd23a58 PASS113. Initial004645-f4b34165 exposed an ambiguous mixed-marker refusal gap;004748-9896f24c failed from an erroneous extra argument in the test edit. Both errors are corrected and retained in the run history. Native cut build004645-18f579b9 PASS, including actual compile-profile proof. The normal500ms/O2 native build also passed its profile checks in parent004958-1bd62862. That parent's physical attempt endedFAIL01:07:02CET: firmware reported successful128-record recovery after8.286secondsOFF, but the host classifier did not recognise the new managed replay marker and timed out. The run remainsFAIL. No continuation occurred until09:56CET; this unattended gap is explicitly recorded in the living handoff.

The classifier now recognises the exact managed marker and native EDLS HALT messages. End-to-end controller tests reproduce the actual managed marker, retained-window corruption and honest watchdog classification; unknown markers still refuse. Corrected full host suite095754-83c4528e PASS117. Native image is unchanged.

Fresh physical plus independent raw audit **20261031T095844-2022f045 PASS**, completed31October10:00:33CET. ControllerOFF8.223seconds. Native7, ROM2 EXT_SYS_RESET then16 RTCWDT_RTC_RESET are preserved as **EXTERNALLY_VERIFIED_POWER_CUT**, not POR_CONFIRMED. All128 protected original records survived, including each independently captured pre-cut occurrence coordinate and all other canonical fields. Actual NVSACK2, incarnation7 and official page/entry/blob CRCs PASS. Final targetOFF independently verified.

FactorySHA256 b228a373d2f7a0bbede0335a27f9e59a0603e9f92c3ecf58d7284fa90a3ee34a. LittleFS d177ef1705c24935fd72dabf7aca886abe1b39c36b77489ba71e4bd45fa16173. NVS ef36c0142a4b9bd9e9da74f9a900112c9c0999ddea9fbe5b7960be1910903a61. Private evidence remains on RPI400 under private-managed-worker-writecut-20261031-02.
