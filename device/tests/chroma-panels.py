#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Check native-size CHROMA editor headers through theme and page transitions."""

from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

from PIL import Image


def main():
    host = Path(sys.argv[1]).resolve()
    graph_fixture = Path(__file__).with_name("graph-navigation.txt").read_text().splitlines()
    assert graph_fixture[0] == "wait 6100"
    panels = {
        "seq": ["tap SEQ", "expect page 1"],
        "sound": ["tap EDIT", "expect page 3"],
        "env": ["tap ENV", "expect page 4"],
        "lfo": ["tap LFO", "expect page 9"],
        "live": ["tap FX", "tap FX", "expect page 14"],
        "mixer": ["tap HOME", "press EDIT", "wait 2100", "release EDIT", "expect page 11"],
    }
    signatures = {panel: set() for panel in panels}
    with TemporaryDirectory() as directory:
        root = Path(directory)
        for theme in range(12):
            output = root / str(theme)
            output.mkdir()
            lines = ["wait 6100", "tap GLO", "turn ALGO 5", "turn SELECT 2",
                     f"turn K1 {theme}", f"expect theme {theme}", "tap HOME"]
            setup = lines.copy()
            for panel, controls in panels.items():
                lines += controls + ["wait 125", f"shot {panel}.png",
                                     "wait 125", f"shot {panel}-settled.png"]
            fixture = output / "panels.txt"
            fixture.write_text("\n".join(lines) + "\n")
            subprocess.run([host, fixture, output], check=True)
            for panel in panels:
                with Image.open(output / f"{panel}.png") as image, \
                     Image.open(output / f"{panel}-settled.png") as settled:
                    assert image.size == settled.size == (240, 240), (theme, panel)
                    header = image.crop((0, 0, 240, 48)).tobytes()
                    assert header == settled.crop((0, 0, 240, 48)).tobytes(), (theme, panel)
                    signatures[panel].add(header)
            # Use a new host instance so each graph starts from the same factory project.
            graph_output = output / "graphs"
            graph_output.mkdir()
            graph_script = graph_output / "navigation.txt"
            graph_script.write_text("\n".join(setup + graph_fixture[1:]) + "\n")
            subprocess.run([host, graph_script, graph_output], check=True)
            for capture in ("fm6-envelope.png", "fm6-feedback.png", "wavetable-scan.png",
                            "filter.png", "slicer.png", "gravity-rings.png", "solar-system.png",
                            "digital-vu.png"):
                with Image.open(graph_output / capture) as image:
                    assert image.size == (240, 240), (theme, capture)
        for panel, headers in signatures.items():
            assert len(headers) == 12, (panel, len(headers))
    print("Six editor headers and eight graph/visualizer captures verified in all 12 CHROMA themes.")


if __name__ == "__main__":
    main()
