#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")/.."
python3 vendor/fm1-base/tools/build.py device --web
out="${1:-device/build/studio}"
mkdir -p "$out/emulator" "$out/protocol" "$out/screens"
cp site/index.html site/flash.html site/flash.js site/studio.html site/studio.css site/studio.js site/project.mjs "$out/"
cp device/build/emu/index.html device/build/emu/app.wasm device/build/emu/worklet.js device/build/emu/app_worklet.js "$out/emulator/"
cp vendor/fm1-base/web/fm1ota.js vendor/fm1-base/web/fm1ports.js "$out/protocol/"
if [ ! -f device/build/host/home.png ]; then
    python3 vendor/fm1-base/tools/build.py device --host
    device/build/host/slap-5_host device/tests/groovebox.txt device/build/host
fi
cp device/build/host/home.png "$out/screens/"
