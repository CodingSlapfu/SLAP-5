# SLAP-5 v0.1.34 wired MIDI engineering acceptance

**The consolidated release remains on hold. v0.1.31 remains the owner-facing experimental release.**

Source [4675afa](https://github.com/CodingSlapfu/SLAP-5/commit/4675afaf13d7807bd7b249c850ce8e8878747fd4). Native candidate `FM-1_75000134`, SHA-256 `f941be9bae52f92d331c3d1f12a8be14048090607c5d33af023c37a9925e708b`. This candidate is for engineering only.

The remaining v0.1.33 requirements carry forward from the [consolidated acceptance matrix](consolidated-acceptance.md) and the [FM6 and LED qualification update](qualification-0.1.33-engineering.md). In the table, **open software** means implementation or available verification is still outstanding. **Physical validation** applies to the hardware checks after software acceptance.

| Requirement | Status | Evidence / remaining work |
|---|---|---|
| Native USB and TRS roles | Physical validation | Reused FM-1's USB MIDI device endpoints and existing USB/TRS input rings. USB host and BLE remain disabled. TRS is input-only and the UART path accepts notes/clock and transport. No endpoint layout, loader or SysEx updater change. |
| Anbernic / tracker path | Physical validation | The current PocketTracker Linux/PortMaster source (`a7ca5ce`) uses ALSA raw MIDI and dynamically loads `libasound.so.2`. muOS lists RG35XX Pro support. These facts do not establish MIDI enumeration on the owner's exact build. A data/host USB port, working kernel USB MIDI driver, visible ALSA raw MIDI endpoint and tracker port selection are required. LGPT is a distinct application. |
| MIDI settings and diagnostics | Open software | MIDI, MIDI SYNC, MIDI ROUTE, MIDI TRACKS and MIDI MONITOR use existing SELECT/K1 layout. Defaults retain original channels and behavior. OFF disables regular musical MIDI while USB updater entry remains available. Monitor shows endpoint ready, clock reception status, RX/TX and app drops. Full layout/extreme-value review, numerical clock monitor and broader settings tests remain. |
| Receive notes and routing | Open software | Five consecutive channels starting at an editable base map to tracks 1–5. Existing Note On/Off velocity and recording state gates retained. Source-tracked all-notes-off and USB disconnect cleanup added. Sustain and pitch bend remain open. |
| Transmit notes | Open software | Per-track routing, asset note pitches, velocity, scheduled onset/gate, ratchets, offs before retrigger, mute/solo and stop cleanup. Fixed 24-slot queue; Note Ons drop on exhaustion and sent Note Offs retry. Host exercises disconnected/reconnected port and backpressure. Full polyphonic, per-track and queue-exhaustion matrices remain open. |
| Clock and transport | Open software | Internal 24-PPQN master and external six-pulse step progression, basic smoothed tempo estimate, Start/Continue/Stop, stopped USB SPP and external clock timeout. Host fixture exercises both clock directions, lock, seek, stop, continue, no clock echo, timeout. Wider cadence/burst, count-in, jitter and long-duration recording sweeps remain open. Clock delivery is callback-quantized. |
| CC | Open software | Optional CC IN maps 7 level, 10 pan, 74 filter low-pass amount and 71 resonance. CC 120/123 release source notes. Host checks CC74 and absence of direct echo. CC transmit, modulation and performance mappings beyond the listed ones remain open. |
| FM6 and Wavetable endpoint sweep | Complete for tested controls | `sound-graph-controls.txt` exercises all six operators, ratio/level, all-op ADSR extremes, feedback, and Morph/Scan/Decay/Drive on Lamp Wave. Fixed an all-zero FM operator config collision with the unedited-patch sentinel, retaining the intended quiet output. Saved legacy-project UI tests and full visual calibration remain open. |
| All-engine graph semantics | Open software | Preserved v0.1.33's 408-frame audit and new FM/Wavetable control evidence. Semantic review and drum, legacy and release-state graph coverage remain open. |
| FILTER and SLICER | Open software | v0.1.33 FILTER gain checks and effects suite preserve prior coverage. Full Slicer stopped/switching/capture/replay transport matrix remains open. |
| Font/knob fit, HOME, LEDs and profiles | Open software | HOME and MIXER mappings, Nexus and HIGH/MEDIUM retained. Host LED persistence and name widths checked. Global visual/endpoint coverage, battery/USB edges, dedicated LED state and speaker/headphone profile checks remain. |
| Factory patches | Open software | All 136 patches continue to produce distinct signatures at three pitches. Euphoria/Soft Detune and existing names preserved. Additional expressive software tuning and playback/recording transition matrix remain. |
| Signature Reactor | External preference | Current Reactor retained. The three historical captures from v0.1.33 remain in that archive. The owner's preferred one is unidentified and is optional for software qualification. |
| DSP, memory and graphics | Open software | Native image 563,884 / 581,564 B, static RAM 88,520 / 98,304 B, pool 335,568 / 344,064 B. Pool headroom 8,496 B. Fixed queues use static storage. Existing dual-core worker/single/fallback PCM suite passed. Native timing, jitter, output queue and LCD stress require hardware after the expanded software MIDI scenarios. |
| Studio, website and docs | Open software | Native-matching Studio/WASM builds and emulator regression passed. README describes implemented controls and connection paths. No next installer or Studio was deployed. Final docs and compatibility matrix require closed gates. |
| Flash and rollback | Physical validation of tested paths | App-only writes confined to `[0x4000, 0x93000)`. Pinned SDK audited. Simulated browser update, loader reconnect/disconnect, interruption and v0.1.31 rollback retained boot, settings and songs. The simulated loader does not exercise a physically running MIDI session during a Web MIDI update. |
| Publication | Open software | No installer, release or stable route changed. The MIDI and existing acceptance gaps above remain. |

## Reproducible acceptance evidence

34 host fixture files, the standalone synthesis/effects suites, dual-core PCM parity, native package audit, update and rollback simulations and WASM tests passed. The timing comparison uses one host CPU-time trial per conditional case. It does not establish native Core 1 behavior or reproducible tail improvement. Individual log files and 240×240 MIDI capture are in the engineering archive.

## Combined physical pass after software qualification

Connect a compatible FM-1 as a USB device to the Anbernic host port. Check tracker detection of the ALSA endpoint and record the muOS and PortMaster build IDs. Play each track from PocketTracker with velocity, release and overdub. Try both clock directions, stopped SPP, Start/Stop/Continue and disconnect/reconnect. Repeat dense 112/137/160 BPM playback with overlays, filters, Slicer and overdubbing while recording XRUNs, task counts and input/clock jitter. Verify USB updater and app-only rollback with MIDI settings enabled, then check LCD, LEDs, power states and speaker/headphone response.

### Additional verification

The MIDI settings, base channel and external sync behavior survived a simulated reboot. Dense playback with TX notes/clock/transport enabled measured P95/P99/peak of 166/211/388 µs at 112 BPM, 157/194/340 µs at 137 BPM, and 189/276/1106 µs at 160 BPM. The simultaneous v0.1.33 baseline values were 156/186/798 µs, 183/229/2160 µs and 175/391/2540 µs. Each row is one inline-worker host CPU run.

[Exact-source hosted Chromium passed](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45002770088). [Verified engineering archive](https://chatgpt.com/library?file_id=libfile_baa7649f94e88191bc89e9a1ed5c8ff8&account_id=personal).
