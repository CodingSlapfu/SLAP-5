#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao

"""Generate one cycle of the shared interpolated oscillator."""

from math import sin, tau
from pathlib import Path
import sys

size = 1024
values = [f"{sin(tau * index / size):.9e}f" for index in range(size)] + ["0.0f"]
lines = [", ".join(values[index:index + 8]) for index in range(0, len(values), 8)]
Path(sys.argv[1]).write_text(",\n".join(lines) + "\n")
