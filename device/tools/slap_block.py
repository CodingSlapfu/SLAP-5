#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Build SLAP Block vector capitals and numerals with OFL Barlow text glyphs."""
from math import hypot
import sys
from pathlib import Path

from fontTools import subset
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "vendor/fm1-base/assets/fonts"


# Original outlines use centerlines in a 400 x 700 cap-height field. A period
# starts a separate stroke; Q defines a quadratic curve from the current point.
DESIGNS = {
    "A": (490, "35 70 165 620 255 620 385 70 . 86 290 334 290"),
    "B": (490, "42 70 42 630 251 630 Q 380 630 380 504 Q 380 350 252 350 42 350 . 252 350 Q 397 350 397 210 Q 397 70 252 70 42 70"),
    "C": (490, "374 590 Q 320 630 244 630 Q 52 630 52 446 52 247 Q 52 70 244 70 Q 320 70 374 110"),
    "D": (500, "43 70 43 630 215 630 Q 407 630 407 449 407 253 Q 407 70 215 70 43 70"),
    "E": (470, "364 630 46 630 46 70 364 70 . 46 353 316 353"),
    "F": (448, "46 70 46 630 356 630 . 46 350 310 350"),
    "G": (500, "390 583 Q 336 630 246 630 Q 51 630 51 451 51 255 Q 51 70 244 70 Q 337 70 393 118 393 320 249 320"),
    "H": (500, "45 70 45 630 . 400 70 400 630 . 45 351 400 351"),
    "I": (320, "44 630 222 630 . 133 630 133 70 . 44 70 222 70"),
    "J": (440, "347 630 347 236 Q 347 70 199 70 Q 110 70 60 122"),
    "K": (485, "46 70 46 630 . 384 630 46 325 . 202 465 388 70"),
    "L": (444, "46 630 46 70 355 70"),
    "M": (542, "44 70 44 630 91 630 211 343 277 343 397 630 444 630 444 70"),
    "N": (510, "45 70 45 630 89 630 368 70 412 70 412 630"),
    "O": (500, "229 635 Q 410 635 410 450 410 250 Q 410 65 229 65 Q 48 65 48 250 48 450 Q 48 635 229 635"),
    "P": (480, "44 70 44 630 241 630 Q 386 630 386 471 Q 386 324 241 324 44 324"),
    "Q": (504, "229 635 Q 410 635 410 450 410 250 Q 410 65 229 65 Q 48 65 48 250 48 450 Q 48 635 229 635 . 268 212 419 49"),
    "R": (490, "44 70 44 630 240 630 Q 385 630 385 471 Q 385 324 240 324 44 324 . 224 324 395 70"),
    "S": (485, "377 592 Q 322 630 227 630 Q 56 630 56 492 Q 56 386 222 353 Q 394 322 394 214 Q 394 70 222 70 Q 126 70 48 117"),
    "T": (470, "28 630 390 630 . 209 630 209 70"),
    "U": (505, "49 630 49 246 Q 49 70 230 70 Q 410 70 410 246 410 630"),
    "V": (492, "39 630 167 70 267 70 392 630"),
    "W": (562, "34 630 99 70 170 70 242 365 309 365 377 70 447 70 477 630"),
    "X": (485, "42 630 390 70 . 390 630 42 70"),
    "Y": (483, "37 630 194 340 230 340 387 630 . 212 340 212 70"),
    "Z": (480, "47 630 380 630 380 597 49 101 49 70 391 70"),
    "0": (478, "212 634 Q 380 634 380 448 380 252 Q 380 66 212 66 Q 44 66 44 252 44 448 Q 44 634 212 634 . 277 466 145 235"),
    "1": (478, "123 535 233 630 233 70 . 116 70 352 70"),
    "2": (478, "50 536 Q 88 630 223 630 Q 376 630 376 492 Q 376 425 299 345 62 116 62 70 388 70"),
    "3": (478, "53 584 Q 114 630 215 630 Q 370 630 370 501 Q 370 363 230 363 163 363 . 230 363 Q 383 363 383 216 Q 383 70 215 70 Q 110 70 45 118"),
    "4": (478, "292 70 292 630 253 630 47 285 47 248 389 248"),
    "5": (478, "370 630 74 630 74 357 218 357 Q 382 357 382 220 Q 382 70 218 70 Q 123 70 53 119"),
    "6": (478, "355 591 Q 305 630 218 630 Q 53 630 53 450 53 236 Q 53 70 218 70 Q 383 70 383 216 Q 383 369 218 369 Q 110 369 53 308"),
    "7": (478, "44 630 385 630 385 584 147 70"),
    "8": (478, "223 630 Q 370 630 370 504 Q 370 382 223 356 Q 52 324 52 206 Q 52 70 223 70 Q 394 70 394 206 Q 394 324 223 356 Q 76 382 76 504 Q 76 630 223 630"),
    "9": (478, "74 108 Q 129 70 219 70 Q 387 70 387 245 387 466 Q 387 630 223 630 Q 53 630 53 482 Q 53 333 223 333 Q 327 333 387 392"),
    "-": (355, "60 307 241 307"),
}


def strokes(design):
    for segment in design.split("."):
        tokens = segment.split()
        points = [(int(tokens[0]), int(tokens[1]))]
        i = 2
        while i < len(tokens):
            if tokens[i] == "Q":
                start = points[-1]
                control = tuple(map(int, tokens[i + 1:i + 3]))
                end = tuple(map(int, tokens[i + 3:i + 5]))
                for n in range(1, 9):
                    t = n / 8
                    points.append(tuple((1-t)**2*a + 2*(1-t)*t*b + t*t*c
                                        for a, b, c in zip(start, control, end)))
                i += 5
            else:
                points.append((int(tokens[i]), int(tokens[i + 1])))
                i += 2
        yield points


def glyph(letter, bold):
    pen = TTGlyphPen(None)
    advance, design = DESIGNS[letter]
    width = 100 if bold else 65
    radius = width * 0.5
    for points in strokes(design):
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            length = hypot(bx - ax, by - ay)
            if not length:
                continue
            nx, ny = -(by - ay) * radius / length, (bx - ax) * radius / length
            pen.moveTo((ax + nx + 23, ay + ny))
            pen.lineTo((bx + nx + 23, by + ny))
            pen.lineTo((bx - nx + 23, by - ny))
            pen.lineTo((ax - nx + 23, ay - ny))
            pen.closePath()
        for x, y in points:
            x += 23
            pen.moveTo((x + radius, y))
            pen.qCurveTo((x + radius, y - radius), (x, y - radius))
            pen.qCurveTo((x - radius, y - radius), (x - radius, y))
            pen.qCurveTo((x - radius, y + radius), (x, y + radius))
            pen.qCurveTo((x + radius, y + radius), (x + radius, y))
            pen.closePath()
    return pen.glyph(), advance


def build(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for style, weight in (("Regular", "Medium"), ("Bold", "Bold"), ("Italic", "Medium")):
        font = TTFont(BASE / f"BarlowSemiCondensed-{weight}.ttf", recalcTimestamp=False)
        cmap = font.getBestCmap()
        for letter in DESIGNS:
            name = cmap[ord(letter)]
            shape, advance = glyph(letter, style == "Bold")
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
