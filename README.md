# SLAP-5

SLAP-5 is an original five-track groovebox for the M-VAVE FM-1. Copyright (C) 2026 Ziao.

**Development branch.** The public installer and recovery links are listed on [SLAP FLASH](https://slap-5.jminie66.chatgpt.site/flash). The [v0.1.21 hardware-testing release](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.21-hardware-testing) is the owner-tested audio baseline for this branch. Check the engineering record before flashing a newer package.

## Performance controls

| Control | Function |
| --- | --- |
| HOME | Return to the looper. Press again to open the six visualizers. |
| SELECT | Choose a track on HOME. During a take, the old track keeps looping and the next selected track starts recording at the current transport position. |
| REC tap | Start or finish a HOME take. Choose count-in versus first note and replace versus overdub in SETTINGS. |
| REC + OCT− | Undo the selected track's last short-loop take or clear. |
| REC + OCT+ twice | Clear the current loop. |
| REC + SEL twice | Clear the complete selected track, including hidden legacy steps. |
| REC + ARP | Cancel the current take and restore its snapshot. |
| Hold REC for 2 seconds | Open advanced RECORD. |
| Tap EDIT | Open PATCH. |
| Hold EDIT for 2 seconds | Open MIXER. Press HOME to exit. The first five white keys mute tracks 1–5 and show their enabled state. ARP toggles the selected track's solo. |
| Tap GLO | Open SETTINGS from a performance page. |
| Hold GLO for 5 seconds | Open DIAGNOSTICS. Short GLO presses inside diagnostics cycle its pages. |
| Double-press FX | Open LIVE FX. Black keys apply momentary effects and white keys play notes. SEL switches between the selected track and the full mix. |
| SEL on HOME | Open KEY & CHORDS for the selected melodic track. OCT− toggles Perfect Note, OCT+ cycles the chord type, K2 sets Split Keys, and SEL+K2 sets the scale. |

HOME offers 16, 32 and 64 steps. Legacy lengths and stored notes remain intact on load. An explicit length change changes the loop boundary while retaining events beyond it. Undo keeps one snapshot per track for short loops. Long legacy edits reuse the bounded whole-track snapshot.

SEQ edits four melodic note lanes or sixteen fixed drum lanes plus the legacy drum row. Turn SELECT to move the cursor and hold SEL to choose a lane. K1–K4 set pitch, velocity, gate and probability. SEL+K3 sets melodic hold duration, and SEL+K4 sets timing within the step. Holding SEL activates Fill in SEQ. SEL+OCT+ cycles Normal, Ratchet and Fill Only for the current note. SEL+EDIT copies a step, SEL+LFO cuts it, SEL+ENV pastes it within the same lane, and SEL+OCT− clears it. Sample-backed drums play one-shots; melodic holds can span step boundaries.

SETTINGS offers Audio, Metronome, Recording, Transport, Keyboard, Display, Diagnostics, Live FX Knobs and Lights. Auto Level is enabled by default and works alongside manual mixer levels. Metronome modes are Off, On and Rec Only. Live FX assignments temporarily control the labeled effect parameters and persist across reboot. The full-mix path uses a centered wet signal and a bounded short echo with a stereo dry return.

USB MIDI channels 1–5 address the five tracks. Use ALGO for tempo and PRESET for one of 128 melodic patches or the 25 sample-backed factory kits. All 56 legacy drum kits remain readable. Press SAVE while stopped, hold the target white key for four seconds to save, or press SEL to load the selected slot.

To generate a drum pattern on the FM-1, select track 5 from HOME and press SEQ.
The drum sequencer shows **SEQ: GEN DRUMS**. Press SEQ again to open the
generator. With transport stopped, set STYLE, HIT %, GROOVE and VAR % with
knobs 1–4 and press SEL to generate. If prompted, press SEL again to replace
edited notes. Press SEQ to return to the editable pattern. In SLAP STUDIO,
select **5 · Drums** and use the generator controls next to the drum sequencer.

## Build and verify

The official repository's original source checkpoint is [9588bb2](https://github.com/CodingSlapfu/SLAP-5/commit/9588bb2d672ab4ee4d7e38e60d272be576d32d84). Its source tree matches the recovered local checkpoint at a3ceb2002d74aab16422d8c4ccccd73114b21966. The fm1-base and felucca-bsp licenses remain in the vendor tree. Board support came from the local AQUORA integration at d2c5087. AQUORA and KIRA-9 were used as read-only references.

```sh
python3 vendor/fm1-base/tools/build.py device --host
for test in groovebox pages storage arp poly; do
    device/build/host/slap-5_host "device/tests/$test.txt" device/build/host
done
site/build.sh
node device/tests/emulator.mjs
python3 -m http.server 8764 --directory device/build/studio
```

The site build requires Emscripten on PATH. Use `http://localhost:8764` to open the site, SLAP STUDIO, and the browser instrument emulator. SLAP STUDIO shares the C DSP through the same WASM AudioWorklet and provides track editing, audition, undo/redo and JSON import/export. SLAP FLASH uses the FM-1 MIDI identity handshake and has a gated USB MIDI update path. No approved release manifest is bundled, so installation remains disabled. See [installer verification and recovery](evidence/installer-verification.md).

An optional Chromium test is available at `site/tests/studio.cjs`. Run it with Playwright installed and the local site server running. This environment returned EPERM when Chromium tried to open its process-singleton socket. The compiled AudioWorklet and project protocol were exercised using Node. Physical USB/Web MIDI interactions remain untested.

For the native development build, set `JIELI_TOOLCHAIN` to the JieLi Linux toolchain (tested with the 20260730.1 bundle) and `AC79_SDK` to the SDK checkout at d179b4484759423312073f5fbb232501aa491047. Run `device/build-native.sh`. This checks the three pinned SDK hashes before invoking the native build. Provenance and both sets of SHA-256 values are in [SDK pinning](evidence/sdk-pinning.md). See `evidence/` for the image sizes, package checks, software tests and SHA-256 hashes.

## Release qualification

See [the engineering record](evidence/groovebox-overhaul.md) and [owner acceptance checklist](https://github.com/CodingSlapfu/SLAP-5/issues/2) for the current tests and remaining physical gates. Version 9 song slots are used only for ratchet or Fill Only metadata. Earlier song versions remain readable. Keep independent project backups before moving between firmware versions.
