# SLAP-5 0.1.27 candidate qualification and resumption

The engineering candidate is backed up with matching Studio and emulator in `engineering/SLAP-5-0.1.27-candidate.tar.xz`. It is not qualified for publication. Keep 0.1.18 as the primary install, and 0.1.25 and 0.1.26 as existing rollback and experimental options. Source checkpoint before final display index guard and expanded category fixture was `48cca92460e8d38b283b1a63671d80ad18c8ff17`. Hosted Chromium workflow 44948957243 passed at that checkpoint. Local Chromium could not launch because the container denies process-singleton sockets. Recheck the hosted run at the final candidate commit.

## EDIT distortion

The physical symptom remains open. The source audit found continuous drum visualization redraw on entering the old PATCH page. The candidate makes EDIT Page 1 a static browser with track, group, patch, tempo and page invalidation, and consumes unrelated encoder input on entry. It removes a measurable source of work and preserves the gain path, limiter, existing audio buffer configuration and dual-core voice scheduling.

The deterministic fixture `device/tests/edit-entry.py` compares recorded PCM against a HOME control for each firmware. It exercises four repeated EDIT and HOME transitions at 112, 137 and 160 BPM with DRUMS, Track 1 and Track 2 selected. All nine candidate and nine baseline comparisons passed at a one-LSB tolerance. Since the baseline also passed, host PCM does not reproduce or identify the hardware cause.

| BPM | DRUMS fixture blits, 0.1.26 | DRUMS fixture blits, 0.1.27 |
|---:|---:|---:|
| 112 | 16,731 | 14,989 |
| 137 | 16,898 | 15,216 |
| 160 | 17,067 | 15,419 |

Counts cover the full host fixture. They do not measure panel scan tearing, DMA wall time or audio latency.

## Desktop callback measurements

Five alternating trials at each tempo and mode, patch-switch fixture and steady playback, baseline 0.1.26 against candidate. Times are host thread CPU time in microseconds. The prior 0.1.25 to 0.1.26 comparison is in [0.1.26 qualification](qualification-0.1.26.md). Hardware contention and shared-host noise limit interpretation. Worker P99 fell at all three tempos. Single-core P99 rose at all three. Peak changes were inconsistent. These results do not establish sustained physical performance acceptance.

| BPM | Mode | 0.1.26 P95 / P99 / peak | 0.1.27 P95 / P99 / peak |
|---:|:---|---:|---:|
| 112 | Worker | 157.71 / 217.39 / 342.41 | 144.18 / 194.93 / 510.99 |
| 112 | Single | 145.28 / 191.56 / 680.37 | 152.77 / 241.95 / 2047.56 |
| 137 | Worker | 157.71 / 207.97 / 833.36 | 155.16 / 199.43 / 443.79 |
| 137 | Single | 154.96 / 204.84 / 389.50 | 157.27 / 213.16 / 437.72 |
| 160 | Worker | 163.17 / 236.18 / 609.61 | 159.00 / 227.35 / 1268.06 |
| 160 | Single | 157.19 / 200.33 / 519.44 | 157.94 / 202.82 / 572.44 |

## Acceptance checklist

| Requirement | Status | Evidence and remaining work |
|---|---|---|
| EDIT browser and patch IDs | Verified complete in host | EDIT opens browser. ALGORITHM pages, SELECT track, K1 group, K2 patch, PRESETS direct. All 136 program IDs and 81 legacy plus factory kit IDs retained. `patch-browser.txt` checks numerical boundaries and both directions across groups. Physical one-detent accuracy and ghost inputs need FM-1. |
| DRUMS → EDIT audio | Physically unverified | Host sample comparison passes both builds. On-device callback, LCD transfer and analog versus USB audio attribution remain open. |
| FM6 | Partially implemented | Routing, operators, envelopes and feedback pages in `app.c`; live operator levels and actual routing. Refine route curves, per-path animation and advanced controls. Validate native LCD readability. |
| Wavetable and other synth graphs | Partially implemented | Page 2 waveform follows tables, band and scan, with K1 morph and K2 movement. Other engines show a shared signal trace and drums show kit voices. Engine-specific editors and harmonic analysis remain open. |
| HOME | Partially implemented | Five colored tracks, patch names, step/hold rendering, peak markers and row invalidation. Host screenshots and fixtures. FM-1 transfer timing, tearing, sustained playback and input latency are open. |
| Settings | Partially implemented | ALGO section, SELECT item, K1 value. Audio, Metronome, Recording, Transport, Keyboard, Display, System and LEDs. Host persistence and recovery tests pass. Full settings audit and native control checks remain open. |
| Three LED tiers | Physically unverified | Firmware passes low glow, medium idle keys and high active controls to the existing matrix driver in `vendor/fm1-base/device/plat_fm1.c`. Idle button choices are soft, off or brighter. Idle keyboard medium or off. High remains fixed and requires independent brightness control to meet the full request. Confirm key scanning, button visibility and sleep. |
| Step, hold and note audition | Verified complete in host | `step-audition.txt`, `step-editing.txt`, `held-recording.txt`, `wrapped-hold.txt`, `poly.txt`; graphical and hardware ergonomics still require a device. |
| Digital VU and visualizers | Partially implemented | Existing VU STEP and staircase display retained. Higher-detail Dancer and complete Solar System, DNA, Ferrofluid and CRT review remain open. |
| Drum kits and patterns | Verified complete in host | `gen-drums.txt`, `drum-lanes.txt`, emulator and Studio cover 25 factory kits, 56 legacy kits, 16 lanes, 400 factory assignments, 16 styles and 48-step generation during playback. Musical listening remains physical. |
| Musical features and Live FX | Partially implemented | Existing Super Saw, FM6, wavetable, 16-voice path, record, transport, MIDI, mixer and fixed Live FX retained. Host fixtures pass for recording, overdub, undo, FX and sequencing. Musical and control-latency acceptance remains open. |
| Other graphical editors and typography | Partially implemented | Existing ENV and LFO direct entry retained and covered by `sound-lab.txt`. HOME label-width test covers all 217 names. Full ENV, LFO, filter, mixer, FX, slicer, drum, step editor and visualizer audit remains open. |
| Studio and emulator | Verified for tested software paths | WASM and host fixtures passed for projects, 136 programs, 81 kits, overdub, 32-bar takes, pattern generation during playback, scenes, automation and transport. Local Chromium platform error requires confirmation by hosted workflow for the final commit. |
| Installer and rollback simulation | Verified in software | Native package audit and browser updater protocol/staging for 0.1.27 and stable 0.1.18. Simulated interrupted install and rollback through 0.1.18, 0.1.22, 0.1.25 and 0.1.26. Boot head and project flash preserved; damaged and foreign packages refused. Flash interval `[0x4000, 0x93000)`. |
| Physical timing and publication | Missing | No FM-1 device capture in this session. Current candidate needs completion and qualification before installer deployment. |

[Native-resolution host captures](graphics/panels-0127.png) show HOME, browser, LED settings, FM6 routing and envelope, and wavetable. They are snapshots from software, not panel recordings.

| Native resource | 0.1.26 | 0.1.27 | Limit |
|---|---:|---:|---:|
| Application bytes | 555,600 | 557,420 | 581,564 |
| Static RAM bytes | 87,288 | 87,416 | 98,304 |
| Pool RAM bytes | 328,648 | 328,648 | 344,064 |

Native package identity `FM-1_75000127`, 611,488 bytes. Archive includes the audited SHA-256, firmware, manifest, and matching Studio/emulator assets.

## Next physical capture

1. Preserve the song and note the installed firmware identity. Establish the unchanged v0.1.26 reproduction with the same dense project at 112, 137 and 160 BPM. Record USB and line/speaker output if available.
2. With Track 5 selected, record five seconds before EDIT, ten seconds on Page 1, and five after HOME. Repeat four times, then repeat on Tracks 1 and 2 and press ALGORITHM to change graph pages. Keep patch and volume settings identical.
3. Hold GLO for five seconds and record diagnostics before and after the sequence. Capture callback P95/P99/peak, missed deadlines, XRUNs, worker wait, late and fallback counters, C1 stack use, DMA trace and per-core spans. Correlate glitch times with the page transition. Direct LCD timing and XIP-stall attribution still need board instrumentation.
4. Film HOME and graphs at the same settings. Compare tearing and control response while entering EDIT, changing categories, reaching every first and last patch, and rapidly switching tracks. Check note highlights and silent-track meters.
5. Check LOW idle function buttons, MEDIUM idle keys and HIGH held notes and transport in darkness. Change LED settings, reboot, repeat and test rapid key input with audio running. Compare physical timing and sound against 0.1.25 and 0.1.26 before accepting a new release.
