#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ziao
"""Verify an FM-1 package against the pinned build inputs and app boundaries."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "vendor/fm1-base/bsp/tools"))
import fm1pkg


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--loader", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--identity", required=True)
    args = parser.parse_args()
    for entry in (ROOT / "evidence/sdk.sha256").read_text().splitlines():
        digest, name = entry.split()
        source = args.sdk / "cpu/wl82/tools" / name
        if sha256(source.read_bytes()).hexdigest() != digest:
            raise SystemExit(f"SDK input hash mismatch: {name}")
    fm1pkg.SDK = args.sdk
    app, loader, package = (item.read_bytes() for item in (args.app, args.loader, args.package))
    if not 0 < len(app) <= 0x8dfbc or not 512 <= len(loader) <= 0x4e20:
        raise SystemExit("App/loader size outside the SLAP-5 limits")
    flash = fm1pkg.flash_image(app, fm1pkg.KEY)
    if len(flash) != 0x93000:
        raise SystemExit("Flash image exceeds the qualified update window")
    expected = fm1pkg.ufw(flash, loader, args.identity)
    if package != expected:
        raise SystemExit("Package differs from regenerated app/loader layout")
    print(json.dumps({
        "identity": args.identity, "packageBytes": len(package),
        "packageSha256": sha256(package).hexdigest(), "appBytes": len(app),
        "appSlotBytes": 0x8dfbc, "loaderBytes": len(loader),
        "flashImageBytes": len(flash), "appWriteStart": "0x4000",
        "appWriteEndExclusive": "0x93000",
    }, indent=2))


if __name__ == "__main__":
    main()
