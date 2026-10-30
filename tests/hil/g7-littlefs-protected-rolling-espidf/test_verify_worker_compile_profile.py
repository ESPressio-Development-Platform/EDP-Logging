"""Compiler command evidence must reject disabled checks and wrong profiles."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

class NativeCompileProfileTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.arguments = [
            "compiler", "-O2", "-DEDP_TEST_BATCH_WORKER",
            "-DEDP_WORKER_DWELL_NS=500000000LL", "-DEDP_WORKER_OPTIMISED_BUILD=1"]
        self.config = [
            "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=y",
            "CONFIG_COMPILER_OPTIMIZATION_ASSERTION_LEVEL=2",
            "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160"]
    def run_check(self):
        database = self.root / "commands.json"
        database.write_text(json.dumps([{"file":"src/main.cpp","arguments":self.arguments}]))
        sdkconfig = self.root / "sdkconfig"
        sdkconfig.write_text("\n".join(self.config)+"\n")
        output = self.root / "profile.json"
        output.unlink(missing_ok=True)
        result = subprocess.run(
            [sys.executable,str(Path(__file__).with_name("verify_worker_compile_profile.py")),
             "--database",str(database),"--sdkconfig",str(sdkconfig),"--output",str(output)],
            text=True,capture_output=True,timeout=5)
        return result,output
    def test_exact_o2_retained_assertions_and_same_cpu(self):
        result,output = self.run_check()
        self.assertEqual(result.returncode,0,result.stderr)
        report=json.loads(output.read_text())
        self.assertEqual(report["status"],"Verified")
        self.assertEqual(report["optimisation_flags"],["-O2"])
        self.assertEqual(report["cpu_mhz"],160)
    def test_debug_or_size_flag_cannot_be_hidden_beside_o2(self):
        for flag in ("-Og","-Os","-O0","-O1","-Ofast"):
            with self.subTest(flag=flag):
                self.arguments=["compiler","-O2",flag,*self.arguments[2:]]
                result,output=self.run_check()
                self.assertNotEqual(result.returncode,0)
                self.assertFalse(output.exists())
                self.arguments=["compiler","-O2",*self.arguments[3:]]
    def test_ndebug_refused(self):
        self.arguments.append("-DNDEBUG=1")
        result,output=self.run_check()
        self.assertNotEqual(result.returncode,0)
        self.assertFalse(output.exists())
    def test_assertions_disabled_refused(self):
        self.config[0]="# CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE is not set"
        result,output=self.run_check()
        self.assertNotEqual(result.returncode,0)
        self.assertFalse(output.exists())
    def test_cpu_change_is_not_a_controlled_o2_sample(self):
        self.config[2]="CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240"
        result,output=self.run_check()
        self.assertNotEqual(result.returncode,0)
        self.assertFalse(output.exists())
    def test_wrong_dwell_or_missing_optimisation_witness_refused(self):
        for missing in ("-DEDP_WORKER_DWELL_NS=500000000LL","-DEDP_WORKER_OPTIMISED_BUILD=1"):
            original=list(self.arguments)
            self.arguments.remove(missing)
            result,output=self.run_check()
            self.assertNotEqual(result.returncode,0)
            self.assertFalse(output.exists())
            self.arguments=original
    def test_monitor_requires_o2_and_matching_cpu_receipt(self):
        from monitor_edls_provider_profile import PATTERNS
        text="EDP_WORKER|PASS|scheduled_records=2000|producers=2|target_rps=100|elapsed_us=1|scheduled_dwell_ns=500000000|application_optimisation=o2|cpu_mhz=160|ack_segment=5"
        pattern=PATTERNS["batch-worker-500-o2"]
        self.assertIsNotNone(pattern.search(text))
        for bad in (text.replace("=o2","=og"),text.replace("=160","=240"),
                    text.replace("500000000","100000000")):
            self.assertIsNone(pattern.search(bad))

if __name__=="__main__":
    unittest.main()
