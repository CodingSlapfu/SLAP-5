#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")/.."
: "${AC79_SDK:?Set AC79_SDK to the pinned JieLi SDK checkout}"
sdk=$(cd "$AC79_SDK" && pwd)
manifest="$PWD/evidence/sdk.sha256"
(cd "$sdk/cpu/wl82/tools" && sha256sum -c "$manifest")
exec python3 vendor/fm1-base/tools/build.py device --sdk "$sdk" "$@"
