# SLAP-5 0.1.26 corrective candidate

Source checkpoint for the candidate is `d20625dc89b60fb122d1a1cd893f8502cf49bc46`. The native package and matching Studio and emulator are backed up in `engineering/SLAP-5-0.1.26-candidate.tar.xz`. This is an engineering candidate.

[Host framebuffer captures](graphics/sound-lab-0126.png) show the five-lane HOME and direct EDIT, ENV and LFO entry points. HOME uses compact number buttons, bold patch names, mixer-level meters and white highlights for sounding sequenced notes. The step editor has pitch names and clean grid lines. Digital VU shows the global step within a bar. Black-key Live FX use fixed settings. Supported acid and saw leads glide on overlapping manual notes in single-note mode.

The native build for `FM-1_75000126` used 555,600 application bytes, 87,288 static RAM bytes and 328,648 pool bytes. The pinned package audit and loader simulations passed. Host script and dual-core fallback tests passed. The compiled WebAssembly passed patch, kit, arrangement and recording regression checks.

## Patch-switch CPU comparison

Two desktop runs per case, comparing 0.1.25 with the candidate while changing saw, FM6 and wavetable presets during a dense recording. Numbers are host callback P99 and peak in microseconds.

| BPM | 0.1.25 worker P99 / peak | Candidate worker P99 / peak |
| --- | --- | --- |
| 112 | 241.82 / 923.51 | 233.66 / 730.22 |
| 137 | 199.30 / 694.45 | 217.93 / 442.33 |
| 160 | 264.60 / 838.59 | 244.99 / 630.87 |

Wavetable frame data moved into internal RAM. The change removes its flash reads during interpolation. This addresses a plausible source of stalls. The sparse host timing does not identify the physical FM-1 slowdown or prove real-time stability. Worker P99 rose at 137 BPM, and single-thread P99 rose at 160 BPM. LCD tearing and physical XRUNs still need physical validation.

The firmware-wide typography audit, full FM6 page and visualization coverage, and genuine full-resolution Dancer replacement remain open. The public installers continue serving their prior firmware.
