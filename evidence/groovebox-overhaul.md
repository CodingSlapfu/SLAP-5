# Groovebox overhaul progress

Baseline [38d5f7a](https://github.com/CodingSlapfu/SLAP-5/commit/38d5f7ab382a21e1ee526c56599f788c3643044c).
The owner brief calls for 128 distinctive patches, 25 sample-backed factory kits with 16 sounds each, legacy handling for all 56 prior kit IDs, 16 drum lanes, HOME and recording changes, black-key LIVE FX, harmony controls, settings and eight visualizers.

## Baseline and sample budget

The v0.1.20 package audit gives 581,564 bytes for the application slot and 162,832 bytes for the app, leaving 418,732 bytes of arithmetic slot headroom before new code, alignment or data residency constraints. This subtraction is an upper bound, not a measured sample budget. At 16 kHz, four-bit mono ADPCM consumes approximately 8,000 bytes per second before headers. Spending the whole upper bound on 400 unique recordings would average about 131 ms per slot. Shared immutable recordings, varying one-shot lengths and a compact index warrant evaluation. The updater write region alone does not demonstrate that samples can be accessed by both cores at the required bandwidth or that linked sample data avoids RAM residency.

Reference static RAM is 56,840/98,304 bytes and pool usage is 312,420/344,064 bytes. The v0.1.20 hardware run reached 16 voices at 137 BPM and recorded a 5,178 µs peak with zero misses, XRUNs or fallbacks. No new hardware timing or native memory figures are available for this checkpoint.

## Implemented and exercised

- Added the host audition runner `device/tests/audition-patches.py`. It selects and checks all 128 IDs on the firmware host and records MIDI input at three pitch and velocity combinations followed by a held chord. It writes unmodified WAVs and an index of peak and RMS levels for matching in a separate listening comparison. All 128 baseline files had nonzero signal. Peaks ranged from −39.81 to −29.24 dBFS and RMS from −59.52 to −44.34 dBFS in this fixture. These measurements establish signal and level ranges. Subjective listening and similarity analysis remain open.
- Renamed SPECK INK to GRAIN BLOOM with its ID and parameters intact.
- HOME and the sequencer draw held durations from the stored recording field, using the full stored timing offset and carrying visible tails across loop boundaries. The display uses the nominal tempo grid and does not estimate per-step swing. The white playhead accent follows the stored note interval and can appear on a probabilistic event even when playback skips that event.
- Selecting a new track from HOME ends the old take through the existing audio-side finalization, while preserving running playback. The extended quick-record regression changes tracks during a held note, confirms the take ends and records again on the new track.
- The harmony header now says KEY & CHORDS. Existing key, scale, chord and strum behavior is preserved.

Desktop build and `held-recording`, `wrapped-hold`, `poly-record`, `quick-record` and `pages` passed. Native build, image-level display verification and full release qualification remain open.

## Remaining acceptance

Redesign and audition the patch library. Implement sample decoding, assign all 400 slots, source acoustic recordings with verified provenance, migrate legacy kit references and expand the sequencer to 16 lanes. Complete transport preferences, playback-safe undo, confirmed clear and cancel controls. Implement black-key LIVE FX and independent Perfect Note controls. Build settings and its persistence. Redesign and benchmark all eight visualizers. Complete the firmware-wide audit, native/WASM and storage regressions, memory report and software release qualification. Physical performance remains unverified. Keep the existing public installers.

## Quick REC regression and native build recovery

The regression at `emulator.mjs:207` came from the physical control poll canceling a take whenever the UI's current track differed from the Studio recording target. The poll now requires an actual track selection change before it ends a take. The original Studio WASM Quick REC assertion passed, and the physical-gesture host fixture still ends a take when SELECT moves to another track.

KEY & CHORDS now uses OCT− for a per-track Perfect Note switch (on by default) and OCT+ to cycle OFF, TRIAD, SEVENTH and FIFTH. With correction off, a chord starts from the played chromatic pitch and retains the intervals derived from the selected scale. Outside that page the OCT controls transpose as before. The switch is session-local; persistent keyboard defaults remain part of SETTINGS. Host tests covered the controls, recording, wrapped holds, arpeggiator and corrective UI fixtures.

Native build recovered by fetching `uboot.boot` and `cfg/eq_cfg_hw.bin` from Gitee at the exact SDK commit specified in [SDK pinning](sdk-pinning.md), and reusing the cached `cfg_tool.bin` that already matched. All three checksums passed with no changes to the manifest. The JieLi compiler built the development package `FM-1_7500` and the package audit regenerated it byte-for-byte. Application size was 163,588 bytes (+756 versus published v0.1.20), static RAM 56,840 bytes and pool 312,420 bytes. This is a build check, with no physical performance claim. The full overhaul and qualified release remain open.

## Recording actions

Hosted browser workflow 44824585526 passed at b6dace0 (owner-confirmed). This milestone changes the firmware controls and merits a new hosted verification run.

Hold REC to open RECORD. While stopped, OCT− twice clears the active loop and OCT+ twice clears the entire track’s stored steps, including steps outside the current loop. ARP undoes the last clear or take while stopped. During recording, ARP cancels and restores the pre-take snapshot after finalization, stopping the transport. Quick REC honors the RECORD mode set by K4. The workflow reuses the bounded single snapshot. Playback-continuing undo and cancel remain open.

Host `record-actions`, `quick-record`, `record` and `held-recording` passed. All pinned native inputs passed. Native application size was 164,348 bytes, static RAM 56,840 bytes and pool 312,420 bytes. Remaining sound, graphics, settings and release qualification are unchanged.

## Factory voicing pass

The [previous recording checkpoint](https://github.com/CodingSlapfu/SLAP-5/commit/c65cbc5d8da5ea03ca29372ba578b4b684f6778c) has owner-confirmed hosted browser run 44825443101.

A first factory voicing pass changed 32 presets across six engines: operator FM (ratio sets and modulation decay), analog bass (sub mix and a separate transient filter), electric piano (tine ratios and hammer decay), acoustic piano (detuned partials and hammer contribution), choir (moving formants and detuned vocal layer) and woodwind (different reed widths, bore filtering and breath). Existing patch IDs and engine IDs remain stable. Existing legacy oscillator branches are preserved. The stored factory parameter changes can still change the timbre of older songs that reference these IDs. Modern bass and choir carry increased worker cost estimates.

The standard host audition fixture rendered all 128 presets before and after. Exactly 32 PCM renders changed. The other 96 matched byte-for-byte. The [comparison index](patch-audition-comparison.csv) gives peak and RMS levels. It is evidence of changed output and gain structure, not a subjective quality sign-off. Recreate playable WAVs with `python3 device/tests/audition-patches.py device/build/host/slap-5_host OUTPUT` using before and after builds. The isolated synthesis suite passed all 25 engines and 128 presets in both synthesis modes. The checksum-verified native build passed with 166,280 bytes of application flash (+1,932 from c65cbc5), 56,840 bytes of static RAM and 312,420 bytes of pool RAM.

The remaining patch families and level-matched listening review are still open, as are the 25 sample-backed kits, 16 drum lanes, LIVE FX, SETTINGS, visualizer work and full release qualification.

## Sample-backed factory drum bank

The hosted browser run 44826392134 passed at c1760175 according to the owner. The bank changes Studio compatibility, so the new checkpoint requires its own browser result.

`device/tools/drum_samples.py` generates 400 original synthetic one-shots at build time under the repository's MPL-2.0 terms. There are no third-party assets or recorded acoustic sources in this candidate. The acoustic-style names describe intended voicings. Convincing acoustic quality and level-matched listening have yet to be established.

The 25 factory kits occupy IDs 56–80. Old IDs 0–55 retain their original synthesis and project references and appear in Studio when an imported project uses them. Older firmware cannot load projects saved with the new kit IDs. New kits map the 16 physical white keys to the 16 one-shots and accept corresponding GM notes. Closed, open and pedal hats share a short fade-out choke group. The decoder uses quarter-rate IMA ADPCM with linear interpolation and per-voice state, and reads an immutable XIP bank. It stays on Core 0 pending physical flash-bandwidth measurements. The sequencer still has six dedicated drum lanes and the existing general drum row. Live audition verifies 16 hits; dedicated recording and sequencing of all 16 are still open.

All 400 new assignments and all 56 legacy kits passed finite, nonzero and unclipped WASM audition checks. The rapid-hit WASM test covered all 81 IDs at 240 BPM. The existing host drum take and generator fixtures passed. This measures functionality, not sound quality or hardware performance.

Native compilation with pinned SDK inputs and exact package regeneration passed. The bank uses 309,343 compressed payload bytes. Application 480,208 bytes leaves 101,356 of the 581,564-byte package application slot. Static RAM rose 288 bytes to 57,128, with pool unchanged at 312,420 bytes. Verified update window 0x4000–0x92FFF inclusive. No physical flash, timing or audio acceptance was performed.

Remaining drum gates: 16 independently editable and recordable lanes, generator updates, on-device XIP/audio performance, and musical comparison, particularly the acoustic-style kits. The rest of the patch review, LIVE FX, SETTINGS, visualizers and full release qualification remain open.

## Sixteen drum sequencer lanes

The sequencer now has 16 fixed-pitch drum lanes plus its existing general drum row. Hardware step editing, MIDI/keyboard recording, velocity, probability, timing, swing and per-lane mute/solo use the expanded lane table. GEN DRUMS keeps its original six voices and adds restrained variation and fills on the new lanes when a sample kit is selected. Existing hat choke behavior is preserved. Studio uses a 16-row drum grid.

Native sparse format V8 appends the ten new drum rows after the old V7 rows, with the timing group stored separately from the row index. Six-lane native formats V2–V7 retain their existing on-disk layout and filter mask interpretation. Studio JSON V6 expands to 16 drum lanes and migrates V1–V5, including remapping the solo bits. Older firmware or Studio cannot load the new expanded format. Dense native banks remain subject to the existing song-object limit and are rejected before overwriting a slot.

The browser regression test now verifies 25 factory choices on a new project, a unique legacy choice alongside all 25 factory IDs after import, and preservation of legacy ID 31 in the subsequent export. A new hosted result is pending. Local WASM passed its patch, sample assignment, rapid triggering, recording and generator tests. The native host lane-16 fixture passed edit, save, clear, reload and playback. The pinned native build and byte-for-byte package audit passed: app 481,056 bytes, static RAM 89,896/98,304, pool 330,852/344,064. Hardware sample throughput and the tight RAM margins still need FM-1 validation. Full musical acceptance and experimental release remain open.

## Undo footprint, momentary LIVE FX and preferences

The owner confirmed hosted browser run 44828481528 passed at b892e30. The undo snapshot now shares storage between drum and melodic takes and packs two timing groups per snapshot byte. Live sequencer arrays and on-flash song formats retain their layouts. Pool usage decreased by 8,440 bytes net to 322,412 / 344,064. Static RAM is 89,976 / 98,304 and application size is 481,996 / 581,564. Physical stack high-water and XIP deadlines remain unmeasured.

Double-press FX to enter LIVE FX, then hold a black key for a momentary overlay on the selected track. The first nine black keys map to gate, stutter, beat repeat, filter, delay throw, reverb bloom, bitcrush, pitch modulation and glitch. The four knobs adjust amount, rate, tone and space. White notes remain playable. Leaving the page clears the overlay and suppressed black notes remain suppressed until released. Overlays reuse FX state and leave saved FX parameters intact. Tape Stop and true pitch-drop/bend are still open. Abrupt parameter changes and audible transitions require hardware listening and timing verification.

The existing preference object now also saves per-track Perfect Note, recording length, count-in, replace and quantization. It still reads old metronome-only values. Writes are debounced and occur only while idle. On rollback, older firmware may ignore extended preferences. The reboot fixture verifies persisted Perfect Note. The full SETTINGS UI and remaining transport preferences remain open.

Host press/release and exit tests passed for all nine overlays. Host recording actions, lane 16, held-note, metronome, 32-bar song round-trip and dense-save rejection fixtures passed. The pinned native build and development package audit passed. The new hosted browser results, physical FX evaluation, sample bandwidth, sound review, visualizers and final release gates remain open.

## Tape Stop and Pitch Drop

The owner confirmed hosted browser run 44856067840 passed at 2294e3f. LIVE FX now maps the last two black keys to Tape Stop and Pitch Drop. Tape Stop increases the read delay as playback decelerates, then fades the stopped sound to silence. Pitch Drop reads two overlapping fractional taps and crossfades their wraps for a sustained downward shift. Both reuse the existing quarter-rate echo line with bounded indices, persist their release coefficients until the wet signal fades, and suspend the selected track's normal echo while engaged. FX rollback checkpoints cover the reused delay writes. No new audio buffer was allocated.

The direct DSP tone fixture verifies bounded output, downward zero-crossing count, stopped tail, release, and unchanged saved delay parameters. The LIVE FX host fixture recognizes all eleven keys and clears their requests on release. The 137 BPM live replay fixture exercises all eleven during five-track, 16-voice playback with drum input distributed across the full keyboard. Worker and local playback matched. The worker recovery suite also passed after restoring a second eligible melodic voice to its sparse worker fixture and explicitly checking that new ADPCM drum-only rendering stays local.

Pinned native build and package regeneration passed. Application: 482,676 / 581,564 bytes. Static RAM: 90,104 / 98,304. Pool: 322,460 / 344,064. Physical FM-1 sound, FX transitions, sample flash bandwidth and deadline measurements remain open. SETTINGS, playback-safe recording and transport refinements, patch and drum listening, visualizers and release qualification remain incomplete.

## SETTINGS, resume transport and visual rendering

GitHub Actions run 44856632392 completed successfully at the owner-specified 7c106cf baseline.

Tap GLO, then SEL to open SETTINGS. SELECT selects the category. GLO+SEL still opens the mixer, and SEL on the Diagnostics category enters the existing telemetry pages. SETTINGS controls output trim, optional speaker low-cut, metronome mode and click level, recording quantization/length/replace/start mode, restart or resume, Perfect Note for all melodic tracks, visualizer selection and Eco/adaptive frame pacing. Existing per-track scales and chord controls remain on KEY & CHORDS. The limiter remains enabled. The same eight-byte preferences object stores new fields in previously unused bits. New firmware reads previous settings. Older firmware may default its preferences when asked to read the expanded word.

Resume freezes the next-step countdown during stop and releases sequenced voices from the audio callback. HOME highlighting checks actual sequenced voice identity, delay and release state before drawing a sounding note. Scrolling still uses the actual swung step interval. Playback-safe undo, cancellation, and broader loop options remain open.

All eight strip-rendered visualizers received geometry and color changes. DNA gains depth-ordered strands and bridges. Ferrofluid adds shaped silhouettes and reflected accents. Gravity Rings adds glow and trails. Solar adds a corona and planet shading. CRT adds strip fills and scanlines. VU adds illuminated faces and pivots. Spectrum bars gain stepped shading. Reactor gains a luminous core and layered particles. No framebuffer was added. Five alternating host trials per visualizer measured entire scripted process user CPU in milliseconds, including startup, audio, drawing and host transfers. Median before → after (host only): DNA 69.00 → 68.68, Ferrofluid 67.87 → 63.63, Gravity 71.53 → 67.94, Solar 68.07 → 65.67, CRT 72.90 → 74.90, VU 62.50 → 66.41, Spectrum 67.47 → 66.58, Reactor 75.33 → 75.57. Each process ran 14 seconds of simulated visual playback after boot. These figures do not isolate LCD transfer time. The screenshot fixture passed across all eight at four master gains.

The SETTINGS fixture passed persistence and resumed step position after a simulated restart, and verified the existing diagnostics and mixer controls. Pinned native build and package regeneration passed with application 485,348 bytes, static RAM 90,120 bytes and pool 322,460 bytes. Physical LCD/audio contention, speaker response, XIP and stack high-water remain unmeasured. Patch/drum listening and qualification remain open.

## Issue #1 — five-track stress test and playback-safe recording

Resumed [checkpoint 0dc97bc](https://github.com/CodingSlapfu/SLAP-5/commit/0dc97bcf10d8b26da31b34f74be8f4cdd17a0c7b) and confirmed the passed [hosted browser job 44857163047](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44857163047). The prior worktree's local commit has the same tree as the verified remote commit. The original worktree and a verified baseline recovery bundle are available for rollback. This continuation tracks the approved workload in [issue #1](https://github.com/CodingSlapfu/SLAP-5/issues/1).

### SLOOP 3 comparison

Read the public design at [f48b4c8](https://github.com/isod89/sloop3-fm1/tree/f48b4c8b40dc39da3a8089045a3034fdea1bfa45). SLOOP 3 uses 64 frames at 44.1 kHz (1.45 ms), split voice buses and a later mix pass. Its long stutter stores IMA ADPCM in a small shared pool with a sequential decode cursor. SLAP-5 callback/DMA halves use 256 frames at 44.1 kHz (5.80 ms). The foreground services input between LCD work, the callback reads performance notes at block entry, and the sequencer dispatches within the sample loop with worker batches split at event boundaries. A smaller half-buffer changes USB/I2S service cadence and worker scheduling overhead. This checkpoint keeps the 256-frame DMA configuration and cost-based split pending physical timing.

SLAP-5 already caches effect coefficients on revision, tempo or LIVE FX changes and skips inactive chains. It processes track insert/chorus before its slicer, so changing the chain order would change existing project sound. The slicer's 256-entry PCM repeat line uses constant-time reads. Drum one-shots use per-voice incremental ADPCM and local flash/XIP reads. SLAP-5 already reuses echo memory for Tape Stop and Pitch Drop, with bounded worker rollback. No new repeat, delay or recording audio buffers were added. The comparison used the architecture as reference and imported no source.

### Host stress workload and measured decision

Run `python3 device/tests/dense-recording.py --extreme --bpm 137` after the host build, substituting 112 and 160 for the other rates. The fixture sets all five tracks to repeat slicer plus insert, reverb and delay before recording. It records seeded chords and rapid notes across four melodic tracks, then cycles the 16 drum keys in overlapping groups. It exercises each LIVE FX, then runs a 60-second simulated visualizer soak. Synchronous worker and single-core renders matched at all three rates. Each reached 16 voices. The synchronous worker is a deterministic host model of firmware dispatch. This fixture measures whole-callback thread CPU time and samples output PCM. It does not measure FM-1 input-to-output latency, isolated CPU0/CPU1 real time, DMA, XIP stalls or LCD transfer contention.

Ran an A/A timing check at the baseline followed by a five-run alternating A/B of insert/slicer coefficient caching, including recording and playback. Median worker-model P95/P99 values in host µs:

| BPM | Before P95 / P99 | Candidate P95 / P99 |
| ---: | ---: | ---: |
| 112 | 149.99 / 197.41 | 157.02 / 220.59 |
| 137 | 152.04 / 200.64 | 151.96 / 200.24 |
| 160 | 150.69 / 187.65 | 155.47 / 196.44 |

The A/A check showed run-to-run variance, especially in isolated maxima. The coefficient cache cost static RAM and showed no repeatable benefit at the relevant tails. It was excluded from the final tree. The benchmark is reproducible with `python3 device/tests/compare-dsp-timing.py 94be086 8d0d3a7 --workload extreme --runs 5 --cpu-time --full` in the local history.

### Recording, graphics and regression

While playing, confirmed clear and undo are consumed by the audio callback before the next sequencer step. Cancel finalizes the held notes, restores the pre-take snapshot at a step boundary and keeps existing playback running. Cancel during a stopped-start count-in stops that count-in. Clear and undo retain the same snapshot footprint and revision guard. Host fixtures verify playback remains running through cancel, clear and undo, and that a two-second hold is restored and survives save/reboot. The old corrective ENV fixture also failed on 0dc97bc because it expected values from before the factory revoicing. Its expected attack values now match the current factory patch.

Strip rendering now clips disc rows before iteration, skips VU panels outside the current strip and computes each Reactor particle's position once per frame. All eight comparison screenshots matched pixel-for-pixel in the scripted host case. The existing four-gain screenshot fixture also passed. Whole-process medians for five paired runs (host CPU ms for startup and 14 simulated seconds of visual playback) were CRT 74.91 → 72.21, VU 62.34 → 62.97 and Reactor 73.91 → 71.10. CRT uses the same drawing path and these timings include noise. VU's script showed no measured gain. The Reactor removes five repeated coordinate calculations per particle without a framebuffer. FM-1 LCD/audio contention still needs a device trace.

All 22 host text fixtures, deterministic worker and injected recovery regressions, all 25 synthesis engines and 128 patches in both modes, seven inserts and LIVE FX tone tests were exercised. Native development build passed with the pinned SDK files verified. Application flash 485,732 / 581,564 B, static RAM 90,312 / 98,304 B, pool 322,460 / 344,064 B. FWSC regeneration matched byte-for-byte, identity `FM-1_7500`, protected update range `[0x4000, 0x93000)`, SHA-256 `46a3f37e845dc2e48a5c658162672e8ec864d8c67da558866c32ce2065d546ac`. This is a development package.

### Remaining qualification

Rendered all 128 patches and all 25 sample-backed kits for level-matched comparison, including all drum slots and a common groove. Automated synthesis and WASM checks provide signal, bounds and assignment coverage. The individual listening assessment, any additional voicing work, and acoustic-style kit acceptance are open. The acoustic-style kit source remains synthetic. Before release, confirm sound quality and qualify physical peak and tail timing, stack, XIP reads, and LCD coexistence. On a versioned, approved device test build, repeat at 112, 137 and 160 BPM, record dense notes and drum lanes with all five inserts and slicers, perform all eleven momentary effects, then loop with CRT, VU and Reactor active. Capture CPU0/CPU1 timing, XRUNs, fallback/recovery, input-to-audio and UI response, stack and any audible transitions. Repeat playback-safe recording operations, verify a saved project after reboot and exercise recovery with the approved rollback image. Retain v0.1.18 as the public primary installer and v0.1.20 Diagnostic as rollback coverage until the release gates pass.

### Dense clear and restore costs

The intermediate GitHub checkpoint [dd24ba4](https://github.com/CodingSlapfu/SLAP-5/commit/dd24ba4a4ec6f92d98feeb38c14d5215599d1099) matched the locally tested tree and passed [hosted browser run 44859535286](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44859535286).

Recording snapshots, clears and restores now traverse consecutive cells and pack timing two steps at a time. A local instrumented desktop build measured 12 alternating runs of a generated 512-step drum clear and undo while a melodic track continued playing. Median time inside the action handler fell from 34.06 to 25.81 µs for clear and from 10.04 to 5.73 µs for undo. The host regression checks the first drum lane and the 16th lane at step 512 after clearing and restoring. A 17-step pattern test checks packed timing on the trailing step. The snapshot capacity and stored project layout are unchanged. The maximum-length operation still needs FM-1 callback timing.

The pinned native rebuild and FWSC regeneration passed. Application 485,764 / 581,564 B, static RAM 90,312 / 98,304 B, pool 322,460 / 344,064 B. Development identity is `FM-1_7500` and FWSC SHA-256 is `748d6bf5d9e06e2254ea9402404d0f39ab6df169163c87db43f2990239315326`.

The gain-independent, coarse spectral scan of the listening renders identified HALL FLUTE / ALTO FLUTE, AIR FLUTE / BAMBOO and HALL GRAND / LONG GRAND as close pairs in the sampled windows. It also identified drum kit IDs 70 / 73 and 60 / 62 for paired comparison. This screen does not score articulation, envelope, musical usefulness or acoustic realism. Use the RMS-matched WAVs to judge those differences before making voicing changes.

## Experimental hardware test v0.1.21

The versioned package comes from the firmware source at cfae060. App size 485,764/581,564 B, static RAM 90,312/98,304 B and pool RAM 322,460/344,064 B. The pinned SDK inputs passed, and complete FWSC reconstruction matched the 611,488-byte package SHA-256 `1cf2b34fa235c0e9d1870429496bdd55b98311d0b466812b51afae9c7483dce9`, identity `FM-1_75000121`. The 256-frame build, project format and factory voicing are carried forward. Browser run 44859843641 and host project tests cover the firmware tree. The versioned package passed browser updater format and simulated pull tests. Host loader simulation installed this package from v0.1.18 and rolled back to v0.1.18, with identical boot bytes, exact target app bytes, app erase range 0x4000–0x92FFF and correct update-record cleanup. The published website manifest stays at v0.1.18. The experimental write plan is restricted to the app window via M-Flasher; use the [release checklist](release-0.1.21-hardware-testing.md) for layout and recovery checks.

All 128 patch clips and all 25 kit clips in the listening pack decoded with nonzero, unclipped signal. Patch clip RMS ranged -31.28 to -24 dBFS after the matching pass and peak cap; kit clips matched -24 dBFS. Ten-millisecond RMS-envelope correlation was 0.925 for HALL/ALTO FLUTE, 0.992 for AIR FLUTE/BAMBOO, 0.940 for HALL/LONG GRAND, 0.931 for TRAP/DUBSTEP and 0.977 for LIVE BREAKS/PUNCH 909. Shared phrases and rhythms increase these scores. They do not establish perceived similarity, articulation, sound quality or realism. Reliable listening was unavailable for this pass; no patches or kits were revoiced solely on the screening metrics. Owner listening and physical results remain open.

## Issue #2 — performance workflow and scrolling

The owner physically tested v0.1.21 and reported excellent sounds and drums, stable audio during a recording session denser than their SLOOP 3 session, and scrolling lag. This is qualitative hardware evidence from v0.1.21. There are no physical timing or capacity measurements for this successor.

- HOME records through track handoffs at callback boundaries and keeps a separate prior take per normal 16/32/64-step track using the existing snapshot arena. Large legacy patterns retain a single wide snapshot. The gesture regression follows all five tracks, holds EDIT during record, toggles mixer keys, cancels, clears and undoes during playback. HOME supports first-note and count-in starts.
- Tap GLO opens SETTINGS. Hold GLO enters diagnostics in five seconds. Hold EDIT enters MIXER in two seconds. The previous GLO+SEL shortcut has been removed. The mixer uses five white-key mute toggles, true center pan, a solo knob, and bounded block-target Auto Level. Functional LEDs have the bright priority and the PLAY pulse follows transport beats.
- The mix-scoped LIVE FX operates on a centered wet bus after summing all tracks and crossfades back to stereo. It has its own short echo (3,072 quarter-rate samples, approximately 279 ms), chorus and room lines. Track sends and inserts retain their saved settings and worker ownership. Assignments are persisted; discrete settings change at callback boundaries and continuous values approach knob targets over successive blocks. The suite exercised all eleven effects on the mixed bus and verified return to dry and saved FX integrity.
- Split Keys stores performed chord voices as separate recorded notes and keeps note-off tied to the physical key. Step clipboard is scoped to its lane. Melodic hold, microtiming and step modes survive save/load. V9 song slots are emitted only when ratchet or Fill Only data is present. Existing V2–V8 slots remain readable. Studio has matching step-mode editing, fill toggle and a 16/32/64 selector with legacy imports retained.
- The six-mode display retains the four established geometry modes and adds Digital VU plus the revised Reactor/intro. Obsolete saved display modes map to the next supported visualizer. The meter counters use the selected track's loop position. Single-shot drum playback remains sample-based.
- Five alternating benchmark runs instrumented host app-frame wall time with a dense generated 64-step drum loop and HOME scrolling. Median run statistics were **77.02 → 56.74 µs** median frame time, **126.32 → 89.29 µs** P95, and **6 → 5** simulated blits per rendered frame. Fixed headers are reused and short drum gates use a visible-column scan. These measurements exclude physical LCD transfer delay.
- All 25 scripted host fixtures passed, plus worker recovery/parity, all six visualizers, and five-track stress at 112/137/160 BPM with the full-mix effects and 16 peak voices. Native SDK reference hashes and exact versioned package regeneration passed. Application 494,448/581,564 bytes, static RAM 91,400/98,304 bytes, pool 334,760/344,064 bytes. Firmware `FM-1_75000122`, 611,488 bytes, SHA-256 `a5fed987e30938862f78dbfbcc6a5b512900879f910930c77089028a78464c79`. Physical LCD/input latency, callback headroom, flash/XIP behavior, LED visibility, stack watermark, direct browser install and recovery on the FM-1 remain part of hardware qualification.

## 0.1.24 continuation from 10b167b

The previous workspace was unavailable after the pause. The existing branch, exact 0.1.23 source, release archives and successful Chromium evidence were recovered from GitHub. The 0.1.23 publishing workflow stopped at its verification step. Reproduction from the committed rollback archive showed the 0.1.22 checksum list covers the separate Studio ZIP as well as the firmware. Extracting only the archive leaves that ZIP missing. `tools/verify-rollback.sh` assembles both verified archives correctly and runs the full interrupted update and reverse-install simulations. Run it in the next release workflow. The live website check returned 0.1.18 as the primary installer and HTTP 404 for the experimental route.

| Native resource | Development rebuild at 10b167b | Versioned 0.1.24 candidate |
|---|---:|---:|
| Application flash, limit 581,564 B | 539,236 B | 542,288 B |
| Static RAM, limit 98,304 B | 96,504 B | 95,544 B |
| Pool RAM, limit 344,064 B | 334,888 B | 326,440 B |

Packed two timing/step-mode values per byte across 33 rows. The existing undo arena already packed these values, so snapshot copies now transfer the packed bytes directly. An interrupt-protected read-modify-write preserves the adjacent value on physical edits. Active slot serialization still uses its existing format. The spectrum handoff uses signed 16-bit PCM with the same bounded queue and no reduction in queue depth. Remaining free memory is 2,760 B static and 17,624 B pool. Both stretch targets remain open. The biggest unoptimized allocations include echo lines, the legacy rollback arena, drum events, string delays and the LCD strip canvas. Do not overlay them without proving concurrency and lifetimes. Physical stack, XIP and deadline headroom remain open.

QG-1 reference at [e41e1df](https://github.com/Guillel51/Quantum-GammaWave/tree/e41e1df6e9892e77f5b264a2df0f055a911fd4a3) uses page-aware invalidation and per-column cached state. SLAP-5 uses an independent state key to skip unchanged graph bands on static HOME, SEQ, ENV and harmony pages. Full live graphics redraw and LCD transfer benchmarks remain open.

FM uses six phase accumulators and three routings for newly selected FM patches. Wavetable uses four compact frames with interpolation and three partial-count bands. Existing projects without the new synth flag retain their original sound. Native saves using that flag require 0.1.24 or later. Dedicated operator and wavetable parameter editors and dense native FM6 qualification remain open.

Hardware and Studio now select 1–4 drum bars and apply live changes at a bar boundary. Hardware regression covers 48-step generation, active transport and undo to 16 steps. Existing locked cells and steps outside the generated region remain intact. Studio Song Mode includes five scene clips, scene names and repeats, reordering, clip copy/paste, autosave, song import/export, scene queues and a worklet playhead. The render splits at the exact scene sample boundary. The WASM fixture verified two audible scenes, three scene passes and concurrent 1–4-bar clip lengths. Live generation during standalone pattern playback is verified. Recording and generation during arranged song playback, full mixer and instrument editors, and comprehensive browser acceptance remain open.

The host fixtures, dual-core rollback/parity, pinned native build, WASM regression, package reconstruction and simulated interrupted installation and rollback to both 0.1.18 and 0.1.22 passed. The local development candidate identifies as `FM-1_75000124`. SHA-256 `20ac146fb8fb04662952cc5cd6d04acbe959351db6751ea5c9f10723858724d9`. It has not been promoted or deployed. Continue with the device and Studio instrument editors, memory and rendering measurements, multi-scene mixer/recording tests, and exact candidate Chromium and release verification before publishing.
