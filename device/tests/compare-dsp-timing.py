#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Measure isolated host audio callback times for two SLAP-5 source revisions."""

import argparse
import array
import os
from pathlib import Path
import re
import runpy
import statistics
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TIMER = "    dt = now_ns() - t0;\n    render_ns_total += dt;"
TRACE = """    dt = now_ns() - t0;
    static FILE *profile;
    static int profile_initialized;
    if (!profile_initialized) {
        const char *path = getenv("SLAP_DSP_TRACE");
        if (path) profile = fopen(path, "wb");
        profile_initialized = 1;
    }
    if (profile) fwrite(&dt, sizeof(dt), 1, profile);
    render_ns_total += dt;"""


def build(revision, destination, cpu_time=False):
    destination.mkdir()
    archive = subprocess.check_output(
        ["git", "archive", revision, "device", "vendor/fm1-base"], cwd=ROOT
    )
    subprocess.run(["tar", "-xf", "-", "-C", destination], input=archive, check=True)
    host = destination / "vendor/fm1-base/host/plat_host.c"
    source = host.read_text()
    assert source.count(TIMER) == 1, "Host callback timing hook changed"
    source = source.replace(TIMER, TRACE)
    if cpu_time:
        clock = "static double now_ns(void)\n{\n    struct timespec t;\n    clock_gettime(CLOCK_MONOTONIC, &t)"
        assert source.count(clock) == 1, "Host callback clock changed"
        source = source.replace(clock, clock.replace("CLOCK_MONOTONIC", "CLOCK_THREAD_CPUTIME_ID"))
    host.write_text(source)
    subprocess.run(
        [sys.executable, "vendor/fm1-base/tools/build.py", "device", "--host"],
        cwd=destination, check=True, stdout=subprocess.DEVNULL,
    )
    return destination / "device/build/host/slap-5_host"


def measure(binary, script, destination, single, full=False, async_worker=False):
    destination.mkdir()
    trace = destination / "trace.bin"
    env = {k: v for k, v in os.environ.items() if not k.startswith("SLAP_TEST_")}
    env["SLAP_DSP_TRACE"] = str(trace)
    if single:
        env["SLAP_TEST_SINGLE_CORE"] = "1"
    if async_worker and not single:
        env["SLAP_TEST_ASYNC"] = "1"
    output = subprocess.check_output([binary, script, destination], env=env, text=True)
    counters = dict(re.findall(r"^  ok (worker_\w+|fx_worker_\w+) == (\d+) \(>-1\)$", output, re.M))
    samples = array.array("d")
    samples.frombytes(trace.read_bytes())
    # Use the same bounded tail across revisions. Patch-switch includes overdubbing.
    assert len(samples) >= 1600
    played = sorted(nanoseconds / 1000 for nanoseconds in (samples if full else samples[-1600:]))
    p95 = played[(95 * len(played) - 1) // 100]
    p99 = played[(99 * len(played) - 1) // 100]
    over_budget = sum(value > 256 * 1e6 / 44100 for value in played)
    return (statistics.mean(played), p95, p99, played[-1]), over_budget, counters


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline")
    parser.add_argument("redesign")
    parser.add_argument("--reference", help="Optional third source revision to compare")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cpu-time", action="store_true", help="Measure synchronous host thread CPU time")
    parser.add_argument("--async-worker", action="store_true", help="Use the pthread-backed host worker and wall time")
    parser.add_argument("--full", action="store_true", help="Include recording and playback in the timing window")
    parser.add_argument("--chords", action="store_true")
    parser.add_argument("--workload", choices=("dry", "wet", "arrangement", "extreme", "patch-switch", "filter-neutral", "filter-active"))
    parser.add_argument("--legacy-settings-baseline", action="store_true", help="Use legacy SETTINGS controls on 0.1.26 and earlier")
    parser.add_argument("--modulated", action="store_true", help="Enable sine LFOs in the wet fixture")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    if args.async_worker and args.cpu_time:
        parser.error("--async-worker requires wall time to include synchronization")
    fixtures = runpy.run_path(str(Path(__file__).with_name("dense-recording.py")))
    print(f"{'Thread CPU' if args.cpu_time else 'Wall'} time, "
          f"{'full fixture' if args.full else 'last 1600 callbacks'}; "
          f"{'pthread' if args.async_worker else 'synchronous inline'} host worker", flush=True)
    with tempfile.TemporaryDirectory() as directory:
        scratch = Path(directory)
        revisions = [("baseline", args.baseline), ("redesign", args.redesign)]
        if args.reference:
            revisions.append(("reference", args.reference))
        binaries = {label: build(revision, scratch / label, args.cpu_time) for label, revision in revisions}
        workloads = [("dry", 519331, 112, False), ("wet", 220507, 137, True),
                     ("arrangement", 519331, 160, False)]
        if args.workload == "extreme":
            workloads = [("extreme", 519331, bpm, True) for bpm in (112, 137, 160)]
        if args.workload in ("patch-switch", "filter-neutral", "filter-active"):
            workloads = [(args.workload, 519331, bpm, True) for bpm in (112, 137, 160)]
        for name, seed, bpm, fx in workloads:
            if args.workload and name != args.workload:
                continue
            case = f"{name}-{bpm}"
            script = scratch / f"{case}.txt"
            if name == "patch-switch":
                commands = fixtures["patch_switch_scenario"](seed, bpm)
            elif name in ("filter-neutral", "filter-active"):
                commands = fixtures["filter_visual_scenario"](seed, bpm, name == "filter-active")
            elif name == "extreme":
                commands = fixtures["extreme_scenario"](seed, bpm)
            else:
                arrangement = name == "arrangement"
                commands = fixtures["scenario"](seed, bpm, 1, fx, fx, chords=args.chords or arrangement,
                                                arrangement=arrangement, modulated=fx and args.modulated)
            if args.async_worker:
                commands += [
                    f"expect {name} >-1" for name in ("worker_jobs", "worker_fallbacks",
                        "fx_worker_jobs", "fx_worker_fallbacks", "worker_wait_us", "worker_work_us")
                ]
            script.write_text("\n".join(commands) + "\n")
            baseline_script = script
            if args.legacy_settings_baseline:
                baseline_script = scratch / f"{case}-legacy.txt"
                legacy = commands[:]
                start = legacy.index("tap GLO")
                assert legacy[start + 1:start + 4] == ["turn ALGO 1", "turn SELECT 2", "turn K1 1"]
                legacy[start + 1:start + 4] = ["turn SELECT 1", "turn K3 1"]
                baseline_script.write_text("\n".join(legacy) + "\n")
            for single in (False, True):
                mode = "single" if single else "worker"
                results = {label: [] for label in binaries}
                for run in range(args.runs):
                    labels = list(binaries)[::1 if run % 2 == 0 else -1]
                    for label in labels:
                        destination = scratch / f"{case}-{mode}-{label}-{run}"
                        results[label].append(measure(binaries[label], baseline_script if label == "baseline" else script, destination, single, args.full, args.async_worker))
                for label, trials in results.items():
                    rows, overruns, counters = zip(*trials)
                    stats = " / ".join(f"{statistics.median(values):.2f}" for values in zip(*rows))
                    worst = max(row[-1] for row in rows)
                    print(f"{case} {mode} {label}   median mean/P95/P99/peak µs   {stats}"
                          f"   worst {worst:.2f}   host callbacks over 5805 us {sum(overruns)}", flush=True)
                    if mode == "worker" and args.async_worker:
                        for trial in counters:
                            print("  " + " ".join(f"{key}={value}" for key, value in trial.items()), flush=True)


if __name__ == "__main__":
    main()
