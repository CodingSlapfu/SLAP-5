#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")/.."
python3 vendor/fm1-base/tools/build.py device --web
out="${1:-device/build/studio}"
mkdir -p "$out/emulator" "$out/protocol" "$out/screens"
mkdir -p "$out/fonts"
cp device/build/gen/fonts/SLAPBlock-*.ttf site/slap-block.css "$out/fonts/"
cp vendor/fm1-base/assets/fonts/Barlow-OFL.txt "$out/fonts/OFL.txt"
cp site/index.html site/flash.html site/flash.js site/flash-release.mjs site/flash-transport.mjs site/studio.html site/studio.css site/studio.js site/project.mjs "$out/"
cp device/build/emu/index.html device/build/emu/app.wasm device/build/emu/worklet.js device/build/emu/app_worklet.js "$out/emulator/"
sed -i -e '/<\/head>/i\  <link rel="stylesheet" href="../fonts/slap-block.css">' \
  -e 's/Barlow Semi Condensed/SLAP Block/g' -e 's/"Barlow"/"SLAP Block"/g' "$out/emulator/index.html"
cp vendor/fm1-base/web/fm1ota.js vendor/fm1-base/web/fm1ports.js vendor/fm1-base/web/fm1pkg.js "$out/protocol/"
cp site/release.json "$out/"
release_dir=$(mktemp -d)
trap 'rm -rf "$release_dir"' EXIT
tar -xJf engineering/SLAP-5-0.1.11-assets.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.11.fwsc" "$out/"
mkdir -p "$out/tests"
cp site/tests/flash.mjs "$out/tests/"
printf '{"type":"module"}\n' > "$out/package.json"
if [ ! -f device/build/host/home.png ]; then
    python3 vendor/fm1-base/tools/build.py device --host
    device/build/host/slap-5_host device/tests/groovebox.txt device/build/host
fi
cp device/build/host/home.png "$out/screens/"
