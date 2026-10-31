#!/usr/bin/env python3
"""Fully offline GPIO17 host-control/serial-buffer replay regression tests."""
from __future__ import annotations

import io
import os
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import relay_cut_monitor as monitor


class EmulatedSerial:
    def __init__(self, pre_cut: bytes, post_cut: bytes):
        self.pre_cut = pre_cut
        self.post_cut = post_cut
        self.power_on_count = 0
        self.powered = False
        self.initial_done = False
        self.recovery_done = False
        self.reset_calls = 0
        self.closed = False

    def read(self, _maximum):
        if self.power_on_count >= 2 and not self.recovery_done:
            self.recovery_done = True
            return self.post_cut
        if self.power_on_count == 1 and not self.initial_done:
            self.initial_done = True
            return self.pre_cut
        return b""

    def reset_input_buffer(self):
        self.reset_calls += 1

    def close(self):
        self.closed = True


class ElectricalControllerAndBufferTests(unittest.TestCase):
    def exercise(self, *, native=7, rom="rst:0x10 (RTCWDT_RTC_RESET)",
                 wrong_off=False, bad_recovery=False,
                 lowspace=False, lowspace_reclaim=False,
                 midwrite=False, strictack=False,
                 typed_ack_gap=False, sealed_write=False, managed=False, capacity=False, retirement=False):
        # Last line of the original UART chunk is STALE pre-cut replay.
        # The event loop MUST discard it when a new power epoch begins.
        before = (
            b"rst:0x1 (POWERON_RESET)\n"
            b"EDP_BND|BOOT|reset=1\n"
            b"EDP_BND|ACK|gen=23\n"
            b"EDP_BND|WRITE_ENTER|gen=24\n"
            b"EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
        )
        after = (
            f"{rom}\n"
            f"EDP_BND|BOOT|reset={native}\n"
            f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
            + (
                "EDP_BND|RECOVERED|newest=22|older=21|valid=2|contiguous=1\n"
                if bad_recovery else
                "EDP_BND|RECOVERED|newest=24|older=23|valid=2|contiguous=1\n"
            )
            + "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
        ).encode()
        if lowspace or lowspace_reclaim:
            before = (
                b"rst:0x1 (POWERON_RESET)\n"
                b"EDP_BND|BOOT|reset=1\n"
                b"EDP_BND|ACK|gen=2\n"
                b"EDP_BND|LOWSPACE_SAFE|protected=1,2|last_ack=2|pads=59|no_ack_3=1\n"
                + (b"EDP_BND|LOWSPACE_RECLAIM_ENTER|pad=5\n"
                   if lowspace_reclaim else b"")
            )
            after = (
                f"{rom}\n"
                f"EDP_BND|BOOT|reset={native}\n"
                f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
                + (
                    "EDP_BND|RECOVERED|newest=2|older=0|valid=1|contiguous=1\n"
                    if bad_recovery else
                    "EDP_BND|RECOVERED|newest=2|older=1|valid=2|contiguous=1\n"
                )
                + ("" if bad_recovery else
                   "EDP_BND|LOWSPACE_ROLLED|ack=3|window=2,3|retired=1\n")
                + "EDP_BND|REPLAY_VERIFIED|TWO_RETAINED_AFTER_RECLAIM_CUT\n"
            ).encode()
        if midwrite:
            before=(
                b"rst:0x1 (POWERON_RESET)\n"
                b"EDP_BND|BOOT|reset=1\n"
                b"EDP_BND|ACK|gen=1\n"
                b"EDP_BND|ACK|gen=2\n"
                b"EDP_BND|MIDWRITE_START|attempt=1\n"
                b"EDP_BND|REPLAY_VERIFIED|TWO_PROTECTED_AFTER_MIDWRITE_CUT\n"
            )
            after=(
                f"{rom}\n"
                f"EDP_BND|BOOT|reset={native}\n"
                f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
                + ("EDP_BND|RECOVERED|newest=2|older=0|valid=1|contiguous=1\n"
                    if bad_recovery else
                    "EDP_BND|RECOVERED|newest=2|older=1|valid=2|contiguous=1\n")
                + "EDP_BND|REPLAY_VERIFIED|TWO_PROTECTED_AFTER_MIDWRITE_CUT\n"
            ).encode()
        if strictack:
            before=(
                b"rst:0x1 (POWERON_RESET)\n"
                b"EDP_BND|BOOT|reset=1\n"
                b"EDP_BND|ACK|gen=1\n"
                b"EDP_BND|ACK|gen=2\n"
                b"EDP_BND|STRICT_CANDIDATE_START|attempt=1\n"
                b"EDP_BND|REPLAY_VERIFIED|STRICT_TWO_PROTECTED_AFTER_REPAIR\n"
            )
            after=(
                f"{rom}\n"
                f"EDP_BND|BOOT|reset={native}\n"
                f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
                + ("EDP_BND|RECOVERED|newest=4|older=2|valid=2|contiguous=1\n"
                    if bad_recovery else
                    "EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n")
                + "EDP_BND|REPLAY_VERIFIED|STRICT_TWO_PROTECTED_AFTER_REPAIR\n"
            ).encode()
        if typed_ack_gap or sealed_write:
            before=(
                b"rst:0x1 (POWERON_RESET)\n"
                b"EDP_BND|BOOT|reset=1\n"
                b"EDP_BND|ACK|gen=1\n"
                b"EDP_BND|ACK|gen=2\n"
                b"EDP_BND|TYPED_ACK_GAP|gen=3|nvs_ack=2|candidate_complete=1\n"
                b"EDP_BND|REPLAY_VERIFIED|STRICT_TYPED15_NVS_ACK_AFTER_CUT\n"
            )
            after=(
                f"{rom}\n"
                f"EDP_BND|BOOT|reset={native}\n"
                f"EDP_BND|RESET_OBSERVED|reason={native}|por={int(native==1)}\n"
                + ("EDP_BND|RECOVERED|newest=4|older=2|valid=2|contiguous=1\n"
                   if bad_recovery else
                   "EDP_BND|RECOVERED|newest=4|older=3|valid=2|contiguous=1\n")
                + "EDP_BND|REPLAY_VERIFIED|STRICT_TYPED15_NVS_ACK_AFTER_CUT\n"
            ).encode()
        if sealed_write:
            before = before.replace(
                b"TYPED_ACK_GAP|gen=3|nvs_ack=2|candidate_complete=1",
                b"SEALED_WRITE_ACTIVITY|gen=3|nvs_ack=2")
        if managed:
            after = after.replace(
                b"STRICT_TYPED15_NVS_ACK_AFTER_CUT",
                b"MANAGED_BATCH128_AFTER_WRITE_ACTIVITY_CUT")
            after = after.replace(b"newest=4|older=3", b"newest=2|older=1")
        if capacity:
            before = before.replace(
                b"SEALED_WRITE_ACTIVITY|gen=3|nvs_ack=2",
                b"MANAGED_CAPACITY_SAFE|ack=2|pending_records=50|native_replace=12|durable=128|free_bytes=4096")
            after = after.replace(b"MANAGED_BATCH128_AFTER_WRITE_ACTIVITY_CUT",
                                  b"MANAGED_BATCH128_AFTER_CAPACITY_CUT")
        if retirement:
            before = before.replace(b"SEALED_WRITE_ACTIVITY|gen=3|nvs_ack=2",
                                    b"SEALED_RETIRE_PENDING|gen=3")
            after = after.replace(b"EDP_BND|REPLAY_VERIFIED|",
                b"EDP_BATCH|RETIREMENT_RECOVERED|ack_segment=3|window=2,3|expired=1\nEDP_BND|REPLAY_VERIFIED|")
        port = EmulatedSerial(before, after)
        elapsed = [0.0]
        actual_clock = time.monotonic

        def emulate_command(args, **_kwargs):
            action = args[-1]
            if action == "power-on":
                port.power_on_count += 1
                port.powered = True
            if action == "power-off":
                port.powered = False
            state = "ON" if port.powered else "OFF"
            if wrong_off and action == "power-off":
                state = "ON"
            return SimpleNamespace(stdout=f"OK {action} POWER={state}\n")

        def emulate_sleep(seconds):
            elapsed[0] += seconds + 0.1

        results = []
        with (
            tempfile.TemporaryDirectory() as td,
            patch.object(monitor.serial, "Serial", return_value=port),
            patch.object(monitor.subprocess, "run", side_effect=emulate_command),
            patch.object(monitor.time, "sleep", side_effect=emulate_sleep),
            patch.object(monitor.time, "monotonic",
                         side_effect=lambda: actual_clock()+elapsed[0]),
            patch.dict(os.environ, {
                "EDP_LFS_CUT_STAGE": ("lowspace_reclaim" if lowspace_reclaim else
                                      "lowspace" if lowspace else
                                      "midwrite" if midwrite else
                                      "strictack" if strictack else
                                      "sealed_retire" if retirement else
                                      "managed_capacity" if capacity else
                                      "sealed_write" if sealed_write else
                                      "typed_ack_gap" if typed_ack_gap else "write"),
                "EDP_LFS_OFF_HOLD_S": "8.0",
            }),
            patch("builtins.print", side_effect=lambda *a, **_k: results.append(
                " ".join(map(str, a))
            )),
        ):
            try:
                exit_code = monitor.run(str(Path(td)/"evidence"))
            except RuntimeError:
                exit_code = 9
            recorded = (Path(td)/"evidence"/"serial.txt").read_text()
        return exit_code, results, recorded, port

    def test_committed_retirement_gate_has_new_epoch_recovery(self):
        code, out, recorded, port = self.exercise(sealed_write=True, retirement=True,
            native=1, rom="rst:0x1 (POWERON_RESET)")
        self.assertEqual(code, 0)
        self.assertIn("RETIREMENT_RECOVERED|ack_segment=3|window=2,3|expired=1", recorded)
        self.assertTrue(any("CUT_STAGE=sealed_retire" in line for line in out))
        self.assertTrue(any("QUALIFICATION=POR_CONFIRMED" in line for line in out))
        self.assertEqual(port.reset_calls, 2)
        self.assertTrue(port.closed)

    def test_committed_retirement_cannot_hide_damaged_protected_window(self):
        self.assertEqual(self.exercise(sealed_write=True, retirement=True,
                                      bad_recovery=True)[0], 4)

    def test_committed_retirement_requires_real_off_confirmation(self):
        self.assertEqual(self.exercise(sealed_write=True, retirement=True,
                                      wrong_off=True)[0], 9)

    def test_retirement_build_pair_is_checked_before_hardware(self):
        import subprocess
        env = dict(os.environ, EDP_LITTLEFS_AGENT_ID="unit-no-hardware",
                   EDP_LITTLEFS_EVIDENCE_DIR="/nonexistent/unit-no-hardware",
                   EDP_LFS_BUILD_ENV="esp32dev-batch-worker-500-o2",
                   EDP_LFS_CUT_STAGE="sealed_retire")
        result = subprocess.run(
            ["bash", str(Path(monitor.__file__).with_name("run_relay_cut_test.sh"))],
            env=env, text=True, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Invalid stage/build pairing", result.stderr)

    def test_capacity_failure_cut_and_recovered_original_window(self):
        code, out, recorded, port = self.exercise(
            sealed_write=True, managed=True, capacity=True)
        self.assertEqual(code, 0)
        self.assertIn("native_replace=12|durable=128", recorded)
        self.assertTrue(any("CUT_STAGE=managed_capacity" in line for line in out))
        self.assertTrue(any("last_ack=2|newest=2|older=1" in line for line in out))
        self.assertTrue(port.closed)

    def test_capacity_cut_corrupt_retention_fails(self):
        self.assertEqual(self.exercise(sealed_write=True, managed=True,
                                      capacity=True, bad_recovery=True)[0], 4)

    def test_capacity_cut_unverified_off_fails(self):
        self.assertEqual(self.exercise(sealed_write=True, managed=True,
                                      capacity=True, wrong_off=True)[0], 9)

    def test_managed_replay_marker_completes_real_controller_loop(self):
        code, out, recorded, port = self.exercise(
            sealed_write=True, managed=True, native=7,
            rom="rst:0x10 (RTCWDT_RTC_RESET)")
        self.assertEqual(code, 0)
        self.assertIn("MANAGED_BATCH128_AFTER_WRITE_ACTIVITY_CUT", recorded)
        self.assertTrue(any("QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT" in line for line in out))
        self.assertTrue(any("last_ack=2|newest=2|older=1" in line for line in out))
        self.assertEqual(port.reset_calls, 2)
        self.assertTrue(port.closed)

    def test_managed_replay_bad_retention_still_fails(self):
        self.assertEqual(self.exercise(
            sealed_write=True, managed=True, bad_recovery=True)[0], 4)

    def test_invalid_backup_policy_refused_before_hardware(self):
        import subprocess
        env = dict(os.environ, EDP_LITTLEFS_AGENT_ID="unit-no-hardware",
                   EDP_LITTLEFS_EVIDENCE_DIR="/nonexistent/unit-no-hardware",
                   EDP_LFS_BUILD_ENV="esp32dev-sealed-batch-64-writecut",
                   EDP_LFS_CUT_STAGE="sealed_write", EDP_LFS_BACKUP_POLICY="invalid")
        result = subprocess.run(
            ["bash", str(Path(monitor.__file__).with_name("run_relay_cut_test.sh"))],
            env=env, text=True, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unsupported backup policy", result.stderr)

    def test_sealed_write_activity_controller_cut_and_new_boot(self):
        code, out, recorded, port = self.exercise(
            sealed_write=True, native=1, rom="rst:0x1 (POWERON_RESET)")
        self.assertEqual(code, 0)
        self.assertEqual(recorded.count("REPLAY_VERIFIED"), 1)
        self.assertTrue(any("CUT_STAGE=sealed_write" in line for line in out))
        self.assertTrue(any("QUALIFICATION=POR_CONFIRMED" in line for line in out))
        self.assertEqual(port.reset_calls, 2)
        self.assertTrue(port.closed)

    def test_sealed_write_activity_damaged_retention_fails(self):
        self.assertEqual(self.exercise(sealed_write=True, bad_recovery=True)[0], 4)

    def test_sealed_write_activity_unconfirmed_relay_fails(self):
        self.assertEqual(self.exercise(sealed_write=True, wrong_off=True)[0], 9)

    def test_typed_gap_requires_power_reset_and_real_postcut_replay(self):
        code, out, recorded, _=self.exercise(
            typed_ack_gap=True,native=1,rom="rst:0x1 (POWERON_RESET)")
        self.assertEqual(code,0)
        self.assertIn("STRICT_TYPED15_NVS_ACK_AFTER_CUT",recorded)
        self.assertTrue(any("last_ack=2|newest=4|older=3" in line
                            for line in out))
        self.assertTrue(any("CUT_STAGE=typed_ack_gap" in line
                            for line in out))

    def test_typed_gap_corrupted_retention_fails(self):
        code,_,_,_=self.exercise(typed_ack_gap=True,bad_recovery=True)
        self.assertEqual(code,4)

    def test_typed_gap_wrong_relay_off_fails(self):
        code,_,_,_=self.exercise(typed_ack_gap=True,wrong_off=True)
        self.assertEqual(code,9)

    def test_strict_nvs_ack_atomicity_cut_and_resume(self):
        code, out, recorded, _ = self.exercise(strictack=True, native=1,
            rom="rst:0x1 (POWERON_RESET)")
        self.assertEqual(code, 0)
        self.assertIn("STRICT_TWO_PROTECTED_AFTER_REPAIR",recorded)
        self.assertTrue(any("last_ack=2|newest=4|older=3" in x for x in out))
        self.assertTrue(any("CUT_STAGE=strictack" in x for x in out))

    def test_strict_nvs_bad_retained_generation_fails(self):
        code, out, _, _ = self.exercise(strictack=True,bad_recovery=True)
        self.assertEqual(code, 4)
        self.assertTrue(any("PROTECTED_RETAINED_WINDOW_FAILED" in x for x in out))

    def test_strict_nvs_controller_false_off_is_rejected(self):
        code, _, _, _ = self.exercise(strictack=True,wrong_off=True)
        self.assertEqual(code, 9)

    def test_midwrite_cut_preserves_oldest_acknowledged(self):
        code, output, recorded, _ = self.exercise(midwrite=True,native=1,
            rom="rst:0x1 (POWERON_RESET)")
        self.assertEqual(code,0)
        self.assertTrue(any("CUT_STAGE=midwrite" in x for x in output))
        self.assertTrue(any("last_ack=2|newest=2|older=1" in x for x in output))
        self.assertTrue(any("QUALIFICATION=POR_CONFIRMED" in x for x in output))
        self.assertIn("TWO_PROTECTED_AFTER_MIDWRITE_CUT",recorded)
    def test_midwrite_cut_damaged_ack_fails(self):
        code, _, _, _ = self.exercise(midwrite=True,bad_recovery=True)
        self.assertEqual(code,4)

    def test_real_por_from_new_epoch(self):
        code, output, recorded, port = self.exercise(
            native=1, rom="rst:0x1 (POWERON_RESET)"
        )
        self.assertEqual(code, 0)
        self.assertTrue(any("QUALIFICATION=POR_CONFIRMED" in x for x in output))
        self.assertEqual(port.reset_calls, 2)
        self.assertTrue(port.closed)

    def test_watchdog_physically_accepted_not_por(self):
        code, output, _, _ = self.exercise()
        self.assertEqual(code, 0)
        self.assertTrue(any(
            "QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT" in x
            for x in output
        ))

    def test_sdio_physically_accepted_not_por(self):
        code, output, _, _ = self.exercise(
            native=10, rom="rst:0x6 (SDIO_RESET)"
        )
        self.assertEqual(code, 0)
        self.assertTrue(any("app_reset=10" in x for x in output))

    def test_precut_uart_replay_is_not_sufficient(self):
        code, _, recorded, _ = self.exercise(bad_recovery=True)
        self.assertEqual(code, 4)
        # Precut replay marker in the buffered chunk was discarded.
        self.assertEqual(recorded.count("REPLAY_VERIFIED"), 1)

    def test_lowspace_failed_successor_preserves_acknowledged_pair(self):
        code, output, _, _ = self.exercise(lowspace=True)
        self.assertEqual(code, 0)
        self.assertTrue(any("QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT"
                            in x for x in output))
        self.assertTrue(any("last_ack=2|newest=2|older=1"
                            in x for x in output))

    def test_lowspace_corrupt_protected_pair_is_rejected(self):
        code, output, _, _ = self.exercise(
            lowspace=True, bad_recovery=True
        )
        self.assertEqual(code, 4)
        self.assertTrue(any("PROTECTED_RETAINED_WINDOW_FAILED"
                            in x for x in output))

    def test_lowspace_controller_failure_is_rejected(self):
        code, _, _, _ = self.exercise(
            lowspace=True, wrong_off=True
        )
        self.assertEqual(code, 9)

    def test_lowspace_reclaim_cut_and_complete_verified_roll(self):
        code, output, _, _ = self.exercise(lowspace_reclaim=True)
        self.assertEqual(code, 0)
        self.assertTrue(any(
            "CUT_STAGE=lowspace_reclaim|CUT_AT=5|LAST_ACK=2" in x
            for x in output
        ))
        self.assertTrue(any(
            "QUALIFICATION=EXTERNALLY_VERIFIED_POWER_CUT" in x
            for x in output
        ))

    def test_lowspace_reclaim_cut_rejects_corrupt_protected_window(self):
        code, output, _, _ = self.exercise(
            lowspace_reclaim=True, bad_recovery=True
        )
        self.assertEqual(code, 4)
        self.assertTrue(any(
            "PROTECTED_RETAINED_WINDOW_FAILED" in x for x in output
        ))

    def test_controller_did_not_confirm_relay_off(self):
        code, _, _, _ = self.exercise(wrong_off=True)
        self.assertEqual(code, 9)


if __name__ == "__main__":
    unittest.main()
