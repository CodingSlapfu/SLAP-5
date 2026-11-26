# SLAP-5 environment recovery

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
