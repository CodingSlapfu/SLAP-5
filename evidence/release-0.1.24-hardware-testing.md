# SLAP-5 0.1.24 experimental hardware testing

Firmware identity `FM-1_75000124`. The firmware and Studio/emulator ZIP are built from the same source checkpoint.

- Six-operator FM with three routings, ratio and level editing for each operator. Four interpolated wavetable frames use pitch-dependent harmonic bands. SELECT in PATCH opens the dedicated editors.
- Drum generation offers 1–4 bars, including 48 steps. It queues at a bar boundary during playback. Existing locked cells and events outside the generated region are retained.
- Studio adds a five-track arranger, editable scenes and repeats, mixed clip lengths, audio-driven playhead, scene queues, a mixer with track meters, and FM6 and wavetable controls. Song projects save separately from standalone pattern projects.
- The firmware packs timing data, fixed-pitch drum cells and large drum undo snapshots. Static graph bands repaint when their display state changes.

## Verification

[Source](https://github.com/CodingSlapfu/SLAP-5/commit/9988b9fb647848d72931ebcc5e204a26281b942a) · [Hosted Chromium](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44884481053)

The pinned native build uses 547,552/581,564 bytes of application flash, 86,280/98,304 bytes of static RAM and 319,368/344,064 bytes of pool RAM. Native installation bytes were independently reconstructed. Simulated forward installation, loss of an in-progress write, retry, rollback to 0.1.18 or 0.1.22 and preservation of the boot area and saved bank bytes passed. Host verification covers 16 voices, dual-core parity, all five slicers and inserts, the eleven Live FX, legacy banks and edited timbre saving. This release remains experimental until physical FM-1 timing, audio, display, stack and USB recovery are verified.

Older banks and Studio projects still load. Edited FM6 or wavetable banks use V10 storage and need 0.1.24 or later. Archive existing projects before updating. Use the current firmware or Studio's standalone project export when moving work between versions. A multi-scene Studio song is a desktop project, with standalone clip/scene editing available through Studio. Recording and live drum generation run in standalone pattern mode.

## Install and recovery

Use the [experimental browser installer](https://slap-5.jminie66.chatgpt.site/flash-testing.html) from a known 0.1.18 or 0.1.22 source on a compatible V15-style FM-1. Verify JEDEC flash ID `0x856014` and chip key `0x980F`, and back up the particular device. The firmware write plan must be application-only within `0x4000–0x92FFF`. M-Flasher is the alternate installer and recovery route. Use its same-unit backup to recover if needed. The primary website installer stays at 0.1.18.
