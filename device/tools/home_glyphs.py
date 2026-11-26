#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Rasterize the bundled OFL Barlow face to a compact 22-pixel HOME atlas."""

from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont


def main(output):
    vendor = Path(__file__).resolve().parents[2] / "vendor/fm1-base"
    font = ImageFont.truetype(str(vendor / "assets/fonts/BarlowSemiCondensed-Bold.ttf"), 128)
    cap = font.getbbox("H", anchor="ls")[1]
    lines = ["static const struct { uint8_t width; uint16_t rows[22]; } home_glyphs[] = {"]
    for char in "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+./?":
        image = Image.new("L", (192, 128))
        ImageDraw.Draw(image).text((8, -cap), char, font=font, fill=255, anchor="ls", stroke_width=2)
        box = image.getbbox()
        cell = image.crop((box[0], 0, box[2], -cap + 2))
        width = min(15, max(3, round(cell.width * 19 / -cap)))
        cell = cell.resize((width, 22), Image.Resampling.LANCZOS)
        rows = [sum(1 << x for x in range(width) if cell.getpixel((x, y)) >= 105)
                for y in range(22)]
        lines.append("    {" + str(width) + ", {" + ", ".join(hex(row) for row in rows) + "}},")
    lines.append("};")
    Path(output).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv[1])
