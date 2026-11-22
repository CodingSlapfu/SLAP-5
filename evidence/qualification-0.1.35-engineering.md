# SLAP-5 v0.1.35 engineering acceptance

**Release remains on hold.** Source [2b8330d](https://github.com/CodingSlapfu/SLAP-5/commit/2b8330d8a559ba5800fc7d07d0e40847a7c1a179). Firmware identity `FM-1_75000135`. Package SHA-256 `82283e8fc42fc3856ccd4f52303662d2d7ed9c45d9873f722f90d97f10cb803e`. The owner-facing release remains v0.1.31.

The [consolidated matrix](consolidated-acceptance.md), [FM/LED report](qualification-0.1.33-engineering.md) and [wired MIDI matrix](qualification-0.1.34-engineering.md) retain the original requirements, prior verification and archival evidence. The following dispositions supersede the corresponding open rows where explicitly verified.

| Requirement | Status | Evidence and limitations |
|---|---|---|
| MIDI performance control and input ownership | Software complete for covered paths | `midi-performance.txt` exercises simultaneous USB/TRS ownership, percussion plus all four melodic channels, Note On/Off, zero-velocity Note On, CC64 pedal hold/release, CC120 immediate sound off, CC123 under sustain, bend and cable loss. Two-semitone bend applies to received oscillator notes. Drums and fixed-length string engines retain their original pitch. Recording still uses the existing track/pitch event format, with no stored independent identities for coincident input sources. Complete overlapping-source recording acceptance remains open. |
| MIDI output ownership and recovery | Software complete for tested cases | The last owner of an outgoing pitch sends its Note Off. The fixture logs sequence/panel overlap across Stop, two-way clock, SPP, dropped clock, room-zero backpressure and reconnect. Full 24-slot saturation and every track mute/solo/tie permutation remain open software. |
| Clock and MIDI monitor | Open software | Up to four external step boundaries can be queued and drained across callbacks. Monitor reports tempo when receiving external clock, USB readiness, RX/TX, drops and last accepted receive channel. Long-duration jitter, recording count-in, source switches and restart stress matrix remains open. Clock is callback-quantized. |
| MIDI CC and PocketTracker | Requires physical validation for tested wired behavior | Legacy behavior preserved. Existing CC level, pan, LP amount and resonance mappings remain, with CC64 and pitch bend added. CC transmit is unimplemented. Exact muOS USB host, ALSA raw MIDI and PocketTracker port enumeration remain device dependencies, along with bidirectional note/clock/interruption verification. |
| FM6 and Wavetable endpoints | Software complete for covered controls | Exact build passes existing six-op ratio, level, ADSR, feedback and Lamp Wave morph/scan/decay/drive tests. The all-zero operator config remains distinct from the factory default. Saved legacy-project graph validation and waveform/engine semantic matrix remain open. |
| Other synthesis graphs and factory patches | Open software | All 136 patches pass both synthesis modes and distinct-signature checks. Prior 408-frame audit preserved. Full state and render correspondence across 25 engines, drum screen extremes, and expressive transition tuning remain open. |
| FILTER and SLICER | Open software | Prior FILTER gain comparisons remain applicable. Expanded effects test checks separate track cursors, subdivisions after tempo change, restart capture reset, and disabled state alongside replay and boundary checks. UI transport entry/exit and full marker coverage remain open. |
| Typography, HOME and visualizers | Open software | New host battery fixture verifies full/empty/delayed low/USB/unknown and captures low/USB screens. Existing name-width checks, knob endpoints and HOME/MIXER fixtures pass. Whole-firmware pixel-fit and visualizer animation audit remain open. Current Reactor and archived reference captures retained. |
| LEDs, configuration and output profile | Open software | HIGH/MEDIUM persistence fixture passes. MIDI CUSTOM and speaker profile restored across reboot. Host selects both output DSP modes and renders audio. Comprehensive LED indication and controlled output-response matrix remain open. Physical brightness and acoustics need hardware. |
| Recording, compatibility, PCM | Software complete for tested paths | Existing recording/overdub/project/settings tests, worker/single/fallback PCM suite, and dense EDIT transition tests passed across the checkpoint sequence. Exact package keeps the on-disk compatibility and USB descriptors. |
| Resource and timing | Open software | App 565,016 / 581,564 B (+1,132 from v0.1.34), static RAM 88,408 / 98,304 B (-112 from removed input bitmap), pool 335,568 / 344,064 B, headroom 8,496 B. See timing below. Native Core 1, DMA/LCD, temperature and USB jitter need hardware. |
| Studio, documentation and updater | Software complete for tested paths | Native build, 36 host fixtures, standalone effects/synthesis and Studio WASM pass. Browser OTA, simulated cable recovery and interrupted v0.1.31 update/rollback pass. App-only bounds are `[0x4000, 0x93000)`. Protected settings, songs and boot areas retained. README describes verified MIDI controls. Final website/manual and any new installer await qualification. |

## Host timing

Two alternating inline-worker CPU trials per tempo compared v0.1.34 to the MIDI input/monitor revision immediately before the final output-ownership edit. MIDI TX/clock/transport and visuals were enabled and the fixture injected a TRS note pair. Figures are medians in µs of the last 1600 recorded callbacks per run. This window may exclude the injected notes and does not qualify sustained simultaneous input. The final output-ownership build has functional regression evidence but the comparison must be repeated on that exact build before drawing capacity conclusions.

| BPM | Baseline P95 / P99 / peak | Candidate P95 / P99 / peak |
|---|---|---|
| 112 | 170 / 214 / 439 | 182 / 215 / 1001 |
| 137 | 162 / 211 / 1210 | 169 / 224 / 1274 |
| 160 | 172 / 202 / 473 | 175 / 213 / 458 |

No observed callback exceeded 5,805 µs. Peak variation and slightly higher P99 remain investigation items. This is no evidence of native Core 1 timing.

## Physical acceptance after qualification

On the owner's exact muOS/PocketTracker build, confirm ALSA raw MIDI enumeration with the Anbernic as host and FM-1 as device. Exercise both clock directions, all five channels, polyphony, sustained and bent notes, overlapped keyboard/sequencer notes, stop and cable removal/reconnect. Run dense five-track playback and overdub at 112/137/160 BPM, measuring XRUN and callback tails while opening the graphs and visualizers. Verify battery/USB, LEDs, output profiles and physical app-only update/rollback on compatible hardware.

[Hosted Chromium workflow](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45014194452). [Verified engineering archive](https://chatgpt.com/library?file_id=libfile_f071757c80e48191a6eb64bc624fb828&account_id=personal).
