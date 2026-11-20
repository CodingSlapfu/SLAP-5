# SHRUB-inspired DSP investigation

Resume from the work on the existing `slap-5-red-alert-dsp` branch. The exact v0.1.18 release source is `9f06a41f58791b9306b2c149636f6b0f9f599ca0`. Its subsequent publication checkpoint `aa6c97f3fc0dc29d8779bdbff1346867326855ba` had the same tree as the clean local continuation checkpoint. See [the v0.1.16 photos and observations](fm1-0.1.16-physical.md) and [v0.1.18 recovery evidence](release-0.1.18.md). The owner's v0.1.17 FM-1 reported one late voice job and a 6,075 µs voice batch. The first missed job's duration and the device's performance under the new renderer remain unknown.

## Reference findings and decisions

| Reference | Documented or inspected behavior | SLAP-5 decision |
| --- | --- | --- |
| SHRUB guide, firmware 1.25 | The manual describes a 16-voice pool, up to eight played notes and a CPU-dependent note budget. It describes shared modulation and a filter applied to the combined audio, while preserving the per-note amplitude envelope. Wavetable morph, warp, unison, and a seven-slot effects rack are exposed. | Share fixed coefficients first. Retain SLAP-5's per-note LFO phase, envelope and note count. Globalizing pitch modulation or summing voices through one shared filter changes the behavior of existing banks. Treat original band-limited tables and deterministic note variation as experiments that require spectral and listening comparisons. The guide does not establish SHRUB's proprietary scheduling or interpolation implementation. |
| SLOOP 3.4.0-beta, `2228313f3abe0ad6c8f28ef0cd9b9d8167c01330` | Publishes 64-sample blocks and two-core playback. Source starts the worker before the local voice pass and mixes two buses after waiting. Its late path stops the worker. | Retain early dispatch and independent buses. Limit remote bus clearing and merging to tracks present in the job. The published block size and SLOOP's timeout provide no validated timing budget for SLAP-5. |
| AQUORA, `6b5dc707e0ceeacb542ad9d2210cfee0597883a7` | Its visual-worker source explicitly keeps audio outside its worker submission and waiting path. | Preserve audio priority and keep visual work outside SLAP-5's DSP mailbox. AQUORA's geometry scheduling does not establish the safe voice split for SLAP-5. |

References: [SHRUB guide](https://shrub.audio/guide/) · [SLOOP 3.4.0-beta](https://github.com/isod89/sloop3-fm1/tree/v3.4.0-beta) · [AQUORA visual worker](https://github.com/CodingSlapfu/AQUORA/blob/6b5dc707e0ceeacb542ad9d2210cfee0597883a7/device/visual_worker.c)

## Implementation

The note-on path caches unchanged track ADSR and LFO coefficients. Voice state still snapshots its own note values. The dispatcher precomputes the existing cost and eligibility at note-on and reads them in each span. The formula, split, wait and retirement rules remain as in v0.1.18. The worker reads a track mask from its private job and Core 0 uses it only after successful mailbox completion. The oscillator skips phase-to-integer conversion below the wrap threshold. The acid, spectral and brushed percussion paths reuse identical waveform calculations. The callback computes fixed reverb, delay, drive and chorus coefficients when settings or tempo change. Feedback and rollback state still use the original processing order, storage and checkpoint.

## Measurements

Pinned desktop CPU, five runs per configuration with the existing 112 BPM dry and 137 BPM wet chord fixtures. Values are median run statistics in µs. Worker here is the synchronous mailbox harness. These timings include host scheduling noise.

| Workload | Mean old → new | P95 old → new | P99 old → new | Max old → new |
| --- | ---: | ---: | ---: | ---: |
| Dry worker | 76.06 → 77.85 | 121.10 → 121.44 | 140.12 → 141.03 | 290.25 → 215.57 |
| Dry single | 64.30 → 65.44 | 116.74 → 114.16 | 137.23 → 136.17 | 503.07 → 704.52 |
| Wet worker | 103.86 → 83.12 | 170.95 → 140.66 | 220.60 → 213.14 | 1,858.81 → 924.92 |
| Wet single | 98.97 → 84.17 | 143.94 → 134.68 | 211.44 → 194.29 | 428.58 → 482.62 |

The genuine desktop thread recorded 17,061 jobs and 93,104 µs overlap on dry playback, and 35,998 jobs and 171,470 µs overlap for the wet run. Audio matched against single-core rendering within the parity fixture's tolerance. The worker tests passed the isolated late-job drain with 245 completed jobs and the two-miss retirement case with four completed jobs. Dry, wet and drum-take WAVs were byte-identical against v0.1.18 within each respective single-core and synchronous worker mode. This checks sound preservation for those takes. It supplies no evidence of improved timbre or per-kit rapid-hit performance.

All script-based host fixtures, eight visualizer audio-parity checks, and the local WASM synthesis and project round-trip suite passed. The native app is 159,048 bytes. Static data and BSS use 55,764 of 98,304 bytes, and pool use is 312,420 of 344,064 bytes. The native build's timing-independent structural audits passed. The default-identity build is for engineering verification.

## Release and continuation

Keep the existing v0.1.18-testing installer and emulator as the published baseline. Dry chord mean and P99 did not show a repeatable worker improvement, and single-core worst callbacks regressed in this benchmark. Hardware confirmation is still required for 5,804 µs peak headroom, zero XRUNs and correct sustained 16-voice playback. Instrument the actual time and overlap of per-engine voice jobs, record short-span skips and remote recovery cost, and correlate these with callback peaks during recording. Use the same saved take at 112 BPM and the same five-chain wet take at 137 BPM. The all-kit rapid-hit, extended soak and level-matched synth/FX quality pass remain open. A qualified new release and deployment remain open.
