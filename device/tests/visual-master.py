#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao

"""Check all eight visualizer frames at identical musical times and four gains."""

from pathlib import Path
import subprocess
import tempfile
import wave

host = Path(__file__).resolve().parents[1] / "build/host/slap-5_host"
with tempfile.TemporaryDirectory() as directory:
    reference = None
    for gain in (4096, 2048, 410, 0):
        target = Path(directory) / str(gain)
        target.mkdir()
        commands = [
            "wait 6100", f"master {gain}", "wait 500",
            "tap SEQ", "key 7 down", "key 7 up", "tap HOME", "turn SELECT 4",
            "tap SEQ", "key 0 down", "key 0 up", "tap HOME", "turn SELECT -4",
            "tap HOME", "tap PLAY", "key 10 down", "wait 400",
            "wav music.wav",
        ]
        for visual in range(8):
            if visual:
                commands.append("turn SELECT 1")
            commands.extend(("wait 160", f"shot visual-{visual}.png"))
        commands += ["wavstop", "key 10 up"]
        script = target / "input.txt"
        script.write_text("\n".join(commands) + "\n")
        subprocess.run([host, script, target], check=True)
        frames = [(target / f"visual-{visual}.png").read_bytes() for visual in range(8)]
        if reference is None:
            reference = frames
        for visual, (actual, expected) in enumerate(zip(frames, reference)):
            assert actual == expected, f"Visualizer {visual} changed at master {gain}"
        with wave.open(str(target / "music.wav")) as wav:
            has_signal = any(wav.readframes(wav.getnframes()))
        assert has_signal == bool(gain), f"Unexpected audio signal at master {gain}"
print("All eight visualizers match at 100%, 50%, 10%, and 0%; zero master is silent.")
