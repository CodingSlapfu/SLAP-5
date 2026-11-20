#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Check the HOME name layout with the generated firmware glyph advances."""

from pathlib import Path
import re

device = Path(__file__).resolve().parents[1]
header = (device / "build/gen/felucca_font.h").read_text()
advances = tuple(map(int, re.search(r"FONT_T_ADV\[\d+\] = \{([^}]+)\}", header)[1].split(",")))

for source, count in [("patches.c", 128), ("drum_kits.h", 32)]:
    names = re.findall(r'^\s*\{\s*"([^"]+)"\s*,', (device / "dsp" / source).read_text(), re.M)
    assert len(names) == len(set(names)) == count
    for name in names:
        remaining = name
        for limit in (34, 41):
            width = end = space = 0
            for char in remaining:
                width += advances[ord(char) - 32]
                if width > limit:
                    break
                if char == " ":
                    space = end
                end += 1
            if limit == 34 and end < len(remaining) and remaining[end] != " " and space:
                end = space
            remaining = remaining[end:].lstrip()
        assert not remaining, f"HOME label truncates {name!r}"

print("All 128 patch and 32 kit names fit the two-line HOME tiles.")
