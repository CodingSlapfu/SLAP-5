# Groovebox overhaul progress

Baseline [38d5f7a](https://github.com/CodingSlapfu/SLAP-5/commit/38d5f7ab382a21e1ee526c56599f788c3643044c).
The owner brief calls for 128 distinctive patches, 25 sample-backed factory kits with 16 sounds each, legacy handling for all 56 prior kit IDs, 16 drum lanes, HOME and recording changes, black-key LIVE FX, harmony controls, settings and eight visualizers.

## Baseline and sample budget

The v0.1.20 package audit gives 581,564 bytes for the application slot and 162,832 bytes for the app, leaving 418,732 bytes of arithmetic slot headroom before new code, alignment or data residency constraints. This subtraction is an upper bound, not a measured sample budget. At 16 kHz, four-bit mono ADPCM consumes approximately 8,000 bytes per second before headers. Spending the whole upper bound on 400 unique recordings would average about 131 ms per slot. Shared immutable recordings, varying one-shot lengths and a compact index warrant evaluation. The updater write region alone does not demonstrate that samples can be accessed by both cores at the required bandwidth or that linked sample data avoids RAM residency.

Reference static RAM is 56,840/98,304 bytes and pool usage is 312,420/344,064 bytes. The v0.1.20 hardware run reached 16 voices at 137 BPM and recorded a 5,178 µs peak with zero misses, XRUNs or fallbacks. No new hardware timing or native memory figures are available for this checkpoint.

## Implemented and exercised

- Added the host audition runner `device/tests/audition-patches.py`. It selects and checks all 128 IDs on the firmware host and records MIDI input at three pitch and velocity combinations followed by a held chord. It writes unmodified WAVs and an index of peak and RMS levels for matching in a separate listening comparison. All 128 baseline files had nonzero signal. Peaks ranged from −39.81 to −29.24 dBFS and RMS from −59.52 to −44.34 dBFS in this fixture. These measurements establish signal and level ranges. Subjective listening and similarity analysis remain open.
- Renamed SPECK INK to GRAIN BLOOM with its ID and parameters intact.
- HOME and the sequencer draw held durations from the stored recording field, using the full stored timing offset and carrying visible tails across loop boundaries. The display uses the nominal tempo grid and does not estimate per-step swing. The white playhead accent follows the stored note interval and can appear on a probabilistic event even when playback skips that event.
- Selecting a new track from HOME ends the old take through the existing audio-side finalization, while preserving running playback. The extended quick-record regression changes tracks during a held note, confirms the take ends and records again on the new track.
- The harmony header now says KEY & CHORDS. Existing key, scale, chord and strum behavior is preserved.

Desktop build and `held-recording`, `wrapped-hold`, `poly-record`, `quick-record` and `pages` passed. Native build, image-level display verification and full release qualification remain open.

## Remaining acceptance

Redesign and audition the patch library. Implement sample decoding, assign all 400 slots, source acoustic recordings with verified provenance, migrate legacy kit references and expand the sequencer to 16 lanes. Complete transport preferences, playback-safe undo, confirmed clear and cancel controls. Implement black-key LIVE FX and independent Perfect Note controls. Build settings and its persistence. Redesign and benchmark all eight visualizers. Complete the firmware-wide audit, native/WASM and storage regressions, memory report and software release qualification. Physical performance remains unverified. Keep the existing public installers.
