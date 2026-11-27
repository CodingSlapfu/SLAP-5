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

## Published v0.1.52 hardware testing

[Direct installer](https://codingslapfu.github.io/SLAP-5/flash-testing-0.1.52.html) · [Matching testing site and Studio](https://codingslapfu.github.io/SLAP-5/testing-0.1.52/) · [Backup and native-scale captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46038176342/artifacts/15217813159). The browser accepts compatible installed v0.1.31 and v0.1.37–v0.1.51 without intermediate flashes. Native and host regressions, v0.1.51 interrupted-update and rollback simulation, Studio staging, and exact Pages file verification passed. Compatibility for every earlier allowed identity has not been physically tested.

Application `e338841ea5cb31099881f286f421f3f0ef12e125`, identity `FM-1_75000152`, firmware SHA-256 `ffddc8b99114caf39026269149b1d8f025389435d9e46c73f1f84d6db8ed2389`, backup SHA-256 `c3c71a81415296e6a9c0810ab9a1e9b07a23b5c15bda2c23f98b88ad33e526f9`. Pages revision `65a80c503e21358d34d4f4e27e5d936b6679de67`. Primary public installer remained in service. Actual LCD review and physical JUNGLE gate remain pending.
