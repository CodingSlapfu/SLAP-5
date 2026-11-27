# SLAP-5 v0.1.48 qualification

Firmware identity: `FM-1_75000148`. Application source: [`4e405ddaecc740254a7ca33dca1641f581c23120`](https://github.com/CodingSlapfu/SLAP-5/commit/4e405ddaecc740254a7ca33dca1641f581c23120).

## Native resources

| Build | Application / 581,564 B | Static RAM / 98,304 B | Pool / 344,064 B |
| --- | ---: | ---: | ---: |
| v0.1.47 released | 576,772 | 87,992 | 335,568 |
| Optimization-only checkpoint | 576,612 | 87,976 | 334,056 |
| v0.1.48 application checkpoint | 576,840 | 87,992 | 335,208 |
| v0.1.48 versioned package | 576,856 | 87,992 | 335,208 |
| Versioned change from v0.1.47 | +84 | 0 | −360 |

[Optimization native gate](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45941817535) · [Application checkpoint native/host gate](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45942312167).

The retired native FX worker and rollback snapshots moved behind host-test guards. Core 0 remains the sole native track-FX owner. Core 1 continues the existing voice split and restart policy. Host FX fault injection remains available. The FM6 cache moves the six ratio conversions and six level conversions to timbre setup. Its 48 bytes per synth instance account for 1,152 bytes of the recovered pool. No capacity or synthesis engine was removed. The final checkpoint gains pool space and uses slightly more flash. Native timing, stack high-water, complete map attribution, and per-engine PCM comparisons remain open.

## Original implementation and comparative review

| Firmware | Inspected source or documentation | Finding and decision |
| --- | --- | --- |
| SLOOP 3, `c49c4b94cb1934c33ade4a68c4936be85817d72d` | GPL-3.0, `seq/seq.h`, `seq/seq.c` | Bounded recorder has 128 events per pattern and 10 held entries. Playback resources have separate limits. SLAP-5 retains its existing project formats and limits. |
| SKIFF v2.7.2 | GPL-3.0, `firmware/src/voice.c` | Released-voice priority supports evaluating quiet-tail reclamation independently. SLAP-5 retains its v0.1.47 quiet-tail behavior pending physical tests. |
| FM1-MOVE, `42fc1af2a2ab6174331d2a4953f7628d6f11b7f8` | GPL-3.0, `firmware/src/voice.c` | `voice_cap_half`, `voice_victim`, and `voice_kill` combine adaptive cap, victim selection, and fades. The block envelope coefficient cache establishes a useful scheduling pattern. Avoid adaptive shedding of held SLAP-5 notes without native evidence. |
| PERF-1, `81c2c4e8f43bea6510204770d33413ac84262280` | GPL-3.0, `firmware/src/dsp/fm6_core.inc` | Separate integer and floating-point operator buses and explicit parity test path. A fixed-point port risks changing feedback and timbre. This candidate only caches SLAP-5's existing scalar conversions. |
| Quantum GammaWave | GPL-3.0, `docs/FASE4_RENDER_v1.5.25.md` | Settled-pixel audit is relevant to strip rendering. Reused the existing SLAP-5 capture and dirty-region qualification infrastructure. |
| Handy-1 v0.3.0 | `jakehandy/h1`, architecture docs | Dirty tiles, snapshots and bounded UI work are applicable. Current SLAP-5 renderer already uses bounded cadence. No renderer replacement introduced. |
| Mighty Arcade, `128c34716705c264d660665fd63d2b65525de48e` | `app/app.c` | Shared scratch and strip rendering support keeping the existing bounded buffers. No additional framebuffer allocated. |
| AQUORA, `6b5dc707e0ceeacb542ad9d2210cfee0597883a7` | `device/visual_worker.c` | Worker dispatch and fault-mode test doubles show where snapshot ownership matters. Do not share the live audio worker with animation. |
| INTREPID 1.0 | [Developer documentation](https://danzaimusic.com/firmware/intrepid/) | Network-driven music and screen mirror are documented. Source scheduling and license remain unverified. Adding network work is outside the audio budget of this candidate. |
| BANG BANG | DanZai demonstration | Public behavior reviewed. Scheduler, map, and license remain unverified. |
| Galactic and SHRUB | Exact source identity unresolved | No implementation attributed to either project. FM6 visualization is based on SLAP-5's actual routing table. |

No external source or artwork was incorporated. A whole-firmware map/hot-path audit, deeper visualizer and knob-reference comparisons, generalized ATS/ASP, predictive DSP admission and Track 1 bypass timing remain open.

## Interface

Short EDIT opens Sound Lab. The existing two-second EDIT hold opens MIXER and now has a HOME hint. FM6 shows the existing routing and carrier roles with flow markers. MIXER adds segmented live levels with duck and filter position indicators. SETTINGS uses larger HOME-style glyphs for key labels. LED controls persist independent LOW/MED/MAX levels for track selection and other buttons. White keys follow the latter. The normal black-key mask and full transport indicators preserve performance cues. Dancer, Solar System and Gravity Rings use revised drawing parameters within the existing visual governor. Physical readability, LED intensity, full visualizer scope and all submenu captures remain open.

## Pinned host audio timing

At 155 BPM, entries are microseconds of callback thread CPU time. Each statistic is the median of three trials. These are host measurements from representative fixtures.

| MIDI profile | v0.1.47 median / P95 / P99 / peak | v0.1.48 checkpoint median / P95 / P99 / peak |
| --- | --- | --- |
| RX | 106.16 / 168.73 / 180.86 / 236.28 | 106.19 / 169.35 / 180.95 / 249.99 |
| TX | 122.00 / 225.77 / 240.75 / 253.76 | 121.30 / 225.03 / 240.00 / 253.86 |
| RX + TX | 105.77 / 168.40 / 181.51 / 252.75 | 105.21 / 167.35 / 180.92 / 252.83 |

[Exact-source pinned host comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45942312181). RX peak increased. TX and bidirectional typical times improved modestly. The results do not establish a universal DSP improvement or native headroom.

## Physical FM-1 gate

Use an untouched backup of the saved JUNGLE project. Cold boot, confirm identity, inspect GLOBAL startup/worker state, then clear counters before the run. At 155 BPM and 60% swing, play all five populated tracks with dense chords and 16 active voices. Exercise Track 1 reverb 50, delay 8, chorus 8, drive 70 and filter, then compare the bypasses one at a time. Record and overdub while using LIVE FX, SLICER and graphical pages, mute/restore tracks, stop and restart. Capture GLOBAL SYSTEM, DISPATCH, Core 1, spike and DMA views. Pass requires clean playback and recording, productive Core 1, zero new deadline misses or DMA XRUNs and peak below 5,804 µs with headroom. v0.1.46 reached 6,981 µs on physical FM-1. Desktop fixtures are representative and cannot certify a hardware fix. The main installer must remain at its qualified release until this gate passes.

## Experimental package and publication

The [release workflow](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45942568719) passed package boundary, identity, rollback and interrupted-update simulations against both v0.1.31 and v0.1.47, verified the republished artifacts byte-for-byte, and checked the deployed installer, package manifest and four website framebuffer images. Installer software verification does not substitute for a physical flash or listening test.

| Artifact | SHA-256 |
| --- | --- |
| Firmware | `9bcf3b342c0e376ab831e6439683c53a78390020cb6d352f7aa7eb248cb97fbe` |
| Engineering backup | `8faf8a267d2804487fc46b5e6ce226c26dbccb237320bda0f748119fef4f6247` |
| Framebuffer archive | `aaaf1fb1f46cbad8d2dd6fd0cf2c28c96d71169ed40e8065b8217fae4960caa1` |
| HOME comparison | `accd55d3e0c16d39bc0b6e8c8ae94ad38de83ba284f06a10c694aa8f4ece33f8` |

[Experimental installer](https://codingslapfu.github.io/SLAP-5/flash-testing-0.1.48.html) · [Website](https://codingslapfu.github.io/SLAP-5/#v0148) · [Release and assets](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.48-hardware-testing). The primary installer remains on its prior v0.1.18 testing distribution. Verified Pages staging revision: `5f4b80fbe25d881650ce78803a3956c9a950a7ec`. Prior Pages revision: `a10a62a54be2b1f8e4fb2c1492d868e625966e68`.

The [expanded Pages verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45942938917) confirmed the live primary installer HTML and its linked v0.1.18 checksum, the v0.1.48 installer and firmware checksum, and all hosted images byte-for-byte at Pages revision `efa956451c0fb77bc730b4e72320b678179b4bdd`. The gallery includes eight SETTINGS panels, four MIXER knob styles, and three visualizers. Desktop/mobile layout checks of the deployed landing page and continuous animation evidence remain open.

The published candidate has a migration edge case for legacy hit-only key lighting: changing brightness can leave idle white keys dark until reboot. The persisted group levels reload correctly. Hardware brightness and appearance still require the FM-1 check.
