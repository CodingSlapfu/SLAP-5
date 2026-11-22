#!/usr/bin/env python3
"""Verify external clock cadence with jitter and tempo changes in the host."""
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


def main():
    host = Path(sys.argv[1]).resolve()
    steps = ["wait 6100", "tap GLO", "turn ALGO 8", "turn K1 1",
             "turn SELECT 1", "turn K1 1", "turn ALGO 1", "turn K1 1",
             "turn SELECT 2", "turn K1 1", "midi FA 00 00", "wait 20",
             "midi-clock-burst 1", "expect step 0"]
    position = 0
    for tempo, duration in [(112, 134), (137, 109), (160, 94)]:
        for index in range(512):
            jitter = (index % 8 - 3) * 2 - 1
            steps += [f"wait {duration - 8 + jitter}", "midi-clock-burst 6"]
            position += 1
            if index % 64 == 63:
                steps += [f"expect step {position}", f"expect tempo >{tempo - 5}",
                          f"expect tempo <{tempo + 5}"]
    steps += ["expect midi_drops 0", "expect clocks_out 0", "midi FC 00 00",
              "wait 20", "expect playing 0"]
    with TemporaryDirectory() as directory:
        fixture = Path(directory) / "midi-clock-stress.txt"
        fixture.write_text("\n".join(steps) + "\n")
        subprocess.run([host, fixture, directory], check=True)


if __name__ == "__main__":
    main()
