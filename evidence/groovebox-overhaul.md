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
