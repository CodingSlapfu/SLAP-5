# SLAP-5 environment recovery

The consolidated release gate remains open. The public hardware-testing firmware is [v0.1.31](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.31-hardware-testing).

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
- The app at this checkpoint compiles to 566,024 / 581,564 bytes, with 88,504 / 98,304 bytes static RAM and 335,568 / 344,064 bytes pool usage. [Candidate Chromium/WASM/Studio](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777705279) · [Candidate package audit, rollback and backup](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777705356)

## Dense host evidence

The existing filter, patch-switch and dense recording scenario reaches 16 voices and runs a visualizer with active MIDI for the measured tail. Three trials alternate on a pinned host CPU per version and scenario. Median wall P95/P99/peak, in µs, for combined MIDI traffic:

| Tempo | v0.1.35 | v0.1.36 |
|---|---:|---:|
| 112 BPM | 185.70 / 196.07 / 289.69 | 185.79 / 195.18 / 242.39 |
| 137 BPM | 188.63 / 203.28 / 249.24 | 189.15 / 202.95 / 241.29 |
| 160 BPM | 190.69 / 213.09 / 300.57 | 189.05 / 211.74 / 236.95 |

The run did not reproduce the prior 137 BPM tail regression. At 137 BPM with combined traffic, wall-minus-thread-CPU P99 was 1.0 µs on both baselines. The separate 137 BPM receive-only run differed by 3.59 µs in wall P99, while transmit-only differed by 0.20 µs. These cases recorded zero host callbacks above the 256-frame budget. The earlier environment and workload remain relevant to attribution. Actual LCD transfers, native Core 1 mailboxes, USB/DMA contention and FM-1 deadline behavior still require device measurements. [Exact baseline comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776943852)

## Reference firmware

- [Quantum GammaWave renderer audit](https://github.com/Guillel51/Quantum-GammaWave/blob/main/docs/FASE4_RENDER_v1.5.25.md) counts pixels over consecutive settled frames to expose alternating redraws. SLAP-5 uses its own HOME meter rectangle and DISPLAY state cache.
- [Handy-1 v0.3.0](https://github.com/jakehandy/h1/blob/v0.3.0/CHANGELOG.md) documents a 0.7 ms limiter, incremental saves into pre-erased flash and compact assets. Its [architecture](https://github.com/jakehandy/h1/blob/v0.3.0/docs/ARCHITECTURE.md) uses UI snapshots and changed display tiles with separate draw and presentation storage. SLAP-5 has kept its current DSP and memory ownership during this stage.
- [Mighty Arcade](https://github.com/NickCulbertson/FM1-Mighty-Arcade/blob/128c34716706a474d660665fd63d2b65525de48e/app/app.c) shares one state block between exclusive modes, builds audio parameters before handing them over, and uses strip rendering. This is relevant to visualizer scratch reuse and avoiding multiple full screen buffers.
- [AQUORA's worker](https://github.com/CodingSlapfu/AQUORA/blob/6b5dc707e0ceeacb542ad9d2210cfee0597883a7/device/visual_worker.c) exposes failures and fallback. [SLOOP 3's master DSP](https://github.com/isod89/sloop3-fm1/blob/159e4b460d81c1e958994ebb1c4012f06244b4e9/engine/master.c) computes filter coefficients by block and keeps reset work out of repeated block processing. SLAP-5 retained its existing fallback and 256-frame schedule.
- The saved research identifies [SKIFF](https://github.com/illuminus5/skiff) as the project by illuminus5. Its current documentation describes a 16-voice shared budget, engine-specific costs, and a single-core fallback on worker startup failure. The renderer and firmware changes here use SLAP-5's own code.

## Remaining qualification

Use the [v0.1.36 engineering report](evidence/qualification-0.1.36-engineering.md) and [consolidated acceptance](evidence/consolidated-acceptance.md) to resume. Time spent on recovery did not close the full CHROMA typography and panel overhaul, remaining MIDI scheduling and overdub permutations, SLICER marker review, legacy and other-engine DSP-to-graph checks, drum screen extremes, expressive performance and LED/output profiles, remaining sequencer and live FX work, or Studio and website deployment. Keep the v0.1.31 rollback available. Hold experimental publication until the integrated software gate passes.

Physical validation after software qualification uses the owner’s FM-1 and a backed-up project. Run five-track chords, dense drums and overdub at 112, 137 and 160 BPM with SLICER, LIVE FX, both bipolar filter sweeps and visualizers. Record P95/P99, peak, deadline misses, XRUNs and Core 1 status. Check the 12 themes on the 240×240 panel, LCD tearing, Sound Lab and LED/output profiles. Connect the RG35XX Pro as USB host to the FM-1 as USB MIDI device. Confirm the muOS ALSA raw MIDI port, note exchange and both PocketTracker clock/transport directions. Confirm app-only OTA and v0.1.31 rollback on the compatible unit.
