#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Build the SLAP Block family from the original display grid and OFL Barlow."""
import re
import sys
from pathlib import Path

from fontTools import subset
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "vendor/fm1-base/assets/fonts"
SOURCE = ROOT / "device/visuals.h"


def contours(rows):
    edges = set()
    for row, word in enumerate(rows):
        for x in range(5):
            if not word & (16 >> x):
                continue
            y = 6 - row
            square = [(x, y), (x, y + 1), (x + 1, y + 1), (x + 1, y)]
            for a, b in zip(square, square[1:] + square[:1]):
                if (b, a) in edges:
                    edges.remove((b, a))
                else:
                    edges.add((a, b))
    while edges:
        a, b = min(edges)
        start, outline = a, [a]
        edges.remove((a, b))
        while b != start:
            outline.append(b)
            dx, dy = b[0] - a[0], b[1] - a[1]
            for tx, ty in ((dy, -dx), (dx, dy), (-dy, dx)):
                c = b[0] + tx, b[1] + ty
                if (b, c) in edges:
                    edges.remove((b, c))
                    a, b = b, c
                    break
            else:
                raise ValueError("Open glyph contour")
        yield [p for i, p in enumerate(outline)
               if (outline[i-1][0] != outline[(i+1) % len(outline)][0] and
                   outline[i-1][1] != outline[(i+1) % len(outline)][1])]


def glyph(rows, bold):
    pen = TTGlyphPen(None)
    outline_width = max(x for path in contours(rows) for x, _ in path)
    left = min(x for path in contours(rows) for x, _ in path)
    inset = 5 if bold else 11
    for outline in contours(rows):
        points = []
        for i, (x, y) in enumerate(outline):
            a = outline[i - 1]
            b = outline[(i + 1) % len(outline)]
            horizontal = (b[1] > a[1]) - (b[1] < a[1])
            vertical = (a[0] > b[0]) - (a[0] < b[0])
            points.append((55 + (x - left) * 87 + horizontal * inset, y * 100 + vertical * inset))
        def toward(a, b):
            dx, dy = b[0] - a[0], b[1] - a[1]
            size = abs(dx) + abs(dy)
            radius = min(16, size // 3)
            return a[0] + dx * radius / size, a[1] + dy * radius / size
        pen.moveTo(toward(points[0], points[-1]))
        for i, point in enumerate(points):
            pen.lineTo(toward(point, points[i - 1]))
            pen.qCurveTo(point, toward(point, points[(i + 1) % len(points)]))
        pen.closePath()
    return pen.glyph(), (outline_width - left) * 87 + 94


def build(directory):
    directory.mkdir(parents=True, exist_ok=True)
    text = SOURCE.read_text().split("slap_block[37][7] = {", 1)[1].split("};", 1)[0]
    grid = [[int(n) for n in row.split(",")] for row in re.findall(r"\{([\d,]+)\}", text)]
    for style, weight in (("Regular", "Medium"), ("Bold", "Bold"), ("Italic", "Medium")):
        font = TTFont(BASE / f"BarlowSemiCondensed-{weight}.ttf", recalcTimestamp=False)
        cmap = font.getBestCmap()
        for letter, rows in zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-", grid, strict=True):
            name = cmap[ord(letter)]
            shape, advance = glyph(rows, style == "Bold")
            font["glyf"][name] = shape
            shape.recalcBounds(font["glyf"])
            font["hmtx"].metrics[name] = advance, shape.xMin
        if style == "Italic":
            glyphs = font.getGlyphSet()
            oblique = {}
            for name in font.getGlyphOrder():
                recorded = DecomposingRecordingPen(glyphs)
                glyphs[name].draw(recorded)
                pen = TTGlyphPen(None)
                recorded.replay(TransformPen(pen, (1, 0, 0.14, 1, 0, 0)))
                oblique[name] = pen.glyph()
            for name, shape in oblique.items():
                font["glyf"][name] = shape
                shape.recalcBounds(font["glyf"])
                advance, _ = font["hmtx"].metrics[name]
                font["hmtx"].metrics[name] = advance, getattr(shape, "xMin", 0)
            font["head"].macStyle |= 2
            font["OS/2"].fsSelection |= 1
            font["OS/2"].fsSelection &= ~64
            font["post"].italicAngle = -8
        if style != "Bold":
            font["OS/2"].usWeightClass = 400
        names = {1: "SLAP Block", 2: style, 3: f"Ziao SLAP Block {style}",
                 4: f"SLAP Block {style}", 6: f"SLAPBlock-{style}", 16: "SLAP Block", 17: style}
        for row in font["name"].names:
            if row.nameID in names:
                row.string = names[row.nameID]
        options = subset.Options()
        options.name_IDs = ["*"]
        subsetter = subset.Subsetter(options=options)
        subsetter.populate(unicodes=list(range(32, 256)) + [0x2022, 0x2013, 0x2014, 0x266f])
        subsetter.subset(font)
        font.save(directory / f"SLAPBlock-{style}.ttf")


if __name__ == "__main__":
    build(Path(sys.argv[1]))
