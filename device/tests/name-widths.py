#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Check the HOME name layout with the generated firmware glyph advances."""

from pathlib import Path
import re

device = Path(__file__).resolve().parents[1]
header = (device / "build/gen/felucca_font.h").read_text()
def advances(font):
    return tuple(map(int, re.search(rf"FONT_{font}_ADV\[\d+\] = \{{([^}}]+)\}}", header)[1].split(",")))


tile_advances = advances("T")
bold_advances = advances("B")
small_advances = advances("S")

for source, count in [("patches.c", 136), ("drum_kits.h", 81)]:
    names = re.findall(r'^\s*\{\s*"([^"]+)"\s*,', (device / "dsp" / source).read_text(), re.M)
    if source == "drum_kits.h":
        names += re.findall(r'^\s+"([^"]+)"\s*,?$', (device / "dsp" / source).read_text(), re.M)
    assert len(names) == count
    groups = [names] if source == "patches.c" else [names[:32], names[32:56], names[56:]]
    assert all(len(group) == len(set(group)) for group in groups)
    for name in names:
        bold_width = sum(bold_advances[ord(char) - 32] for char in name)
        strip_width = bold_width if bold_width <= 148 else sum(small_advances[ord(char) - 32] for char in name)
        assert strip_width <= 156, f"Scrolling HOME name overlaps the meter: {name!r}"
        remaining = name
        for row, limit in enumerate((33, 40)):
            width = end = space = 0
            for char in remaining:
                width += tile_advances[ord(char) - 32]
                if width > limit:
                    break
                if char == " ":
                    space = end
                end += 1
            if row == 0 and end < len(remaining) and remaining[end] != " " and space:
                end = space
            remaining = remaining[end:].lstrip()
        assert not remaining, f"HOME label truncates {name!r}"

print("All 136 patch and 81 kit names fit the tiles and scrolling HOME lanes.")
