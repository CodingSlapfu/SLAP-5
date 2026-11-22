# SLAP-5 v0.1.29 owner acceptance

Engineering candidate. [Exact source 2df799c](https://github.com/CodingSlapfu/SLAP-5/commit/2df799cfa0320bf3c6de97777b41b7b86fe642c3), tree `1cc1dc97dae771f1ec8f49e5f10b372e098f8249`. The [v0.1.28 evidence](qualification-0.1.28.md) and candidate backup are retained. The public installer still serves the established release.

| # | Owner requirement | Status | Evidence and remaining work |
| --- | --- | --- | --- |
| 1 | HOME controls | COMPLETE | Host checks level endpoints and one-detent increments, pan, 16/32/64 steps and global swing up to 70. Length edits change the loop boundary while retaining stored events. Physical encoders remain to be checked. |
| 2 | MIXER controls | COMPLETE | Host checks K1 solo, K2 duck, K3 all filter and K4 track filter. The drums supply the existing ducking trigger, with SOURCE shown on Track 5. Mute and ARP solo are still available. |
| 3 | Independent bipolar filters | PARTIAL | The prior attenuation result is preserved. New dense host recordings with all five filters and a swept mix filter match v0.1.28 PCM at 112, 137 and 160 BPM. Neutral runs also match. Physical sweeps and timing remain open. |
| 4 | Persistence and old projects | COMPLETE | Slot round-trip checked disparate track values and a separate all-filter value. V2–V11 remain readable and initialize neutral. V12 is written only with active filters and needs this firmware or later. Studio JSON accepts optional filter fields with neutral defaults. |
| 5 | Firmware-wide knobs | PARTIAL | Shared hardware-style dials are used on HOME, MIXER, Sound Lab, wavetable, ENV, LFO, drum generation, all FX pages and ARP. The remaining parameter pages still need the same treatment and a full extreme-value fit audit. |
| 6 | Five-track scrolling HOME | PARTIAL | The fixed playhead, per-track lengths and held tails are preserved. Grid drawing now wraps its traversal index without a division for each column. Held-recording and wrapped-hold fixtures pass. Native-LCD tearing and longer legacy-loop transfer budgets remain open. |
| 7 | BAR 1–4 and battery | PARTIAL | The header uses global transport position and the existing measured battery/USB input. The native panel and real USB/battery transitions remain unverified. |
| 8 | Selected track pulse | COMPLETE | A beat-driven highlight and steady selected-track stripe are captured at 240×240. |
| 9 | Patch name readability | PARTIAL | Refined cap outlines, smaller track numerals and five simultaneous rows. All 136 patch and 81 kit names fit the generated glyph advances for scrolling HOME. Physical readability remains open. |
| 10 | SLAP Block type | PARTIAL | Original vector caps and numerals replace the pixel-grid glyphs. Bold and Regular use distinct strokes. Device glyphs are generated at build time and the web preview uses the same family. Device fit and antialiasing audit remains open. |
| 11 | Saturated RGB565 palette | PARTIAL | Five saturated track colors are shared with Studio. Native panel color and readability checks remain open. |
| 12 | FM6 operator graph | PARTIAL | Curved modulation connections, envelope-driven path activity and operator selection are implemented. Full operator telemetry, graph routing audit and physical LCD checks remain open. |
| 13 | Wavetable graph | PARTIAL | Two-strip waveform shows the actual scan phase and band. It holds steady without a sounding voice. Morph, Scan, Decay and Drive are available. Harmonic-detail and sensitivity checks remain open. |
| 14 | Other engines | PARTIAL | Signal traces are paired with amplitude envelopes, track filter curves or live voice modulation by engine family. Individual engines need a complete timbre-specific visual audit. |
| 15 | Sound Lab controls | COMPLETE | EDIT enters Sound Lab with four parameter controls and ALGO accesses a separate browser. Host verifies an actual parameter change for all four knobs and patch browsing. |
| 16 | FILTER FX graph | PARTIAL | The existing response function drives the dedicated curve and visibly follows cutoff and resonance. Full DSP/curve and control-latency calibration remains open. |
| 17 | Live SLICER graph | PARTIAL | The cursor follows the effect subdivisions, and live state identifies capture, replay and gate. Audible boundary and waveform correspondence remain to be verified. |
| 18 | Listening profiles | PARTIAL | Both labels are mapped to the persistent shared-output speaker processing switch. Native acoustic response and output measurements remain open. |
| 19 | Digital VU | PARTIAL | Five larger LED scales retain peaks and STEP 01–16. Host verifies master-independent activity. Ballistics and physical display verification remain open. |
| 20 | DNA Helix | PARTIAL | Bass changes separation and twist, pad activity affects depth and note attacks affect highlights. The captured frames change over time. Independent track acceptance remains open. |
| 21 | Gravity Rings | PARTIAL | Per-track attacks, bass-dependent depth, ring energy and transport phase change the motion. Physical smoothness remains open. |
| 22 | Solar System | PARTIAL | Track-specific speed, size, lighting, trail and accent responses are implemented. A full physical solo-all-five acceptance pass remains open. |
| 23 | Dancer | PARTIAL | The authorized PARALLAX-8 Dancer 036 artwork has 160 source scanlines and 28 distinct images across 30 indexed poses. A compact span encoding preserves original pixels and the existing pose cycle, controls and black background. Physical rendering cost and visual acceptance remain open. |
| 24 | Audio/graphics performance | PARTIAL | Native resource audit and host timing are available below. Matched PCM for all nine dense EDIT entry/exit pairs. Physical distortion, XRUNs, LCD tearing and worker tails remain open. |
| 25 | Earlier owner items | PARTIAL | See the [active issues](https://github.com/CodingSlapfu/SLAP-5/issues) and linked 0.1.27 evidence for remaining LED levels, DSP/worker regressions, visual review, patch/kit listening and Studio/installer acceptance. |
| 26 | Compatibility and recovery | PARTIAL | Package boundary/hash audit, bad/foreign-package rejection and interrupted install/rollback simulation passed. Physical USB update and rollback remain open. |
| 27 | Source backup | COMPLETE | Exact candidate tree verified by a fresh GitHub fetch. The backup ZIP contains the source, staged site/Studio, candidate package, captures, evidence and SHA-256 manifest. |
| 28 | Experimental release and installer | BLOCKED | Native identity `FM-1_75000129`. Real-time timing remains unresolved. The archive is an engineering checkpoint and the public installer continues serving the prior release. |

## Timing and rendering findings

The original “worker” column measures audio callback time with synchronous host dispatch. By default, the host calls the worker function inline. With the original CPU clock option the outer timer counts callback thread CPU time, while the firmware-style job limits still use a monotonic clock. This cannot show native Core 1 wakeup delay, contention, cache/XIP stalls, or LCD DMA interference. The original 112 BPM peak did not reproduce consistently as an invariant of the v0.1.28 tree.

The continuation harness distinguishes inline CPU time from pthread wall time. It reports the median of each trial statistic, the worst observed callback, counts above the nominal FM-1 callback interval on the host, and asynchronous job/fallback and wait/work counters. Host XRUN reporting is stubbed, so the host figures are not physical deadline or XRUN evidence.

The native DSP change skips poles whose current and target depths are zero. It preserves LP-before-HP processing when both poles are still audible during a cross-neutral transition. Coefficients are still recalculated only on detent changes. All six five-track neutral and active runs matched v0.1.28 to the PCM comparator tolerance. The fixture sweeps K3 across neutral, exercises K4 with alternating filter types across the tracks, records/overdubs, enters EDIT, scrolls HOME and traverses seven visualizers.

HOME now computes wraparound once for the grid and reuses one swing offset per lane. This reduces render arithmetic and preserves redraw ordering. The native LCD layer waits for transfer completion before reusing a single canvas, or chooses the other canvas in a double-buffer build. The transfer path does not synchronize visible updates to the panel scan. DMA buffer reuse is guarded, while visible tearing and transfer cost still need on-device measurement. No scan-sync fix or physical EDIT fix has been established.

## Exact-source inline callback timing

Three alternating trials at each tempo on `350929e`, `8ef9725` and `2df799c`. Values in microseconds are callback thread CPU time over the last 1,600 callbacks of dense patch-switch overdubbing. P95/P99 and median peak are medians across the three trials. Worst is the maximum observed across trials. No callbacks in the sampled windows passed 5,805 µs of host thread CPU time. The 112 BPM 3,137.79 µs v0.1.28 observation did not repeat in either controlled run, and the 137 BPM tail remains less consistent than the baseline. At 160 BPM the candidate lowered P99 versus the simultaneous v0.1.28 control, while still slightly above the earlier baseline. These are full callback times, inclusive of any inline worker execution.

| BPM | Host mode | P95 baseline / 0.1.28 / 0.1.29 | P99 baseline / 0.1.28 / 0.1.29 | Median peak baseline / 0.1.28 / 0.1.29 | Worst baseline / 0.1.28 / 0.1.29 |
| --- | --- | ---: | ---: | ---: | ---: |
| 112 | worker | 182.81 / 144.28 / 144.74 (-20.8% vs base, +0.3% vs .28) | 288.24 / 193.85 / 193.00 (-33.0% vs base, -0.4% vs .28) | 1663.49 / 336.29 / 388.24 (-76.7% vs base, +15.4% vs .28) | 2139.06 / 451.24 / 1351.95 (-36.8% vs base, +199.6% vs .28) |
| 112 | single | 159.40 / 148.71 / 144.47 (-9.4% vs base, -2.9% vs .28) | 216.54 / 192.21 / 209.06 (-3.5% vs base, +8.8% vs .28) | 389.13 / 323.66 / 704.10 (+80.9% vs base, +117.5% vs .28) | 497.12 / 400.02 / 2189.79 (+340.5% vs base, +447.4% vs .28) |
| 137 | worker | 155.15 / 150.87 / 154.20 (-0.6% vs base, +2.2% vs .28) | 197.40 / 182.55 / 202.97 (+2.8% vs base, +11.2% vs .28) | 1274.55 / 365.11 / 1154.81 (-9.4% vs base, +216.3% vs .28) | 1766.20 / 1727.35 / 2687.17 (+52.1% vs base, +55.6% vs .28) |
| 137 | single | 161.98 / 152.17 / 164.21 (+1.4% vs base, +7.9% vs .28) | 252.48 / 184.44 / 226.82 (-10.2% vs base, +23.0% vs .28) | 703.00 / 782.50 / 747.07 (+6.3% vs base, -4.5% vs .28) | 3103.81 / 2133.75 / 2010.47 (-35.2% vs base, -5.8% vs .28) |
| 160 | worker | 159.01 / 162.76 / 159.18 (+0.1% vs base, -2.2% vs .28) | 201.81 / 262.43 / 205.73 (+1.9% vs base, -21.6% vs .28) | 950.35 / 1017.35 / 557.98 (-41.3% vs base, -45.2% vs .28) | 1697.11 / 1966.67 / 636.05 (-62.5% vs base, -67.7% vs .28) |
| 160 | single | 175.19 / 149.97 / 176.14 (+0.5% vs base, +17.5% vs .28) | 254.94 / 183.18 / 224.77 (-11.8% vs base, +22.7% vs .28) | 854.41 / 446.17 / 524.58 (-38.6% vs base, +17.6% vs .28) | 1333.78 / 903.26 / 1259.03 (-5.6% vs base, +39.4% vs .28) |

## Asynchronous host sweep with visualizers

Three alternating runs per tempo and mode with active filtering and the visualizer sequence. Values are microseconds. Candidate measurement used source `44237f0`, before two FX label abbreviations and test-harness-only follow-up changes. “Worker” enables pthread dispatch at startup. Every worker-enabled trial in both revisions recorded an FX fallback. Completed jobs varied from a few thousand to over 75,000. The tail windows mix different execution histories and cannot be interpreted as sustained worker timings. There were zero host wall callbacks over the nominal 5,805 µs interval in the sampled windows. The host does not measure device XRUNs.

| BPM | Host mode | P95 v0.1.28 → candidate | P99 v0.1.28 → candidate | Median peak v0.1.28 → candidate | Worst v0.1.28 → candidate |
| --- | --- | ---: | ---: | ---: | ---: |
| 112 | worker | 152.62 → 156.55 (+2.6%) | 191.85 → 209.37 (+9.1%) | 371.68 → 353.86 (-4.8%) | 1731.07 → 4279.44 (+147.2%) |
| 112 | single | 156.25 → 154.51 (-1.1%) | 206.40 → 197.13 (-4.5%) | 375.44 → 585.33 (+55.9%) | 1536.42 → 943.72 (-38.6%) |
| 137 | worker | 161.38 → 154.07 (-4.5%) | 204.35 → 174.96 (-14.4%) | 575.40 → 462.86 (-19.6%) | 1320.66 → 2340.30 (+77.2%) |
| 137 | single | 162.22 → 161.27 (-0.6%) | 232.33 → 209.72 (-9.7%) | 674.41 → 363.43 (-46.1%) | 1604.85 → 736.35 (-54.1%) |
| 160 | worker | 177.29 → 163.78 (-7.6%) | 241.84 → 198.98 (-17.7%) | 485.70 → 376.90 (-22.4%) | 789.02 → 481.26 (-39.0%) |
| 160 | single | 167.23 → 165.13 (-1.3%) | 215.14 → 224.30 (+4.3%) | 1031.65 → 1406.86 (+36.4%) | 1702.34 → 2933.25 (+72.3%) |

## Native build and verification

- Identity `FM-1_75000129`, package 611,488 bytes, SHA-256 `52aa1473d14caea60f0f83245f612579bdb6f05085bb68aea1e39f785c24d745`. Pinned SDK and native package audit passed, as did simulated interruption, app-only bounds and rollback with project/settings preservation.
- Application 564,400 / 581,564 bytes, +352 bytes from v0.1.28. Static RAM 87,752 / 98,304 bytes and pool 328,648 / 344,064 bytes are unchanged. Flash optimization headroom is 17,164 bytes.
- [Hosted Chromium passed](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44964343882) on exact source. Focused host tests covered ARP, pages, transport and mixer persistence, graphics, held-recording, wrapped holds, six dense filter PCM comparisons and nine EDIT transitions. All seven visualizers retained identical frames across four master levels, with silence at zero. Five concurrent effects remained bounded through all seven inserts in the DSP fixture. Ten 240×240 captures exercise the new FX and ARP controls. The font-width test covers name fit in both tile and scrolling layouts.
- Gate A packaging and simulated flashing checks passed. The real-time qualification decision remains open because host job retirement obscures the worker comparison and the original tail regression has not been attributed on FM-1. Gate B remains open for actual audio, timing and LCD acceptance.

## Device verification still required

Use the same saved dense song at 112, 137 and 160 BPM with the same input order. For each tempo, repeat with both filters neutral, active per-track and mix filters, and rapid sweeps through neutral. Include overdubs, EDIT entry/exit, HOME scrolling and all seven visualizers.

Hold GLO for five seconds for callback and worker diagnostics. Move K3 on the diagnostics page to reset the counters before each matched trial. Capture P95, P99, peak, deadline misses, XRUNs, Core 1 work/wait/late/recovery and DMA trace with the same reset window on the previous qualified build and the candidate. The native callback interval is 256 frames at 44,100 Hz, about 5,805 µs. LCD scan timing and encoder-to-visible latency require the physical panel or bench instrumentation. Check USB versus battery indicators, headphone/speaker profiles, LED levels, and app-only browser rollback independently of the musical tests.
