# RED ALERT DMA attribution and corrected stress fixtures

This pass resumes remote checkpoint `84932e446e48b1bc0e4d9289e9414e2009b52f54`. [Prior report and physical baseline](160-bpm-headroom.md). The owner measured 6,053 µs versus 5,804 µs, at 160 BPM and 16 voices with 97 deadline misses and 99 DMA XRUNs. No FM-1 was attached for this pass. The cause of those physical misses remains unknown.

## Fixture correction

The prior 160 BPM desktop fixture moved from FX page zero to page one and edited FILTER. The 137 BPM fixture had the same page-selection issue, and its first step back toward the main FX page clamped at zero. The revised tests assert page three in the host's one-based numbering and assert the selected mode. The 160 BPM fixture uses drum GATE, and the wet fixture uses STUTTER and main reverb, delay, chorus and drive across all tracks. An additional wet variant enables sine LFOs before recording. New host assertions establish a peak of 16 voices and voice stealing in each run. The measured fixtures approximate the owner's take with seeded notes. Historical desktop “slicer” figures must be read as filter-workload figures. The owner's physical readings remain independent of the fixture error.

## Implementation

- Advance a silent delayed start as a span when it has no steal tail. The exact onset frame, release behavior, modulation and envelope are preserved. The ordered sample-and-hold path still renders sample by sample.
- Bypass the FX worker partition search if fewer than two tracks have active effects. A lone effect chain cannot be split by the current worker. The serial effect and UI inspection paths are preserved.
- Snapshot arpeggiator configuration once per callback and use that same snapshot for span boundaries. Avoid rereading configuration in the sample loop. The published voice split, worker wait/retire rules, DMA ownership and saved formats are unchanged.
- Add a bounded eight-entry DMA ring. With DMA TRACE selected, the native IRQ snapshots a half-buffer's time from before rendering to after acknowledgement and the free-half check, then records a new peak or DMA overrun. The matching app time, note-on and span timing, worker wait/overlap, nested timer IRQ count and DMA-overrun flag share one record. The UI displays the latest stored half. Two trace clock reads and the record writes are bypassed when the page is off. The page has no tracing I/O or dynamic allocation.
- ENGINE SPAN PEAKS measures the longest voice span per synthesis family and percussion, separately by core. The worker's diagnostic scratch is consumed only after validated completion. The UI shows the worst family per core. These figures measure whole voice spans and exclude the ordered sample-and-hold path. They do not isolate individual FM operators or attribute an underrun to one engine. Per-engine clocks are off in DMA TRACE mode. Existing serial and spike profiling pages remain available. Nested IRQ counting covers the vendor's timer IRQ counter, not every source of preemption. These modes have overhead and are excluded from the ordinary callback statistics.

## Desktop thread CPU

Median per-run results, in µs, across three paired trials of recording and playback. Worker here means the synchronous mailbox harness. `python3 device/tests/compare-dsp-timing.py 9f06a41 HEAD --reference 84932e4 --runs 3 --chords --cpu-time --full`. The timed implementation is `70a076b`; subsequent changes separate opt-in diagnostic paths and extend host assertions and fixtures.

| Load | Source | Mean | P95 | P99 | Peak |
|---|---|---:|---:|---:|---:|
| dry worker | v0.1.18 | 44.83 | 98.10 | 146.02 | 833.07 |
| dry worker | candidate | 39.69 | 90.72 | 143.82 | 1791.06 |
| dry worker | 84932e4 | 42.15 | 91.43 | 153.54 | 597.98 |
| dry single | v0.1.18 | 45.42 | 102.57 | 163.48 | 1118.37 |
| dry single | candidate | 39.39 | 89.04 | 136.72 | 2144.50 |
| dry single | 84932e4 | 40.42 | 87.48 | 129.14 | 806.21 |
| wet worker | v0.1.18 | 60.14 | 121.70 | 175.59 | 2399.15 |
| wet worker | candidate | 50.39 | 110.47 | 155.23 | 2470.97 |
| wet worker | 84932e4 | 59.58 | 119.25 | 176.02 | 3157.73 |
| wet single | v0.1.18 | 58.94 | 119.78 | 168.66 | 4017.64 |
| wet single | candidate | 50.83 | 115.20 | 171.72 | 2627.24 |
| wet single | 84932e4 | 53.82 | 110.36 | 160.22 | 2343.99 |
| arrangement worker | v0.1.18 | 42.19 | 104.87 | 143.38 | 377.22 |
| arrangement worker | candidate | 37.28 | 97.73 | 122.03 | 1126.94 |
| arrangement worker | 84932e4 | 40.20 | 100.40 | 145.32 | 1043.34 |
| arrangement single | v0.1.18 | 42.43 | 104.41 | 156.20 | 2183.72 |
| arrangement single | candidate | 41.29 | 109.30 | 171.51 | 2474.55 |
| arrangement single | 84932e4 | 42.25 | 103.59 | 169.67 | 2137.18 |

The corrected 160 BPM candidate's worker mean improved 11.6% and P99 improved 14.9% against the published baseline. Its worker peak rose from 377 to 1,127 µs. The 112 BPM worker peak also rose in this comparison. Independent wall-clock dry worker runs produced a 1,375 to 3,953 µs peak increase. These are desktop observations. Thread CPU excludes time when the thread is descheduled, while interrupt and machine variability can still affect it. No physical deadline reserve was demonstrated. In one matched host profiling observation, steady-playback mean rose from 65.7 to 70.5 µs with ENGINE SPAN PEAKS enabled, and P99 rose from 175.8 to 185.7 µs. This estimates host observer overhead and does not calibrate DMA TRACE on the FM-1.

## Desktop wall clock

Separate three-run paired series using the same fixtures without `--cpu-time`. Values in µs. The harness includes OS scheduling delays.

| Load | Source | Mean | P95 | P99 | Peak |
|---|---|---:|---:|---:|---:|
| dry worker | v0.1.18 | 44.59 | 95.74 | 143.63 | 1375.15 |
| dry worker | candidate | 40.29 | 91.37 | 149.69 | 3952.81 |
| dry worker | 84932e4 | 40.17 | 86.36 | 111.45 | 1996.09 |
| dry single | v0.1.18 | 42.69 | 91.77 | 123.38 | 2145.21 |
| dry single | candidate | 36.67 | 85.25 | 112.53 | 434.07 |
| dry single | 84932e4 | 40.36 | 87.46 | 129.49 | 648.68 |
| wet worker | v0.1.18 | 58.20 | 116.32 | 161.68 | 1578.96 |
| wet worker | candidate | 50.55 | 109.71 | 150.68 | 3772.72 |
| wet worker | 84932e4 | 58.40 | 119.35 | 175.42 | 3160.38 |
| wet single | v0.1.18 | 59.13 | 120.55 | 170.71 | 2117.55 |
| wet single | candidate | 50.86 | 109.36 | 157.66 | 4382.06 |
| wet single | 84932e4 | 55.46 | 112.71 | 177.83 | 2372.30 |
| arrangement worker | v0.1.18 | 44.81 | 114.28 | 165.37 | 910.36 |
| arrangement worker | candidate | 40.87 | 108.33 | 160.77 | 2186.98 |
| arrangement worker | 84932e4 | 42.91 | 108.55 | 148.19 | 1014.06 |
| arrangement single | v0.1.18 | 45.48 | 132.38 | 167.23 | 1886.83 |
| arrangement single | candidate | 44.47 | 103.08 | 164.10 | 3170.27 |
| arrangement single | 84932e4 | 39.53 | 98.16 | 136.24 | 768.02 |

## Correctness and limits

- A corrected 160 BPM real-host-thread run reached 16 voices and 3,841 steals. It completed 20,905 voice jobs, reported 131,460 µs worker work, 107,959 µs overlap, 31,729 µs wait and a 340 µs maximum voice job. It ended online after seven fallbacks. The modulated 137 BPM threaded run reached 16 voices and ended offline after two fallbacks. These are observed host outcomes, including scheduler interruptions.
- The deterministic isolated-late fixture drained and continued to 245 completed jobs, and the repeated-late fixture retired at four. The corrected ten-minute simulated soak completed 434,614 synchronous voice jobs with zero fallbacks. Threads and physical FM-1 timing are separate from that soak.
- Dry chord, wet chord, 160 BPM arrangement and drum-take PCM matched published v0.1.18 byte for byte within corresponding single-core and synchronous worker render modes. Threaded arrangement and modulated wet renders passed harness PCM tolerance against single-core. The separate modulated heavy-FX WAV also matched byte for byte in both corresponding render modes. No human listening test was conducted. The build makes no intended sonic changes.
- Native structural checks, targeted arp/storage/held-note regressions and Studio/WASM passed, including all 128 patch auditions and all 56 kits at 240 BPM. Native app 161,288 bytes, data/BSS 56,472/98,304 bytes, and pool 312,420/344,064 bytes. The default-identity native engineering build used `FM-1_7500` and SHA-256 `38604c2a1174bc618b888e7b73df4b80c7f1f5f8744670c3c6d3217067661e75`.
- Release is withheld because the corrected critical workload's peaks did not improve consistently and threaded worker continuity varied. Keep v0.1.18-testing and its rollback assets. No hardware acceptance, zero-XRUN result, or 15% peak reserve can be claimed. On an eventual qualified experimental build, replay the existing 160 BPM take and check peak, miss and XRUN counters. Use DMA TRACE only for attribution of a reproduced miss. No additional photographs are requested.
