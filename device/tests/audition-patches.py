#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Render all factory patches through the desktop firmware host."""

import argparse
import array
import csv
import math
from pathlib import Path
import re
import subprocess
import sys
import wave


def note(pitches, velocity, hold_ms, release_ms):
    return [
        *(f"midi 90 {pitch:02x} {velocity:02x}" for pitch in pitches),
        f"wait {hold_ms}",
        *(f"midi 80 {pitch:02x} 00" for pitch in pitches),
        f"wait {release_ms}",
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", type=Path, help="Compiled slap-5_host executable")
    parser.add_argument("output", type=Path, help="Output directory for WAVs and index")
    args = parser.parse_args()
    names = re.findall(r'^\s*\{"([^"]+)",', (Path(__file__).resolve().parents[1] /
                                             "dsp/patches.c").read_text(), re.M)
    if len(names) != 128:
        parser.error("Expected 128 factory patches")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    commands = ["wait 6100", "turn PRESET -127 16"]
    phrase = [
        *note([42], 58, 150, 300),
        *note([60], 88, 450, 400),
        *note([78], 112, 230, 350),
        *note([55, 58, 62, 65], 96, 680, 1200),
    ]
    for index in range(len(names)):
        if index:
            commands.append("turn PRESET 1")
        commands.extend([f"expect patch {index}", f"wav patch-{index:03d}.wav",
                         *phrase, "wavstop", "wait 4000"])
    script = output / "auditions.txt"
    script.write_text("\n".join(commands) + "\n")
    with (output / "host.log").open("w") as log:
        subprocess.run([str(args.host.resolve()), str(script), str(output)],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    with (output / "index.csv").open("w", newline="") as index_file:
        index = csv.writer(index_file)
        index.writerow(["patch_id", "name", "file", "peak_dbfs", "rms_dbfs"])
        for patch_id, name in enumerate(names):
            path = output / f"patch-{patch_id:03d}.wav"
            with wave.open(str(path)) as audio:
                if audio.getsampwidth() != 2:
                    raise ValueError(f"Expected 16-bit PCM in {path.name}")
                samples = array.array("h", audio.readframes(audio.getnframes()))
            if sys.byteorder != "little":
                samples.byteswap()
            peak = max(map(abs, samples), default=0)
            if not peak:
                raise ValueError(f"Silent audition for patch {patch_id}: {name}")
            rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples))
            index.writerow([patch_id, name, path.name,
                            f"{20 * math.log10(peak / 32768):.2f}",
                            f"{20 * math.log10(rms / 32768):.2f}"])


if __name__ == "__main__":
    main()
