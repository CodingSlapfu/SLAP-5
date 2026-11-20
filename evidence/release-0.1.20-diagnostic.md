# SLAP-5 0.1.20 Diagnostic

**EXPERIMENTAL — PHYSICAL PERFORMANCE UNVERIFIED**

Firmware identity `FM-1_75000120`. Use the release FWSC with M-Flasher on the same supported FM-1 configuration as 0.1.18/0.1.19. Back up your projects before flashing. The package uses the V15-style application layout, 1 MiB 0x856014 flash, chip key 0x980F, and application write window 0x4000–0x92FFF. The primary installer stays on v0.1.18. To roll back, install the original v0.1.18 FWSC with M-Flasher. [Installation and recovery instructions](https://slap-5.jminie66.chatgpt.site/flash#m-flasher) · [Rollback package](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.18-testing)

## Physical evidence, v0.1.19

Four frozen pages at 165 BPM are consistent with trace 6. The scope is a clean half-buffer. It reports 5,112 µs DMA, 5,109 µs app, 0 µs displayed acknowledgement, 329 µs Timer 5 across 51 timer interrupts, no half XRUN, no deadline flag, and zero fault records. Voice batch 3,296 µs, local voice 3,202 µs, note-on 10 µs, dispatch 47 µs, max span 785 µs, batch FX 860 µs, other app 952 µs, and outside app 3 µs. Core 1 reports 1,651 µs work with the same measured overlap, 1 µs wait and seven jobs completed on buffer 0. The live footer shows 16 voices and zero fallbacks or DMA XRUNs. Its cumulative worker counts advance while the trace remains frozen.

This half had 692 µs reserve and was 179 µs above the 4,933 µs target. Local voices dominate the measured work. Timer 5 is a measurable contribution and acknowledgement and wait are small in this half. Timing fields overlap. The previous 160 BPM miss was not captured. The photos cannot establish whether its cause was synthesis, interrupts, contention or DMA timing, or quantify on-device trace overhead.

## Change and verification

Unmodulated tonal voices without a steal tail render attack and sustain in a bounded span. The renderer keeps envelope state and countdown outside the general per-sample path. It breaks at the same 0.94 attack threshold and hands the exact gate-off sample back to the general renderer. The worker policy, effects and 16-voice budget are preserved. Native disassembly shows the dedicated attack/sustain span in the same SRAM path. Separate baseline WAV comparisons were byte-identical in synchronous worker and single-core mode for 112 BPM dry chords, 137 BPM modulated wet FX and 160 BPM GATE.

Five paired host thread-CPU runs, synchronous worker. The wet timing fixture here does not enable the separate sine LFO option. Values are median run statistics in µs.

| Fixture | Mean old → new | P95 old → new | P99 old → new | Peak old → new |
|---|---:|---:|---:|---:|
| 112 BPM | 40.20 → 39.17 | 96.04 → 92.53 | 139.25 → 146.37 | 1,215.41 → 811.04 |
| 137 BPM | 54.21 → 59.59 | 115.78 → 130.43 | 166.28 → 186.28 | 2,643.84 → 4,486.88 |
| 160 BPM | 39.33 → 38.62 | 100.45 → 99.91 | 140.87 → 140.02 | 2,189.29 → 1,485.82 |

The critical fixture showed small mean and P99 changes; results are mixed. Single-core 160 BPM median peak increased from 344.57 to 1,893.76 µs, with P99 109.31 to 111.00 µs. The worker increase on the wet timing fixture and higher peaks require watching. One threaded host run had nine fallbacks and ended offline, but matched the audio within harness tolerance. The ten-minute synchronous GATE soak maintained the 16-voice maximum and completed 434,613 jobs with no fallbacks. Deterministic recovery tests, native package regeneration, updater identity checks and Studio/WASM checks including all 56 rapid-hit kits passed. No physical timing improvement is established.

App 162,832 bytes, static RAM 56,840/98,304, pool 312,420/344,064. The verified package SHA-256 is `daa5b5a85a1969187e63e2346b45edf25828383d664bd1090ae52347065ab856`.

Optional physical check: replay the same 165 BPM project once in DMA TRACE. Type the peak, deadline/XRUN counts and Core 1 status. If a fault occurs, freeze it and include its trace ID, local voice time, batch FX, worker wait and nested Timer 5 time. No additional photos are needed.
