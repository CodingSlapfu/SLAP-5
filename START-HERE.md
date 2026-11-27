# SLAP-5 environment recovery

## v0.1.48 experimental FM-1 candidate

Source `4e405ddaecc740254a7ca33dca1641f581c23120`. Identity `FM-1_75000148`. [Qualification and FM-1 acceptance procedure](evidence/qualification-0.1.48.md). [Exact-source native packaging, rollback and deployed installer](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45942568719).

- [Experimental installer](https://codingslapfu.github.io/SLAP-5/flash-testing-0.1.48.html) · [Release and engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.48-hardware-testing) · [HOME comparison](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.48-hardware-testing/SLAP-5-0.1.48-HOME-before-after.png).
- Firmware SHA-256 `9bcf3b342c0e376ab831e6439683c53a78390020cb6d352f7aa7eb248cb97fbe`. Backup SHA-256 `8faf8a267d2804487fc46b5e6ce226c26dbccb237320bda0f748119fef4f6247`.
- Main promotion requires documented owner FM-1 JUNGLE acceptance. Keep an untouched legacy project for rollback. The candidate package and site revision can be audited independently.

## v0.1.47 experimental firmware

Application `64b528982d45b01ab4da2fd094d5352b71798ac8`. Identity `FM-1_75000147`. [Qualification, acceptance matrix and owner retest](evidence/qualification-0.1.47.md). [Verified package and live installer audit](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45936534502).

- [Installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47-engineering-backup.tar.gz), [HOME comparison](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.47-hardware-testing/SLAP-5-0.1.47-HOME-before-after.png).
- Firmware SHA-256 `e9b284de383d387d5a5116d1210fde4a265d19211838fd6194824003ff226235`. Backup SHA-256 `453753205fac500311d5a4e149528ce438225328163a67f9385cea69dff12044`.
- Sustained native DSP load can shift eligible work to Core 1 and reclaim an already quiet release tail. The handoff, Core 0 FX ownership and 16 voice slots remain intact. Physical headroom and the actual JUNGLE peak remain open.
- HOME and MIXER separate full-size knob labels, shared BAR/LOOP rendering covers Digital VU, and SETTINGS uses the HOME glyphs. Direct MIXER mappings are LEVEL, DUCK, ALL and TRK. Hold SELECT for K2 source; ARP toggles SOLO. v0.1.46 remains the immediate rollback.


## v0.1.46 experimental firmware

Application `e5e7f371a13f74dcee242a806962df61ffed7cea`. Firmware `FM-1_75000146`. [Qualification and acceptance matrix](evidence/qualification-0.1.46.md). [Verified release and live installer audit](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45932228132).

- [Installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46-engineering-backup.tar.gz), [framebuffer comparison](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.46-hardware-testing/SLAP-5-0.1.46-HOME-before-after.png).
- Firmware SHA-256 `2c295b18f69a93e8064cfd55fccd300445b54d8131dfdd433177c9920ff993a5`. Engineering backup SHA-256 `f0d781d820ee24b715cf2cfe4fa12d7f1dc29d5623e5a78f5fcea049945f68be`.
- FX holds its live state on Core 0 and uses less repeated coefficient and ring math. The mixer skips inactive-track scans inside the sample loop. Physical JUNGLE deadlines and peak lag remain open.
- Black CHROMA, 12/14 px utility glyphs, full-size dials, updated orbit/tilt and loop metering, and MIXER SELECT-shift controls are included. Keep v0.1.45 available as rollback.


## v0.1.45 experimental firmware

Application `e6c3f7498d64c0763ceaab10bb1f9fa5d4e844eb`. Firmware identity `FM-1_75000145`. [Acceptance matrix and physical test procedure](evidence/qualification-0.1.45.md). [Verified release and served installer](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45903088951).

- [Experimental browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45-engineering-backup.tar.gz), [HOME before and after](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45-HOME-before-after.png), [theme and knob framebuffer evidence](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.45-hardware-testing/SLAP-5-0.1.45-framebuffer.zip).
- Firmware SHA-256 `a72db8e95d7caf0f8a04f487f5376ef92a934ec39605a644095292e3ce5f21e8`. Backup SHA-256 `84c3793113aef1080d5582bf1fa223a84fe5332d04979fdeaaaef864973b4dc9`.
- Project loads clear stale sequencer and audition voices at the audio boundary and reset mixer smoothing. The exact cause of the owner's fresh-load lag remains unconfirmed. Core 0 retains exclusive native FX ownership and the 16-voice architecture is preserved.
- HOME shares full-size MIXER knobs. DISPLAY adds four background-only themes, the measure counter follows the active loop, and the shared utility face is larger. A post-release source adjustment addresses the lower-edge label clipping visible in the released framebuffer.
- Preserve v0.1.44 as immediate rollback and repeat the owner stress case before physical acceptance.


## v0.1.44 experimental firmware

Application source `da17fe35bbde5b41865c896cfbcbd98f5c2786a0`. Identity `FM-1_75000144`. [Qualification and hardware test procedure](evidence/qualification-0.1.44.md). [Verified build, release and deployed installer](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45897199511).

- [Experimental browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44-engineering-backup.tar.gz), [HOME before and after](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44-HOME-before-after.png), [framebuffer archive](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44-framebuffer.zip).
- Firmware SHA-256 `18ed006e09a75678aca7d9ef968b34d84b9f09956ed45f251538cacf7bbdd553`. Backup SHA-256 `c2eae900974f384560c098a78e8233b275ac58ba7d5469e186356cde4f9e3a51`.
- Native track FX now runs on Core 0 to prevent FX-LATE retirement. Core 1 remains available for voice rendering. Five wet chains matched PCM in hosted validation. Physical Core 1 operation and deadline margin with the owner's JUNGLE project are still open.
- HOME restores compact dials, adds bar and selected-loop progress, brightens lane text and pulse, and limits names to eight characters. Small utility text uses Barlow Semi Condensed SemiBold. Review legibility on the physical panel before acceptance.
- Use the v0.1.43 package as immediate rollback and preserve the project used for the physical comparison.


## v0.1.43 experimental firmware

Application `720683d2e7974274d67163670bd9f9d6fdd0ad53`. Identity `FM-1_75000143`. [Engineering qualification and physical acceptance procedure](evidence/qualification-0.1.43.md). [Verified build and live installer](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45891460507).

- [Experimental browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.43-hardware-testing/SLAP-5-0.1.43.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.43-hardware-testing/SLAP-5-0.1.43-engineering-backup.tar.gz), [24-bit preview of the 240×240 firmware framebuffer and all four MIXER knob styles](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.43-hardware-testing/SLAP-5-0.1.43-framebuffer.zip).
- Firmware SHA-256 `387a7a96b30e6234411a17eb8c6a7deec11c3b3e7c335f4ef1755ec8170f774f`. Backup SHA-256 `77ab14a61502b7eac335d7dece0c8499662c3352bc67edf5b7cae63581cdcd80`.
- Core 1 gets one guarded boot-time retry after a hard late job. GLOBAL shows live worker state and boot-lifetime jobs separately from K3-clearable counters. The acoustic-piano engine caches repeated terms at voice start.
- HOME refreshes the layout with 22-pixel proportional glyphs, tinted gradients, a same-hue full-lane pulse, a fine grid and segmented meters. MIXER uses enlarged dials. Firmware-wide compact controls and the final visual match remain open.
- Physical timing, XRUNs and playback of the owner's exact project still require FM-1 acceptance. Preserve the v0.1.42 project copy and use the v0.1.42 package as the immediate rollback. Keep a legacy copy for v0.1.31.


## v0.1.42 experimental firmware

Application source `2d89ffe83b8146917837092698b42136e4add881`. Firmware identity `FM-1_75000142`. [Engineering and physical test instructions](evidence/qualification-0.1.42.md). [Exact-source software gates, package and deployed installer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45885709203).

- [Experimental browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.42-hardware-testing/SLAP-5-0.1.42.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.42-hardware-testing/SLAP-5-0.1.42-engineering-backup.tar.gz), [firmware framebuffer captures](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.42-hardware-testing/SLAP-5-0.1.42-framebuffer.zip).
- Firmware SHA-256 `7a2f7730a99b144c2390d6e42682082358e3c9921efe031cb777de19ac4ab1cc`. Backup SHA-256 `d9762dcca9156eb55bc3000eaa21a2181b4bb2f1b39c019621b6c1bde0dba127`.
- Acknowledged late voice work drains safely and resumes. Repeated late jobs narrow the remote batch, while FX admission uses remaining budget and the last matching job's runtime. GLOBAL identifies VOICE LATE or FX LATE after a hard timeout. The platform hard stop remains enabled for an unacknowledged job.
- SETTINGS → KNOB STYLE uses K1 to select four persistent appearances. HOME uses revised SIGNATURE glyph height and shadow. The single CHROMA palette and hardware assignments are retained.
- The 155 BPM dense recording and JUNGLE fixtures, worker timeout injection, MIDI, host/browser, native package, interrupted OTA and rollback checks passed. Physical testing is still required for crackling, XRUNs and the first late job. Keep an untouched legacy project for v0.1.31 rollback. Schema 13 projects need v0.1.41 or newer.

## v0.1.41 experimental firmware

Exact application source `a3d4f2ccb71a0344c81a2a593d8db377952e9ab6`. Firmware identity `FM-1_75000141`. [Release verification and deployed installer check](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45845292454). [Browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.41-hardware-testing/SLAP-5-0.1.41.fwsc), [engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.41-hardware-testing/SLAP-5-0.1.41-engineering-backup.tar.gz), [framebuffer captures](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.41-hardware-testing/SLAP-5-0.1.41-framebuffer.zip). Firmware SHA-256 `1511fbf8916ebc0e91c8d2981df813de96dfd8fb723077c7e809c509f4d8aebd`. Backup SHA-256 `1935bbfa06f8ce7ffc0b0d3cefdf581a035db4c190d2c2dc8fe184ccb97865fe`.

- In Sound Lab, hold SEL and turn ALGO to open Expression. K1–K4 set glide time, mode, vibrato depth and rate. Hold OCT+ and use K1 for Auto/Manual/Off, K3 for delay and K4 for fade. Edits apply to new notes. Fixed-length plucked strings retain fixed pitch. Factory profiles distinguish lead and acid patches, bowed strings, choir, brass/reeds and flutes. Other patches can use manual control where pitch is variable.
- Expression settings are independent for four melodic tracks. Existing songs remain readable. Songs saved with Expression settings use schema 13 and require v0.1.41 or newer. Keep an untouched legacy copy of songs needed during rollback. App-only firmware rollback preserves song storage.
- The sixteen shared voices now prefer inaudible lanes when they need to steal a voice, then the quietest releases. Idle mixer lanes with zero duck settings can sleep while another lane is configured to duck. Core 1 cost hints account for vibrato.
- [Host and native verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45845037630) covers manual vibrato changing held-note PCM, MIDI chords, save/reload, worker PCM equivalence and simulated interrupted OTA/rollback. [Studio](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45845038166) passed. Image 568,784/581,564 bytes, static RAM 89,176/98,304, pool 335,568/344,064.
- Continue with individual factory patch listening, fuller synth and FX track sleep, chord retrigger and note-init optimizations, physical timing, broader SIGNATURE adoption and direct comparison to the approved image. The archived LOOPER screenshots compare against the historical firmware baseline. FM-1 chord-overload resolution remains a working assumption for software development pending physical acceptance. Refer to the consolidated backlog below for remaining requirements.

## v0.1.40 experimental firmware

Exact source `843718ab19e7caca442db8cbf17effa59c608e15`. [Native, host, framebuffer and recovery qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45842544639) passed. [Experimental release gate](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45842706224) passed the exact-source Studio and pinned DSP checks and verified deployed firmware bytes.

- [Browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.40-hardware-testing/SLAP-5-0.1.40.fwsc), [verified engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.40-hardware-testing/SLAP-5-0.1.40-engineering-backup.tar.gz). Identity `FM-1_75000140`. Firmware SHA-256 `d6b3736d93a2c4dbe32d9da0012dc61c85b7827c50be945f8af2fd2e15df91ff`. Backup SHA-256 `fc11118744878cf540e0e6646fb6d83885598cbce172222e666a55fec5a12dc8`.
- [Framebuffer captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45842544639/artifacts/15130115193). [Pinned 155 BPM host run](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45842544557) used v0.1.38 and pre-DSP baselines. Mixed-MIDI median wall P99 was 230.78 to 230.74 µs against v0.1.38, and median peak 313.06 to 317.44 µs.

- LOOPER uses original, editable SIGNATURE bitmap row sources in `device/app.c`. Lettering occupies up to 33 pixels in each 35-pixel lane with one pixel above and below; long names are fitted horizontally. The approved reference remains the visual target. Wider firmware typography adoption and a reference-image comparison remain open.
- Automatic leveling no longer prevents settled tracks from entering idle mix sleep. Unity gain smoothing finishes when a floating-point step rounds away. The GLOBAL fixture covers the default project going from sleep to MIDI wake to sleep, and K3 peak reset.
- Generalized track sleep, chord-overload diagnosis and ASP, SOUND LAB EXPRESSION, broader diagnostics, and the remaining qualification backlog are still open. Physical FM-1 timing is unverified.

## v0.1.39 experimental firmware and next development checkpoint

- [Verified direct browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html), [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.39-hardware-testing/SLAP-5-0.1.39.fwsc), [v0.1.31 rollback](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.31-hardware-testing/SLAP-5-0.1.31.fwsc).
- Application source `f948fa24b6547533be9e9465e4b7224136984405`. Identity `FM-1_75000139`. Package SHA-256 `eb6404d11ba2353ef86bdebdda9ab72db5e62edd14c89ddbcbfeb0dbc4d0ab22`. [Deployment verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45840727368).
- [Engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.39-hardware-testing/SLAP-5-0.1.39-engineering-backup.tar.gz), SHA-256 `47f11f56c0cf957fa43477559aecd080ba1fa2aaed72fac2d77180d88f73d04c`. [Qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45840388180) includes native build, app boundaries, package integrity, rollback and interrupted OTA simulation, and settings and song preservation. Native image 565,532/581,564 bytes, static RAM 88,456/98,304, pool 335,568/344,064.
- Voice worker resumes after at most two consecutive late jobs under bounded recovery. Forced-late and retirement tests check PCM parity. Idle mix-stage skipping preserves configured effects, filter and ducking state plus audible tails. Sixteen shared voices and 256-frame configuration remain. The physical overload trigger remains open.
- [Dense 155 BPM host benchmark](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45840093665), wall P95/P99/peak median v0.1.38 to v0.1.39 was 166.78/181.00/245.25 to 167.10/179.99/251.16 µs. [Zero through five sounding tracks](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45840962603) shows median wall P99 changes from 19.04/31.84/56.61/66.40/71.38/73.60 to 19.52/31.26/55.64/64.71/68.52/71.35 µs. These are host measurements.
- [Framebuffer captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45840093678/artifacts/15126404304) include 240px and enlarged baseline and candidate PNGs. LOOPER uses existing custom fonts, a single palette, stationary patch names, right-edge meters and independent MIDI status. The original SIGNATURE font and exact lane typography are open.
- Development continues from `746a7f1ba5a32b3e818202412373405eb4276f8b`. GLOBAL now exposes per-track voice counts, settled mix sleep and resettable peak voices on the development branch. Continue profiling voice initialization and stealing, implementing generalized Track Sleep, SOUND LAB EXPRESSION, SIGNATURE glyphs and remaining LOOPER details. The MIDI, Sound Lab, sequencer, visualizer, LED, output profile and Studio acceptance backlog below remains active. Test the saved 155 BPM / 60% swing JUNGLE project on FM-1 with dense chords, SLICER, live undo and clear, and compare fresh deadline misses, DMA XRUNs and worker counts after K3 reset.

## v0.1.38 experimental audio-repair checkpoint

[Published hardware-testing prerelease](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.38-hardware-testing). Exact firmware source `c116d28ddb17ab6f7294bb44a42d51af3f3d3726`. Identity `FM-1_75000138`. Package SHA-256 `852e1a6fc4c77baf97eb47e9e3c5bc787136b93ebc1c04966c6fa03e5d1b86df`. The prerelease contains the checked firmware, source, application, loader, and recovery instructions. Package, native limits, interruption and v0.1.31 rollback with settings and songs preserved passed. Public assets were downloaded and checksum-verified. [Publication run](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45838756162)

- Muted and solo-excluded tracks skip new sequenced voice allocation. Existing releases and effect state are retained. Sixteen shared voices and 256-frame blocks are unchanged.
- The Core 1 FX path can accept an acknowledged completion within 480 µs of the wait start and within the existing three-quarter callback cutoff. A job that completes during the extra wait moves FX work to Core 0 for 64 callbacks. Core 1 remains available for voices. A hard timeout still stops the worker before rollback and rerender. Forced recoverable and hard-failure PCM parity passed. [Native, host and forced-worker checks](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45838409672)
- At 155 BPM with mixed MIDI, median host wall P95/P99/peak were 141.37/156.48/200.06 µs on v0.1.37 and 141.07/157.06/182.10 µs on v0.1.38. There were zero measured host budget overruns in the comparison. [Pinned measurements](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45838409609)
- The synthetic reproduction verified 155 BPM, 60% swing, JUNGLE drums, SLICER and 16 voices before and after saving and reloading. It does not contain the owner's original project or reproduce physical timing. [Jungle project fixture](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45838883526)
- Native resource use is 565,000 / 581,564 bytes flash, 88,424 / 98,304 bytes static RAM and 335,568 / 344,064 bytes pool. [Exact-source browser and Studio checks](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45838409600)

Next physical test: copy the v0.1.37 project, reset GLOBAL stats, reproduce the chords at 155 BPM and 60% swing, and capture the worker status before reboot if lag appears. Compare sustained Core 1 job progress, late voice and FX counters, XRUNs and deadline misses with FX active, FX off, and all tracks muted. The original hardware trigger remains open. Full adaptive voice and tail handling, generalized track sleep, expression, SIGNATURE typography, and the complete LOOPER remain open. Stable and browser installer tracks remain unchanged. Use the standalone experimental package with M-Flasher.

## Active single-palette development checkpoint

The owner approved implementation with a single default CHROMA palette. The 12-theme references below describe the older release. Candidate source `53a5f30d310e98bd1af34136ac0de2fef70cf8de` uses one fixed palette, discards the retired theme bits on settings load and removes the selector. HOME uses contiguous lane backgrounds. During a running take, hold REC and tap OCT− to undo the selected track's take or clear. Hold REC and tap OCT+ twice to confirm a selected-track loop clear. Playback and recording stay active. The host fixture verifies clearing and restoring the loop during the same take. [Candidate checks](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45837309094)

Read [the 155 BPM hardware report](evidence/hardware-0.1.37-155bpm.md) before changing voice dispatch. Muted tracks no longer schedule new sequenced voices in the development branch. This does not establish the cause of the chord-load overload or worker fallback. The remaining approved LOOPER, font, expression, adaptive polyphony, track sleep and hardware qualification work is open. The public hardware-testing release remains v0.1.37.

## Latest experimental release

**v0.1.37 is published for hardware testing.** The application source is `74f8fb192e6de777c4fa02c110c3e90702b88366` and the release is pinned to qualification checkpoint `a55fdc8fdb93958514c735be354f231413a0ed4f`. The owner deferred the noncritical backlog below for a later window. The historical continuation notes retain their original checkpoint context.

- Package identity `FM-1_75000137`, SHA-256 `0255ef443a95e909c4a93b4d0c17d0d358a418b7b2733fb0a11217047128dfe1`. [Prerelease and package](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.37-hardware-testing)
- Native compile, app-only package audit, interrupted-write recovery and v0.1.31 rollback with song/settings preservation passed in the loader simulator. The public assets were re-downloaded and checked against the audited checksums. [Qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45781972179) · [Public asset verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45782237725)
- The release includes the downloadable source, package, application, loader, component checksums, and recovery notes. The original development package and frozen source are available separately in the [original checkpoint archive](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45781972179/artifacts/15098278259), which has limited Actions retention.
- Use the standalone package with [M-Flasher](https://github.com/jvitkauskas/m-flasher/releases/latest) on a confirmed compatible FM-1. The existing browser installer is unchanged. [v0.1.31 rollback](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.31-hardware-testing/SLAP-5-0.1.31.fwsc)
- Physical acceptance remains pending. This prerelease does not change the stable track or deploy Studio.

The consolidated release gate remains open. The public hardware-testing firmware is [v0.1.31](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.31-hardware-testing).

## Current continuation checkpoint

Application code `74f8fb192e6de777c4fa02c110c3e90702b88366`. Integrated host/native test source `7d2cff4672a31ea8e773fead78a83b9830112ea4`. The public release gate remains open.

- Shared editor headers now use the selected CHROMA palette and cache the unchanged 240×48 title strip. Wider values use a full-width slider and readable text. The HOME transfer fixture retained all 60 pixel-identical captures. The DISPLAY fixture moved from 6,189,600 to 2,733,600 RGB565 pixels, about 55.8% below baseline.
- The all-theme fixture verifies six settled editor headers and eight graph and visualizer captures for each of the twelve themes. Sound Lab compares changed FM6 routing and wavetable morph/scan graphs with changed held-note PCM. This covers these edit paths and leaves full per-operator and per-engine correspondence open.
- The MIDI host fixtures verify independent live ownership of one pitch on the panel, USB, and TRS across release and USB reconnect. Five dense sequencer tracks fill all 24 outbound slots. The fixture blocks output and requires all scheduled Note Offs to drain after output resumes. It also verifies that solo limits the transmitted notes to the selected track and that muting that track suppresses them. A separate clean-boot case verifies overlapping sequencer ties share one MIDI Note On and one final Note Off. The broader overdub matrix remains open.
- Native size is 565,704 / 581,564 bytes. Static RAM is 88,520 / 98,304 bytes. Pool usage is unchanged at 335,568 / 344,064 bytes. Host/native compilation, PCM parity, package audit, interrupted OTA and v0.1.31 rollback simulation passed. The engineering package is `FM-1_7500`, SHA-256 `57a60ba3265b869d71876df78173c6b062557adbaf253d7ee987bcfb1a5add3d`. [Integrated qualification and artifact](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45780896755/artifacts/15098147578) · [Chromium and Studio on the current app source](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45779927512)
- The latest package artifact expires in 30 days. GitHub refused the runner token permission to create an unpublished archival draft (HTTP 403). The attempt was removed from CI. Preserve a copy of the exact package, binary, loader, and checksums in an authorized durable destination before calling the backup gate complete. The earlier v0.1.36 durable backup remains available below.
- Continue full typographic fit and control checks on sequencer/drums/patch/kit screens, all Sound Lab engines, expressive and legacy projects, remaining MIDI/overdub permutations, LED and listening profiles, and Studio/installer deployment. The earlier 137 BPM timing observation remains open for actionable or hardware evidence. Keep the experimental installer on v0.1.31 until the integrated software gate passes.

The [latest full app comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45779927589) ran dense five-track mixed-MIDI traffic. Median host wall timings are in microseconds.

| Tempo | v0.1.35 P95 / P99 / peak | Current P95 / P99 / peak |
|---|---:|---:|
| 112 | 166.47 / 192.56 / 221.58 | 168.31 / 189.90 / 226.84 |
| 137 | 180.38 / 213.55 / 264.09 | 180.18 / 214.06 / 263.44 |
| 160 | 143.91 / 156.25 / 193.71 | 143.91 / 157.03 / 194.91 |

This is host evidence with zero recorded deadline overruns. Native scheduling and LCD timing still require the FM-1.

## Source and backup

- Consolidated engineering baseline `8d68d757a39acb71f3d73fe491f86f33b6a653ac` on `slap-5-consolidated`. Firmware source `1ee54631cbc1c0f1e5a8a4c2734e29942bd27bc0`.
- Preserved firmware `FM-1_75000136`. SHA-256 `1b8f1edc80ad3ea092f7ef8c10dc6637a9e90566015155874f54618f61ad3b70`.
- [Baseline recovery run and build artifact](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776393861) reproduced that SHA-256. The artifact holds the package, application, OTA loader and `SHA256SUMS`. It expires after 30 days. The [engineering archive](https://chatgpt.com/library?file_id=libfile_9f841ae0cfc88191ae382cdfafb9b38d&account_id=personal) is the durable backup.
- Candidate firmware work is on `slap-5-environment-recovery`. Its app commit at this checkpoint is `e20f669af872ed752f3d7e816159f4262848740c`. This branch enabled GitHub Actions on Linux x86-64 when the Windows shell could not resolve GitHub and had no checkout, WSL or native SDK.

## Recover the build

Use Linux x86-64. Work in the existing SLAP-5 checkout if available. Verify ownership and current changes before creating a working tree. This branch already exists remotely.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install Pillow==11.3.0 fonttools==4.59.2
sh vendor/fm1-base/tools/get_toolchain.sh "$PWD/../slap-jieli"
export JIELI_TOOLCHAIN="$(realpath "$PWD/../slap-jieli/toolchain")"
test "$(basename "$JIELI_TOOLCHAIN")" = jieli-linux-toolchains-20260730.1
export AC79_SDK="$PWD/../slap-ac79"
SDK_URL=https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git sh vendor/fm1-base/bsp/tools/get_sdk.sh "$AC79_SDK"
sh device/build-native.sh
python3 vendor/fm1-base/tools/build.py device --host
sh device/tests/dual-core.sh
python3 device/tests/midi-clock-stress.py device/build/host/slap-5_host
```

For the known baseline, build `8d68d757a39acb71f3d73fe491f86f33b6a653ac` with `sh device/build-native.sh --release 0.1.36` and compare package SHA-256. JieLi SDK commit `d179b4484759423312073f5fbb232501aa491047` is checked by the vendored script. Its GitHub default returned an authentication error on the hosted runner. The upstream Gitee URL retains the vendor pin and file checksum checks.

## Candidate evidence

- HOME sends a 44×7 region for a meter-only update. The settled HOME test preserves 60 PNG captures byte for byte and transmits 2,091,808 RGB565 pixels instead of 3,516,000 in the same fixture. [LCD and native result](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776708143)
- Twelve RGB565 palettes persist through DISPLAY → COLOR THEME using SELECT and K1. Alternate palettes add a cached gradient to the selected HOME track. The default palette preserves existing HOME pixels. The 12-theme fixture covers every theme selection, restart at COSMIC and return to CHROMA. [Theme, host and native result](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777004882)
- A fast-path skips unchanged DISPLAY settings frames. It preserves the section screen after re-entry while reducing the fixture from 6,189,600 to 2,906,400 RGB565 pixels. [Display transfer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777378516)
- The external recording count-in is clocked by MIDI sixteenths and produces four quarter-note clicks. [Integrated verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777378516)
- The lighter pinned 112, 137 and 160 BPM comparison measured 137 BPM mixed-traffic wall P99 of 62.14 µs on 0.1.35 and 62.20 µs on 0.1.36. It did not reproduce the previous 232 → 336 µs regression. An experimental skipped MIDI scan made 137 BPM mixed-traffic timing worse in its same-run comparison, so the patch was reverted. [Baseline comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776254468) · [Experiment](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776451001) · [Dense comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777378746)

- SLICER hides its pattern and replay markers while transport is stopped and labels the enabled stopped state. The synthesis and capture paths are unchanged. [Latest app host tests and native build](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777577383)
- The app at this checkpoint compiles to 566,024 / 581,564 bytes, with 88,504 / 98,304 bytes static RAM and 335,568 / 344,064 bytes pool usage. [Candidate Chromium/WASM/Studio](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777705279) · [Candidate package audit, rollback and backup](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45778375289)

The unpublished development package is `FM-1_7500`, SHA-256 `77667971c5c7685733d824d242493c9207cd1fbb4eaab994f6df5171a4490cb7`. The package audit, interrupted-update recovery and rollback to the checksum-verified v0.1.31 package passed in the loader simulation. The [engineering artifact](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45778375289/artifacts/15097381512) retains firmware, application, OTA loader and component hashes for 30 days. Gitee also timed out in one later attempt. The candidate workflow can recover all three required non-BLE packaging inputs from an alternate source at the same SDK revision and validates every file against the checked-in SHA-256 manifest.

## Dense host evidence

The existing filter, patch-switch and dense recording scenario reaches 16 voices and runs a visualizer with active MIDI for the measured tail. Three trials alternate on a pinned host CPU per version and scenario. Median wall P95/P99/peak, in µs, for combined MIDI traffic:

| Tempo | v0.1.35 | v0.1.36 |
|---|---:|---:|
| 112 BPM | 185.70 / 196.07 / 289.69 | 185.79 / 195.18 / 242.39 |
| 137 BPM | 188.63 / 203.28 / 249.24 | 189.15 / 202.95 / 241.29 |
| 160 BPM | 190.69 / 213.09 / 300.57 | 189.05 / 211.74 / 236.95 |

The run did not reproduce the prior 137 BPM tail regression. At 137 BPM with combined traffic, wall-minus-thread-CPU P99 was 1.0 µs on both baselines. The separate 137 BPM receive-only run differed by 3.59 µs in wall P99, while transmit-only differed by 0.20 µs. These cases recorded zero host callbacks above the 256-frame budget. The earlier environment and workload remain relevant to attribution. Actual LCD transfers, native Core 1 mailboxes, USB/DMA contention and FM-1 deadline behavior still require device measurements. [Exact baseline comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776943852)

The [final app comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777577375) tested exact app checkpoint `e20f669` against both releases. With traffic in both directions, wall-timing medians were:

| Tempo | v0.1.35 P95/P99/peak (µs) | v0.1.36 P95/P99/peak (µs) | Candidate P95/P99/peak (µs) |
|---|---:|---:|---:|
| 112 BPM | 167.70 / 191.42 / 244.59 | 167.59 / 191.75 / 235.63 | 168.79 / 189.96 / 230.08 |
| 137 BPM | 180.12 / 212.43 / 255.90 | 179.71 / 211.61 / 259.75 | 179.78 / 211.76 / 256.58 |
| 160 BPM | 144.99 / 158.06 / 192.20 | 144.97 / 158.92 / 200.89 | 145.12 / 159.85 / 198.28 |

No measured callback in any of the receive-only, transmit-only or combined trials crossed the simulator budget. The kernel and VM varied across hosted runs, so use the within-run comparisons.

## Reference firmware

- [Quantum GammaWave renderer audit](https://github.com/Guillel51/Quantum-GammaWave/blob/main/docs/FASE4_RENDER_v1.5.25.md) counts pixels over consecutive settled frames to expose alternating redraws. SLAP-5 uses its own HOME meter rectangle and DISPLAY state cache.
- [Handy-1 v0.3.0](https://github.com/jakehandy/h1/blob/v0.3.0/CHANGELOG.md) documents a 0.7 ms limiter, incremental saves into pre-erased flash and compact assets. Its [architecture](https://github.com/jakehandy/h1/blob/v0.3.0/docs/ARCHITECTURE.md) uses UI snapshots and changed display tiles with separate draw and presentation storage. SLAP-5 has kept its current DSP and memory ownership during this stage.
- [Mighty Arcade](https://github.com/NickCulbertson/FM1-Mighty-Arcade/blob/128c34716706a474d660665fd63d2b65525de48e/app/app.c) shares one state block between exclusive modes, builds audio parameters before handing them over, and uses strip rendering. This is relevant to visualizer scratch reuse and avoiding multiple full screen buffers.
- [AQUORA's worker](https://github.com/CodingSlapfu/AQUORA/blob/6b5dc707e0ceeacb542ad9d2210cfee0597883a7/device/visual_worker.c) exposes failures and fallback. [SLOOP 3's master DSP](https://github.com/isod89/sloop3-fm1/blob/159e4b460d81c1e958994ebb1c4012f06244b4e9/engine/master.c) computes filter coefficients by block and keeps reset work out of repeated block processing. SLAP-5 retained its existing fallback and 256-frame schedule.
- The saved research identifies [SKIFF](https://github.com/illuminus5/skiff) as the project by illuminus5. Its current documentation describes a 16-voice shared budget, engine-specific costs, and a single-core fallback on worker startup failure. The renderer and firmware changes here use SLAP-5's own code.

## Remaining qualification

Use the [v0.1.36 engineering report](evidence/qualification-0.1.36-engineering.md) and [consolidated acceptance](evidence/consolidated-acceptance.md) to resume. Time spent on recovery did not close the full CHROMA typography and panel overhaul, remaining MIDI scheduling and overdub permutations, SLICER marker review, legacy and other-engine DSP-to-graph checks, drum screen extremes, expressive performance and LED/output profiles, remaining sequencer and live FX work, or Studio and website deployment. Keep the v0.1.31 rollback available. Hold experimental publication until the integrated software gate passes.

Physical validation after software qualification uses the owner’s FM-1 and a backed-up project. Run five-track chords, dense drums and overdub at 112, 137 and 160 BPM with SLICER, LIVE FX, both bipolar filter sweeps and visualizers. Record P95/P99, peak, deadline misses, XRUNs and Core 1 status. Check the 12 themes on the 240×240 panel, LCD tearing, Sound Lab and LED/output profiles. Connect the RG35XX Pro as USB host to the FM-1 as USB MIDI device. Confirm the muOS ALSA raw MIDI port, note exchange and both PocketTracker clock/transport directions. Confirm app-only OTA and v0.1.31 rollback on the compatible unit.
