#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check that Sound Lab editor changes affect the graph and rendered audio."""

from array import array
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import wave

from PIL import Image


def render(host, directory, controls, changes):
    directory.mkdir()
    script = directory / "sound.txt"
    lines = ["wait 6100", *controls, *changes, "wait 120",
             "wav note.wav", "key w2 down", "wait 500", "shot graph.png",
             "key w2 up", "wait 150", "wavstop"]
    script.write_text("\n".join(lines) + "\n")
    subprocess.run([host, script, directory], check=True)
    with wave.open(str(directory / "note.wav")) as audio:
        assert audio.getsampwidth() == 2
        pcm = array("h")
        pcm.frombytes(audio.readframes(audio.getnframes()))
        assert max(map(abs, pcm)) > 30
    with Image.open(directory / "graph.png") as image:
        assert image.size == (240, 240)
        graph = image.crop((0, 48, 240, 188)).tobytes()
    return pcm, graph


def main():
    host = Path(sys.argv[1]).resolve()
    scenarios = {
        "fm6-routing": (
            ["turn SELECT 1", "tap EDIT", "turn ALGO 2", "turn ALGO -1"],
            ["turn K4 12"],
        ),
        "wavetable-morph-scan": (
            ["turn SELECT 3", "turn PRESET -25", "expect patch 12", "tap EDIT", "turn ALGO 1"],
            ["turn K1 127", "turn K2 127", "expect wave_morph 127", "expect wave_scan 127"],
        ),
    }
    scenarios["expression-vibrato"] = (
        ["tap EDIT", "press SEL", "turn ALGO 1", "release SEL", "turn K4 14"],
        ["turn K3 18"],
    )
    with TemporaryDirectory() as directory:
        root = Path(directory)
        for label, (controls, changes) in scenarios.items():
            baseline, before = render(host, root / (label + "-baseline"), controls, [])
            edited, after = render(host, root / (label + "-edited"), controls, changes)
            assert len(baseline) == len(edited), label
            assert before != after, label
            # Compare the same steady-state interval, excluding host button edges.
            window = slice(12000, 28000)
            delta = sum((a - b) ** 2 for a, b in zip(baseline[window], edited[window]))
            signal = sum(a * a for a in baseline[window])
            assert signal and delta > signal / 100, (label, delta, signal)
            print(label, "graph and rendered held-note audio both changed")


if __name__ == "__main__":
    main()
