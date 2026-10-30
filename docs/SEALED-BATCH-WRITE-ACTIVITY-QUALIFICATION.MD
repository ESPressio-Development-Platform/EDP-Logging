# Sealed batch write-activity power-cut qualification

This is a native application fixture over the existing Logging batch/archive and conservative LittleFS provider. It does not change FileStorage capability offers, the production archive transaction or the ACK rule. All versions remain 0.1.0.

## Configuration and reuse

Select `esp32dev-sealed-batch-64-writecut` through the existing `build_typed_strict.sh`, then `EDP_LFS_CUT_STAGE=sealed_write` through the existing `run_relay_cut_test.sh`. A lock-owning RPI400 process and a unique private evidence directory are mandatory. The existing GPIO17 controller remains the only electrical switch owner. Provisioning holds the target in ROM through complete factory-image and seeded-filesystem verification before application startup.

The first boot commits and independently compares two sealed generations, each containing 64 owner-generated full fifteen-field records. Their strict NVS ACK is 2. It commits the existing test-session armed flag before beginning candidate generation3. The candidate is constructed by the existing successor implementation from canonical individual EDLG frames and the complete segment seal.

At the existing provider ReplaceFile boundary, a test-only forwarding adapter starts a finite 15-second sequence of real replacements of that same unacknowledged spare. Each invocation delegates to the existing conservative provider. It never rewrites either protected predecessor, returns an archive success or advances the NVS watermark. One tick between calls permits watchdog service. Any reported replacement failure or absent cut fails the fixture.

## Meaning of the external cut

The host reacts to SEALED_WRITE_ACTIVITY, independently commands and checks supply OFF, retains it for at least eight seconds, checks OFF again, commands ON and starts a new serial epoch. The existing reset classifier preserves POR_CONFIRMED versus EXTERNALLY_VERIFIED_POWER_CUT and the actual ROM/native diagnostics.

UART and controller latency do not prove which flash instruction was interrupted. A cut may land inside a replacement or in the interval between calls. This is write-activity-window qualification. It is not an instruction-exact torn-write, metadata-GC or endurance certificate, and does not qualify the managed source queue path.

## Recovery and independent evidence

A new boot must observe the old NVS ACK2. The existing archive Recover operation must preserve both protected generations and resolve only its unacknowledged spare, which may be absent, complete or incomplete. The application then compares every original record in generations1/2, publishes generations3/4 through the ordinary strict batch path, and compares all retained originals in generations3/4. It cannot ACK a partial candidate or retry an uncertain transaction in the interrupted owner.

The existing raw audit reads actual LittleFS and NVS partitions independently, extracts the exact two retained files, validates EDLS/EDLC and every EDLG CRC, compares every canonical record field to the deterministic original fixture, and uses Espressif's NVS parser to verify the committed ACK4 watermark and CRCs. A serial replay marker alone is insufficient.

## Execution status

Native build20261031T002959-a64a190f PASS. Physical plus independent raw audit20261031T003134-4bb013c2 PASS, finished31October00:41:59CET. The controller attested8.232secondsOFF. Native reset10/ROM6 SDIO_RESET is preserved as **EXTERNALLY_VERIFIED_POWER_CUT**, not POR_CONFIRMED. Both original protected batches were checked after recovery, followed by normal commits3/4. Raw extraction validated128 original full15-field records129..256 and the NVSACK4 watermark, with official page/entry/blob CRCs PASS. Final targetOFF independently verified.

Factory SHA256 bde92246745d5974cb3146701ecd7399f25c5be72e213fe826571fc97d5b6f3b; postcut LittleFS0a49465ea46a465134d03a0ae0f17cd052a0242b0b28d845f88c7702be0c72ec; NVS733eb3e7f94cbbd3f9673c19d8b3d2bce6403b21a3a91112c2d8307a383b98ac. Actual private UART/controller logs and raw partitions remain on RPI400 in private-sealed64-write-activity-20261031-01. Controller/media regression run 20261031T003048-62c31b03 passed105 tests on RPI400. Initial run20261031T003021-d8bf0735 failed six module imports because the invocation omitted the existing tools PYTHONPATH; corrected invocation passed without changing the tested decoder logic.

Final source includes policy formatting/documentation of the test forwarding adapter. Rebuild20261031T004220-17236074 PASS produced factory2b0ce0183459f4782a1bc6b39a096cbb1b427fa6218c00d1f09b17f10dd5dd29. Since the image hash changed, the exact final image was physically requalified:20261031T004328-1221099b PASS, finished00:45:26CET. Controller8.257sOFF, native10/ROM6 SDIO_RESET, EXTERNALLY_VERIFIED_POWER_CUT. All128 retained original fields and NVSACK4/CRCs again PASS. LittleFS12d1dcf5f82998726308246fa3d324b00f607b8b024f79ce83d73019ef4c3c6d; NVS733eb3e7f94cbbd3f9673c19d8b3d2bce6403b21a3a91112c2d8307a383b98ac. Final targetOFF. Host20261031T004321-80663e22 PASS106.

Backup defaults to full; explicit EDP_LFS_BACKUP_POLICY=disposable skips only preservation of a user-authorised disposable target. Full image/filesystem verification before boot remains mandatory. The first physical run tested full backup; the second tested disposable mode. Invalid policy refuses before hardware.
