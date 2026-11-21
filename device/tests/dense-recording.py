#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao

"""Run the same seeded five-track recording with and without the voice worker."""

import argparse
import os
from pathlib import Path
import random
import subprocess
import tempfile
import wave

from pcm_parity import compare_pcm


def scenario(seed, bpm, bars, fx, visual, chords=False, arrangement=False, modulated=False, live_fx=False):
    rng = random.Random(seed)
    commands = ["wait 6100", f"turn ALGO {bpm - 112}", f"expect tempo {bpm}"]
    if bars > 1:
        steps = bars * 16
    else:
        steps = 16
    for track in ([0, 1, 4] if arrangement else range(5)):
        if track:
            commands.append(f"turn SELECT {3 if arrangement and track == 4 else 1}")
        commands.append(f"expect track {track + 1}")
        if steps > 16:
            commands += [f"turn K2 {steps - 16}", f"expect length {steps}"]
        if chords and track < 4:
            commands += ["tap SEL", "turn K3 1", "expect chord_mode 1", "tap HOME"]
        if modulated and track < 4:
            commands += ["tap LFO", "expect page 9", "turn K1 1", "turn K2 17",
                         "expect lfo_rate 48", "tap HOME"]
        commands += ["tap REC", "wait 6400", "expect rec_running 1"]
        for step in range(steps if bars == 1 else steps // 2):
            keys = rng.sample(range(8 if track == 4 else 19), rng.choice((2, 2, 3, 4)))
            if live_fx and track == 4:
                keys = [f"w{(step + offset) % 16}" for offset in (0, 5, 11)]
            commands += [f"key {key} down" for key in keys]
            commands.append(f"wait {rng.randrange(8, 75)}")
            commands += [f"key {key} up" for key in reversed(keys)]
            commands.append(f"wait {rng.randrange(28, 115)}")
        commands += ["tap REC", "expect rec_running 0", "expect playing 1"]
    commands += ["expect recorded >35", "expect events >35"]
    if fx:
        commands += ["tap FX", "turn SELECT -4", "turn ALGO 2", "expect fx_page 3"]
        for track in range(5):
            if track:
                commands.append("turn SELECT 1")
            commands += ["turn K1 2", "expect fx_k1 2", "turn K2 5", "turn ALGO -2", "turn K1 50",
                         "turn K2 45", "turn K3 40", "turn K4 30", "turn ALGO 2"]
        commands.append("tap HOME")
    if arrangement:
        commands += ["tap FX", "turn ALGO 2", "expect fx_page 3", "turn K1 1", "expect fx_k1 1", "turn K2 5", "tap HOME"]
    if visual:
        commands += ["tap HOME", "turn SELECT 6", "expect page 6"]
    return commands + ["wav dense.wav", "wait 10000", "wavstop"]


def extreme_scenario(seed, bpm, soak_ms=60000):
    commands = ["wait 6100", "tap FX", "turn ALGO 2"]
    for track, insert in enumerate((6, 7, 5, 4, 7)):
        if track:
            commands.append("turn SELECT 1")
        commands += ["expect fx_page 3", "turn K1 2", "expect fx_k1 2",
                     "turn K2 5", "turn ALGO 1", f"turn K1 {insert}",
                     f"expect fx_k1 {insert}", "turn ALGO 2", "turn K1 25",
                     "turn ALGO 1", "turn K1 30", "turn ALGO -4"]
    commands += ["turn SELECT -4", "tap HOME"]
    dense = scenario(seed, bpm, 1, False, False, chords=True,
                     modulated=True, live_fx=True)
    commands += dense[1:-3]
    commands += ["tap FX", "tap FX", "expect page 14", "wav dense.wav"]
    for key in range(11):
        commands += [f"key b{key} down", f"expect live_fx {key + 1}",
                     "wait 860", f"key b{key} up", "expect live_fx 0", "wait 100"]
    commands += ["tap HOME", "tap HOME", "expect page 6",
                 f"wait {max(10000, soak_ms)}", "wavstop", "expect playing 1"]
    return commands


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=519331)
    parser.add_argument("--bpm", type=int, choices=range(40, 241), metavar="40..240", default=112)
    parser.add_argument("--bars", type=int, choices=(1, 16, 32), default=1)
    parser.add_argument("--fx", action="store_true")
    parser.add_argument("--visual", action="store_true")
    parser.add_argument("--live-fx", action="store_true", help="Exercise all black-key effects during replay")
    parser.add_argument("--chords", action="store_true")
    parser.add_argument("--voice-split", choices=("cost", "slots", "tracks"), default="cost")
    parser.add_argument("--async-worker", action="store_true")
    parser.add_argument("--modulated", action="store_true")
    parser.add_argument("--soak-ms", type=int, default=0)
    parser.add_argument("--arrangement", action="store_true", help="Use two synth tracks and drum slicer")
    parser.add_argument("--extreme", action="store_true", help="Five wet tracks, LIVE FX and visualizer soak")
    args = parser.parse_args()
    host = Path(__file__).resolve().parents[1] / "build/host/slap-5_host"
    commands = scenario(args.seed, args.bpm, args.bars, args.fx, args.visual, args.chords, args.arrangement, args.modulated, args.live_fx)
    if args.extreme:
        commands = extreme_scenario(args.seed, args.bpm, args.soak_ms or 60000)
    elif args.live_fx:
        commands = commands[:-3]
        commands += ["tap FX", "wait 450", "tap FX", "tap FX", "expect page 14", "wav dense.wav"]
        for key in range(11):
            commands += [f"key b{key} down", f"expect live_fx {key + 1}",
                         "wait 860", f"key b{key} up", "expect live_fx 0", "wait 100"]
        commands += ["wavstop"]
    if args.soak_ms < 0:
        parser.error("--soak-ms must be nonnegative")
    if args.soak_ms and not args.extreme:
        commands += [f"wait {args.soak_ms}"]
    with tempfile.TemporaryDirectory() as work_path:
        work = Path(work_path)
        wavs = {}
        for mode in ("worker", "single"):
            directory = work / mode
            directory.mkdir()
            script = directory / "case.txt"
            worker_assert = "expect worker_jobs >0" if mode == "worker" else "expect worker_jobs 0"
            assertions = [worker_assert, "expect peak_voices 16", "expect voice_steals >0"]
            if mode == "worker" and args.async_worker:
                # A general-purpose OS can preempt the worker. Verify actual
                # overlap and PCM parity here. The deterministic failure tests
                # verify which faults retire the worker and which can drain.
                assertions += ["expect worker_overlap_us >0", "expect worker_wait_us >-1",
                               "expect worker_work_us >0", "expect worker_voice_peak_us >0",
                               "expect worker_fallbacks >-1", "expect worker_online >-1"]
            else:
                assertions += ["expect worker_fallbacks 0"]
            script.write_text("\n".join([*commands, *assertions]) + "\n")
            env = {key: value for key, value in os.environ.items() if not key.startswith("SLAP_TEST_")}
            if mode == "worker":
                env["SLAP_TEST_VOICE_SPLIT"] = args.voice_split
            if mode == "worker" and args.async_worker:
                env["SLAP_TEST_ASYNC"] = "1"
            if mode == "single":
                env["SLAP_TEST_SINGLE_CORE"] = "1"
            subprocess.run([host, script, directory], env=env, check=True)
            wavs[mode] = directory / "dense.wav"
        compare_pcm(wavs["worker"], wavs["single"])
        with wave.open(str(work / "worker/dense.wav")) as audio:
            assert audio.getnframes() >= audio.getframerate() * 9
    print(f"Dense recording and replay matched at {args.bpm} BPM, seed {args.seed}, {args.bars} bars.")


if __name__ == "__main__":
    main()
