SLAP-5 v0.1.13 engineering TESTING for compatible M-VAVE FM-1 hardware. Copyright Ziao.

- Ending a take now finalizes pending note lengths and records a pattern revision. It no longer hashes the entire pattern on the audio callback. The undo snapshot covers the recorded range and invalidation still protects intervening edits.
- The native worker accepts drum-only, eligible synth-only, and mixed batches. Private oscillator and voice state is committed in original slot order. A late voice job can finish its discarded copy while Core 0 recomputes the output. Faults and stuck workers retain the local path.
- GLOBAL K3 resets the measurement window. The display shows estimated P95/P99, peak, deadline misses, DMA XRUNs, complete worker job counts, fallbacks and active voices.
- Large SLAP BLOCK lettering is preserved. The LCD uses a clearer companion font for small text and short, width-checked patch and kit names. Studio reads the same names from the firmware tables.

The host worker simulator matched single-core PCM for drum-only playback after recording, synth-only playback, and the existing mixed and heavy-FX cases. Native FM-1 deadline, DMA and readability acceptance remain open. No new drum timbres or visual effects were changed in this release.

Firmware identity `FM-1_75000113`. FWSC SHA-256 `430fc1fb93587f13a353a7522be5a80091228fb425e6d8a140381196000708cd`.
