#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Export the firmware's ordered factory names for website and Studio use."""
import json
from pathlib import Path
import re
import sys

dsp = Path(__file__).resolve().parents[1] / "dsp"
patches = re.findall(r'^\s*\{\s*"([^"]+)"\s*,', (dsp / "patches.c").read_text(), re.M)
kit_source = (dsp / "drum_kits.h").read_text()
legacy = re.findall(r'^\s*\{\s*"([^"]+)"\s*,', kit_source, re.M)
samples = re.findall(r'^\s*"([^"]+)"\s*,?$', kit_source, re.M)
assert len(patches) == 136 and len(legacy) == 56 and len(samples) == 25
Path(sys.argv[1]).write_text(json.dumps({"patches": patches, "kits": legacy + samples},
                                       indent=2) + "\n")
