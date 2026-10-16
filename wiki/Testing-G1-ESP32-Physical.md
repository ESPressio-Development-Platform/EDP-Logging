# Physical ESP32-WROOM G1 validation

**Status: PASS for the opt-in partial G1 bounded Queue/Memory lifecycle, not a production end-to-end Logging pipeline.** The RPI400 integrated ESP32-D0WD completed three independent real boot-and-run test cycles using the final ESP-IDF application firmware compiled and linked with the source-pinned EDP dependency graph. All expected machine-readable Serial assertions passed, including actual FreeRTOS producer wake on CPU1 and shutdown of an actively queue-blocked producer. RPI400 detached run IDs: `20261016T191309-7b39f6ef`, `20261016T191342-ec7b821b`, `20261016T191409-e454c545` (all PASS).

The target's **entire original 4 MB flash was restored** and independently verified byte-identical before final normal reboot, after account was taken of runtime NVS updates caused by the integrated reset helper. Durable restoration: `20261016T192020-381dcdea`, PASS. No original firmware images or data are committed.

See [physical HIL test source](../tests/hil/g1-espidf/src/main.cpp), [PlatformIO project](../tests/hil/g1-espidf/platformio.ini), [test documentation](../tests/hil/g1-espidf/README.MD), and [run-by-run evidence](../tests/hil/g1-espidf/VALIDATION.MD). The Policy/main Logging living handoff records the complete G1-G15 remaining work and distinguishes physical proof from unimplemented sink, durable archive and Clock/Mesh integration.
