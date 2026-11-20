# Core 0 callback pipeline and span-boundary experiment

Baseline `6220494fb95f26ba9ef201e2cf5a45cc0ec11ebf`. The [owner's 137 BPM run](fm1-0.1.20-137-bpm.md) reached 16 voices, peaked at 5,178 µs with 626 µs reserve, and reported zero misses, XRUNs and worker fallbacks. Voice batch was 3,352 µs, FX batch was 759 µs, and other application work was reported as 1,065 µs. The 1,893 µs of Core 1 work overlaps callback stages. The separately captured Timer 5 measurement does not establish its contribution at the peak.

## Callback audit

The voice batch spans dispatch, local rendering, synchronization and worker result handling. The FX batch includes its own dispatch and completion. Other application time is the residual around these batches. It can contain setup, sequencing, track mixing and meters, serial FX where used, limiting, visualizer capture, output, bookkeeping and nested interrupt time. Profiling scopes can include preemption and overlap. DMA acknowledgement and work outside the application have their own trace fields. FX checkpoints support recovery, meter state feeds AUTO, and effect state can remain relevant while a track is muted.

The retained change exposes the existing correlated SERIAL PROFILE capture through the native desktop host script's `expect` command. Use GLOBAL > SERIAL PROFILE, enable profiling with K3, then run the workload and read `profile_peak_us`, `profile_input_us`, `profile_events_us`, `profile_voices_us`, `profile_serial_fx_us`, `profile_fx_batch_us`, `profile_mix_us` and `profile_output_us`. Results are integer microseconds from one selected peak callback. Profiling performs added timer reads. The diagnostic additions are excluded from FM-1 and WASM.

Three separate host wall-clock smoke captures illustrate the scope of the instrumentation. Dry showed 2,010 µs total with 1,725 µs falling in mix scopes. Wet showed 610 µs total, with 481 µs voice, 43 µs FX batch and 35 µs mix. GATE showed 1,357 µs total with 88 µs voice, 7 µs serial FX, 39 µs mix and 6 µs output. The isolated dry mix outlier and the GATE residual cannot establish an FM-1 bottleneck.

## Isolated change and paired tests

The experiments moved sequencer and arpeggiator polling and countdown updates to existing event-bounded render spans. They preserved event order on the boundary sample. One variant kept the logic inline and the other put it in an outlined helper. Both kept the cost-based worker partition and existing synthesis kernels.

Five paired trials per series using `device/tests/compare-dsp-timing.py --runs 5 --chords --cpu-time --full --modulated`. Values are median run statistics, each tuple mean / P95 / P99 / observed max, in host thread-CPU microseconds. Worker timing below uses the synchronous worker harness. The threaded playback and recovery tests are separate.

| Fixture | Baseline | Outlined | Inline |
|---|---:|---:|---:|
| 112 dry worker | 40.49 / 91.00 / 143.20 / 2424.43 | 40.08 / 89.81 / 136.86 / 2140.00 | 39.54 / 90.11 / 145.12 / 1781.41 |
| 112 dry single | 37.91 / 86.24 / 126.74 / 2855.43 | 37.31 / 85.92 / 127.30 / 1900.17 | 36.93 / 86.16 / 120.83 / 524.28 |
| 137 wet worker | 72.86 / 135.99 / 186.56 / 4075.94 | 75.05 / 140.29 / 201.35 / 6203.56 | 73.75 / 139.36 / 187.94 / 14820.92 |
| 137 wet single | 72.91 / 143.16 / 209.11 / 5995.21 | 73.45 / 141.01 / 201.37 / 3566.53 | 71.81 / 140.22 / 193.75 / 3124.71 |
| 160 GATE worker | 37.08 / 97.66 / 123.40 / 833.92 | 38.15 / 99.52 / 148.37 / 2088.83 | 40.29 / 99.70 / 147.64 / 11172.36 |
| 160 GATE single | 37.52 / 96.69 / 127.08 / 2124.31 | 36.53 / 96.75 / 128.46 / 1695.28 | 36.62 / 96.81 / 127.18 / 424.24 |

The separate inline-only series produced wet worker P99 of 208.31 → 214.64 µs and GATE worker P99 of 116.71 → 129.24 µs. A further five-pair repeat yielded wet 190.52 → 210.59 µs and GATE 120.47 → 126.44 µs. Thread CPU excludes time when the measured thread is off CPU, while isolated peaks still vary across runs. The repeated direction of worker P99 on both heavy fixtures is the reason to drop both variants.

PI32V2 disassembly showed the inline callback growing from 2,145 to 2,205 static instructions. Native app sizes were 162,816 bytes baseline, 163,040 inline and 162,944 outlined. Static counts and application size cannot determine native cycle counts or bus contention.

The inline variant produced byte-identical fixture WAVs versus baseline in both synchronous worker and single-core paths, including a metronome comparison. Host recovery, recording, and Studio/WASM checks passed, including all engine identities, 128 patch auditions, all 56 rapid-hit kits, and bank migration. Its 600-second simulated synchronous run kept 16 voices and completed without fallbacks. The separate threaded desktop comparison recorded four fallbacks, retained an online worker, and met audio tolerance. The outlined variant passed the native build and host recovery and dense replay checks. Both firmware experiments were removed from the final tree.

Retain the host profile readouts and the existing firmware. No experimental firmware is justified by this pass. The 4,933 µs physical target and attribution of older physical overruns remain unverified.
