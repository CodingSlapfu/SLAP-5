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
python3 tests/pcm_parity.py "$tmp/single/dual-core.wav" "$tmp/worker/dual-core.wav"
python3 tests/pcm_parity.py "$tmp/single/dual-core.wav" "$tmp/discard/dual-core.wav"

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
for mode in worker single discard grace timeout local_fx; do
    mkdir "$tmp/fx/$mode"
    cp "$tmp/fx/input.txt" "$tmp/fx/$mode/input.txt"
    case $mode in
        worker) printf '%s\n' 'expect fx_worker_jobs >128' 'expect fx_worker_fallbacks 0' ;;
        single) printf '%s\n' 'expect fx_worker_jobs 0' 'expect fx_worker_fallbacks 0' ;;
        local_fx) printf '%s\n' 'expect fx_worker_jobs 0' 'expect worker_jobs >128' 'expect worker_online 1' 'expect fx_worker_fallbacks 0' ;;
        discard) printf '%s\n' 'expect fx_worker_jobs 128' 'expect fx_worker_fallbacks 1' ;;
        grace) printf '%s\n' 'expect fx_worker_jobs >128' 'expect fx_worker_fallbacks 0' ;;
        timeout) printf '%s\n' 'expect fx_worker_jobs 128' 'expect worker_late_source 2' 'expect worker_online 0' ;;
    esac >> "$tmp/fx/$mode/input.txt"
    (
        unset SLAP_TEST_SINGLE_CORE SLAP_TEST_DISCARD_WORKER SLAP_TEST_DISCARD_FX
        unset SLAP_TEST_LEGACY_FX_WORKER
        [ "$mode" = local_fx ] || export SLAP_TEST_LEGACY_FX_WORKER=1
        case $mode in
            single) export SLAP_TEST_SINGLE_CORE=1 ;;
            discard) export SLAP_TEST_DISCARD_FX=1 ;;
            grace) export SLAP_TEST_FX_GRACE=1 ;;
            timeout) export SLAP_TEST_FX_TIMEOUT=1 ;;
        esac
        build/host/slap-5_host "$tmp/fx/$mode/input.txt" "$tmp/fx/$mode"
    )
done
python3 tests/pcm_parity.py "$tmp/fx/single/fx.wav" "$tmp/fx/worker/fx.wav"
python3 tests/pcm_parity.py "$tmp/fx/single/fx.wav" "$tmp/fx/discard/fx.wav"
python3 tests/pcm_parity.py "$tmp/fx/single/fx.wav" "$tmp/fx/grace/fx.wav"
python3 tests/pcm_parity.py "$tmp/fx/single/fx.wav" "$tmp/fx/timeout/fx.wav"
python3 tests/pcm_parity.py "$tmp/fx/single/fx.wav" "$tmp/fx/local_fx/fx.wav"

# Record Track 5 from a fresh boot, disarm, then let the drum-only take loop
# with FX off. Compare the whole playback window after recording has ended.
for mode in worker single; do
    mkdir "$tmp/drum-$mode"
    cp tests/drum-take.txt "$tmp/drum-$mode/input.txt"
    (
        unset SLAP_TEST_SINGLE_CORE SLAP_TEST_DISCARD_WORKER SLAP_TEST_DISCARD_FX
        # ADPCM voices stay on Core 0 until physical XIP timing is qualified.
        echo 'expect worker_jobs 0' >> "$tmp/drum-$mode/input.txt"
        echo 'expect worker_fallbacks 0' >> "$tmp/drum-$mode/input.txt"
        if [ "$mode" = single ]; then export SLAP_TEST_SINGLE_CORE=1; fi
        build/host/slap-5_host "$tmp/drum-$mode/input.txt" "$tmp/drum-$mode"
    )
done
python3 tests/pcm_parity.py "$tmp/drum-single/drum-take.wav" "$tmp/drum-worker/drum-take.wav"

for mode in worker single; do
    mkdir "$tmp/synth-$mode"
    {
        printf '%s\n' 'wait 6100' 'wav synth.wav' 'key 7 down' 'key 8 down' \
            'key 10 down' 'wait 500' 'key 7 up' 'key 8 up' 'key 10 up' \
            'wait 300' 'wavstop'
        if [ "$mode" = worker ]; then
            echo 'expect worker_jobs >0'
            echo 'expect worker_fallbacks 0'
        else
            echo 'expect worker_jobs 0'
        fi
    } > "$tmp/synth-$mode/input.txt"
    (
        unset SLAP_TEST_SINGLE_CORE SLAP_TEST_DISCARD_WORKER SLAP_TEST_DISCARD_FX
        [ "$mode" != single ] || export SLAP_TEST_SINGLE_CORE=1
        build/host/slap-5_host "$tmp/synth-$mode/input.txt" "$tmp/synth-$mode"
    )
done
python3 tests/pcm_parity.py "$tmp/synth-single/synth.wav" "$tmp/synth-worker/synth.wav"

# An isolated late voice output is discarded, then the worker resumes.
mkdir "$tmp/late"
cp tests/dual-core.txt "$tmp/late/input.txt"
printf '%s\n' 'expect voice_late_jobs 1' 'expect worker_jobs >4' 'expect voice_drained_jobs 1' 'expect worker_fallbacks 1' >> "$tmp/late/input.txt"
SLAP_TEST_LATE_VOICE=1 build/host/slap-5_host "$tmp/late/input.txt" "$tmp/late"
python3 tests/pcm_parity.py "$tmp/single/dual-core.wav" "$tmp/late/dual-core.wav"

# Two recoverable late completions discard stale scratch and resume.
mkdir "$tmp/repeated"
cp tests/dual-core.txt "$tmp/repeated/input.txt"
printf '%s\n' 'expect voice_late_jobs 2' 'expect worker_jobs >4' 'expect voice_drained_jobs 2' 'expect worker_fallbacks 2' >> "$tmp/repeated/input.txt"
SLAP_TEST_LATE_VOICE=2 build/host/slap-5_host "$tmp/repeated/input.txt" "$tmp/repeated"
python3 tests/pcm_parity.py "$tmp/single/dual-core.wav" "$tmp/repeated/dual-core.wav"

# Repeated late jobs that acknowledge can resume with a smaller remote batch.
mkdir "$tmp/persistent"
cp tests/dual-core.txt "$tmp/persistent/input.txt"
printf '%s\n' 'expect voice_late_jobs 3' 'expect worker_jobs >4' 'expect voice_drained_jobs 3' 'expect worker_fallbacks 3' >> "$tmp/persistent/input.txt"
SLAP_TEST_LATE_VOICE=3 build/host/slap-5_host "$tmp/persistent/input.txt" "$tmp/persistent"
python3 tests/pcm_parity.py "$tmp/single/dual-core.wav" "$tmp/persistent/dual-core.wav"

# A voice job that cannot acknowledge by the hard deadline stays retired.
mkdir "$tmp/voice-timeout"
cp tests/dual-core.txt "$tmp/voice-timeout/input.txt"
printf '%s\n' 'expect voice_late_jobs 1' 'expect voice_drained_jobs 0' 'expect worker_late_source 1' 'expect worker_online 0' >> "$tmp/voice-timeout/input.txt"
SLAP_TEST_LATE_VOICE=1 SLAP_TEST_VOICE_TIMEOUT=1 build/host/slap-5_host "$tmp/voice-timeout/input.txt" "$tmp/voice-timeout"
python3 tests/pcm_parity.py "$tmp/single/dual-core.wav" "$tmp/voice-timeout/dual-core.wav"

# Held chord arpeggios can allocate several voices. Batch only up to their
# next sample-accurate tick and compare with the single-core render.
for mode in worker single; do
    mkdir "$tmp/arp-$mode"
    {
        printf '%s\n' 'wait 6100' 'tap ARP' 'turn K1 5' 'wav arp.wav' \
            'key 7 down' 'key 11 down' 'key 14 down' 'wait 800' \
            'key 7 up' 'key 11 up' 'key 14 up' 'wait 400' 'wavstop'
        if [ "$mode" = worker ]; then
            echo 'expect worker_jobs >0'
            echo 'expect worker_fallbacks 0'
        else
            echo 'expect worker_jobs 0'
        fi
    } > "$tmp/arp-$mode/input.txt"
    (
        unset SLAP_TEST_SINGLE_CORE SLAP_TEST_DISCARD_WORKER SLAP_TEST_DISCARD_FX SLAP_TEST_LATE_VOICE
        [ "$mode" != single ] || export SLAP_TEST_SINGLE_CORE=1
        build/host/slap-5_host "$tmp/arp-$mode/input.txt" "$tmp/arp-$mode"
    )
done
python3 tests/pcm_parity.py "$tmp/arp-worker/arp.wav" "$tmp/arp-single/arp.wav"
