#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Build the original Nexus caps and symbols, with OFL text fallback glyphs."""
from html import escape
from math import hypot
from pathlib import Path
import sys

from fontTools import subset
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

from nexus_master import KERNING, STROKES

BASE = Path(__file__).resolve().parents[2] / "vendor/fm1-base/assets/fonts"


def outline(text, weight, tabular):
    paths = [[tuple(float(n) * 50 for n in pair.split(",")) for pair in row.split()]
             for row in text.split(";")]
    xmin = min(x for path in paths for x, _ in path)
    xmax = max(x for path in paths for x, _ in path)
    gap = 28 if not tabular else 34
    radius = weight / 2
    ink = xmax - xmin + weight
    advance = 200 + weight + gap if tabular else ink + gap
    shift = (advance - ink) / 2 + radius - xmin
    pen = TTGlyphPen(None)

    def polygon(points):
        pen.moveTo(points[0])
        for point in points[1:]:
            pen.lineTo(point)
        pen.closePath()

    for path in paths:
        for (ax, ay), (bx, by) in zip(path, path[1:]):
            scale = radius / hypot(bx - ax, by - ay)
            nx, ny = (ay - by) * scale, (bx - ax) * scale
            polygon([(ax + shift + nx, ay + ny), (bx + shift + nx, by + ny),
                     (bx + shift - nx, by - ny), (ax + shift - nx, ay - ny)])
        for x, y in path:
            x += shift
            bevel = radius * 0.42
            polygon([(x + radius, y + bevel), (x + radius, y - bevel),
                     (x + bevel, y - radius), (x - bevel, y - radius),
                     (x - radius, y - bevel), (x - radius, y + bevel),
                     (x - bevel, y + radius), (x + bevel, y + radius)])
    return pen.glyph(), int(advance)


def build(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for style, weight, base in (("Regular", 52, "Medium"), ("Bold", 84, "Bold")):
        font = TTFont(BASE / f"BarlowSemiCondensed-{base}.ttf", recalcTimestamp=False)
        cmap = font.getBestCmap()
        for letter, design in STROKES.items():
            name = cmap.get(ord(letter), f"uni{ord(letter):04X}")
            if ord(letter) not in cmap:
                font.setGlyphOrder([*font.getGlyphOrder(), name])
                for table in font["cmap"].tables:
                    if table.isUnicode():
                        table.cmap[ord(letter)] = name
            shape, advance = outline(design, weight, letter.isdigit())
            font["glyf"][name] = shape
            shape.recalcBounds(font["glyf"])
            font["hmtx"].metrics[name] = advance, shape.xMin
        if "kern" in font:
            del font["kern"]
        if "GPOS" in font:
            del font["GPOS"]
        pairs = " ".join(f"pos {cmap[ord(a)]} {cmap[ord(b)]} {value};"
                         for (a, b), value in KERNING.items())
        addOpenTypeFeaturesFromString(font, "feature kern { " + pairs + " } kern;")
        for row in font["name"].names:
            names = {1: "SLAP Nexus", 2: style, 3: f"Ziao SLAP Nexus {style}",
                     4: f"SLAP Nexus {style}", 6: f"SLAPNexus-{style}", 16: "SLAP Nexus", 17: style}
            if row.nameID in names:
                row.string = names[row.nameID]
        options = subset.Options()
        options.name_IDs = ["*"]
        subsetter = subset.Subsetter(options=options)
        subsetter.populate(unicodes=list(range(32, 256)) + [ord(c) for c in STROKES])
        subsetter.subset(font)
        font.save(directory / f"SLAPNexus-{style}.ttf")
        glyphs = font.getGlyphSet()
        mapping = font.getBestCmap()
        svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 9600 4900">']
        svg.append(f'<title>{escape(style)} original SLAP Nexus outlines</title>')
        for i, char in enumerate(STROKES):
            pen = SVGPathPen(glyphs)
            glyphs[mapping[ord(char)]].draw(pen)
            x, y = 240 + i % 12 * 800, 700 + i // 12 * 800
            svg.append(f'<path aria-label="{escape(char)}" transform="translate({x} {y}) scale(1 -1)" d="{pen.getCommands()}"/>')
        svg.append("</svg>")
        (directory / f"SLAPNexus-{style}.svg").write_text("\n".join(svg) + "\n")


if __name__ == "__main__":
    build(Path(sys.argv[1]))
