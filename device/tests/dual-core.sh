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

# Five simultaneous wet FX chains. The discard occurs after feedback has built
# up, so the WAV comparison covers restored delay, chorus, room and slicer state.
mkdir "$tmp/fx"
{
    echo 'wait 6100'
    for track in 0 1 2 3 4; do
        [ "$track" -eq 0 ] || echo 'turn SELECT 1'
        printf '%s\n' 'tap SEQ' 'key 0 down' 'key 0 up' 'tap HOME'
    done
    printf '%s\n' 'tap FX' 'turn SELECT -4' 'turn ALGO 1'
    for track in 0 1 2 3 4; do
        [ "$track" -eq 0 ] || echo 'turn SELECT 1'
        printf '%s\n' 'turn K1 2' 'turn K2 5' 'turn ALGO -2' \
            'turn K1 50' 'turn K2 45' 'turn K3 40' 'turn K4 30' 'turn ALGO 2'
    done
    printf '%s\n' 'tap HOME' 'wav fx.wav' 'tap PLAY' 'wait 5500' 'wavstop'
} > "$tmp/fx/input.txt"
for mode in worker single discard; do
    mkdir "$tmp/fx/$mode"
    cp "$tmp/fx/input.txt" "$tmp/fx/$mode/input.txt"
    case $mode in
        worker) printf '%s\n' 'expect fx_worker_jobs >128' 'expect fx_worker_fallbacks 0' ;;
        single) printf '%s\n' 'expect fx_worker_jobs 0' 'expect fx_worker_fallbacks 0' ;;
        discard) printf '%s\n' 'expect fx_worker_jobs 128' 'expect fx_worker_fallbacks 1' ;;
    esac >> "$tmp/fx/$mode/input.txt"
    (
        unset SLAP_TEST_SINGLE_CORE SLAP_TEST_DISCARD_WORKER SLAP_TEST_DISCARD_FX
        case $mode in
            single) export SLAP_TEST_SINGLE_CORE=1 ;;
            discard) export SLAP_TEST_DISCARD_FX=1 ;;
        esac
        build/host/slap-5_host "$tmp/fx/$mode/input.txt" "$tmp/fx/$mode"
    )
done
cmp "$tmp/fx/single/fx.wav" "$tmp/fx/worker/fx.wav"
cmp "$tmp/fx/single/fx.wav" "$tmp/fx/discard/fx.wav"
