#!/usr/bin/env python3
"""Summarize measured strict EDLS Publish provider boundaries, no inference."""
import argparse
import re
from pathlib import Path

def summarise(lines):
    wall = None
    provider = None
    observed = {}
    for line in lines:
        if "EDP_PROFILE|PASS|" in line:
            match = re.search(r"\|count=(\d+)\|ack=(\d+)\|window=255,256"
                              r"\|elapsed_us=(\d+)\|p50_us=(\d+)"
                              r"\|p95_us=(\d+)\|p99_us=(\d+)\|max_us=(\d+)",line)
            if match:
                count,ack,wall,p50,p95,p99,peak=map(int,match.groups())
        if "EDP_PROFILE|OP|" in line:
            match = re.search(r"\|name=(\w+)\|calls=(\d+)\|total_us=(\d+)"
                              r"\|max_call_us=(\d+)\|p50_generation_us=(\d+)"
                              r"\|p99_generation_us=(\d+)",line)
            if match:
                name=match[1]
                if name in observed:
                    raise ValueError("duplicate op: "+name)
                observed[name]=tuple(map(int,match.groups()[1:]))
        if "EDP_PROFILE|ATTRIBUTION|" in line:
            match=re.search(r"\|provider_total_us=(\d+)\|other_us=(\d+)"
                            r"\|provider_percent_x100=(\d+)\|timer_errors=(\d+)",line)
            if match: provider=tuple(map(int,match.groups()))
    if wall is None or provider is None or len(observed)!=8:
        raise ValueError("incomplete HIL profile transcript")
    if count!=256 or ack!=256 or provider[3]!=0 or provider[0]!=sum(t[1] for t in observed.values()):
        raise ValueError("unqualified result (count/ACK/timers/sum mismatch)")
    if abs(wall-provider[0]-provider[1])>50:
        raise ValueError("unaccounted provider wall clock mismatch")
    if not (0<p50<=p95<=p99<=peak):
        raise ValueError("invalid elapsed latency distribution")
    print(f"PROFILE_AUDIT=PASS records={count} ACK={ack} total_ms={wall/1000:.3f}"
          f" p50_ms={p50/1000:.3f} p95_ms={p95/1000:.3f} p99_ms={p99/1000:.3f}"
          f" throughput_rec_s={count*1e6/wall:.3f}")
    print("operation,calls,total_ms,percent_total_workload,avg_calls_record,mean_call_us,p50_per_record_ms,p99_per_record_ms,max_call_ms")
    for name,(calls,us,largest,median,p99in) in sorted(
            observed.items(),key=lambda item:-item[1][1]):
        print(f"{name},{calls},{us/1000:.3f},{us*100/wall:.2f},"
              f"{calls/count:.2f},{us/max(1,calls):.1f},"
              f"{median/1000:.3f},{p99in/1000:.3f},{largest/1000:.3f}")
    print(f"other_including_serialisation_and_test_validation,{0},{provider[1]/1000:.3f},"
          f"{provider[1]*100/wall:.2f}")
    return observed

if __name__=="__main__":
    cli=argparse.ArgumentParser()
    cli.add_argument("uart_log",type=Path)
    args=cli.parse_args()
    summarise(args.uart_log.read_text(errors="replace").splitlines())
