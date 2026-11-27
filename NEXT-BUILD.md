# SLAP-5 next build

## v0.1.54 resource recovery and remaining work

- The native packed image uses 565,172 of 581,564 bytes. The v0.1.53 image used 580,032 bytes. Startup expands the 15,508-byte load records into 30,368 bytes of executable RAM and initialized data. The round-trip and flash-only decoder linkage checks passed. Static RAM remains 88,104 of 98,304 bytes. The pool remains 335,208 of 344,064 bytes. The 64 KiB flash and pool and 16 KiB RAM goals remain open.
- Start the next resource pass from the linked ELF section and symbol report in the verified backup. Measure each large reservation before changing it. Prioritize a sparse or packed in-memory sequencer and undo representation with exact round-trip coverage of the legacy 512-step drum data, fine timing, long melodic notes, held recordings and schema-13 projects. Keep on-disk serialization compatible. Prove simultaneous FX operation and full release tails before overlaying delay, chorus, reverb or rollback storage. Relocate losslessly encoded assets only after verifying resource-region ownership, migration, OTA boundaries and power-loss recovery.
- The native firmware package and host checks provide no new FM-1 audio timing evidence. The reported VHS SAW / DEEP DUB overload remains open. First collect matched on-device callback and DMA traces from the original session, including wet FX, SLICER, recording and visible animation. Compare v0.1.53 and v0.1.54 with identical settings. Use the traces to choose one processing change and require bitwise comparison or defined PCM tolerances where appropriate.
- The four persistent knob themes and the v0.1.53 ROLE generator carry over. Reference-based LCD redesign and expanded generator coverage remain open. Compare the seven original references against real-size RGB565 captures, then qualify all styles and roles on four melodic tracks through generation, editing, save/load and undo. Do not interpret the existing gallery as a new redesign.

---

# SLAP-5 v0.1.50 testing and next build

This testing sprint preserves the existing primary release and the owner's positive v0.1.49 project creation, FX and save report. Physical JUNGLE quantitative qualification remains open.

- HOME uses native-size shadow-free headline glyphs, deeper patch shadows, black lane rules, a dashed beat-flashing playhead, brighter orange notes and an always-visible vertical battery outline. The gray dash indicates an unavailable charge level. Active charging and cutoff are unverified because the existing hardware API does not expose an established charging signal. Next build: verify the FM-1 hardware signal on battery, USB, charging and full before enabling green or a cutoff.
- Live STEPS queues to a measure boundary during playback and applies while stopped. Recorded cells beyond the new length are retained. In-progress recorded-note durations are closed at the boundary so they do not span two recording lengths. Next build: repeat the saved-project case with held notes, mixed independent lengths, external MIDI clock and count-in, and verify MIDI note-offs and undo on device.
- GEN DRUM uses existing finite stored sequencer events and now offers 1/2/4 bars. Existing locks, bounded editing, publication and save/load paths remain. Physical regenerate and live edit should preserve other tracks.
- Dancer uses transport beat phases, three GROOVE rates, bass accents on REACT, bounded SWAY and a LIGHT toggle. The existing 20 FPS rendering cap and fade remain. Next build: record 95/155 BPM comparison clips with and without bass, and capture each control at its endpoints.
- The six original dial references remain unavailable for a verified inspection. This sprint preserves the owner-approved sizing and existing four persistent themes. Next build: recover the reference PNGs and adapt the materials and shapes, then compare all four at native resolution on HOME, MIXER and compact controls. The full reference fidelity audit remains open.
- Main release requires physical JUNGLE at 155 BPM and 60% swing, including record/overdub and FX and visual pages. Require clean playback, active Core 1, zero new misses/XRUNs and peak below 5,804 µs with headroom. Keep an untouched project copy for rollback.

## v0.1.52 follow-up

The FM-1 platform reports 0–3 battery bars on battery power and returns a USB-powered sentinel when configured. Its source documents no charger line. This explains the gray dash on USB. Preserve the orange battery indication and neutral USB state. Verify a charge-state signal and trustworthy USB-powered level readings on hardware before enabling green or reporting a level on USB. Physical LCD and saved-JUNGLE stress acceptance remain open. The dial redesign uses the available concept descriptions and native layout. Verify finish and contrast on the actual display.

# v0.1.53 qualification and next build

Physical saved projects and the latest diagnostic screenshots were unavailable. VHS SAW with SLICER and DEEP DUB with reverb/drive remain the priority physical reproduction. Capture GLOBAL, DISPATCH, CORE 1, SPIKE PROFILE and DMA TRACE at the recurring peak with the full arrangement, individual tracks and controlled FX bypass. Restore the intended settings before acceptance. The sprint does not claim a proven bottleneck or a physical callback improvement.

ROLE GENERATOR stores editable 1/2/4-bar patterns on any melodic track, using track key/scale and an undo snapshot. Press SEQ twice while stopped. K1 role, K2 style, K3 bars, SEL generate or confirm replacement. Live generation was deferred to avoid adding bulk writes at the audio boundary. Next build: add staged revision-checked publishing with a measured callback budget, then test live overdub and save/load. Musical acceptance across all 22 presets is pending.

The seven-image ZIP was identified but not materialized for inspection. Dial rendering changes preserve the shared dimensions; native comparison against the actual seven images and FM-1 visual approval remain open. The green outline denotes USB power only; the dash stays until charge level is known.

Keep the existing primary release and archive until both projects pass. Require clean audio, productive Core 1, zero misses/XRUNs, and peak below 5,804 µs with headroom. Archive extension and current-only primary deployment remain gated.
