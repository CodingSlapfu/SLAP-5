# SLAP-5 v0.1.46 qualification

## Audio

The owner reports peak-area lag with the five-track 155 BPM, 60% swing JUNGLE project. Track 1 muting suggests it contributes with reverb 50, delay 8, chorus 8, drive 70 and filters. The source includes a targeted FX arithmetic reduction, but attribution of the physical peak remains open. The original project bytes were unavailable for A/B profiling.

- The mixer builds the existing awake-track list once per voice span and processes it in original track order. This removes per-sample inactive-track branches while maintaining the existing sleep and tail semantics.
- FX caches chorus depth and drive fold scale with other callback coefficients. Chorus and room reads use power-of-two ring masks. The echo ring and processing order are preserved. Core 0 continues to own mutable live FX. Voice dispatch, 16 voice slots and 256-frame callback architecture remain.
- The drum level path is linear from the saved byte and has no explicit 15% gate. A linear 15% setting is approximately −16.5 dB before kit/sample and output gains. Kit-specific audibility and gain-law refinement remain open. Saved levels were preserved.

## Comparative research

[SLOOP 3 sequencer source](https://github.com/isod89/sloop3-fm1/blob/c49c4b94cb1934c33ade4a68c4936be85817d72d/seq/seq.h) is GPL-3.0-only. At the inspected revision it has 128 events per pattern and 10 concurrent recorder held entries. [Insertion source](https://github.com/isod89/sloop3-fm1/blob/c49c4b94cb1934c33ade4a68c4936be85817d72d/seq/seq.c) rejects a new event at capacity, and overdubbing the same event replaces the existing one. These limits differ from the separately described playback voice and release capacity. The installed firmware version and the actual cause of the owner's observed limit were not established. [SKIFF's GPL-3.0 voice allocator](https://github.com/illuminus5/skiff/blob/v2.7.2/firmware/src/voice.c) uses a shared allocation budget with released voice priority and fading victims. No material from these sources was copied. Independent predictive overload policy and FM1-MOVE comparison remain open.

## Interface and controls

- Black CHROMA replaces the selectable surfaces while retaining the settings record format and fixed track colors. Small Barlow slots grow to 12/14 pixels. SETTINGS has neutral rows, a focused edge marker and a compact control hint.
- HOME and MIXER retain the same expanded 42-pixel ring. Dials gain finer contrast, brushed accents and a digital accent layer. The six original reference PNGs could not be opened because the workspace shell launcher could not start; these changes follow the written issue directions.
- In MIXER, K1 toggles SOLO and K2 edits the selected melodic track's DUCK amount. Hold SELECT for K1 LEVEL and K2 SOURCE. The source is session-local and defaults to drums. K3 and K4 retain the global and selected-track filters in both banks. White keys mute, ARP toggles solo.
- HOME playhead brightens on the beat and stays visible between beats. BAR and LOOP retain selected-pattern modulo wrapping. Gravity Rings have stronger tilt; Solar System motion runs at about double the former speed with moving reactive stars. Digital VU displays selected-loop progress. Dancer has reactive sway and sweeping lights with its existing settings. Four new movement mappings and timed after-image/burst are open.

## Software verification

- [Native and host suite](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45931793048), [browser tests](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45931793047), [pinned timing](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45931793368) and [versioned release, synthetic heavy Track 1 and served installer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45932228132) passed.
- Synthetic Track 1 applied the owner's reported wet values and a filter, then verified the host worker stayed online, zero voice fallbacks and a 16-voice peak. This is an approximation of the saved hardware project.
- Host wall median mixed MIDI P95/P99/peak (µs) was 168.48/182.93/255.74 → 168.31/182.08/254.64. The transmit-only peak rose from 260.67 to 292.58 µs while P99 fell from 243.16 to 240.72 µs. Median host budget overruns were zero; neither result qualifies FM-1 timing.
- Application 576,180/581,564 bytes; static RAM 87,944/98,304; pool 335,568/344,064.
- [HOME before and after](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46-HOME-before-after.png) and [paired HOME/MIXER styles and host framebuffers](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46-framebuffer.zip).
- Source `e5e7f371a13f74dcee242a806962df61ffed7cea`; identity `FM-1_75000146`. [Experimental installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html).
- [Firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46.fwsc) SHA-256 `2c295b18f69a93e8064cfd55fccd300445b54d8131dfdd433177c9920ff993a5`. [Verified engineering archive](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46-engineering-backup.tar.gz) SHA-256 `f0d781d820ee24b715cf2cfe4fa12d7f1dc29d5623e5a78f5fcea049945f68be`.

## Acceptance matrix

| Area | Status |
| --- | --- |
| Project continuity, published baseline digest and rollback | Software qualified |
| Peak lag, 15% headroom, zero DMA XRUNs | Awaiting physical FM-1 A/B |
| ATS and shared DSP budget | Partial. Inactive mixer scan optimized; predictive admission and release-tail redesign open |
| Core 1 runtime recovery | Existing safety preserved. Hard-timeout runtime re-entry open |
| MIXER controls, Black CHROMA and shared dial rendering | Software qualified. Drum audibility and all-page/physical LCD review open |
| 240×240 utilization and readability | Partial. HOME/MIXER evidence retained; all-page framing and larger-label audit open |
| Progress and visualizers | Partial. Core changes present; external-clock/loop-edge visual audit and full Dancer redesign open |
| EXPRESSION, Sound Lab, browser, sequencing, drums, live FX and SLICER | Existing host coverage and capabilities retained; outstanding feature refinements open |
| Wired MIDI and USB, external devices, output modes, LEDs | Host and browser coverage applies; native external-device acceptance open |
| Studio, installer and backups | Software qualified for this release if deployment checks pass |

## Physical retest

Use a copy of the same saved project. From a fresh load run an equal recording window at 155 BPM and 60% swing with Track 1's original effects. Repeat after muting only Track 1, then with individual FX bypassed for diagnosis, and after mute/restore and stop/restart. Reset stats between comparable windows. Capture GLOBAL, CORE 1, SPIKE PROFILE and DMA TRACE before changing state. Require continuing worker progress, 16-voice playback, zero new misses or XRUNs, clean audio and a peak below 5,804 µs with headroom. Compare drum kits below 15% and test the five track colors, larger labels and shifted MIXER controls on the LCD.
