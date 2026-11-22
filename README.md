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
| Tap EDIT | Open SOUND LAB. K1 Filter, K2 Resonance, K3 Decay (Room on drums), K4 Drive. Turn ALGO right for the patch browser and synthesis display. |
| Hold EDIT for 2 seconds | Open MIXER. Press HOME to exit. The first five white keys mute tracks 1–5 and show their enabled state. ARP toggles the selected track's solo. |
| Tap GLO | Open SETTINGS from a performance page. |
| Hold GLO for 5 seconds | Open DIAGNOSTICS. Short GLO presses inside diagnostics cycle its pages. |
| Double-press FX | Open LIVE FX. Black keys apply momentary effects and white keys play notes. SEL switches between the selected track and the full mix. |
| SEL on HOME | Open KEY & CHORDS for the selected melodic track. OCT− toggles Perfect Note, OCT+ cycles the chord type, K2 sets Split Keys, and SEL+K2 sets the scale. |

HOME assigns K1 to selected-track level, K2 to loop length, K3 to stereo pan and K4 to global swing. K2 offers 16, 32 and 64 steps. Legacy lengths and stored notes remain intact on load. An explicit length change changes the loop boundary while retaining events beyond it. Undo keeps one snapshot per track for short loops. Long legacy edits reuse the bounded whole-track snapshot.

MIXER assigns K1 to selected-track solo and K2 to kick-triggered ducking on the four melodic tracks. The drum track supplies the sidechain. K3 filters the whole mix and K4 filters the selected track. Both sweep from −100 low-pass through 0 bypass to +100 high-pass. Each track remembers its own filter. Saving an active filter writes a V12 song slot. Older slots open with all filters at zero. Keep a pre-V12 song backup for use with older firmware.

In SETTINGS → AUDIO, select **FM-1 SPEAKER** for speaker processing or **HEADPHONES / AUDIO OUT** for full-range stereo. The selection applies to the shared output mix and persists across reboot.

SEQ edits four melodic note lanes or sixteen fixed drum lanes plus the legacy drum row. Turn SELECT to move the cursor and hold SEL to choose a lane. K1–K4 set pitch, velocity, gate and probability. SEL+K3 sets melodic hold duration, and SEL+K4 sets timing within the step. Holding SEL activates Fill in SEQ. SEL+OCT+ cycles Normal, Ratchet and Fill Only for the current note. SEL+EDIT copies a step, SEL+LFO cuts it, SEL+ENV pastes it within the same lane, and SEL+OCT− clears it. Sample-backed drums play one-shots; melodic holds can span step boundaries.

SETTINGS offers Audio, Metronome, Recording, Transport, Keyboard, Display, Diagnostics, Live FX Knobs and Lights. Auto Level is enabled by default and works alongside manual mixer levels. Metronome modes are Off, On and Rec Only. Live FX assignments temporarily control the labeled effect parameters and persist across reboot. The full-mix path uses a centered wet signal and a bounded short echo with a stereo dry return.

USB MIDI channels 1–5 address the five tracks. Use ALGO for tempo and PRESET for one of 136 melodic patches or the 25 sample-backed factory kits. All 56 legacy drum kits remain readable. Press SAVE while stopped, hold the target white key for four seconds to save, or press SEL to load the selected slot.

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

### Sound Lab graphs and active LED intensity

Tap EDIT for the selected track. Turn ALGORITHM one detent right for the engine graph. PRESETS changes the patch. In FM6, ALGORITHM steps through routing, operators, envelope and feedback. SELECT chooses an operator on operator and envelope pages. Hold SEL while turning SELECT to switch tracks there. K1–K4 on the envelope page set attack, decay, sustain and release. The A/D/R readouts show the underlying smoothing time scales in milliseconds and S shows the sustain percentage. Hold OCT+ while turning K4 for feedback. Released notes may still contribute to the live gain display while their envelope decays. Older FM projects show the fixed legacy routing until a parameter edit enables FM6.

With a Wavetable patch selected, K1 Morph and K2 Scan change the active waveform. The two reference traces show neighboring frames on the modern engine. The lower bars show the first eight harmonics of the displayed wave. Tap FX and turn ALGORITHM to FILTER for the response plot or SLICER for the current pattern, captured buffer and replay position.

Under SETTINGS, use ALGORITHM to reach LEDS. SELECT the ACTIVE LIGHTS row and use K1 to choose HIGH or MEDIUM. This changes illuminated button and active key intensity, while IDLE BUTTONS and NORMAL KEYS control their respective idle states. Existing settings default to HIGH for active lights.

### Engineering MIDI controls and PocketTracker

The consolidated development branch adds MIDI pages under SETTINGS (tap GLO, then turn ALGORITHM). Use SELECT for the row and K1 to edit. **LEGACY** is the migration default and preserves USB/TRS note receive and local-key USB transmit on channels 1–5. **OFF** disables musical MIDI while retaining the USB updater. **CUSTOM** opts into the individual controls.

| Page | Controls |
|---|---|
| MIDI | Mode, input, output, and CC receive |
| MIDI SYNC | Internal/external clock, clock send, transport receive, transport send |
| MIDI ROUTE | First channel for the five consecutive track channels, all-track/melody/drum send |
| MIDI TRACKS | Individual melodic track transmit switches |
| MIDI MONITOR | USB endpoint readiness, clock reception, RX/TX counts, output and clock-step drops |

Track 5 uses the drum kit's existing note map. Enable a transmit track for its keys and sequenced notes. Notes and clock received from the other device are never echoed. CC 7 controls level, 10 pan, 74 track filter low-pass amount, and 71 resonance. CC 120 and 123 release notes from the sender. CC 64 enables sustain when CC receive is on. CUSTOM mode also supports two-semitone pitch bend for melodic oscillator voices. Drum and fixed-length string voices retain their original pitch. MIDI input ports and panel keys have separate note ownership. CC 120 silences that input immediately and CC 123 releases its notes subject to sustain. CC transmit remains unavailable.

For a wired validation, use the Anbernic RG35XX Pro's host/data port with a data cable to the FM-1, which stays a USB MIDI device. Confirm that muOS exposes the FM-1 as an ALSA raw MIDI port before configuring the tracker. PocketTracker's current Linux/PortMaster source uses `libasound.so.2` to enumerate and open raw MIDI endpoints. A missing port points to the USB role, cable, kernel driver or ALSA layer. Little Piggy Tracker is a separate application and its target build needs its own capability check. Wired bidirectional MIDI and clock on the specific RG35XX Pro have yet to be tested physically.

- **Tracker leads:** Select the FM-1 MIDI output in PocketTracker, route an EXTERNAL instrument to the matching channel, and enable clock and transport send. Select CUSTOM, NOTE INPUT on, EXTERNAL and PLAY RECEIVE on in SLAP-5. Run only one clock master.
- **SLAP-5 leads:** Select CUSTOM, NOTE OUTPUT on, INTERNAL, CLOCK SEND and PLAY SEND on. Select the FM-1 input on the tracker and enable clock/transport follow. Turn on the track transmit switches if the tracker should receive notes as well.
- **Transport:** Six MIDI clock pulses advance one sixteenth. Start resets the sequence, Continue resumes, and Stop releases scheduled playback. USB Song Position Pointer is accepted while stopped in external transport mode, in standard sixteenth-note units. A 500 ms clock timeout stops external playback. The monitor reports USB endpoint readiness, which does not establish tracker routing.
- **Recovery:** FM-1 USB descriptors and updater entry are unchanged. TRS remains input-only. BLE MIDI and USB host on the FM-1 are disabled for this build.
