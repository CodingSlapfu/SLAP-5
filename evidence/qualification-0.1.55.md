# SLAP-5 v0.1.55 hardware testing

Issue #9 Section E records five separate v0.1.52 snapshots at 95 BPM. The 6,232 µs callback exceeds the 5,804 µs deadline, while Core 1 remained online and worker wait was zero in the photographed breakdown. v0.1.54 still lagged according to the owner. Its callback timings were not captured.

Cold boot after direct installation, load working copies of both saved projects, then reset diagnostics. Replay the actual VHS SAW / SLICER / DEEP DUB session at 95 BPM and JUNGLE at 155 BPM / 60% swing. Include recording, overdub, visible animations and transport restart. Capture peak callback, P95/P99, deadline misses, DMA XRUNs and Core 1 jobs and fallbacks. Target 4,643–4,933 µs peak, zero new misses and XRUNs, and clean sound. Obtain explicit approval before public promotion.

## v0.1.55 performance checkpoint

The owner reports that v0.1.54 still lags during the same peaks. The five independently captured v0.1.52 pages at 95 BPM show a 6,232 µs peak against 5,804 µs, 192 deadline misses on the first page and 798 XRUNs on the last. The 3,887 µs local voice figure, 1,144 µs worker work, zero worker wait and 486 µs nested Timer 5 figure come from separate pages. The worker was online with zero fallbacks. No synchronized trace or v0.1.54 timing follows from those observations.

- v0.1.55 specializes the no-steal-tail ADPCM drum loop and reuses each packed XIP byte for both nibbles. The SLICER skips the second read at integer capture positions and FX caches the active live-insert mask at configuration changes. The original sample bank, effect topology, worker restrictions and 16 voice slots remain available.
- Native parallel FX remains disabled. The host path checkpoints mutable delay, chorus, reverb, slice and FX state before launch, then rolls back and rerenders only after ownership returns. On native hardware a hard timeout here can retire Core 1, and the cost of checkpointing plus XIP contention has not been bounded by measured FM-1 traces. The FX batch diagnostic is therefore not a measure of serial FX cost. Next step is same-callback serial FX, mixer, voice-engine spans and IRQ timing under the actual owner projects, then a bounded native batching design with an explicit state-ownership and failure test.
- Timer 5 services input and LED scanning and scheduled USB audio. USB MIDI and UART MIDI polling are deferred during nested audio render. Measure the nested input and UAC portions separately with connected and disconnected USB before altering cadence or priority.
- Free space is 15,848 B application flash, 8,856 B pool and 10,088 B static RAM. Compared with v0.1.54 this spends 544 B of application flash and 112 B of static RAM. Boot compression remains enabled. No sequencer/undo, asset migration or FX arena changes qualified in this build.
- JUNGLE and the wet-track SLICER simulation are representative host fixtures. Their matched PCM is bit-identical. The owner’s two saved projects are needed for FM-1 acceptance. The optional typography and lane note color changes, and outstanding knob redesign, carry forward.

## Matched results

Three alternating CPU-pinned host trials per MIDI mode at 155 BPM. Median of trial CPU-time percentiles and maxima in µs.

| MIDI mode | P50 v54 → v55 | P99 v54 → v55 | Max v54 → v55 |
| --- | ---: | ---: | ---: |
| Receive | 114.50 → 113.01 | 157.72 → 155.03 | 214.01 → 222.75 |
| Transmit | 115.18 → 114.47 | 184.93 → 183.92 | 214.62 → 220.95 |
| Both | 117.41 → 116.67 | 178.62 → 178.08 | 203.44 → 203.66 |

Host P99 improved modestly and trial maxima rose. The host measurements do not establish a native peak improvement. Matching WAVs for JUNGLE and a wet-track/SLICER variant were byte-identical. [Pinned benchmark](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099941380) · [Native and rollback qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099825024).
