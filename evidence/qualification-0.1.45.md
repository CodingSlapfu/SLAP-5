# SLAP-5 v0.1.45 experimental qualification

## Owner evidence and load boundary

The reported 155 BPM, 60% swing, five-track JUNGLE project showed overload with 16 voices while Core 1 remained ONLINE and continued to complete jobs. The report gives a 6,113 µs peak against 5,804 µs and 514 misses in one window. The owner then muted and restored the four melodic tracks, after which the lag cleared during observation. The zero-XRUN window after recovery is separate from the earlier cumulative window.

Muting suppresses new sequenced note work on the excluded tracks and lets existing envelopes, effect tails and mixer estimates settle. That changes the active voice population and work per callback without restarting Core 1. The effect of each change in the original recording has not been isolated. The original project data and equivalent on-device A/B timing were unavailable.

Previously, a project load published the normal transport epoch, which left old sequencer voices in release under the next project's configuration. The new project epoch clears old sequencer and audition voices at the audio callback boundary while retaining held manual voices. The callback resets mixer smoothing, envelope activity and duck history. Ordinary stop and restart preserve their existing release behavior. This addresses stale project-bound state and is not evidence that the physical latency is fixed. Core 0 continues to own live FX; Core 1 still handles eligible voices. Workload, FX-tail and starting-state differences in the owner's project remain open.

## UI changes

- HOME and MIXER use the same expanded 42-pixel nominal ring and shared renderer for all four persistent knob styles. HOME has five 32-pixel lanes, independent progress strips, eight-character names and its existing same-hue pulse.
- The BAR strip resets each measure and displays the selected loop's current/total measure. LOOP wraps at the selected track length. Both use the transport position and existing fractional step clock.
- DISPLAY offers four persistent background-only palettes: Titanium, Crimson, Fuchsia and Burgundy. The existing settings record had two spare bits, which preserves rollback-compatible record width. The renderer changes shared panel surfaces, HOME framing and relevant SETTINGS and LIVE FX surfaces. Intentional black art and shadow remain. The tests verify four distinct RGB surfaces and unchanged five lane borders. Visual contrast on the physical LCD remains for owner review.
- Shared utility Barlow sizes increased from 10/12 to 11/13 px. Display headings and lane font were preserved. A page-by-page clipping and distance audit remains open.

## Hosted software results and release

- [Native package, four theme framebuffers, dense host recording and rollback](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45902866551), [browser suite](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45902866456), [pinned host comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45902669609) and [deployed installer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45903088951).
- Same-run median mixed MIDI host wall P95/P99/peak in µs was 168.24/181.42/248.58 → 168.57/180.84/252.93. Median host budget misses stayed zero. These results cannot quantify physical headroom.
- Versioned application 573,944/581,564 bytes. Static RAM 87,896/98,304 and pool 335,568/344,064 bytes.
- [HOME playing comparison](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45-HOME-before-after.png) and [themes and paired HOME/MIXER styles](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45-framebuffer.zip). The released HOME frame shows lower-edge label clipping. A follow-up source change uses the smaller utility face for expanded labels. The released bytes are unchanged.
- Application source `e6c3f7498d64c0763ceaab10bb1f9fa5d4e844eb`, identity `FM-1_75000145`. [Verified live installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html).
- [Firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45.fwsc) SHA-256 `a72db8e95d7caf0f8a04f487f5376ef92a934ec39605a644095292e3ce5f21e8`. [Engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45-engineering-backup.tar.gz) SHA-256 `84c3793113aef1080d5582bf1fa223a84fe5332d04979fdeaaaef864973b4dc9`.

## Acceptance matrix

| Master assignment | Status | Evidence or remaining work |
| --- | --- | --- |
| 1–2 Continuity and physical incident | Completed and software verified | Existing branch and rollbacks preserved. Owner observations recorded separately from simulated evidence. |
| 3 Fresh load vs mute/unmute | Partially completed | Published distinct load reset and retained regression fixture. Exact-project A/B, repeat load, mute/recover and settled-state physical timing remain open. |
| 4 FX-LATE repair | Completed and software verified | Native serial FX policy and host fault/PCM tests retained. |
| 5 Runtime hard-timeout re-entry | Partially completed | Existing guarded boot retry intact. Runtime re-entry remains unqualified. |
| 6–8 Track sleep, shared polyphony and DSP headroom | Partially completed | Stale voice/runtime reset, 16 voices preserved in host fixtures. Deeper ATS, ASP and DSP headroom work remain open. |
| 9–10 HOME and secondary type | Partially completed | HOME redesign and 11/13 px utility glyphs. Physical readability and all-page clipping audit remain open. |
| 11 Background palettes | Partially completed | Four persistent surfaces with invariant lane colors in framebuffer tests. Additional palettes and physical color review remain open. |
| 12–13 Progress, names and selection | Partially completed | Corrected loop-relative measure count and kept existing names/pulse. Physical external clock and recorded-loop progression still need verification. |
| 14 HOME knob parity | Partially completed | Same expanded renderer and paired native-size captures for all four styles. Released footer labels show lower-edge clipping. |
| 15 Firmware-wide knob overhaul | Partially completed | Shared HOME/MIXER size. Compact controls elsewhere remain. |
| 16–17 Expression, patches and Sound Lab | Partially completed | Existing features retained and host suite exercised. Further musical refinement remains open. |
| 18–20 Sequencer, keys/chords and drums | Partially completed | Synthetic JUNGLE and existing sequencer tests retained. Further musical and workflow refinements remain open. |
| 21–22 Live FX, SLICER and visualizers | Partially completed | Existing paths retained in host coverage. Feature-level qualification remains open. |
| 23 MIDI and USB | Awaiting owner hardware verification | Host MIDI tests pass. Simultaneous USB audio/MIDI and external gear tests remain open. |
| 24 Output profiles and LEDs | Awaiting owner hardware verification | Existing controls retained. Device-side acceptance remains open. |
| 25 Prior firmware research | Partially completed | Remaining comparative investigations were deferred in this build. |
| 26–27 Resources, rendering and diagnostics | Partially completed | Bounds and framebuffer checks applied. Timer/DMA contention and deeper instrumentation remain open. |
| 28 Software regression | Completed and software verified | Hosted suite, PCM tests, native package and rollback qualification passed. |
| 29 Owner stress workload | Awaiting owner hardware verification | Synthetic host JUNGLE coverage does not qualify the saved instrument project. |
| 30 Studio and website | Completed and software verified | Browser tests and byte-for-byte Pages verification passed. Further Studio features remain open. |
| 31–32 Backups and reporting | Completed and software verified | Versioned assets, checksums, rollback and this record are linked below. |

## Physical retest

Back up the saved project and keep the v0.1.44 rollback. In v0.1.45, use the same fixture, immediately play and overdub after loading, and capture GLOBAL CORE 1, SPIKE PROFILE and DMA TRACE before any workaround. Run the same passage after muting and restoring the four melodic tracks, then repeat after reloading and after stop/restart alone. Use matching measurement windows and note when K3 reset is used. Verify zero misses and XRUNs, no audible lag, 16-voice operation and continued Core 1 progress. Target a callback below 5,804 µs with headroom. Also verify BAR 1/1, 1/2 and 1/4 through rollover and check the four surfaces for contrast at playing distance.
