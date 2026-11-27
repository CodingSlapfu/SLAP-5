# v0.1.53 qualification and next build

Physical saved projects and the latest diagnostic screenshots were unavailable. VHS SAW with SLICER and DEEP DUB with reverb/drive remain the priority physical reproduction. Capture GLOBAL, DISPATCH, CORE 1, SPIKE PROFILE and DMA TRACE at the recurring peak with the full arrangement, individual tracks and controlled FX bypass. Restore the intended settings before acceptance. The sprint does not claim a proven bottleneck or a physical callback improvement.

ROLE GENERATOR stores editable 1/2/4-bar patterns on any melodic track, using track key/scale and an undo snapshot. Press SEQ twice while stopped. K1 role, K2 style, K3 bars, SEL generate or confirm replacement. Live generation was deferred to avoid adding bulk writes at the audio boundary. Next build: add staged revision-checked publishing with a measured callback budget, then test live overdub and save/load. Musical acceptance across all 22 presets is pending.

The seven-image ZIP was identified but not materialized for inspection. Dial rendering changes preserve the shared dimensions; native comparison against the actual seven images and FM-1 visual approval remain open. The green outline denotes USB power only; the dash stays until charge level is known.

Keep the existing primary release and archive until both projects pass. Require clean audio, productive Core 1, zero misses/XRUNs, and peak below 5,804 µs with headroom. Archive extension and current-only primary deployment remain gated.

## Published testing build

[Installer](https://codingslapfu.github.io/SLAP-5/flash-testing-0.1.53.html) · [Testing site, exact screenshots and Studio](https://codingslapfu.github.io/SLAP-5/testing-0.1.53/) · [Verified package and backup](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46054811836/artifacts/15225703063).
Application `cbde9b426af24a2ebec35f02516354c32cde2ff8`, identity `FM-1_75000153`. Firmware SHA-256 `b8141b03d288688a902867a4cb1747bbb00f0fe1890e0658a41b3c250cc4a9cd`; backup SHA-256 `6df60c08634df2b605aba4eabfd4f5966b2ca1761283759cb86e7f4de0e0d05d`. Native app 580,032 of 581,564 B, RAM 88,104 B, pool 335,208 B. Native package, host/LCD regressions, representative stopped generation and playback, prior-version interrupted-update/rollback simulation, and Studio staging passed. [Live checks](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46054811858) verified the exact deployed files at `843eba84fb25fc0de59607191ad8172548b499ce`.

The existing 155 BPM host benchmark uses a prior v0.1.49 baseline and unrelated MIDI fixture. Its branch P99s were 155.26, 184.68 and 179.60 µs for RX, TX and bidirectional operation, versus 153.73, 160.21 and 156.44 µs at that baseline. This cannot isolate the physical SLICER workload. The DSP audio path was preserved in this sprint. No causal diagnosis or DSP speedup is claimed. The next comparison needs the exact prior v0.1.52 and the saved VHS SAW / DEEP DUB arrangement on hardware.
