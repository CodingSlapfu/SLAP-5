# SLAP-5 v0.1.49 engineering qualification

Source checkpoint: `7fd92a81fc5f684d0aa591de8e7e59114d36ced1`. Candidate identity: `FM-1_75000149`. Public promotion is gated on physical playback across pages and the remaining release acceptance items in Issue #8.

## Audio and UI scheduling

The owner reported clean ordinary JUNGLE playback in v0.1.48 and distortion when viewing live Sound Lab and FX pages. The callback previously forced all five track FX paths on in inspection views, including normally sleeping chains. The candidate makes the same active FX decision on every page, captures only the selected track for Sound Lab/FX at one point per 64 samples, and leaves live FX ownership on Core 0. The snapshot observes the signal after the active track-FX blend. Native effects no longer update five old trace rings. Spectrum processing runs only when the visualizer is rendered. Graphics have a 50 ms minimum interval, with a slower visualizer update under diagnostic pressure. Input handling remains outside the display timer.

The existing project format and voice capacity are unchanged. Generalized ATS/ASP, predictive admission, per-engine comparisons, and exact device timing attribution remain open. Prior comparative research remains in [the v0.1.48 record](qualification-0.1.48.md).

## UI and reference

EDIT hold is one second. SETTINGS → DISPLAY lists KNOB THEME alongside other display settings, reusing the same persisted style bits. The prior LED migration edge case is fixed on changing the setting. HOME gains a white title, adjacent vertical charge-level gauge, clearer tempo and EDIT labels, smaller patch lettering, a dotted beat playhead, and a bar-number repaint gate. Dial values/labels have shadows and digital styles have distinct panel and ring details. The pinned SVG remains authoritative and is compared with the actual firmware host framebuffer in the qualification artifact. Full geometry alignment across screens and reference-based knob redesign acceptance remain open.

The platform exposes a coarse battery gauge. Its USB-powered state does not prove active charging. The candidate makes no claim of a working 80% cutoff or verified charging colors. No supported charger inhibit or reliable 80% measurement was established from the repository interface or the developer hardware overview.

## Owner physical test

Use an untouched copy of the saved five-track JUNGLE project. Start from a cold boot at 155 BPM and 60% swing. Observe equal windows at HOME, Sound Lab entry and live graph on OCTAVE SUB, FX pages with the same wet chain, MIXER and visualizers. Capture GLOBAL, Core 1, DISPATCH, spike, and DMA summaries with frame cadence, overhead and page transitions. Repeat with recording and overdub. The public gate requires clean audio, productive Core 1, zero new deadline misses or DMA XRUNs, and peak callback below 5,804 µs with headroom. Host captures and package audits do not answer this test.

## Historical recovery

The [historical archive through v0.1.48](https://github.com/CodingSlapfu/SLAP-5/releases/tag/historical-firmware-through-0.1.48) contains 122 assets across 33 releases and the complete Pages tree at `efa956451c0fb77bc730b4e72320b678179b4bdd`. [Archive audit](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45952474629) re-downloaded and extracted the published archive, verified the nested checksum manifest, and ran loader rollback and interruption simulations. Archive SHA-256: `cd3c24eccc7dba13899a56af482af723b6bdc23df429d9ea6e35183181d6a67a`. The site and Studio deployment are pending the public qualification gate.

The Dancer afterimage now samples rendered pose and position, holds in place, then fades. Under DSP pressure it uses sparser scanlines. Leaving and re-entering the visualizer resets the trail after a frame gap.
