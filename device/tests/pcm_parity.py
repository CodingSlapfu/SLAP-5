#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao

"""Compare complete simulator WAVs, allowing one 16-bit rounding unit."""

from array import array
import sys
import wave


def compare_pcm(left, right):
    with wave.open(str(left), "rb") as a, wave.open(str(right), "rb") as b:
        assert a.getparams() == b.getparams()
        assert a.getsampwidth() == 2
        first = a.readframes(a.getnframes())
        second = b.readframes(b.getnframes())
    if first == second:
        return
    a, b = array("h", first), array("h", second)
    if sys.byteorder == "big":
        a.byteswap()
        b.byteswap()
    largest = max(abs(x - y) for x, y in zip(a, b))
    assert largest <= 1, f"PCM differs by {largest} samples of full-scale 32768"


if __name__ == "__main__":
    compare_pcm(*sys.argv[1:])
