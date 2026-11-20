#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Generate original synthesized one-shots at 11,025 Hz as IMA ADPCM."""

import math
from pathlib import Path
import random
import sys

RATE = 11025
STEPS = (7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
         34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
         143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
         494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
         1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
         4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
         10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
         24623, 27086, 29794, 32767)
ADJUST = (-1, -1, -1, -1, 2, 4, 6, 8)
# Tuning, room tail, brightness, saturation. These are synthesized kits.
VOICINGS = (
    (1.00, 1.00, .49, .05), (.87, 1.35, .57, .13), (1.07, .86, .32, .02),
    (.94, .71, .48, .12), (1.13, .85, .65, .19), (.70, 1.67, .29, .23),
    (1.10, .98, .72, .30), (.92, .84, .69, .19), (.81, 1.27, .36, .14),
    (.98, .69, .83, .46), (.83, 1.25, .62, .54), (1.26, .79, .74, .31),
    (.88, .69, .29, .27), (.98, 1.05, .63, .32), (.74, 1.46, .76, .23),
    (1.18, .65, .72, .24), (1.23, .72, .88, .44), (.72, 1.24, .63, .56),
    (1.06, .78, .43, .13), (.92, 1.37, .57, .29), (1.37, .57, .94, .48),
    (1.53, .82, .78, .35), (.81, .82, .24, .41), (1.02, 1.50, .38, .04),
    (.76, 1.57, .55, .21),
)
LENGTHS = (.21, .24, .14, .16, .06, .17, .075, .12,
           .08, .16, .14, .13, .24, .19, .095, .13)


def samples(kit, hit):
    tune, room, bright, drive = VOICINGS[kit]
    length = int(RATE * LENGTHS[hit] * min(room, 1.24))
    rng = random.Random(25_016 * kit + hit)
    phase = low = 0.0
    for i in range(length):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low += (noise - low) * (.13 + .30 * bright)
        air = noise - low
        tail = math.exp(-5.2 * i / length)
        if hit < 2 or 9 <= hit <= 11:
            fundamental = (48, 39, 0, 0, 0, 0, 0, 0, 0, 98, 144, 197)[hit] * tune
            phase += math.tau * fundamental * (1 + 1.9 * math.exp(-t * 115)) / RATE
            body = math.sin(phase)
            x = .86 * body + .12 * math.sin(1.59 * phase) * math.exp(-t * 34)
            x += (.12 + .25 * bright) * air * math.exp(-t * 270)
            if hit == 1 and kit in (5, 14, 17):
                tail = math.exp(-3.4 * i / length)
        elif hit in (2, 3, 7, 8):
            pitch = (173 if hit == 2 else 222 if hit == 3 else 460) * tune
            body = math.sin(math.tau * pitch * t)
            snap = air * bright + low * (1 - bright)
            cluster = max(math.exp(-abs(t - middle) * 260) for middle in (.003, .013, .024))
            x = (.76 * snap * (.24 + .76 * cluster) if hit == 7 else
                 .77 * snap + (.33 if hit != 8 else .57) * body * math.exp(-t * 27))
        else:
            frequencies = (472, 831, 1269, 1787) if hit == 15 else (951, 1571, 2387, 3731)
            metal = sum(math.sin(math.tau * f * tune * t) for f in frequencies) * .25
            x = (.62 - .18 * bright) * air + (.20 + .28 * bright) * metal
            if hit == 14:
                x *= .55 + .45 * math.sin(math.tau * 24 * t) ** 2
        x = math.tanh(x * (1.35 + drive)) * tail
        fade = min(1, i / 7, (length - 1 - i) / 100)
        yield round(x * fade * 28000)


def encode(values):
    predictor = index = 0
    data = bytearray()
    for i, sample in enumerate(values):
        step = STEPS[index]
        diff = sample - predictor
        magnitude = min(7, abs(diff) * 4 // step)
        delta = (step >> 3) + sum(step >> shift for bit, shift in ((4, 0), (2, 1), (1, 2)) if magnitude & bit)
        code = magnitude | (8 if diff < 0 else 0)
        predictor = max(-32768, min(32767, predictor + (-delta if diff < 0 else delta)))
        index = max(0, min(88, index + ADJUST[magnitude]))
        if i & 1:
            data[-1] |= code << 4
        else:
            data.append(code)
    return data


def main():
    output = Path(sys.argv[1])
    data = bytearray()
    table = []
    for kit in range(len(VOICINGS)):
        for hit in range(16):
            frames = list(samples(kit, hit))
            table.append((len(data), len(frames)))
            data.extend(encode(frames))
    with output.open("w") as out:
        out.write("static const uint8_t drum_sample_data[] = {\n")
        for at in range(0, len(data), 24):
            out.write("    " + ", ".join(str(n) for n in data[at:at + 24]) + ",\n")
        out.write("};\nstatic const DrumSample drums_sample_index[] = {\n")
        for offset, length in table:
            out.write(f"    {{{offset}u, {length}u}},\n")
        out.write("};\n")
    print(f"{len(table)} synthesized drum one-shots, {len(data)} compressed bytes")


if __name__ == "__main__":
    main()
