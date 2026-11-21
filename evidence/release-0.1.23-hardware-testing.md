# SLAP-5 v0.1.23 experimental hardware testing

Firmware identity `FM-1_75000123`. The matching Studio ZIP ships with the same source revision and its compiled FM-1 emulator.

- HOME now displays REC READY before a first-note take. Count-in clicks precede playback and the dedicated green PLAY LED marks the quarter note after transport starts. Restart mode returns to step 1.
- The mixer offers 0–100 level, center pan, solo and drum-triggered ducking. Auto Level defaults on. Audio settings can enable bounded Auto Mix independently. FX PROC K2 controls the overall track effect blend.
- Slicer repeats use a bounded capture and windowed playback on the transport grid. The eleven Live FX include Halo Room, Vinyl Stop and Rewind.
- The same 25 factory kits and 400 sounds have stronger gain staging and more differentiated voices. Drum generation can queue a new pattern during playback and apply it on a bar boundary. Use SEL+GLO in the drum sequencer to protect a cell. The lock is session-local.
- Melodic SEQ puts note, accent, rest and hold on K1–K4. All seven visualizers are in the emulator, including a five-track Digital VU and the original PARALLAX-8 Dancer poses. HOME displays battery state.

## Software verification

[Source](https://github.com/CodingSlapfu/SLAP-5/commit/42012026a19ec27d68972f275e59d7b1096dec7c) and [hosted Chromium](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44874753233) passed. The dense host fixture exercised 16 voices, all five inserts and slicers, four ducked tracks, Auto Mix, repeated bar-boundary drum generation and all eleven Live FX at 112, 125, 137 and 160 BPM. Worker and serial output matched within one 16-bit PCM unit. The pinned native build uses 539,252/581,564 bytes of application flash, 96,504/98,304 bytes of static RAM and 334,888/344,064 bytes of pool RAM.

The firmware package regenerates byte-for-byte. Loader simulation exercised 0.1.18 and 0.1.22 installs, an interrupted app write, retry, rollback, foreign-key rejection and exact preservation of the boot area and song/settings flash.

## Hardware testing and recovery

Use a compatible V15-style FM-1 with JEDEC flash `0x856014` and chip key `0x980F`. Confirm the supported hardware and back up this unit before updating. Its app window is `0x4000–0x92FFF`. Keep the bootloader and project store. The experimental browser route accepts known 0.1.18 and 0.1.22 source identities. M-Flasher can inspect hardware and restore the unit's automatic backup.

Physical LED, battery, LCD, DSP timing, stack watermark, speaker response, USB update and recovery still require on-device confirmation. Kit and effect listening quality still needs an audition. The Studio editor generates drums while stopped. The FM-1 emulator shares the firmware's live generation workflow. Song format remains V9-compatible. Firmware predating the expanded preferences may fall back to default preferences on rollback. Retain independent backups of song banks and settings.
