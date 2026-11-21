# SLAP-5 0.1.25 experimental hardware testing

Firmware identity `FM-1_75000125`. This candidate is for owner testing on a compatible M-VAVE FM-1. The owner's successful hardware feedback applies to 0.1.24. Physical install, sustained real-time audio, LCD timing and physical rollback for 0.1.25 are pending. The primary installer remains on 0.1.18 and 0.1.24 remains available from its GitHub release.

## Changes

- One row of five taller HOME track cards, with patch labels and activity strips. Sixteenth-note edit grids have distinct bar, beat and step marks. SELECT previews stored notes and drum hits through available voices when moving the edit cursor.
- Vinyl Stop slows the captured signal and drops its pitch. The full-mix stop retains stereo and the song clock keeps running. The host stereo test measured 27, 17 and 9 positive crossings across successive equal windows.
- Independent Live FX assignments include Cutoff, Res, Soft, Hard, Fold, Flang and Phaser.
- Eight new saw programs use IDs 128–135. Existing patch and kit IDs remain in place. The HOME font-width check covers 136 patches and 81 kit labels.
- FM6 operator envelopes and feedback use V11 banks. V10 FM6 and wavetable banks remain readable. Back up V11 work in Studio or keep this firmware available before downgrading.
- Studio uses the sixteenth-note grid and supports active-scene Quick REC, drum generation, clip undo and per-bar level and pan automation.
- Dancer has a black field, gentle idle motion and the existing 30 poses. Genuine full-resolution source masters were unavailable in the checked archive. The current render uses the retained indexed artwork. Solar orbit speed is tripled. Digital VU is stepped, and DNA and Ferrofluid use band energy.

## Verification

The pinned JieLi package rebuild validated the identity, loader and application-only window from `0x4000` through `0x92FFF`. Host loader simulation completed new installs, interrupted-install recovery and rollback while retaining song and settings sectors. Host and WebAssembly checks covered sequencing, DSP, patches, drum kits and arranged recording and automation. The isolated callback comparison is a host measurement. The separate hardware gate still requires the actual FM-1.

Use the [experimental installer](https://slap-5.jminie66.chatgpt.site/flash-testing) for V15-style hardware with JEDEC `0x856014`, a 1 MiB flash chip and chip key `0x980F`. Keep a verified backup from the same device. M-Flasher is available for recovery when appropriate.
