#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")/.."
python3 vendor/fm1-base/tools/build.py device --web --release "${SLAP_WEB_RELEASE:-0.1.25}"
out="${1:-device/build/studio}"
mkdir -p "$out/emulator" "$out/protocol" "$out/screens"
mkdir -p "$out/fonts"
cp device/build/gen/fonts/SLAPBlock-*.ttf site/slap-block.css "$out/fonts/"
cp vendor/fm1-base/assets/fonts/Barlow-OFL.txt "$out/fonts/OFL.txt"
cp site/index.html site/flash.html site/flash-testing.html site/flash.js site/flash-release.mjs site/flash-transport.mjs site/studio.html site/studio.css site/studio.js site/project.mjs "$out/"
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
tar -xJf engineering/SLAP-5-0.1.12-candidate.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.12.fwsc" "$out/"
tar -xJf engineering/SLAP-5-0.1.13-candidate.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.13.fwsc" "$out/"
tar -xJf engineering/SLAP-5-0.1.14-candidate.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.14.fwsc" "$out/"
tar -xJf engineering/SLAP-5-0.1.16-candidate.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.16.fwsc" "$out/"
tar -xJf engineering/SLAP-5-0.1.17-candidate.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.17.fwsc" "$out/"
tar -xJf engineering/SLAP-5-0.1.18-candidate.tar.xz -C "$release_dir"
(cd "$release_dir" && sha256sum -c SHA256SUMS)
cp "$release_dir/SLAP-5-0.1.18.fwsc" "$out/"
mkdir -p "$out/tests"
cp site/tests/flash.mjs "$out/tests/"
printf '{"type":"module"}\n' > "$out/package.json"
if [ ! -f device/build/host/slap-5_host ]; then
    python3 vendor/fm1-base/tools/build.py device --host
fi
device/build/host/slap-5_host device/tests/groovebox.txt device/build/host
device/build/host/slap-5_host device/tests/transport-mixer.txt device/build/host
cp site/screens/*.png "$out/screens/"
cp device/build/host/home.png device/build/host/dancer.png "$out/screens/"
if [ -n "${SLAP_TESTING_ASSETS:-}" ]; then
    cp "$SLAP_TESTING_ASSETS/release-testing.json" "$SLAP_TESTING_ASSETS/SLAP-5-0.1.24.fwsc" \
        "$SLAP_TESTING_ASSETS/SLAP-5-0.1.24-Studio.zip" "$SLAP_TESTING_ASSETS/SHA256SUMS-0.1.24" "$out/"
fi
