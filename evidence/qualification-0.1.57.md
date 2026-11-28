# SLAP-5 v0.1.57 hardware test

Physical acceptance is pending. Install only on compatible FM-1 hardware. Keep untouched project exports and a recovery backup. Cold-boot after direct upgrade, then replay the original 95 BPM VHS SAW / SLICER / DEEP DUB session and JUNGLE at 155 BPM and 60% swing. Reset diagnostics and record callback peak, deadline misses, DMA XRUNs and Core 1 status. Include overdub, transport restart, dense wet tracks and animated screens, while adjusting dials. Verify all four themes on the native LCD at minimum, midpoint and maximum and across bipolar center. Public promotion requires explicit approval.

## Acceptance matrix

| Area | Result and remaining gate |
| --- | --- |
| Dial themes | Four original packed RGB565 materials, distinct scales and indicators, shared renderer, persistent setting. Native-size host captures and distinct MIXER footer assertions passed. Physical LCD appearance and full endpoint sweep pending. |
| HOME | LOOPER and BPM share the patch-name glyph and 16 px height without shadows. Orange and yellow notes use distinct inks. Native-size HOME captures and LCD traffic regression passed. |
| Performance | Carries forward v0.1.56 ADPCM descriptors, cached synth coefficients and reduced Timer 5 timestamp reads. Batches eligible independent FX tracks even when ducking is active, with the dry sidechain source kept serial. Native peer ownership is unchanged. |
| Audio and compatibility | Matched JUNGLE and wet SLICER PCM bit-identical to v0.1.55. MIDI clock, dense recording, Sound Lab, transport and 16-voice host checks passed. Native package, interruption and rollback simulation passed. Physical FM-1 timing remains unverified. |
| Release | Matching Studio, site, captures and exact live installer verified. Current-only promotion staged separately. Explicit owner approval required. |

Application size 571,016 / 581,564 B, +2,304 B since internal v0.1.56. Static RAM 88,216 / 98,304 and pool 335,208 / 344,064, both unchanged. No net resource recovery in this pass. Native handoff cost and same-callback FX/voice/Timer 5 profiling still need physical data before retuning the worker.

In the existing pinned 155 BPM dense-host benchmark, candidate P99 was 131.85/155.22/129.31 µs in receive/transmit/bidirectional MIDI. The workflow's old v0.1.49 baseline was 179.16/239.01/180.61 µs, so this is a cumulative comparison, not an incremental v0.1.56 speedup. The previous internal v0.1.56 same-run baseline comparison remains in [the checkpoint](performance-internal-0.1.56.md).

[Native backup and captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105825310/artifacts/15247727215) · [LCD/feature regression](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105825326) · [Host timing](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105825256) · [Promotion and checksums](../PROMOTE-0.1.57.md).
