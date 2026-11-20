# SLAP-5 next TESTING release acceptance

Resume from `e8bd106cfce7298d5d6605b8c1de4c95d267202c`. Published rollback and live baseline: v0.1.7-testing. The baseline v0.1.8 candidate is unpublished. Preserve quick recording, REC hold, FX navigation, the drum worker, all GEN DRUMS styles and eight visualizers.

| Requirement | State | Source, verification and limits |
|---|---|---|
| A. Audio and polyphony | IN PROGRESS | `device/app.c` has repeat-note release and same-track steal decay. Prior host stress did not clip. Multi-patch mixed DSP testing and physical diagnosis remain. |
| B. Recording | IN PROGRESS | `device/app.c`, `device/tests/record.txt`, `device/tests/quick-record.txt`. Quick recording and hold passed previously. Longer takes, enhanced timing, migration and RECORD design remain. |
| C. Metronome | VERIFIED | Simulated restart and playback checks passed. See milestone below. |
| D. Typography | NOT STARTED | Refined shared Bold, Regular and Italic family and layout verification pending. |
| E. Color | IN PROGRESS | `device/app.c`, `device/visuals.h` have saturated track identities. Complete all-page and web rollout. |
| F. Drum kits | IN PROGRESS | `device/dsp/drum_kits.h` has 32 stable IDs and four base circuits. Distinct synthesis and listening evidence pending. |
| G. Synth patches | NOT STARTED | Review all 25 engines and 128 factory patches for balance, quality and repetition. |
| H. Studio and emulator | IN PROGRESS | `site/studio.js`, `site/project.mjs` integrate 32 kits. Full recording, generation, metronome, FX and project parity remain. |
| I. Website | NOT STARTED | v0.1.7 live installer is the rollback baseline. Deploy matching assets only when qualified. |
| J. Optimization | IN PROGRESS | `device/app.c`, `device/tests/dual-core.sh`. Prior simulated worker/fallback produced identical audio. Actual FM-1 timing remains final owner acceptance. |
| Release gates | IN PROGRESS | Prior v0.1.8 package audit passed for baseline only. Rebuild and audit the final tree, run all host/WASM/OTA tests, verify backups, publish and verify deployed hashes. |

Advance each row only on behavioral or measurable evidence. Continue independent software work without intermediate owner testing.

## Verified milestone after e8bd106

- **A — IN PROGRESS.** `device/dsp/synthesis.c` uses periodic phase modulation in the spectral engine to remove an oscillator-wrap step. `device/tests/emulator.mjs` rendered 128 auditions and a 200 BPM five-track arrangement with overlapping notes, reverb, delay, chorus and drive. Peaks 0.0057–0.0283 for single auditions and 0.2373 for the mix. Real FM-1 distortion cause remains undetermined.
- **B — IN PROGRESS.** `device/app.c`, `device/tests/record.txt`. ARP on the stopped RECORD page undoes the previous take, restores its length and event words, and refuses a restore if its pattern has changed since the take. Host regression verified restoration after replace. The undo is in RAM. The existing format has 64 steps, a 3,840-byte per-object payload limit, and two stored offset bits. Implement a versioned extended format before offering 8/16/32 bars or finer timing. Finish layout and MIDI/overlap coverage.
- **C — VERIFIED in software.** `device/app.c`, `device/app.h`, `device/tests/metronome.txt`. OFF, REC and ON survive simulated restart, ON produces an audible click without recording, and OFF leaves the HOME count-in independent. Preference saves after a 500 ms settle while stopped and no keys are held. The A/B pair at 0xD0000–0xD1FFF lies outside the released song/panel pairs, known AQUORA bank pairs and the OTA staging window. Physical restart acceptance pending.
- **F — IN PROGRESS.** `device/dsp/drum_kits.h`, `device/app.c`, `device/tests/emulator.mjs`. Stable IDs, plus per-kit noise coloration, resonance, transient and clap shaping. All 192 kit/lane auditions passed finite-value and level checks (peaks 0.0039–0.0155). Full listening and comparisons remain.
- **G — IN PROGRESS.** `device/tests/emulator.mjs` exercises all patch IDs for finite, audible and unclipped output at MIDI note 60. This provides baseline level evidence. Musical balance and repetition still require review.
- **H — IN PROGRESS.** `device/app.c`, `device/web/emu/app_worklet.js`, `site/studio.html`, `site/studio.js`, `site/project.mjs`, `device/tests/emulator.mjs`. Studio can edit per-track FX and metronome and round-trip them through WASM and project JSON. Older JSON gets defaults. GEN DRUMS and recording parity remain.
- **J — IN PROGRESS.** Ten host fixtures and deterministic worker/single/fallback WAV comparison passed. Native report: 48,472 bytes of 98,304 bytes for data and BSS, 210,244 of 344,064 bytes of pool, and 149,328 of 581,564 bytes of application slot. Neither host simulation nor map size measures device timing.
- **Release — IN PROGRESS.** Audited candidate `0.1.9` / `FM-1_75000109`, SHA-256 `d030cc213f4ea4b57d9aa4727ab96f9d592598a3a9ccca5b5078411c2ee1577f`. `engineering/SLAP-5-0.1.9-candidate.tar.xz` contains the exact native assets and checksums. Simulated package and transfer-request validation passed. This archive is a backup and remains outside release workflows. The live installer remains on v0.1.7.

Resume from the commit containing this milestone. Continue with versioned extended takes and Studio parity, then finish typography, all-page color, sound review and profiling. Rebuild and rerun tests affected by changes. Publish only after the full gate; reserve physical restart, load and listening checks for owner acceptance of the qualified release.

## Follow-up verification

- The standalone synthesis suite rendered all 128 patches across 25 engines at three pitches, with finite audio, measurable energy and unique sample signatures. The standalone FX suite verified five simultaneous effect chains across all seven inserts.
- The first milestone passed GitHub's Chromium Studio workflow (run 44616065257). On the local runner Chromium could not start because the process-singleton socket was denied with EPERM. Use the GitHub browser workflow for this runner limitation.
- **B — IN PROGRESS.** RECORD now shows bar/step, a segmented beat grid, take-local note count and an audio activity meter. Existing quick record, settings hold and undo host scenarios passed. Longer takes, finer timing and complete layout remain.
- **H — IN PROGRESS.** Studio's Track 5 generator calls the native DSP's 16-style generator through WASM, preserves ordinary lane editability, requires confirmation before replacement, and supports project undo. The WASM suite verifies all 16 styles produce distinct valid patterns and confirms playback blocks generation. Chromium coverage added for generator confirmation, replacement and FX isolation. Full recording parity remains.
- All ten host fixtures and worker/fallback equality passed after the RECORD screen change. WASM and existing rollback OTA simulation passed. The 0.1.9 archive is the audited previous milestone backup. Build final packages from the current source after completing the remaining acceptance rows.

## 0.1.11 software gate

- **Recording and migration — VERIFIED in software.** Native fixtures captured 128, 256 and 512 steps, saved and reloaded a note past step 400 with offset 13/16, reopened a legacy bank, and preserved a save when a dense V5 bank exceeded capacity. V5 stores 587 events per bank. Melodic recording uses two editable notes per step and one RAM undo.
- **Studio — VERIFIED in WASM.** JSON v3 carries 512 steps and explicit offsets with legacy import. Studio on-screen keys and Web MIDI feed the firmware recorder. The suite passed a two-note 32-bar take and stopped Quick REC without stopping playback.
- **Typography and color — implemented and checked in host output.** Rounded SLAP Block Bold, Regular and Italic ship via generated firmware glyphs and web fonts. Track-colored surfaces, separators and meters applied across native panels. Physical LCD acceptance remains.
- **Sound and optimization — software measurements passed.** Three-register sweep and gain trims for 12 outliers. All 128 patch signatures and 192 kit/lane auditions passed. Dense effects peaked at 0.2608 full scale. Five-chain benchmark median fell from 1.094 s to 0.979 s. Worker-disabled, worker-enabled and discarded-worker WAVs match. Physical performance and listening remain.
- **Release — native and automated OTA passed.** Candidate identity `FM-1_75000111`, package SHA-256 `ea1357f1b8a9030d2cea776506c461e7687ab3d0ec5b170ced5ab0d29d92074c`. Publication workflow waits for Chromium to pass against the exact tagged source. Preserve v0.1.7 for rollback.

## Published 0.1.11 TESTING

- **Release and deployment — VERIFIED.** Release source `e8172330923058b7e4f8322d5d9e7a972ee29d0c`. GitHub Chromium run 44619668838 and release run 44619896505 succeeded. GitHub publishes the verified package as v0.1.11-testing. The matching Studio, emulator, fonts and installer deployed to the existing site. The live /flash page and manifest select `FM-1_75000111`; a fresh live firmware download matched the GitHub asset SHA-256.
- v0.1.7 remains available for rollback. Hardware listening, actual callback timing and physical LCD acceptance remain owner checks. Use a spare bank for V5 recording tests so the prior release can still load the older banks.

## v0.1.12 engineering TESTING publication

- Source tag commit 4027d816cdeceee171cc3bd72dd7c7669b835628. Audited native backup a1dfdaecae73bf8b585aebc6d56ba85f35b6b4f8. The fetched backup matched the local archive and passed all three SHA-256 checks.
- GitHub Actions Chromium run 44628603559 and publication run 44629111156 succeeded. Package FM-1_75000112 is 611,488 bytes with SHA-256 fde204437fcbb6fa1bd855e0e78cad8eae0ea69aded2db0100cb6233b148b297.
- Site version 8 from source 131e4af9f02d10255133dcb87667cb56c55f7165 served the v0.1.12 manifest, direct installer and matching versioned emulator. The live installer FWSC and GitHub asset were byte-identical. The live WASM matched the staged versioned binary.
- Native audit reported a 152,132-byte image, 48,132 / 98,304 bytes of .data+.bss and 268,928 / 344,064 bytes of pool RAM. Host scripted regressions, dual-core output parity and state rollback, effects, name uniqueness, Studio keyboard/recording/project/generator checks and simulated OTA passed.
- A baseline-to-candidate host slicer comparison produced byte-identical samples over five simultaneous tracks and changing rates and BPM. A short single-thread host kernel timing probe had variable tails and did not establish a speedup. The native worker implementation and the tested fallbacks are detailed in dual-core-fx.md. Hardware timing, underruns, kit listening and final LCD acceptance remain open. Use the existing test procedure once on the published TESTING build.
- [GitHub release](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.12-testing) and [direct installer](https://slap-5.jminie66.chatgpt.site/flash). Both v0.1.11 and v0.1.7 rollback tags were verified.

## v0.1.13 engineering TESTING publication

- v0.1.12 physical report at 112 BPM: 58% average load, 3,391 µs average, 6,646 µs peak against a 5,804 µs budget, 425 deadline misses and approximately 445 DMA XRUNs. Audio degraded after drum recording with FX off. Cumulative counts did not isolate the failing phase.
- Corrective source and verified binary backup: 07f482e8b561e5a84e367d3382492ae505f556d6. End-of-take hashing was replaced with pattern revision tracking. A normal short take copies only its recordable range at start. The bounded start snapshot still runs on the callback for a 32-bar take.
- Voice batches can now assign drum-only, synth-only or mixed work to Core 1. String delay-line engines and shared-RNG sample-and-hold LFO voices remain local. Separate worker DSP state preserves voice order and makes ordinary late voice results discardable without immediately retiring Core 1. FX retains the earlier track-state rollback and retire-on-late behavior.
- Host no-FX post-recording playback for all 32 kits, synth-only chords, mixed voices and five-track FX passed PCM parity. Host restart/save, 32-bar takes, undo, generator, visual and sequencing scripts passed. The host mailbox simulator executes synchronously. The native board still needs deadline, worker and DMA measurements.
- Native image is 153,200 bytes. RAM uses 52,532 / 98,304 bytes of .data+.bss and 287,652 / 344,064 bytes of pool. Native package reconstruction, WebAssembly, Studio, simulated OTA from v0.1.12, exact-checkpoint Chromium run 44660917180 and publication run 44661167126 passed.
- GitHub and the live installer serve byte-identical firmware FM-1_75000113 with SHA-256 aa2a2fa68666390fc6b1d6b49d0d988084ab445a1d5ca6d7f0e97269a62800d4. The matching emulator, Studio and installer deployed in Site version 9. Both v0.1.11 and v0.1.12 rollback tags and assets were retained.
- Large SLAP BLOCK retained. The Barlow companion face is used for small LCD text. All 128 patch names and 32 kit names fit the measured HOME layout. Studio reads both name lists from WASM. The next unresolved requirements are FM-1 timing results, measured load-based partition tuning, a stage-level timing breakdown and listening acceptance of the inherited 32 kits.
- On hardware, turn K3 on GLOBAL to zero the measurement window. Use the same pattern and BPM. Read peak, P95/P99, misses, XRUNs and fallbacks separately before recording, across take completion and during sustained no-FX replay. Repeat on dense five-track playback with slicers and effects.
- [GitHub release](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.13-testing) and [direct installer](https://slap-5.jminie66.chatgpt.site/flash).

## Consolidated Red Alert and recording acceptance for v0.1.14

This table is the current acceptance record for both owner work orders.
Earlier sections describe the released baselines and historical milestones.
v0.1.13 failed on the physical FM-1 at 112 BPM after dense five-track recording:
107% load, 8,630 µs peak, 5,400 µs P95, 6,200 µs P99, 5,804 µs budget,
1,092 cumulative deadline misses, 1,113 cumulative XRUNs and 46 worker fallbacks.

| Requirement | Status | Evidence or remaining acceptance |
|---|---|---|
| Preserve ongoing work, patch/kit IDs, controls, releases and banks | Verified in software | Resume base fae1422. Source and package are pinned together on the single v0.1.14 backup branch. Existing host controls, save/load, graphics, GEN DRUMS and all 128 patch and 32 kit auditions passed. |
| AQUORA and SLOOP architecture comparison | Reviewed | AQUORA's experimental FM worker freezes independent inputs and uses SRAM oscillators; its display frame policy is separate. SLOOP renders independent local buses before track FX and master mixing, and feeds its foreground visualizer through compact telemetry. SLAP-5 uses original estimated-cost allocation, an SRAM sine table and a bounded spectrum queue. See the repository references in evidence/dual-core-fx.md. |
| Audio scheduling and rendering | Verified for tested sequences | device/app.c. An eligible batch previously crossed a sequencer boundary when the countdown was zero. The seeded 144 BPM 32-bar fixture differed at 34,841 PCM samples before the correction and matched byte-for-byte afterward. The renderer compacts active slots and batches independent local voices, keeping sample/slot order for shared random modulation. |
| Dual-core DSP, workload allocation, waits and fallback | Partially verified | Estimated engine costs choose worker lanes. Private state, ordered mixing, existing bounded wait and recomputation remain in use. Host worker/single comparisons and forced result rejection passed. Actual Core 0/Core 1 overlap, worst fallback cost and deadline behavior require FM-1 timing; string delay lines and shared-RNG modulation stay local. |
| Voice engine and overload protection | Partial | Active voice list, SRAM interpolated oscillator, sustained-event retrigger guard and capacity reporting are implemented. Existing 16-voice capacity and fallback policies remain. Further voice-steal and overload tuning requires device traces. |
| FX, slicers and shared buses | Partial | Existing concurrent per-track FX, five-slicer stress and independent controls are preserved. Fully inactive chains bypass per-sample FX outside inspection pages. Common-bus redesign and live tail tuning remain open because the current wet paths have independent feedback state and no comparable on-device profiling data. |
| Graphics and visual scheduling | Partial | The callback sends decimated samples to a bounded queue; foreground processing performs spectral filtering. Existing load-based frame pacing and all eight visualizers remain. Exact LCD hold time, dirty-region work and adaptive P99/XRUN tuning remain open on hardware. |
| GLOBAL measurement windows | Verified in software | K3 resets the window. GLO again shows the worst callback's voice-batch wall time, FX-batch wall time, successful Core 1 work, wait and unclassified remainder. C1 work and wait overlap the other figures; the remainder includes direct synthesis, sequencing, individual FX, mix and telemetry. |
| Polyphonic recording and capacity | Verified within documented limits | Four per-step melodic lanes. Fifth note reports capacity and preserves the first four. device/tests/poly-record.txt verifies save/reload and four triggers. A V7 bank can store at most 440 sparse events; over-capacity saves report BANK TOO DENSE and retain the active song. |
| Held notes and intentional note-off | Verified in software | Independent elapsed sample intervals at 128-sample resolution. device/tests/held-recording.txt preserved a 2.002 s hold through V7 save/reload and the voice remained held on replay. Replace/undo restored the same interval. Supplemental save/reload cases preserved 499 ms, 2,142 ms and 4,284 ms holds. device/tests/wrapped-hold.txt kept a 4.702 s note held across two wraps without a duplicate trigger. |
| Quantization, timing and expression | Verified within supported resolution | Default 50%. Off, 25%, 50%, 75% and 100% plus straight 1/8, 1/16 and 1/32 grids. The WASM test produced five different onsets for the same ~0.5 s hold. Onsets use sixteenth-step subdivisions; triplet grids and finer timestamp capture remain open. |
| Migration, Studio and emulator | Verified in software | Native V2–V6 reader retained. V7 stores explicit held durations; v0.1.13 cannot reopen V6/V7 saves. Studio imports JSON V1–V4 and uses v5 for explicit holds. Four-note 32-bar capture, old project migration, long-note length rendering and Hold ms edit are integrated. |
| Heavy randomized recording and replay | Verified for listed host workloads | device/tests/dense-recording.py. Worker and single-core audio matched at 112 BPM with no FX, 137 BPM with five slicers, wet FX and spectrum, and for 16- and 32-bar scenarios at 112 and 144 BPM. Seed 78893 is the step-boundary regression. Host execution is synchronous and does not establish native timing. |
| Callback improvements, zero deadline misses, zero XRUNs and physical listening | Blocked on access to an FM-1 | Host WASM with the same preloaded dense five-track pattern and 10,000 blocks per pass had median-of-three mean 138.89 to 77.31 µs and P99 535.15 to 396.73 µs. These are x86 host results. They do not resolve the owner's device baseline. |
| Native audit, backup, browser and OTA | Verified | Audited app 152,580 bytes, .data+.bss 58,932 / 98,304 and pool 328,612 / 344,064 bytes. Firmware 8cf3240a239e92eee2b7fef2c9ef5fec6803a3444ebb035faf5e422484bb2689. The fetched archive and all three assets matched. Exact candidate Chromium run 44666852284 passed. Simulated v0.1.13 OTA and rollback-path rejection passed. |
| Release publication and direct installer | Verified | v0.1.14 TESTING tagged at 6a402b176ebf14949d13035fd21a8fea96902934. Publication run 44667212052 passed. Site version 10 deployed source 160eef9af7a4aef4d591fe35a6b3a3c41df5da85. Live manifest, FWSC, Studio HTML and WASM matched the verified builds. v0.1.13 remains available for rollback. |

The FM-1 acceptance run should start with a spare bank at 112 BPM, K3 reset
on GLOBAL, then record sustained chords and dense input on all five tracks
with FX off. Capture both GLOBAL pages after the take and after replay.
Repeat with five slicers, wet FX, and the spectrum visualizer. Compare
peak/P99, deadline misses, XRUNs, fallbacks and the peak breakdown against
the v0.1.13 baseline. Listen for full held lengths and a clean release.

Host reproduction: build the host target, run each device/tests/*.txt,
device/tests/dual-core.sh, and device/tests/dense-recording.py using
--bars 16 and --bars 32 --bpm 144 --seed 78893. Run the 137 BPM wet/visual
case with --fx --visual --seed 220507. The benchmark script is in
evidence/slap14-host-profile.mjs and accepts a built emulator directory and
a short label. Run the same Node runtime and fixture for both release builds.
