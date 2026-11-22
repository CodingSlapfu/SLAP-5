# SLAP-5 v0.1.28 owner acceptance

Engineering candidate only. The candidate has not passed the full release gate. Keep the public installer and rollback choices in service. The native package, web build and source snapshots in the engineering backup match [source 8ef9725](https://github.com/CodingSlapfu/SLAP-5/commit/8ef9725f6925ff7b4efa88aa1189d64f644aaa64), tree `4f816a80595961824b051655a2a193465e5b201f`. The previous candidate and its performance comparisons remain documented in [0.1.27 qualification](qualification-0.1.27.md).

| # | Owner requirement | Status | Evidence and remaining work |
| --- | --- | --- | --- |
| 1 | HOME controls | COMPLETE | Host checks level endpoints and one-detent increments, pan, 16/32/64 steps and global swing up to 70. Length edits change the loop boundary while retaining stored events. Physical encoders remain to be checked. |
| 2 | MIXER controls | COMPLETE | Host checks K1 solo, K2 duck, K3 all filter and K4 track filter. The drums supply the existing ducking trigger, with SOURCE shown on Track 5. Mute and ARP solo are still available. |
| 3 | Independent bipolar filters | PARTIAL | Dense host recordings show the expected attenuation for global LP, global HP and track-only and combined processing. Physical sweeps and worst-case control/audio timing remain open. |
| 4 | Persistence and old projects | COMPLETE | Slot round-trip checked disparate track values and a separate all-filter value. V2–V11 remain readable and initialize neutral. V12 is written only with active filters and needs this firmware or later. Studio JSON accepts optional filter fields with neutral defaults. |
| 5 | Firmware-wide knobs | PARTIAL | Shared hardware-style dials are used on HOME, MIXER, Sound Lab, wavetable, ENV, LFO and drum generation. The remaining parameter pages still need the same treatment and a full extreme-value fit audit. |
| 6 | Five-track scrolling HOME | PARTIAL | A fixed playhead, scrolling grid, per-track lengths and held tails are implemented. Native-LCD tearing and longer legacy-loop transfer budgets remain open. |
| 7 | BAR 1–4 and battery | PARTIAL | The header uses global transport position and the existing measured battery/USB input. The native panel and real USB/battery transitions remain unverified. |
| 8 | Selected track pulse | COMPLETE | A beat-driven highlight and steady selected-track stripe are captured at 240×240. |
| 9 | Patch name readability | PARTIAL | Refined cap outlines, smaller track numerals and five simultaneous rows. A full longest-name audit on the physical display remains open. |
| 10 | SLAP Block type | PARTIAL | Original vector caps and numerals replace the pixel-grid glyphs. Bold and Regular use distinct strokes. Device glyphs are generated at build time and the web preview uses the same family. Device fit and antialiasing audit remains open. |
| 11 | Saturated RGB565 palette | PARTIAL | Five saturated track colors are shared with Studio. Native panel color and readability checks remain open. |
| 12 | FM6 operator graph | PARTIAL | Curved modulation connections, envelope-driven path activity and operator selection are implemented. Full operator telemetry, graph routing audit and physical LCD checks remain open. |
| 13 | Wavetable graph | PARTIAL | Two-strip waveform shows the actual scan phase and band. It holds steady without a sounding voice. Morph, Scan, Decay and Drive are available. Harmonic-detail and sensitivity checks remain open. |
| 14 | Other engines | PARTIAL | Signal traces are paired with amplitude envelopes, track filter curves or live voice modulation by engine family. Individual engines need a complete timbre-specific visual audit. |
| 15 | Sound Lab controls | COMPLETE | EDIT enters Sound Lab with four parameter controls and ALGO accesses a separate browser. Host verifies an actual parameter change for all four knobs and patch browsing. |
| 16 | FILTER FX graph | PARTIAL | The existing response function drives the dedicated curve and visibly follows cutoff and resonance. Full DSP/curve and control-latency calibration remains open. |
| 17 | Live SLICER graph | PARTIAL | The cursor follows the effect subdivisions, and live state identifies capture, replay and gate. Audible boundary and waveform correspondence remain to be verified. |
| 18 | Listening profiles | PARTIAL | Both labels are mapped to the persistent shared-output speaker processing switch. Native acoustic response and output measurements remain open. |
| 19 | Digital VU | PARTIAL | Five larger LED scales retain peaks and STEP 01–16. Host verifies master-independent activity. Ballistics and physical display verification remain open. |
| 20 | DNA Helix | PARTIAL | Bass changes separation and twist, pad activity affects depth and note attacks affect highlights. The captured frames change over time. Independent track acceptance remains open. |
| 21 | Gravity Rings | PARTIAL | Per-track attacks, bass-dependent depth, ring energy and transport phase change the motion. Physical smoothness remains open. |
| 22 | Solar System | PARTIAL | Track-specific speed, size, lighting, trail and accent responses are implemented. A full physical solo-all-five acceptance pass remains open. |
| 23 | Dancer | PARTIAL | The authorized PARALLAX-8 Dancer 036 artwork has 160 source scanlines and 28 distinct images across 30 indexed poses. A compact span encoding preserves original pixels and the existing pose cycle, controls and black background. Physical rendering cost and visual acceptance remain open. |
| 24 | Audio/graphics performance | PARTIAL | Native resource audit and host timing available. Physical EDIT distortion, XRUNs, LCD transfer/tearing and callback/worker tails under simultaneous filtering remain open. |
| 25 | Earlier owner items | PARTIAL | See the [active issues](https://github.com/CodingSlapfu/SLAP-5/issues) and linked 0.1.27 evidence for remaining LED levels, DSP/worker regressions, visual review, patch/kit listening and Studio/installer acceptance. |
| 26 | Compatibility and recovery | PARTIAL | Package boundary/hash audit, bad/foreign-package rejection and interrupted install/rollback simulation passed. Physical USB update and rollback remain open. |
| 27 | Source backup | COMPLETE | Exact candidate tree verified by a fresh GitHub fetch. The backup ZIP contains the source, staged site/Studio, candidate package, captures, evidence and SHA-256 manifest. |
| 28 | Experimental release and installer | BLOCKED | Native identity `FM-1_75000128`. This candidate is archived for engineering recovery. No public release or new installer entry while software acceptance and device timing remain incomplete. |

## Verification

- Native JieLi 20260730.1 build with all three pinned SDK inputs. Application 564,048 / 581,564 bytes. Static RAM 87,752 / 98,304 bytes. Pool 328,648 / 344,064 bytes. Package 611,488 bytes, SHA-256 `95d0e751754950f3300a9c8e63e07bc233be4bbb17bde849c3182732a1e8632c`. The simulator verified the permitted app-write interval `[0x4000, 0x93000)` and preserved song/settings flash.
- The [hosted Studio Chromium workflow](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44961257381) passed on the exact candidate commit. The matching local WASM and updater protocol tests also passed.
- 31 host fixtures passed. Nine dense EDIT entry/exit PCM comparisons passed at 112, 137 and 160 BPM. Host and WASM verify software behavior. The reported device distortion was not reproduced on host.
- In steady dense host recordings at 112, 137 and 160 BPM, global LP at −100 reduced 2.5–10 kHz energy by about 25.3 dB. Global HP at +100 reduced 45–200 Hz energy by 39.2–42.3 dB. The track-only and combined states produced separate responses. The recorded samples stayed inside the PCM limits.
- The configured callback is 256 frames at 44,100 Hz, or 5,805 µs before the integer-tick budget calculation. Three alternating host runs per tempo and mode compared this exact candidate with `350929e`. Worker P99 increased at all three tempos. Single-core P99 decreased at all three. The 112 BPM worker peak also rose sharply, so the worker qualification gate remains open. These timings measure host thread CPU. Physical USB/LCD timing, worker tails and XRUNs remain unverified.

| BPM | Worker P99 baseline → candidate (µs) | Worker peak baseline → candidate (µs) | Single-core P99 baseline → candidate (µs) |
| --- | ---: | ---: | ---: |
| 112 | 211.09 → 299.33 | 379.41 → 3137.79 | 230.20 → 191.26 |
| 137 | 205.56 → 216.50 | 381.57 → 473.19 | 203.72 → 197.02 |
| 160 | 195.35 → 244.69 | 385.22 → 430.64 | 210.29 → 201.40 |

- See the paired 240×240 frames and fixture logs in the backup. Frame sequences show software animation and layout.
- Quantum GammaWave informed the use of live operator state and separate graph drawing. No Quantum GammaWave source, fonts or graphics were imported.

## Resume

Start from the exact source tree identified above. Reuse the package hashes, host results and rollback evidence. The remaining software work includes full control-page dial adoption, exhaustive FM6 routing/patch compatibility, held-loop edge cases, slicer waveform/capture review and stronger benchmark qualification. Reproduce and attribute the physical EDIT symptom and HOME tearing on an FM-1 before changing the known-good worker design. Stage the website only for the firmware and installer version that passes release qualification.
