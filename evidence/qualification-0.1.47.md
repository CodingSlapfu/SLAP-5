# SLAP-5 v0.1.47 qualification

## Physical evidence and audio policy

The owner reports that the saved 155 BPM JUNGLE recording still lags in v0.1.46. Issue #6 transcribes a 6,981 µs peak against a 5,804 µs deadline, a 1,177 µs overrun. The Core 1 photo shows 213,284 jobs both sent and completed. The spike photo corroborates a 6,124 µs callback. These photos span separate windows. The reported 3,796 µs local voice work points to a local synthesis constraint, while the zero batch-FX field does not measure serial Core 0 FX.

- A native callback-history controller reserves a load window. After repeated callbacks above 90% it raises the existing voice dispatch cost target from 7/16 to 8/16 while a healthy worker is available. Below 80% it decays toward the normal split. The thresholds are initial engineering estimates, not physically calibrated thresholds. Following a late voice job the existing reduced split and scratch ownership guard take precedence.
- During pressure, at most one already-quiet sequenced melodic release per callback can be reclaimed. It must have no pending start or steal tail, very low envelope/velocity product, and a prior sample close to zero. Active notes, held live voices, recorded events and FX tails are retained. This is bounded ATS/ASP work; generalized track suspension, per-engine prediction, musically broader voice admission and safe runtime hard-timeout re-entry remain open.
- GLOBAL dispatch diagnostics show pressure splits and quiet-tail reclaims. The host forced-pressure fixture reached 16 voices, reported 3,865 pressure splits and kept the voice worker online. It exercises the routing path. Native hysteresis thresholds and audible/timing outcomes still need hardware verification.
- Core 0 continues to own live FX; the v0.1.45 stale-load fix and 256-frame architecture remain. The owner's project binary is unavailable. The synthetic heavy Track 1 fixture models the reported wet settings and confirms host worker operation; individual bypass/mute timings on the actual passage remain unverified.

The verified SLOOP 3 recording and SKIFF allocation findings are carried forward in [v0.1.46 qualification](qualification-0.1.46.md). They distinguish storage limits from shared voice budgeting. FM1-MOVE comparison and wider firmware research remain open.

## Controls and 240×240 layout

- HOME and MIXER share full-size rings in a 52-pixel footer. Labels sit below the dial body. HOME keeps all five lanes, stronger shadows, and a larger tempo number. The redundant header BAR line was removed.
- HOME and Digital VU share measure-local BAR and whole-pattern LOOP rendering. The current/total count follows the selected track's position modulo its pattern length. Physical external-clock transitions, overwrites and edge timing still require review.
- MIXER K1 LEVEL, K2 DUCK, K3 ALL and K4 TRK operate directly. Hold SELECT and turn K2 to choose source. ARP toggles SOLO. Short filter labels and signed magnitude values fit the individual dial cells. Drum gain-law refinement remains open.
- SETTINGS rows use the HOME glyph atlas with fitted width. Gravity Rings tilt and rotate faster. Dancer K1 ENERGY, K2 GROOVE, K3 AMP and K4 POSE affect the motion, with per-track accents and sweeping lights. The sampled after-image is sparse, runs for two seconds and sweeps outward over the next half second in a five-second cycle. It is suppressed under pressure and clears when stopped. Physical frame pacing and wider artwork refinements remain open.
- Original knob-reference ZIP rendering remains unavailable in this workspace. Full all-page LCD/typography audit, extreme control captures outside the existing fixture and on-device readability still need verification.

## Software results and release

[Host and native qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45936387078), [browser](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45936387056), [pinned host timing](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45936301892) and [deployed installer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45936534502) passed.

| 155 BPM host case | v0.1.46 P99 / peak µs | v0.1.47 P99 / peak µs |
| --- | ---: | ---: |
| MIDI receive | 119.86 / 253.72 | 115.30 / 191.22 |
| MIDI transmit | 120.78 / 143.93 | 123.07 / 144.98 |
| Receive and transmit | 116.86 / 158.40 | 114.71 / 141.09 |

Values are same-run wall-time medians. Median host deadline misses were zero. The ordinary host benchmark does not activate the native pressure thresholds.

- Application 576,772/581,564 bytes; static RAM 87,992/98,304 bytes; pool 335,568/344,064 bytes.
- [Native-size HOME before and after](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47-HOME-before-after.png) and [paired dial styles and host framebuffers](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47-framebuffer.zip).
- Application `64b528982d45b01ab4da2fd094d5352b71798ac8`; firmware `FM-1_75000147`. [Verified installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html).
- [Firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47.fwsc) SHA-256 `e9b284de383d387d5a5116d1210fde4a265d19211838fd6194824003ff226235`. [Engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47-engineering-backup.tar.gz) SHA-256 `453753205fac500311d5a4e149528ce438225328163a67f9385cea69dff12044`. [Prior rollback](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.46-hardware-testing).

## Acceptance matrix

| Area | Status |
| --- | --- |
| Source continuity, exact native package, rollback and backup | Software qualified |
| Physical peak lag, Timer 5 and DMA headroom | Awaiting exact-project FM-1 A/B |
| Adaptive DSP Budget, ATS, ASP | Partial. Callback feedback, bounded balancing and quiet releases added; wider measured admission and sleep remain open |
| Core 1 hard-timeout runtime recovery | Existing safe behavior preserved; controlled re-entry remains open |
| HOME, MIXER, SETTINGS and shared progress | Software updated; physical readability, exhaustive loop cases and LCD audit remain open |
| Dancer and Gravity Rings | Software updated; physical motion, trails and scheduling acceptance remain open |
| Patch refinement, EXPRESSION, Sound Lab, sequencing, recording, drums, LIVE FX and SLICER | Existing features and host regression coverage retained; remaining issue #5 refinements open |
| MIDI, USB and external-sync devices | Host and browser checks apply; simultaneous and external hardware acceptance remains open |
| Studio, web installer and verified assets | Software qualification only |

## Owner retest

Open a copy of the original JUNGLE project, reset GLOBAL counters and run matching windows after load, after stop/restart and after mute/restore. Use 155 BPM and 60% swing. Compare Track 1 live, muted and with each effect bypassed individually. Capture GLOBAL timing, CORE 1 status, SPIKE PROFILE, DMA TRACE and DISPATCH, including pressure splits and quiet-tail reclaims. Preserve 16-voice capacity and require clean audio, no new misses or XRUNs, and below 5,804 µs peak with headroom. Check the 16/32/64-step BAR counts, separate LOOP wrap, filter endpoints and footer labels on the actual display.
