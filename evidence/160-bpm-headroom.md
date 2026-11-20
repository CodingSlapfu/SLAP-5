# 160 BPM headroom and spike attribution

## Physical baseline

The owner's v0.1.18 test used two CHORD tracks, drums and drum GATE/SLICER at 160 BPM with 16 active voices. The owner heard smooth playback. The device reported 77% average DSP, P95/P99 of 5,000/5,200 µs and a 6,053 µs peak. Against the 5,804 µs deadline it recorded 97 misses and 99 DMA XRUNs. Core 1 was online with zero fallbacks, no retirement, reported stack use of 256 bytes and advancing mailbox counters. The diagnostics also showed 1,194 µs of overlap, zero wait, 545 µs maximum reported voice job, 8,913 short spans and 1,856 budget skips. These snapshots can describe separate callbacks and reporting windows.

The source starts from SHRUB engineering commit `c6f8a280c4cd0b3432f260d0ce45e024e31abdd8` and inherits its fixed coefficients, voice hints and track-selective worker buses. [Earlier investigation](shrub-dsp-investigation.md). The published reference is `9f06a41f58791b9306b2c149636f6b0f9f599ca0`.

## Changes and instrumentation

Unmodulated tonal release spans can now keep the envelope and last sample in registers and update their remaining gate countdown at span end. Delayed voices, active modulation and steal tails still use the general renderer. The release threshold, render order, envelopes and sixteen voice slots are retained. Voice stealing calculates each releasing candidate's loudness once and retains the original preference and tie order. The arpeggiator resets once per callback with no held keys. The published worker ownership, dispatch thresholds, scheduling and recovery rules are retained.

GLOBAL's new SPIKE PROFILE page captures total note-on duration, total local voice work, total voice dispatch work, largest voice span and batch FX work at the heaviest callback captured in that mode. Timers are active on this page and can change the callback cost. Observer mode is excluded from normal callback statistics. BATCH FX covers work sent through the batch path and does not include serial FX. Local and dispatch figures are native/host-worker builds only. These fields are not a per-engine, individual overlap or DMA trace. Diagnose overlap and mailbox from the existing CORE TIMING and DISPATCH pages. No new owner photos are needed.

## Comparisons

`python3 device/tests/compare-dsp-timing.py 9f06a41 HEAD --reference c6f8a28 --runs 3 --chords --cpu-time --full` measures thread CPU time across recording and replay. Worker mode here is synchronous. CPU timing removes time spent off the thread while CPU-frequency variation and system interruptions can still affect cost. Values are medians across runs, in µs. The 160 BPM fixture approximates the documented arrangement and does not assume the owner's exact patch and note data.

| Fixture | Source | Mean | P95 | P99 | Peak |
|---|---|---:|---:|---:|---:|
| dry worker | v0.1.18 | 46.61 | 106.17 | 133.40 | 914.28 |
| dry worker | candidate | 46.85 | 113.17 | 143.68 | 691.91 |
| dry worker | SHRUB checkpoint | 47.71 | 106.20 | 157.51 | 1675.47 |
| dry single | v0.1.18 | 42.86 | 91.80 | 125.29 | 1465.01 |
| dry single | candidate | 42.26 | 98.11 | 126.38 | 1118.74 |
| dry single | SHRUB checkpoint | 49.58 | 115.77 | 154.49 | 1788.51 |
| wet worker | v0.1.18 | 64.91 | 130.91 | 193.74 | 9107.24 |
| wet worker | candidate | 59.01 | 123.16 | 192.27 | 2550.91 |
| wet worker | SHRUB checkpoint | 59.92 | 122.08 | 183.37 | 3570.14 |
| wet single | v0.1.18 | 62.06 | 125.56 | 184.74 | 2718.87 |
| wet single | candidate | 55.89 | 115.98 | 155.80 | 2123.87 |
| wet single | SHRUB checkpoint | 56.84 | 117.20 | 161.96 | 1737.63 |
| arrangement worker | v0.1.18 | 55.69 | 148.57 | 178.91 | 1067.99 |
| arrangement worker | candidate | 48.84 | 129.10 | 162.83 | 3975.58 |
| arrangement worker | SHRUB checkpoint | 55.18 | 142.43 | 168.16 | 1970.55 |
| arrangement single | v0.1.18 | 54.39 | 148.45 | 180.28 | 1463.73 |
| arrangement single | candidate | 50.08 | 134.37 | 170.54 | 2163.10 |
| arrangement single | SHRUB checkpoint | 51.14 | 126.05 | 178.38 | 1424.51 |

At 112 BPM the worker peak decreased 24%, with a 7.7% P99 increase versus v0.1.18. At 137 BPM worker mean decreased 9.1% and P99 decreased 0.8%. At 160 BPM worker mean decreased 12.3% and P99 decreased 9.0%, while peak increased from 1,068 to 3,976 µs. Steady playback wall-clock measurements on intermediate builds were also variable. The candidate has not established consistent worst-case improvement.

Real threaded 160 BPM playback recorded 20,980 jobs and 114,122 µs of aggregate overlap in one run. Recording/replay PCM matched the single-core render within harness tolerance. Isolated lateness drained safely and continued to 245 jobs. The repeated-late fixture retired at four jobs. The scripted host suite and visualizer audio parity passed. Native audits passed with a 160,016-byte application, 55,812/98,304 bytes data/BSS and 312,420/344,064 bytes in the pool. WASM checks covered project migration, recording, quantization, 128 patches, 16 GEN DRUMS styles, and rapid 240 BPM six-lane retriggering across all 56 kit IDs. Rapid kit peaks ranged from 0.0264 to 0.0495.

Cross-revision dry chords, wet chords, 160 BPM arrangement and drum takes were byte-identical against v0.1.18 in each corresponding synchronous worker and single-core mode. A 600-second simulated arrangement soak completed 434,613 jobs with zero fallbacks in the synchronous harness. This confirms exercised audio equivalence and software continuity.

Keep v0.1.18-testing as the published installer and emulator baseline. This checkpoint does not qualify for release. Native per-engine and DMA timing, repeatable reduction of the 160 BPM peaks and intentional synth/FX quality A/B remain open. The setup has no attached physical FM-1. On a later qualified build, physical acceptance requires a smooth 16-voice arrangement, zero new misses and underruns, and measured peaks below 5,804 µs.
