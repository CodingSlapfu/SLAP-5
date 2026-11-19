Native FM-1 engineering TESTING, identity `FM-1_75000107`.

- RECORD captures a one-, two- or four-bar take with first-note or four-beat count-in start, independent metronome, quantization, and overdub or replace. Existing projects keep their format. Off-grid events use the stored eighth-step offsets and note lengths use the existing gate field.
- 32 procedural drum kits, including the four original kit IDs, with the established 16 GEN DRUMS styles and eight visualizers.
- Host fixtures verify recording, playback, generator editing, and single-core/worker/fallback audio equality. The native package passed application, loader and image checks. The WebAssembly studio and simulated USB MIDI upgrade passed.
- Physical listening, dual-core headroom and dense-recording timing are pending. The refined font family and full Studio recording/generator editing remain in development.
- For rollback use the previous 0.1.4 TESTING release. Save patterns using original drum kits before rolling back. The older firmware cannot load banks that use the new kit IDs.
