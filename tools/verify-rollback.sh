#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")/.."
package=$(realpath "$1")
sandbox=$(mktemp -d)
trap 'rm -rf "$sandbox"' EXIT
cc -O2 -o "$sandbox/ldr-test" vendor/fm1-base/tests/ldr_test.c
for version in 0.1.18 0.1.22; do
    rollback="$sandbox/$version"
    mkdir "$rollback"
    case "$version" in
        0.1.18)
            tar -xJf "engineering/SLAP-5-$version-candidate.tar.xz" -C "$rollback"
            ;;
        0.1.22)
            tar -xJf "engineering/SLAP-5-$version-hardware-testing.tar.xz" -C "$rollback"
            cp "engineering/SLAP-5-$version-Studio.zip" "$rollback/"
            ;;
    esac
    (cd "$rollback" && sha256sum -c SHA256SUMS)
    "$sandbox/ldr-test" "$rollback/SLAP-5-$version.fwsc" "$package" --interrupt
    "$sandbox/ldr-test" "$package" "$rollback/SLAP-5-$version.fwsc"
done
