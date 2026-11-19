# SLAP-5

SLAP-5 is an original five-track groovebox for the M-VAVE FM-1. Copyright (C) 2026 Ziao.

**Development source.** Keep the current package off physical hardware. SDK and package integrity checks pass. Physical timing, flash compatibility and update recovery remain unverified, and the full TESTING release has additional feature work.

## Controls

| Control | Function |
| --- | --- |
| HOME | Return to the track sequencer. Press again to open the nine visualizers. |
| SELECT | Choose a track on HOME, a step in SEQ, a patch in EDIT, a page in FX or a slot in SAVE. |
| ALGO | Set BPM. |
| PRESET | Select one of 128 factory patches for melodic tracks, or one of four drum kits on track 5. |
| HOME knobs | Pan, steps (1–64), level, swing. |
| SEQ knobs | Pitch, velocity, note gate, hit probability. Press a key to insert or toggle the current note. |
| FX | Main, filter, slicer, insert, processor, reverb, tempo delay, chorus and drive. |
| SEL | Open harmony on HOME for a melodic track. Set key, scale, chords and strum. |
| ENV / LFO | Edit ADSR or pitch LFO on a melodic track. |
| ARP | Choose the keyboard arpeggiation mode, tempo division, octave range and gate. |
| SAVE | While stopped, hold the selected white key for four seconds to save. Press SEL to load. |
| PLAY / OCT± | Transport and keyboard octave. |

USB MIDI channels 1–5 address the five tracks. The groove uses one note event per track per step, and the harmony engine can spread a chord across voices. The display uses SLAP BLOCK lettering on the logo. The opening animation plays two brief startup tones. During playback HOME scrolls note blocks past a fixed playhead.

## Build and verify

The official repository's original source checkpoint is [9588bb2](https://github.com/CodingSlapfu/SLAP-5/commit/9588bb2d672ab4ee4d7e38e60d272be576d32d84). Its source tree matches the recovered local checkpoint at a3ceb2002d74aab16422d8c4ccccd73114b21966. The fm1-base and felucca-bsp licenses remain in the vendor tree. Board support came from the local AQUORA integration at d2c5087. AQUORA and KIRA-9 were used as read-only references.

```sh
python3 vendor/fm1-base/tools/build.py device --host
for test in groovebox pages storage arp; do
    device/build/host/slap-5_host "device/tests/$test.txt" device/build/host
done
site/build.sh
node device/tests/emulator.mjs
python3 -m http.server 8764 --directory device/build/studio
```

The site build requires Emscripten on PATH. Use `http://localhost:8764` to open the site, SLAP STUDIO, and the browser instrument emulator. SLAP STUDIO shares the C DSP through the same WASM AudioWorklet and provides track editing, audition, undo/redo and JSON import/export. SLAP FLASH uses the FM-1 MIDI identity handshake and has a gated USB MIDI update path. No approved release manifest is bundled, so installation remains disabled. See [installer verification and recovery](evidence/installer-verification.md).

An optional Chromium test is available at `site/tests/studio.cjs`. Run it with Playwright installed and the local site server running. This environment returned EPERM when Chromium tried to open its process-singleton socket. The compiled AudioWorklet and project protocol were exercised using Node. Physical USB/Web MIDI interactions remain untested.

For the native development build, set `JIELI_TOOLCHAIN` to the JieLi Linux toolchain (tested with the 20260730.1 bundle) and `AC79_SDK` to the SDK checkout at d179b4484759423312073f5fbb232501aa491047. Run `device/build-native.sh`. This checks the three pinned SDK hashes before invoking the native build. Provenance and both sets of SHA-256 values are in [SDK pinning](evidence/sdk-pinning.md). See `evidence/` for the image sizes, package checks, software tests and SHA-256 hashes.

## Remaining TESTING release work

- Expand the sequencer to independent multi-hit drum lanes and polyphonic steps, ties, microtiming, recording and arrangement. Finish individual drum mute/solo and pattern variation.
- Add LIVE FX performance controls, arpeggiator project persistence and MIDI input, Nature beds, comprehensive GLO diagnostics, Bluetooth and clock sync. Complete patch favorites, categories, parameter editing and the remaining studio device transfer and editing workflows.
- Extend the scrolling sequencer and display typography across the remaining pages. Listen through the synth and patch library, tune the drum kits and effects, and profile the heaviest DSP and visuals on an FM-1. Implement or demonstrate the dual-core strategy and safe fallback.
- Verify the storage sectors, update compatibility, bootloader retention and recovery on supported devices. Qualify the gated SLAP FLASH transfer and rollback paths on supported devices and complete browser integration testing.
- Publish the TESTING release and complete HTTPS site together once the build passes the release gates. Publish version, identity, checksums, release and source links, and tested installation and recovery instructions.

The SDK check verifies the reference file set only. Native static checks and host timing cannot establish physical-device acceptance.
