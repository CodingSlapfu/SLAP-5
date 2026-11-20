# Dual-core architecture comparison

Continues `0fd646f441e8eeda78628a45790ecdab129d7eb3` and [DMA attribution](red-alert-dma-attribution.md). The published hardware reference stays v0.1.18-testing. The owner's 160 BPM take reached 16 voices with zero worker fallbacks, a 6,053 µs peak against 5,804 µs, 97 misses and 99 DMA XRUNs. These native observations are distinct from host profiling.

## Reference systems and decisions

| System | Relevant source evidence | Decision for SLAP-5 |
| --- | --- | --- |
| AQUORA [6b5dc70](https://github.com/CodingSlapfu/AQUORA/tree/6b5dc707e0ceeacb542ad9d2210cfee0597883a7) | The inspected revision has a 16-voice pool over its musical roles. Core 0 calls the audio renderer. The optional Core 1 worker handles geometry; audio does not submit or wait for it. The renderer advances by event-bounded spans with one shared FX path and bypasses unused effects. | Adopt the principle of avoiding work on unchanged or disabled stages. The pinned public revision predates the later integrated KIRA-9 workload, so it cannot provide timing for that workload. Its role count does not establish a matched cost-per-voice comparison. |
| SLOOP [3.4.0-beta](https://github.com/isod89/sloop3-fm1/tree/2228313f3abe0ad6c8f28ef0cd9b9d8167c01330) | 64-frame processing, 32 voice slots plus eight fade slots, even/odd voice ownership and separate per-core float buses. Block LFO and glide updates, dedicated envelope runs, silence checks and shared final mixing. Sequencer and DMA polling are in the main loop, and its overload guard can reduce active voices. | Test fixed slots with SLAP-5's existing mailbox and buffer bounds. Retain independent accumulation and bounded recovery. Keep the 16-voice requirement and current DMA ownership. |
| SKIFF [b257ddf](https://github.com/illuminus5/skiff/tree/b257ddf6a15a21f6c5956abc9c19c0571daa8e15) | Fixed-point, block-level track rendering with separate sums and a previous-block estimated track-cost split. Tests compare partition outputs bit for bit. Core 1 source documents bad mail reads on some units and guards job sequence, word integrity and cache ownership. Its firmware measures nested Timer 5 cost separately. It exposes a 360/320 MHz setting and includes a distinct overload shedding mechanism. | Test an original bounded whole-track policy. Add correlated nested interrupt duration. Preserve existing worker safety and clock configuration. SKIFF reports do not identify the cause of SLAP-5's physical misses. |
| SLAP-5 | Voice-cost partition by each event-bounded span, snapshot/commit through a validated mailbox and independent buses. FX has a separate worker phase. It can skip short spans, rerender after a late job and retire a repeatedly late worker. | Keep the current production split. Changes to riskier scheduling require consistent evidence. Keep buffer sizes, clocks, DSP, note timing and patch/kit IDs. |

SLOOP and SKIFF were used as implementation references. No external source was copied. No matched native cost-per-voice or cache-contention measurement was available. The independent measurements below compare only SLAP-5 policies running the same DSP and input.

## Prototypes and selection

All three policies use identical mailbox submission, cost ceilings, voice safety exclusions, worker scratch and recovery. Fixed slots assign eligible odd-numbered voices to Core 1 and stay local if the partition exceeds the existing cost ceiling. Track mode sends one complete eligible track within the same ceiling and eight-voice capacity. The prototypes are host-only. They do not port the other projects' schedulers, renderers, block lengths or overload behavior.

Run `python3 device/tests/compare-voice-splits.py f2cb184 --runs 3`. Add `--threaded` for actual host concurrency. Below are median per-run statistics. Times are µs. Worker jobs, overlap and wait cover the full scripted take. Online is the numeric median terminal state. Fixture definitions include seeded note generation, 137 BPM sine modulation and five FX/slicer chains, and 160 BPM drum GATE.

### Synchronous worker, thread CPU time

| Workload and split | Mean | P95 | P99 | Peak | Jobs | Overlap | Wait | Fallbacks | Online |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| dry cost | 39.23 | 89.42 | 136.53 | 3109.72 | 32056.00 | 0.00 | 882.00 | 0.00 | 1.00 |
| dry slots | 39.35 | 89.75 | 141.97 | 3497.41 | 3431.00 | 0.00 | 91.00 | 0.00 | 1.00 |
| dry tracks | 39.30 | 89.58 | 134.80 | 1102.77 | 20927.00 | 0.00 | 633.00 | 0.00 | 1.00 |
| wet cost | 86.27 | 179.36 | 240.51 | 5172.95 | 85692.00 | 0.00 | 3949.00 | 0.00 | 1.00 |
| wet slots | 85.85 | 183.64 | 221.47 | 10549.07 | 1989.00 | 0.00 | 1341.00 | 0.00 | 1.00 |
| wet tracks | 82.62 | 183.86 | 231.03 | 8124.21 | 72244.00 | 0.00 | 3667.00 | 0.00 | 1.00 |
| gate cost | 41.63 | 99.72 | 132.77 | 2043.23 | 20980.00 | 0.00 | 619.00 | 0.00 | 1.00 |
| gate slots | 36.55 | 97.47 | 132.51 | 419.61 | 1493.00 | 0.00 | 40.00 | 0.00 | 1.00 |
| gate tracks | 37.68 | 99.14 | 145.56 | 407.92 | 12915.00 | 0.00 | 453.00 | 0.00 | 1.00 |

### Actual pthread worker, wall time

| Workload and split | Mean | P95 | P99 | Peak | Jobs | Overlap | Wait | Fallbacks | Online |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| dry cost | 34.24 | 79.28 | 122.05 | 4512.06 | 32025.00 | 128886.00 | 45356.00 | 5.00 | 1.00 |
| dry slots | 47.02 | 112.72 | 151.49 | 4128.95 | 3425.00 | 14671.00 | 4234.00 | 1.00 | 1.00 |
| dry tracks | 41.06 | 109.72 | 145.37 | 1908.02 | 9312.00 | 25052.00 | 13956.00 | 3.00 | 0.00 |
| wet cost | 76.74 | 159.06 | 225.46 | 4720.89 | 48082.00 | 272131.00 | 69041.00 | 6.00 | 0.00 |
| wet slots | 77.41 | 170.86 | 218.27 | 3419.51 | 1989.00 | 22935.00 | 8180.00 | 0.00 | 0.00 |
| wet tracks | 77.28 | 175.29 | 221.94 | 3057.88 | 28774.00 | 130781.00 | 80458.00 | 8.00 | 0.00 |
| gate cost | 30.57 | 87.32 | 121.01 | 3311.73 | 19935.00 | 100550.00 | 27375.00 | 4.00 | 1.00 |
| gate slots | 44.09 | 121.72 | 154.44 | 1694.84 | 1437.00 | 6155.00 | 1227.00 | 1.00 | 1.00 |
| gate tracks | 41.68 | 111.83 | 170.41 | 3897.29 | 12710.00 | 28629.00 | 18298.00 | 5.00 | 1.00 |

All prototype WAVs passed one-PCM-unit parity against cost mode. At 160 BPM, the fixed slots prototype had the lowest threaded peak but completed about 7% as many voice jobs as cost mode and had 28% higher P99. Whole-track mode had 41% higher P99 and 72% less overlap. These results expose underutilization from a fixed grouping under the existing conservative bounds. The current cost split best sustained useful work across the fixtures, though all policies showed limitations and the modulated wet test could retire the worker. Neither prototype supports switching the firmware's scheduler.

## Integrated production changes

Start the bucket search at the maximum worker budget rather than iterating over buckets whose costs cannot fit. Reuse the exact existing FX partition while track cost values stay the same. The costs are packed into a five-bit field per track. The selected partition and tie order remain identical. Keep these scheduling changes and host comparison tooling. Add opt-in nested Timer 5 duration to the same DMA half record as the XRUN flag. The timer clock read is confined to the active trace path and measures from the existing timer entry timestamp through timer housekeeping. It covers Timer 5 only and still needs target calibration.

Against published v0.1.18, three paired desktop CPU-time trials with the production policy gave the following synchronous worker results. This series uses the standard wet fixture without the additional sine-LFO variant.

| Load | Mean old → new | P95 old → new | P99 old → new | Peak old → new |
|---|---:|---:|---:|---:|
| dry | 46.06 → 41.13 | 104.11 → 95.44 | 154.46 → 140.25 | 492.70 → 443.55 |
| wet | 59.68 → 52.37 | 116.58 → 114.63 | 155.19 → 165.94 | 3133.10 → 2196.94 |
| arrangement | 41.70 → 37.19 | 103.16 → 97.96 | 138.09 → 124.25 | 713.46 → 1249.71 |

Default-policy dry, modulated wet and GATE WAVs were byte-identical against v0.1.18 in corresponding single-core and synchronous-worker modes. Deterministic late-worker recovery and safety tests passed. The 600-second simulated soak completed 434,603 synchronous jobs with zero fallbacks. The final threaded GATE run reached 16 voices and passed PCM tolerance but had nine fallbacks and ended with Core 1 offline. These are host observations. Studio/WASM tests covered recording, stored-project migration, 128 patch auditions and all 56 kits under 240 BPM rapid retriggers. No intentional sonic change or human listening test.

Native structural checks and pinned package verification passed. Default engineering identity `FM-1_7500`, application 161,448 bytes, static RAM 56,520 / 98,304, and pool 312,420 / 344,064. Package SHA-256 `9c8b4c30b8fcbdfcf3f4e765b507e696ffdd8c22c1c3cc9580dc26df52c1e53b`. The pinned SDK inputs were recovered from the unchanged v0.1.18 package and independently matched all reference hashes.

The critical desktop peak did not improve consistently. No qualified experimental firmware was published. Existing rollback releases and installer remain the baseline. Native DMA/Timer 5 attribution and isolation of the physical spike remain unverified. When a qualified experimental firmware is available, the minimum physical validation is to replay the existing 160 BPM take and read peak, misses, XRUNs and worker state. Use the optional trace only to attribute a reproduced miss. No new photos are needed.
