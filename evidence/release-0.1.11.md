# SLAP-5 0.1.11 TESTING

Native identity `FM-1_75000111`.

- RECORD supports 1, 2, 4, 8, 16 and 32 bars, sixteenth-step timing, first-note or count-in start, replace or overdub, and undo of the previous take. HOME quick REC and the two-second settings gesture remain available.
- Legacy V2–V4 firmware banks can still be opened. Long or finer-timed takes use sparse V5 storage. It fits 587 events per bank in the existing flash map. An over-capacity save leaves the original bank untouched. Save V5 work in Studio JSON when working in the browser.
- Studio adds advanced and quick takes, on-screen keys, optional Web MIDI, project JSON v3 with legacy import, and paged editing across 512 steps.
- SLAP Block Bold, Regular and Italic ship in the firmware and web tools. Track colors carry through the main pages. The font is derived from the original SLAP display grid and SIL OFL Barlow outlines.
- Improved gain balance for 12 patch outliers, plus drum noise, resonance, transient and clap shaping on all 32 kit IDs. All 128 patches, 32 kits, 16 generator styles and eight visualizers remain available.
- Five-chain host benchmark: median 1.094 s before and 0.979 s after caching effect coefficients and removing three filter divisions per sample. Host timing cannot establish FM-1 timing.
- Dual-core deterministic host parity passed with worker enabled, disabled and discarded. Physical listening, timing and the exact cause of the earlier FM-1 crackling remain owner acceptance items.

## FM-1 acceptance

Back up saves first. Install via SLAP FLASH. Use a spare bank for a 32-bar off-grid take, save and reload it, then test HOME overdub and stopped count-in. Replay a dense pattern with effects and Spectrum Reactor, and compare GLOBAL deadline misses and DMA overruns. Test rapid chords on FM Bass, a pad and a physical-model patch. Note any crackle that continues during replay.

Rollback is available through v0.1.7 TESTING. Use a different bank before downgrading because older firmware cannot load V5 banks.
