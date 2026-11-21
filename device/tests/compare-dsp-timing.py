#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Measure isolated host audio callback times for two SLAP-5 source revisions."""

import argparse
import array
import os
from pathlib import Path
import runpy
import statistics
import subprocess
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
        ["python3", "vendor/fm1-base/tools/build.py", "device", "--host"],
        cwd=destination, check=True, stdout=subprocess.DEVNULL,
    )
    return destination / "device/build/host/slap-5_host"


def measure(binary, script, destination, single, full=False):
    destination.mkdir()
    trace = destination / "trace.bin"
    env = {k: v for k, v in os.environ.items() if not k.startswith("SLAP_TEST_")}
    env["SLAP_DSP_TRACE"] = str(trace)
    if single:
        env["SLAP_TEST_SINGLE_CORE"] = "1"
    subprocess.run([binary, script, destination], env=env, check=True, stdout=subprocess.DEVNULL)
    samples = array.array("d")
    samples.frombytes(trace.read_bytes())
    # The fixture ends with ten seconds of steady playback after recording.
    assert len(samples) >= 1600
    played = sorted(nanoseconds / 1000 for nanoseconds in (samples if full else samples[-1600:]))
    return statistics.mean(played), played[(95 * len(played) - 1) // 100], played[(99 * len(played) - 1) // 100], played[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline")
    parser.add_argument("redesign")
    parser.add_argument("--reference", help="Optional third source revision to compare")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cpu-time", action="store_true", help="Measure synchronous host thread CPU time")
    parser.add_argument("--full", action="store_true", help="Include recording and playback in the timing window")
    parser.add_argument("--chords", action="store_true")
    parser.add_argument("--workload", choices=("dry", "wet", "arrangement", "extreme", "patch-switch"))
    parser.add_argument("--modulated", action="store_true", help="Enable sine LFOs in the wet fixture")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    fixtures = runpy.run_path(str(Path(__file__).with_name("dense-recording.py")))
    print(f"{'Thread CPU' if args.cpu_time else 'Wall'} time, "
          f"{'full recording and playback' if args.full else 'steady playback'}", flush=True)
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
        if args.workload == "patch-switch":
            workloads = [("patch-switch", 519331, bpm, True) for bpm in (112, 137, 160)]
        for name, seed, bpm, fx in workloads:
            if args.workload and name != args.workload:
                continue
            case = f"{name}-{bpm}"
            script = scratch / f"{case}.txt"
            if name == "patch-switch":
                commands = fixtures["patch_switch_scenario"](seed, bpm)
            elif name == "extreme":
                commands = fixtures["extreme_scenario"](seed, bpm)
            else:
                arrangement = name == "arrangement"
                commands = fixtures["scenario"](seed, bpm, 1, fx, fx, chords=args.chords or arrangement,
                                                arrangement=arrangement, modulated=fx and args.modulated)
            script.write_text("\n".join(commands) + "\n")
            for single in (False, True):
                mode = "single" if single else "worker"
                results = {label: [] for label in binaries}
                for run in range(args.runs):
                    labels = list(binaries)[::1 if run % 2 == 0 else -1]
                    for label in labels:
                        destination = scratch / f"{case}-{mode}-{label}-{run}"
                        results[label].append(measure(binaries[label], script, destination, single, args.full))
                for label, rows in results.items():
                    stats = " / ".join(f"{statistics.median(values):.2f}" for values in zip(*rows))
                    print(f"{case} {mode} {label}   mean/P95/P99/peak µs   {stats}", flush=True)


if __name__ == "__main__":
    main()
