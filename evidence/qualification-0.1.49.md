# SLAP-5 v0.1.49 engineering qualification

Source checkpoint: `4f2048879e930a9091d1bb3ef73f8346daca6643`. Candidate identity: `FM-1_75000149`. Public promotion is gated on physical playback across pages and the remaining release acceptance items in Issue #8.

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

The drum Sound Lab uses the same visible audio snapshot as melodic live-signal pages, with an idle redraw when activity stops.

## Verified engineering package and website preview

| Resource | v0.1.48 | v0.1.49 | Delta |
| --- | ---: | ---: | ---: |
| Application flash | 576,856 B | 577,776 B | +920 B |
| Static RAM | 87,992 B | 88,056 B | +64 B |
| Memory pool | 335,208 B | 335,208 B | 0 B |

[Native package, rollback and framebuffer checks](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45956039967) · [Host LCD and worker regressions](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45955978342) · [Studio/browser regressions](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45956107948).

- [Engineering package and visual comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45956039967/artifacts/15178946600). Firmware SHA-256 `b84455e7a6d85c3b802e1b0f3d0dbd97ddbfb6e76f0d11c24e6575a03144fe35`; backup SHA-256 `d8aad43463657fd27bcb4a41bafe084beee62fbfeb8fcbe519d1e3272d50e59a`. The package includes a 480×240 paired HOME comparison with the pinned SVG on the left and the host framebuffer on the right, plus captures of all four HOME and MIXER knob themes and the live drum Sound Lab.
- [Current-only website and Studio preview](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45956107778/artifacts/15178587685). [Preview audit](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45956107778) checked mobile and desktop landing/install layouts, gallery URLs, installer metadata and package digest, and Studio. It contains one firmware package. The Pages branch remains at `efa956451c0fb77bc730b4e72320b678179b4bdd`. Deployment, cleanup, and physical flashing await release acceptance.

The HOME comparison is engineering evidence. Full alignment of proportions, type hierarchy and dial geometry with the reference remains open. The six original handoff knob PNGs were not available for inspection in this session. Global design audit, guarded REC reset gestures, remaining transport boundaries, drum dynamics, Dancer control overlays, wider compatibility tests and complete firmware comparison remain open. Release promotion requires both the physical test and resolution of the remaining release acceptance items.

[The pinned 155 BPM host timing run](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45955978275) passed. Compared with its v0.1.47 reference, callback thread-CPU P99s were 204.91 → 206.10 µs (RX), 207.94 → 208.57 µs (TX), and 204.15 → 204.77 µs (both). The corresponding median-of-trials peaks were 261.49 → 240.95 µs, 243.04 → 242.01 µs, and 241.08 → 236.60 µs. This measures representative host load against v0.1.47. No same-run v0.1.48 or physical cross-page timing comparison is available.

## Consolidation checkpoint

- **Source:** `aa313d7497d1468320f58e69d7f8183bc89d5032`. [Native build, host gestures, rollback and framebuffer output](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45960084692).
- **Checksums:** firmware `30fe33b895e4eebdcf3f575a156995665ba8288971bf5a59210d0a7639b42712`; engineering backup `4332ce896444bfb0ef32314b44828dab46111a1a19bad7f1763aa936b37144af`; reference comparison `ac978b483d5712c97f69582f27ed9ee3c146a589618f99b4dd6ce3337f0a95e7`.
- **Native resources:** application 577,712 of 581,564 B, RAM 88,088 of 98,304 B, pool 335,208 of 344,064 B. Application flash is 1,232 B below the 578,944 B pre-compression checkpoint and 856 B above v0.1.48. Dancer stores the first nonblank row for each pose. An identical animation fixture compared the compact output to the full-row assets at the same clock; all captured PNGs matched byte-for-byte.
- **LCD:** The approved layout now has a dedicated two-row header, shared side-by-side BAR/LOOP strip, five 25-pixel lanes and full-width dial area. The 52-row scratch limit is preserved. The controlled host LCD fixture reported 2,032,760 RGB565 pixels versus 3,516,000 on the historic regression baseline, with 127 meter-only transfers. This is a host transfer measurement, not FM-1 audio timing.
- **Gestures:** HOME supports the two timed guarded commands. A track clear uses the existing undo snapshot. Song reset stops the working transport, clears musical data, stale drum locks and the step clipboard, and restores default song/FX/mixer values at the audio boundary. Saved files and device preferences are outside the reset. Host fixtures cover early release, both chord orders, successful actions, and the two-modifier interlock.
- **Animation:** Sound Lab and FX collect waveform samples only where the visible graph uses them. Dancer now uses real milliseconds for its fade and knob readout. The four readouts correspond to REACT, GROOVE, SWAY and LIGHT.
- **Research:** The [SHRUB v1.39 guide](https://shrub.audio/guide/) documents the split between on-device controls, web editor and USB installer. Its publicly documented design offers an interaction reference. The source-level DSP comparison remains unavailable. The previous comparative firmware record still applies.

### Release gates still open

The HOME comparison and knob captures show the current build against the approved layout. Exact visual matching, uniform all-page dial geometry, and comparison against the six original handoff reference PNGs remain incomplete. Remaining sound/feature work includes predictive DSP/ATS/ASP, drum dynamics and broader compatibility. The next consolidated physical test is held until the release software scope is closed. No public candidate page or promotion has been deployed. The historical archive and current Pages revision remain as previously recorded.

[The exact-source publication preview](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45960084724/artifacts/15180786498) passed the desktop/mobile layout, manifest, firmware checksum, gallery and Studio audits. [Engineering package and captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45960084692/artifacts/15180995753). [Native LCD and host regressions](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45959918221) and [exact application-tree Studio regressions](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45960084785) passed. The final chord interlock is explicitly covered by the native qualification's local host build.

[The pinned benchmark on the preceding reset checkpoint](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45959918271) reported callback CPU P99 of 178.68 µs RX, 240.11 µs TX and 180.90 µs bidirectional, versus 179.16, 239.50 and 180.20 µs in its v0.1.47 comparison. These mixed results cannot establish a physical improvement.
