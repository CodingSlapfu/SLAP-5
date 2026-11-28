# SLAP-5 internal v0.1.56 performance checkpoint

Application `aad93d454eefc15920b8d39beb91fc6316e6b550`, identity `FM-1_75000156`. No browser installer or public deployment was made. Public Pages remains at `d597971d8ab68922c6bb9478286964bb5abee5ba`. Resume from this compiled checkpoint for profiling. The last hardware-testing build remains v0.1.55.

## Changes and tradeoffs

- Keep native stateful track FX on Core 0. For eligible spans, batch each track's FX and wet blend in its existing voice buffer. This eliminates per-sample cross-file dispatch and uses contiguous samples for each FX instance. Ducking, count-in, detailed profiling, very short spans and the host offload fixture retain the scalar route. The batch boundary respects internal transport and arpeggiator transitions. MASTER FX state is separate.
- Cache the two fixed VHS SAW coefficients and the two string feedback coefficients in existing mutually exclusive engine storage. Consolidate end-of-work Timer 5 timestamp reads while retaining the existing key, USB and UAC service cadence. The Timer 5 reading now ends just before the ISR diagnostic and clock bookkeeping.
- Store each drum bank offset and frame count in one 32-bit descriptor. The generator checks both field capacities, and encoded sound bytes and playback resolution are identical. The descriptors recover 1,600 nominal bytes. Combined with the other changes, the linked application image still grows.
- SKIFF's pinned `firmware/src/fx.c` compares estimated track work and credits handoff overhead before splitting. SLAP-5 has different voice placement and independent mutable FX rollback buffers, so this sprint keeps its existing voice admission and FX ownership rules. Melodee's pinned RAM/CPU report supports moving invariant kernel work out of inner loops and validating the complete linked image. The SLAP-5 changes are original. Boot packing remains enabled. No sample-rate, polyphony, clock or project-format change was made.

## Results

[Native qualification and lockstep JUNGLE/wet-SLICER replay](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46103659561) passed with bit-identical WAVs against v0.1.55, verified package boundaries, update recovery, rollback, and a 16-voice host run. [FX-only experiment](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46103495792) and [FX plus kernels](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46103609641) are retained as separate comparisons.

[Consolidated pinned host timing](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46103659635), 155 BPM, seeded dense FX/visual/MIDI replay, median across three trials per variant. CPU time in µs. The wet-SLICER PCM fixture is representative and does not use the owner's original saved project.

| Mode | P99 .54 / .55 / internal | Trial maxima .54 / .55 / internal |
| --- | --- | --- |
| Receive | 157.56 / 154.48 / 147.39 | 199.13 / 214.77 / 216.29 |
| Transmit | 185.26 / 184.51 / 176.29 | 229.99 / 270.25 / 192.43 |
| Both | 178.53 / 178.08 / 170.36 | 197.71 / 200.55 / 204.40 |

The P99 trend repeated across experiments, but peak gains were not consistent. App usage is 568,712 / 581,564 B, up 2,996 B from .55. Static RAM is 88,216 / 98,304 B and pool is 335,208 / 344,064 B, both unchanged. No resource stretch target was attained.

The next blocker is native attribution: measure per-engine local work, serial FX cost and nested Timer 5/UAC within the same FM-1 callback on the original 95 BPM wet project. Capture current .55 first with resets and USB connected versus disconnected, so an intermediate flash is unnecessary. Host measurements cannot establish effective native handoff cost, the worst-case worker wait margin, or the contribution from XIP contention. Use those measurements to tune a bounded split or decide whether to stage independent FX state. Replaying a desktop benchmark cannot answer those questions.

## Verified backup

- Firmware SHA-256 `7ba615bd717e8ca5a9970a5e5ded2e22120b8c4ec803c8fd683310358e17d33f`
- Backup SHA-256 `2d6417eafd407d21675cca4ca08e14f17cbdef93a054b84e947cd043108172b9`
- [Native artifact with firmware, source, layout report and SHA256SUMS](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46103659561/artifacts/15246855106)
