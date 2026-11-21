#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Compare dense playback PCM through repeated EDIT entry and exit."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

import runpy
from pcm_parity import compare_pcm

scenario = runpy.run_path(str(Path(__file__).with_name("dense-recording.py")))["scenario"]


def capture(binary, destination, bpm, track, patched, enter_edit):
    destination.mkdir()
    setup = scenario(519331, bpm, 1, True, False, chords=True, modulated=True, live_fx=True)[:-3]
    if not patched:
        start = setup.index("tap GLO")
        setup[start + 1:start + 4] = ["turn SELECT 1", "turn K3 1"]
    setup += [f"turn SELECT {track - 5}", f"expect track {track}",
              "wav playback.wav", "wait 240"]
    transition = ["tap EDIT", "expect page 3", "wait 450", "tap HOME", "expect page 0"] if enter_edit else ["wait 610"]
    script = destination / "fixture.txt"
    script.write_text("\n".join(setup + transition * 4 + ["wait 240", "wavstop", "expect playing 1"]) + "\n")
    output = subprocess.check_output([binary, script, destination], text=True)
    count = int(re.search(r"(\d+) blits, render", output)[1])
    return destination / "playback.wav", count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for patched, binary in ((False, args.baseline), (True, args.candidate)):
            if binary is None:
                continue
            for bpm in (112, 137, 160):
                for track in (5, 1, 2):
                    label = f"{patched}-{bpm}-{track}"
                    control, idle_blits = capture(binary.resolve(), root / (label + "-control"), bpm, track, patched, False)
                    edit, edit_blits = capture(binary.resolve(), root / (label + "-edit"), bpm, track, patched, True)
                    compare_pcm(control, edit)
                    print(f"{'candidate' if patched else 'baseline'} {bpm} BPM track {track}: PCM match; HOME/EDIT fixture blits {idle_blits}/{edit_blits}")


if __name__ == "__main__":
    main()
