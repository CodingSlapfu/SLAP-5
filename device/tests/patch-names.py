#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Compare the factory names against a local SLOOP 3 checkout."""

import argparse
from collections import Counter
from pathlib import Path
import re


def normalized_names(path):
    names = re.findall(r'^\s*\{\s*"([^"]+)"\s*,', path.read_text(), re.MULTILINE)
    return Counter(re.sub(r"[^A-Z0-9]", "", name.upper()) for name in names)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("sloop", type=Path, help="Path to the SLOOP 3 checkout")
args = parser.parse_args()
slap = normalized_names(Path(__file__).resolve().parents[1] / "dsp/patches.c")
sloop = normalized_names(args.sloop / "app/preset_data.c")
sloop.update(normalized_names(args.sloop / "engine/fm6_patches.c"))
if slap.total() != 128 or len(slap) != 128 or "" in slap:
    parser.exit(1, "Expected 128 distinct normalized SLAP-5 factory patch names.\n")
if not sloop:
    parser.exit(1, "SLOOP 3 factory name extraction returned no names.\n")
overlap = sorted(slap.keys() & sloop.keys())
if overlap:
    parser.exit(1, f"Normalized factory patch names overlap: {', '.join(overlap)}\n")
print(f"128 SLAP-5 patch names are unique across {len(sloop)} SLOOP 3 names.")
