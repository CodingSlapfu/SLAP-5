#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Compare host dispatch prototypes with the same DSP and input script."""
import argparse
import array
import os
from pathlib import Path
import runpy
import statistics
import subprocess
import tempfile

from pcm_parity import compare_pcm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("revision")
    parser.add_argument("--runs", default=3, type=int)
    parser.add_argument("--threaded", action="store_true")
    arguments = parser.parse_args()
    if arguments.runs < 1:
        parser.error("--runs must be positive")
    test_dir = Path(__file__).resolve().parent
    build = runpy.run_path(str(test_dir / "compare-dsp-timing.py"))["build"]
    scenario = runpy.run_path(str(test_dir / "dense-recording.py"))["scenario"]
    print("Wall clock, real host worker" if arguments.threaded else "Thread CPU, synchronous worker", flush=True)
    fields = ["worker_jobs", "worker_overlap_us", "worker_wait_us", "worker_fallbacks", "worker_online"]
    modes = ("cost", "slots", "tracks")
    with tempfile.TemporaryDirectory() as scratch:
        root = Path(scratch)
        binary = build(arguments.revision, root / "build", cpu_time=not arguments.threaded)
        for workload, seed, bpm, fx in [("dry", 519331, 112, False), ("wet", 220507, 137, True),
                                        ("gate", 519331, 160, False)]:
            script = root / (workload + ".txt")
            commands = scenario(seed, bpm, 1, fx, fx, chords=True, arrangement=workload == "gate", modulated=fx)
            script.write_text("\n".join(commands + [f"expect {name} >-1" for name in fields]) + "\n")
            results = {mode: [] for mode in modes}
            parity = {mode: True for mode in modes}
            for repeat in range(arguments.runs):
                renders = {}
                for mode in modes[repeat % 3:] + modes[:repeat % 3]:
                    dest = root / f"{workload}-{mode}-{repeat}"
                    dest.mkdir()
                    trace = dest / "times.bin"
                    env = {k: v for k, v in os.environ.items() if not k.startswith("SLAP_TEST_")}
                    env.update(SLAP_DSP_TRACE=str(trace), SLAP_TEST_VOICE_SPLIT=mode)
                    if arguments.threaded:
                        env["SLAP_TEST_ASYNC"] = "1"
                    run = subprocess.run([binary, script, dest], env=env, text=True, capture_output=True)
                    if run.returncode:
                        raise RuntimeError(run.stdout + run.stderr)
                    measured = array.array("d")
                    measured.frombytes(trace.read_bytes())
                    times = sorted(time / 1000 for time in measured)
                    stats = [statistics.mean(times), times[(95 * len(times) - 1) // 100],
                             times[(99 * len(times) - 1) // 100], times[-1]]
                    counters = dict(line.split(" == ", 1) for line in run.stdout.splitlines() if line.startswith("  ok worker_"))
                    stats.extend(int(counters["  ok " + name].split()[0]) for name in fields)
                    results[mode].append(stats)
                    renders[mode] = dest / "dense.wav"
                for mode in modes[1:]:
                    try:
                        compare_pcm(renders["cost"], renders[mode])
                    except AssertionError:
                        parity[mode] = False
            for mode in modes:
                medians = " / ".join(f"{statistics.median(values):.2f}" for values in zip(*results[mode]))
                print(f"{workload} {mode} mean/P95/P99/max/jobs/overlap/wait/fallbacks/online {medians} PCM={parity[mode]}", flush=True)


if __name__ == "__main__":
    main()
