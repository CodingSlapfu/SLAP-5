# Core 0 voice-rendering experiments

Resume baseline: `66841f68b7256863bd7688c1991c30a3834ae4b4`. See [the 137 BPM hardware record](fm1-0.1.20-137-bpm.md). The physical 5,178 µs peak includes a 3,352 µs voice-batch category and 1,065 µs other app. The voice-batch category includes dispatch and overlap, and is not a pure Core 0 or engine time. Other app includes serial FX where used, per-track metering and gains, summing, limiting, event per-sample work and output. Neither category identifies an individual engine in the captured half. There were no new native timing measurements. The separate physical readings should not be combined.

## Isolated synthesis probe

The new `device/tests/profile-synth.c` uses 16 live instances per engine and times 4,194,304 sample calls plus a sample-bit digest per engine, alternating legacy and modern operation. Pitch, color, motion and seeds are identical across builds. This isolates synthesis from app spans, envelopes, scheduler, FX, copying and ISR overhead. It uses host thread CPU time. Example build from the repository root:

```sh
mkdir -p /tmp/slap-probe
python3 device/tools/oscillator_table.py /tmp/slap-probe/slap_sine_table.inc
cc -O2 -ffp-contract=off -DPLAT_HOST -I device/dsp -I vendor/fm1-base/include \
  -I /tmp/slap-probe device/tests/profile-synth.c device/dsp/synthesis.c -o /tmp/slap-probe/profile
taskset -c 3 /tmp/slap-probe/profile
```

Each line is engine ID, ns/sample and digest. Baseline OP FM was the highest-cost engine in this fixture at roughly 26 ns/sample, ahead of Grain Cloud and Spectral. OP FM uses multiple interpolated sine reads and feedback. The waveform lookup, phase wrapping and fixed coefficients were tested independently. The profiler provides no native engine timing or attribution for a physical callback. Peripheral work, filters/modulation and envelope timing were also covered in the actual application fixtures.

## Independent changes

- Sine lookup reused the float conversion of the table index. All 25 engine sample-bit digests matched. The probe improved 11 engines, with representative OP FM 25.85 → 26.36 ns, wavetable 11.35 → 16.82 ns, flute 11.35 → 13.90 ns. Whole-callback results were mixed. Worker 160 BPM P99 137.47 → 130.13 µs, yet 112 BPM single-core P99 116.50 → 125.73 µs.
- Primary-phase wrap used a subtraction for its bounded single-cycle advance. Higher partial wrapping stayed unchanged. Worker 160 BPM P99 135.58 → 119.70 µs, but modulated 137 BPM P99 169.53 → 196.35 µs and median peak 1,457.21 → 2,666.77 µs.
- OP FM note-on caching removed repeated invariant arithmetic and two ratio decisions. The isolated OP FM median was 25.918 → 25.209 ns/sample over three paired runs. All 25 engine digests matched. The native package passed structural checks and exact regeneration. It took 162,908 application bytes, 56,840/98,304 static RAM and 313,188/344,064 pool bytes, 768 more pool bytes than the baseline. Worker snapshots carry the larger state. The controlled callback results below determined whether to retain it.

The application timings used the same fixtures across revisions, with modulated 137 BPM, 112 BPM dry, corrected 160 BPM GATE and recording included. Synchronous host worker and single-core modes are separate. Peak is the median of run maxima, in µs. Thread CPU cannot substitute for an FM-1 DMA measurement.

### Initial three OP FM pairs

| Fixture / revision | Mean | P95 | P99 | Peak |
|---|---:|---:|---:|---:|
| dry worker baseline | 37.72 | 87.27 | 122.47 | 454.97 |
| dry worker redesign | 37.06 | 83.58 | 119.22 | 822.78 |
| dry single baseline | 36.53 | 85.74 | 111.88 | 995.15 |
| dry single redesign | 35.64 | 81.42 | 96.72 | 545.42 |
| wet worker baseline | 80.27 | 159.93 | 217.39 | 2393.60 |
| wet worker redesign | 75.08 | 142.64 | 202.00 | 3382.95 |
| wet single baseline | 77.48 | 169.31 | 218.25 | 3353.05 |
| wet single redesign | 72.03 | 137.94 | 198.65 | 8172.53 |
| arrangement worker baseline | 46.14 | 127.34 | 169.83 | 487.20 |
| arrangement worker redesign | 40.86 | 115.13 | 152.98 | 1376.84 |
| arrangement single baseline | 39.50 | 102.53 | 161.57 | 1164.32 |
| arrangement single redesign | 43.40 | 134.34 | 160.69 | 764.20 |

### Independent five wet pairs

| Fixture / revision | Mean | P95 | P99 | Peak |
|---|---:|---:|---:|---:|
| wet worker baseline | 73.00 | 139.63 | 194.09 | 2090.34 |
| wet worker redesign | 75.01 | 145.78 | 226.25 | 3162.15 |
| wet single baseline | 75.25 | 150.27 | 206.21 | 3235.95 |
| wet single redesign | 75.78 | 156.50 | 229.42 | 3040.53 |

### Independent five GATE pairs

| Fixture / revision | Mean | P95 | P99 | Peak |
|---|---:|---:|---:|---:|
| arrangement worker baseline | 38.67 | 99.90 | 153.98 | 618.36 |
| arrangement worker redesign | 38.98 | 96.23 | 158.84 | 2282.26 |
| arrangement single baseline | 37.17 | 98.63 | 136.89 | 916.85 |
| arrangement single redesign | 38.77 | 95.62 | 161.04 | 1250.94 |

The first series showed three worker P99 improvements, while several run maxima increased. The second series reversed the wet and GATE P99 gains. Wet worker P99 worsened 16.6%, and 160 BPM worker P99 worsened 3.2%. Median worker maxima were higher in both repeats. The data do not support a repeatable integrated improvement. The three DSP changes were excluded from the tip. The isolated benchmark remains available for later targeted work.

The OP FM experiment preserved byte-identical WAVs to the baseline in all six fixture/mode comparisons. Deterministic worker recovery, host storage, the 16-voice ten-minute simulated GATE soak (434,601 synchronous jobs and zero fallbacks), Studio/WASM, 128 patches, and rapid-hit checks for all 56 kit IDs passed. It was also compiled and audited for the native package format as an unpublished engineering build. The final production DSP source matches the verified baseline. The release threshold was not met and no new package was published. The 4,933 µs physical objective and the cause of prior deadline misses remain open.
