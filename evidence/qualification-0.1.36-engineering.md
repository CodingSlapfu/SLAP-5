# SLAP-5 v0.1.36 engineering acceptance

**Release hold remains in effect.** Owner-facing firmware is v0.1.31. Engineering source [1ee5463](https://github.com/CodingSlapfu/SLAP-5/commit/1ee54631cbc1c0f1e5a8a4c2734e29942bd27bc0), firmware `FM-1_75000136`, package SHA-256 `1b8f1edc80ad3ea092f7ef8c10dc6637a9e90566015155874f54618f61ad3b70`.

This report carries forward [consolidated acceptance](consolidated-acceptance.md), [MIDI integration](qualification-0.1.34-engineering.md) and [v0.1.35 qualification](qualification-0.1.35-engineering.md). No previous open work is closed by implication.

| Area | Status and evidence |
|---|---|
| MIDI clock and recovery | Software complete for tested bursts and tempo changes. Averaging across pulses drained together stops duplicate USB timestamps from biasing tempo. A fixture queues 12 pulses in a callback, then deliberately exceeds the four-step backlog and checks a counted drop without echo. A second fixture passes about three minutes, 1,536 steps, 112/137/160 BPM, timing jitter and zero drift in sequencer step count. Start/Continue/Stop, SPP, timeout and USB reconnect fixtures pass. External count-in and the full scheduling matrix remain open. Clock delivery is callback-quantized. |
| MIDI recording | Software complete for tested ownership. Held records now identify panel, USB or TRS independently of the note's current audio voice. A fixture records the same pitch from two inputs at different times, releases TRS first and verifies the USB duration remains open. On-disk format and one-event-per-cell constraints are preserved. Dense MIDI overdub permutations remain open. |
| MIDI output and control | Due sequencer Note Offs are serviced ahead of clock and transport, with Note Ons after transport. Existing backpressure/reconnect and co-owned note fixtures pass. The full outbound slot-saturation, mute, solo and tie permutations remain open. CC receive, sustain and supported bend retained. CC transmit remains unavailable. |
| Anbernic interoperability | Requires physical validation for the implemented wired workflow. RG35XX Pro must act as USB host, FM-1 as USB device, and the actual muOS/PocketTracker build must expose the ALSA raw MIDI endpoint. Verify port discovery and both clock directions on the paired hardware. |
| SLICER and FILTER | Software complete for tested SLICER transport paths. The new fixture distinguishes stopped silence from playing capture, verifies isolation when switching tracks, changes tempo, restarts capture and disables the effect. DSP replay/cursor tests and prior measured FILTER gain correspondence remain applicable. A full visual marker review remains open. |
| FM6, Wavetable and other engines | Endpoint fixtures pass. Saved legacy UI, all-engine DSP-to-graph correspondence and drum/kit screen extremes remain open. No factory sound or graph implementation changed here. |
| Typography, visualizers and HOME | All 136 patch and 81 kit names fit the measured fields. All seven visualizer frames match across four master levels and zero master is silent. Existing power, HOME and controls fixtures passed. Firmware-wide font and knob fitting and complete animation audit remain open. Historical Reactor captures remain preserved in the v0.1.33 archive. |
| Factory sound, LEDs and output | All 136 factory patches passed three-pitch distinct-signature synthesis across 25 engines in the legacy and modern modes. Existing glide, voicing and profile behavior retained. Full expressive playback/recording acceptance and LED/profile state permutations remain open. Physical brightness and output quality need hardware. |
| Resources and timing | Image 564,920 / 581,564 B, down 96 B from v0.1.35. Static RAM 88,424 / 98,304 B, up 16 B. Pool 335,568 / 344,064 B, leaving 8,496 B. The first two alternating host measurements with continuous MIDI input, active MIDI output, filters and visuals produced a worse 137 BPM tail. Native Core 1, jitter, XRUN and LCD latency remain physical tests and host timing variability remains a software investigation item. |
| Build, Studio and update | 39 host fixtures, clock soak, dual-core PCM, native build, synthesis/effects tests, Studio/WASM, browser OTA, interrupted-update and v0.1.31 rollback passed. App write window `[0x4000, 0x93000)`; tested protected flash, song and settings bytes preserved. The stable installer and public release are untouched. Publication documentation and Studio deployment await software qualification. |

## Physical pass, after software qualification

Check the exact handheld/PocketTracker build's ALSA endpoint, all five MIDI channels, both clock directions, Start/Stop/Continue, SPP, sustain and cable removal. Run dense 112/137/160 BPM playback and overdubbing with filters, Sound Lab, SLICER and visuals while measuring device jitter, XRUNs, Core 1 and LCD timing. Verify screens, LEDs, USB/battery states, output profiles and app-only update/rollback on compatible hardware.

## Timing comparison with v0.1.35

Exact-source synchronous inline-worker host CPU time, with a steady stream of incoming notes during the measured tail, sequencer TX/clock, all five tracks, active filters and a visualizer. Each cell shows median P95/P99/peak in microseconds across two alternating trials.

| BPM | v0.1.35 | v0.1.36 |
|---|---|---|
| 112 | 219 / 261 / 1685 | 171 / 305 / 1378 |
| 137 | 173 / 234 / 381 | 212 / 295 / 3945 |
| 160 | 161 / 214 / 9015 | 175 / 237 / 907 |

The 137 BPM repeat pinned the host process to one CPU and alternated order across three trials. Median P99 rose from 232 to 336 µs and median peak from 736 to 1303 µs. None of those six repeat trials crossed 5,805 µs. The baseline had an outlier over that interval in the first 160 BPM comparison. The worse 137 BPM host tail reproduced, and its attribution remains open. These host measurements do not qualify native callback deadlines.

[Hosted Chromium workflow](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45040102072). [Verified engineering archive](https://chatgpt.com/library?file_id=libfile_9f841ae0cfc88191ae382cdfafb9b38d&account_id=personal).
