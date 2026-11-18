#!/usr/bin/env python3
"""RPI400 user-qualified GPIO17 true VDD cut for NVS EDLS snapshot retention.

Only the Policy-owned esp32-integrated daemon switches VDD; no direct GPIO.
Monitor captures ACKed generations BEFORE relay cut, checks reboot after
power-on reset, and requires recovered generation >= last visible ACK.
"""
import pathlib
import re
import serial
import subprocess
import sys
import time

CONTROL = "/srv/espressio/bin/esp32-integrated"
def command(kind):
    process=subprocess.run(
        [CONTROL,kind],text=True,capture_output=True,check=True,timeout=10
    )
    print(f"HOST|{kind}|{process.stdout.strip()}",flush=True)

def main(folder):
    path=pathlib.Path(folder)
    path.mkdir(parents=True,exist_ok=False)
    acked=0
    cut=False
    second_boot=False
    boot_count=0
    saw_recovery=False
    recovered=0
    raw=bytearray()
    buffered=bytearray()
    started=time.monotonic()
    command("status")
    command("power-on")
    uart=serial.Serial("/dev/serial0",115200,timeout=0.1)
    try:
        uart.reset_input_buffer()
        with (path/"serial.raw").open("wb") as rawlog,(path/"serial.txt").open("w",encoding="utf8") as log:
            command("release")
            while time.monotonic()-started<105.0:
                chunk=uart.read(4096)
                if not chunk:continue
                rawlog.write(chunk);rawlog.flush()
                for octet in chunk:
                    if octet!=10 and len(buffered)<4095:
                        buffered.append(octet)
                        continue
                    line=buffered.decode("utf8",errors="replace").strip()
                    buffered.clear()
                    if not line:continue
                    log.write(line+"\n");log.flush()
                    if "EDP_JOURNAL|" not in line:continue
                    line=line[line.index("EDP_JOURNAL|"):]
                    print(f"UART|{time.monotonic()-started:.4f}|{line}",flush=True)
                    if "|BOOT|" in line:
                        boot_count+=1
                        if cut:second_boot=True
                    if "|ACK|gen=" in line and not cut:
                        m=re.search(r"\|ACK\|gen=(\d+)",line)
                        if m:acked=max(acked,int(m.group(1)))
                    if "|COMMIT_ENTER|gen=" in line and not cut:
                        n=re.search(r"\|COMMIT_ENTER\|gen=(\d+)",line)
                        if n and int(n.group(1))>=8:
                            assert acked>=3
                            print(f"HOST|CUT_AT_ATTEMPT={n.group(1)}|LAST_ACKED={acked}",flush=True)
                            cut=True
                            command("power-off")
                            then=time.monotonic()
                            time.sleep(8.0)
                            command("status")
                            command("power-on")
                            print(f"HOST|OFF_DURATION_S={time.monotonic()-then:.3f}",flush=True)
                    if cut and "|RECOVERY|" in line:
                        m=re.search(r"\|RECOVERY\|status=\d+\|gen=(\d+)",line)
                        if m:
                            recovered=int(m.group(1))
                            saw_recovery=True
                    if cut and "|PASS|POWERLOSS_HISTORY_VALID" in line:
                        ok=second_boot and saw_recovery and recovered>=acked
                        print(f"HOST|CHECK_SECOND_BOOT={second_boot}|LAST_ACKED={acked}|RECOVERED={recovered}|PASS={ok}",flush=True)
                        return 0 if ok else 4
                    if "|HALT|" in line:
                        print("HOST|FIRMWARE_HALT=FAIL",flush=True)
                        return 5
        print(f"HOST|TIMEOUT|cut={cut}|ack={acked}|recover={recovered}",flush=True)
        return 6
    finally:
        uart.close()
        command("power-off")

if __name__=="__main__":
    try:
        if len(sys.argv)!=2:
            raise ValueError("one unique evidence path required")
        sys.exit(main(sys.argv[1]))
    except Exception as err:
        print("HOST|FAIL",repr(err),flush=True)
        sys.exit(9)
