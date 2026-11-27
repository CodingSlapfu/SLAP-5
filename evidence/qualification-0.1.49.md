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

## HOME geometry and measured worker budget checkpoint

**Application:** `b1eb81f34fb8d3d1d340009b5dee7db4f3297ba9`, identity `FM-1_75000149`. [Native package and captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965065136/artifacts/15182238906). Firmware SHA-256 `355749f6e7095408aac69f7fdbb205efdcf232552b4c77d32e52cf3dce8840a5`. Engineering backup SHA-256 `36dd0040a4dbfc22045f946f416569394f774f65fec96098e8acbdb8e35e95d9`.

- HOME now uses a fixed-width title, adjacent vertical gauge and brighter lane gradients. An actual side-by-side renderer inspection showed the header and lane palette are closer. The regional mean absolute RGB channel delta moved 60.09 → 53.16 for the header and 81.52 → 60.63 for the lanes. Progress and rotary differences remain substantial. The reference uses illustrative names, note positions, and playhead state. [Framebuffer inspection](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965065121).
- All four dial themes share the same precomputed arc and a position marker independent of tick spacing. Long values keep a ring and move to the edge of the control rather than becoming sliders. Some compact pages still use a smaller radius. Review of the six original PNGs and the complete page-by-page audit remain open.
- Core 1 dispatch uses a peak-biased observed ticks-per-weighted-frame model to cap workload before the last quarter of the callback. The voice limit and fallback ownership remain as before. Per-sample scope and mixer bookkeeping was reduced. Automatic leveling retains more of hot drum transients; manual level response remains unchanged. Generalized ATS/ASP and the drum balance acceptance pass remain open.
- [Same-run host DSP comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45964734685) used the preceding verified v0.1.49 as baseline. RX P99 160.00 → 153.76 µs, TX 165.85 → 163.00 µs, bidirectional 156.95 → 157.00 µs. Bidirectional peak 179.62 → 211.46 µs. No FM-1 timing or audio acceptance is implied.
- Native application 577,536 B of 581,564 B, RAM 88,088 B of 98,304 B, pool 335,208 B of 344,064 B. Application flash fell 176 B from the prior verified checkpoint and remains 680 B above v0.1.48.
- [Host LCD, recording, and worker regressions](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965027077), [16/32/64-step transport and progress captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965330712), and [current-only site and Studio staging](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965065188) passed. Native rollback and interrupted-update simulations passed. Pages remains at `efa956451c0fb77bc730b4e72320b678179b4bdd`. The existing public installer and historical archive stay available. The direct candidate installer and next owner flash remain gated on the open software/design acceptance items.

## Current checkpoint — silent-track fast path

Application `544340e649f3024d462509aac3ff5a1ecb3222c4`. [Native qualification and package](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965939328/artifacts/15182802098). Firmware SHA-256 `88c02ea29801d96d35cdac442bf6f9ddf78f8c603f7ab1744ebb6782b2e71b93`. Backup SHA-256 `058d0119eefbff50156c3187c71cb7086543a747cba9c774a564a15df278964a`.

The mixer skips per-sample work on tracks with no source, running effects, filter, scope or pending gain/duck response. Meter and envelope decay advance once per long silent span. Short spans with residual decay use the original mixer path. [Host activity, MIDI, PCM and worker regressions](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965912648) passed. Image 577,632 B, static RAM 88,088 B, pool 335,208 B. That is 80 application bytes below the incoming v0.1.49 checkpoint.

[Same-run pinned host benchmark](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965912691) used the incoming verified v0.1.49 as baseline. Callback CPU P99 in µs: RX 180.54 → 179.18, TX 239.63 → 238.90, bidirectional 180.51 → 180.13. TX peak 261.36 → 290.50 µs. The preceding all-span decay experiment showed TX P99 240.60 → 246.95 µs; the final implementation restricts the gain-settled residual-decay shortcut to spans of at least 32 samples. Independent runs cannot attribute all timing variance to the change. Physical DSP acceptance remains open.

The [exact current-only staged website and Studio](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45965939619/artifacts/15183505129) passed Chromium desktop/mobile, manifest, checksum, image and Studio tests. The existing public installer remains active. The next owner flash and separate candidate installer are held while visual matching and software acceptance are incomplete.
