# SLAP-5 environment recovery

The consolidated release gate remains open. The public hardware-testing firmware is [v0.1.31](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.31-hardware-testing).

## Source and backup

- Consolidated engineering baseline `8d68d757a39acb71f3d73fe491f86f33b6a653ac` on `slap-5-consolidated`. Firmware source `1ee54631cbc1c0f1e5a8a4c2734e29942bd27bc0`.
- Preserved firmware `FM-1_75000136`. SHA-256 `1b8f1edc80ad3ea092f7ef8c10dc6637a9e90566015155874f54618f61ad3b70`.
- [Baseline recovery run and build artifact](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776393861) reproduced that SHA-256. The artifact holds the package, application, OTA loader and `SHA256SUMS`. It expires after 30 days. The [engineering archive](https://chatgpt.com/library?file_id=libfile_9f841ae0cfc88191ae382cdfafb9b38d&account_id=personal) is the durable backup.
- Candidate firmware work is on `slap-5-environment-recovery`. Its app commit at this checkpoint is `ed57cfa2454a6cc3d3f630fe5613bfbe19fe7825`. This branch enabled GitHub Actions on Linux x86-64 when the Windows shell could not resolve GitHub and had no checkout, WSL or native SDK.

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
- A fast-path skips unchanged DISPLAY settings frames. [Display transfer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777187060)
- The external recording count-in is clocked by MIDI sixteenths and produces four quarter-note clicks. [Integrated verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777378516)
- The lighter pinned 112, 137 and 160 BPM comparison measured 137 BPM mixed-traffic wall P99 of 62.14 µs on 0.1.35 and 62.20 µs on 0.1.36. It did not reproduce the previous 232 → 336 µs regression. An experimental skipped MIDI scan made 137 BPM mixed-traffic timing worse in its same-run comparison, so the patch was reverted. [Baseline comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776254468) · [Experiment](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45776451001) · [Dense comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45777378746)

## Remaining qualification

Use the [v0.1.36 engineering report](evidence/qualification-0.1.36-engineering.md) and [consolidated acceptance](evidence/consolidated-acceptance.md) to resume. Time spent on recovery did not close the full CHROMA typography and panel overhaul, remaining MIDI scheduling and overdub permutations, SLICER marker review, legacy and other-engine DSP-to-graph checks, drum screen extremes, expressive performance and LED/output profiles, remaining sequencer and live FX work, or Studio and website deployment. Keep the v0.1.31 rollback available. Hold experimental publication until the integrated software gate passes.

Physical validation after software qualification uses the owner’s FM-1 and a backed-up project. Run five-track chords, dense drums and overdub at 112, 137 and 160 BPM with SLICER, LIVE FX, both bipolar filter sweeps and visualizers. Record P95/P99, peak, deadline misses, XRUNs and Core 1 status. Check the 12 themes on the 240×240 panel, LCD tearing, Sound Lab and LED/output profiles. Connect the RG35XX Pro as USB host to the FM-1 as USB MIDI device. Confirm the muOS ALSA raw MIDI port, note exchange and both PocketTracker clock/transport directions. Confirm app-only OTA and v0.1.31 rollback on the compatible unit.
