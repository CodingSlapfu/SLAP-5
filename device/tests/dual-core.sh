#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for mode in worker single discard; do
    mkdir "$tmp/$mode"
    cp tests/dual-core.txt "$tmp/$mode/input.txt"
    case $mode in
        worker) printf '%s\n' 'expect worker_jobs >0' 'expect worker_fallbacks 0' ;;
        single) printf '%s\n' 'expect worker_jobs 0' 'expect worker_fallbacks 0' ;;
        discard) printf '%s\n' 'expect worker_jobs 0' 'expect worker_fallbacks 1' ;;
    esac >> "$tmp/$mode/input.txt"
    (
        unset SLAP_TEST_SINGLE_CORE SLAP_TEST_DISCARD_WORKER
        case $mode in
            single) export SLAP_TEST_SINGLE_CORE=1 ;;
            discard) export SLAP_TEST_DISCARD_WORKER=1 ;;
        esac
        build/host/slap-5_host "$tmp/$mode/input.txt" "$tmp/$mode"
    )
done
cmp "$tmp/single/dual-core.wav" "$tmp/worker/dual-core.wav"
cmp "$tmp/single/dual-core.wav" "$tmp/discard/dual-core.wav"
