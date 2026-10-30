"""Independent worker-media negatives with valid replacement frame and seal CRCs."""
import tempfile
import unittest
from pathlib import Path
from edp_logging.archive import SegmentHeader, encode_segment_header
from edp_logging.framing import FrameCodec, crc32c, encode_frame
from audit_batch_worker_media import audit, receipt
from audit_typed_extracted_edls import DEVICE
from test_audit_typed_edls import typed
from monitor_edls_provider_profile import PATTERNS

def segment(slot, generation, tickets, overrides=None):
    header=encode_segment_header(SegmentHeader(slot=slot,generation=generation,
        incarnation=7,device_identifier=DEVICE,profile=1))
    frames=[]
    for ticket in tickets:
        fields={6:1000000000+ticket}
        fields.update((overrides or {}).get(ticket,{}))
        frames.append(encode_frame(FrameCodec.Cbor,typed(ticket,field_overrides=fields)))
    body=b"".join(frames)
    prefix=header+body+b"EDLC"+len(frames).to_bytes(4,"little")+len(body).to_bytes(4,"little")
    return prefix+crc32c(prefix).to_bytes(4,"little")

class WorkerMediaTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root=Path(temp.name)
        (self.root/"seed.txt").write_bytes(b"EDP1")
        (self.root/"slot0").write_bytes(segment(0,4,[2100,2102]))
        (self.root/"slot1").write_bytes(segment(1,5,[2101]))
        self.text="".join(f"EDP_WORKER|ORIGINAL|ticket={n}|monotonic_ns={1000000000+n}\n"
            for n in [2100,2102,2101])
        self.text+="EDP_WORKER|PASS|scheduled_records=2000|producers=2|target_rps=100|elapsed_us=20000000|ack_segment=5\n"
    def test_actual_partial_segments_and_reordered_ticket_history(self):
        self.assertEqual(audit(self.root,self.text),(5,3))
    def test_each_dynamic_original_coordinate_and_all_fields_checked(self):
        overrides={0:[1,0,6],1:2,2:b"Z"*16,3:8,4:2101,5:32,6:1,
            7:123,8:1,9:1,10:0,11:[0,0],12:[0]*16,13:[1,2],14:1}
        for field,value in overrides.items():
            with self.subTest(field=field):
                (self.root/"slot0").write_bytes(segment(0,4,[2100,2102],{2100:{field:value}}))
                with self.assertRaises(ValueError):
                    audit(self.root,self.text)
    def test_valid_seal_cannot_hide_missing_original(self):
        (self.root/"slot0").write_bytes(segment(0,4,[2100]))
        with self.assertRaises(ValueError):audit(self.root,self.text)
    def test_duplicate_unknown_and_wrong_identity_refused(self):
        for tickets in ([2100,2100],[2100,2099]):
            (self.root/"slot0").write_bytes(segment(0,4,tickets))
            with self.assertRaises(ValueError):audit(self.root,self.text)
        (self.root/"slot0").write_bytes(segment(0,7,[2100,2102]))
        with self.assertRaises(ValueError):audit(self.root,self.text)
    def test_missing_duplicate_or_out_of_bound_receipt_refused(self):
        for text in ("",self.text+self.text,self.text.replace("ticket=2101","ticket=2162"),
                     self.text.replace("monotonic_ns=1000002101","monotonic_ns=0")):
            with self.assertRaises(ValueError):receipt(text)
    def test_receipt_coordinates_are_independent_of_tampered_media(self):
        altered=self.text.replace("monotonic_ns=1000002101","monotonic_ns=1000002102")
        with self.assertRaises(ValueError):audit(self.root,altered)
    def test_host_monitor_requires_expected_worker_configuration(self):
        pattern=PATTERNS["batch-worker"]
        self.assertIsNotNone(pattern.search(self.text))
        for bad in (self.text.replace("producers=2","producers=1"),
                    self.text.replace("scheduled_records=2000","scheduled_records=20"),
                    self.text.replace("EDP_WORKER|PASS","EDP_WORKER|IDLE"),
                    self.text.replace("target_rps=100","target_rps=10")):
            self.assertIsNone(pattern.search(bad))


class WorkerMonitorPowerTests(unittest.TestCase):
    def test_power_is_verified_before_serial_open(self):
        from unittest.mock import patch
        import monitor_edls_provider_profile as monitor
        events=[]
        class Uart:
            def __enter__(self):
                events.append("serial-enter")
                return self
            def __exit__(self,*args):return False
            def reset_input_buffer(self):pass
            def readline(self):
                return b"EDP_WORKER|PASS|scheduled_records=2000|producers=2|target_rps=100|ack_segment=5\n"
        def power(command,**kwargs):
            events.append(command)
            return "POWER=ON"
        def serial_open(*args,**kwargs):
            self.assertEqual(events,["power-on"])
            events.append("serial-open")
            return Uart()
        with tempfile.TemporaryDirectory() as tmp, patch.dict(
            monitor.os.environ,{"EDP_PROFILE_KIND":"batch-worker"}), patch.object(
            monitor,"control",side_effect=power), patch.object(
            monitor.serial,"Serial",side_effect=serial_open):
            self.assertEqual(monitor.main(str(Path(tmp)/"monitor")),0)
        self.assertEqual(events,["power-on","serial-open","serial-enter","release"])
    def test_failed_power_never_opens_uart(self):
        from unittest.mock import patch
        import monitor_edls_provider_profile as monitor
        with tempfile.TemporaryDirectory() as tmp, patch.object(
            monitor,"control",side_effect=RuntimeError("power denied")), patch.object(
            monitor.serial,"Serial") as opened:
            with self.assertRaises(RuntimeError):
                monitor.main(str(Path(tmp)/"monitor"))
            opened.assert_not_called()
    def test_unknown_backup_policy_refused_before_hardware(self):
        import os,subprocess
        env=dict(os.environ,EDP_PROFILE_AGENT_ID="unit-test-no-hardware",
            EDP_PROFILE_EVIDENCE_DIR="/nonexistent/unit-test",EDP_PROFILE_BUILD_ENV="esp32dev-batch-worker",
            EDP_PROFILE_BACKUP_POLICY="invalid")
        result=subprocess.run(["bash",str(Path(__file__).with_name("run_edls_provider_profile.sh"))],
            text=True,capture_output=True,env=env,timeout=5)
        self.assertEqual(result.returncode,2)
        self.assertIn("Unsupported backup policy",result.stderr)

if __name__=="__main__":unittest.main()
