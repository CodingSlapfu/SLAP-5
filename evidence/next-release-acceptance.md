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
