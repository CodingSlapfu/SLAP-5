# SLAP-5 v0.1.30 — Nexus and owner feedback qualification

The owner physically tested v0.1.29 and confirmed it was working overall. The reported typography, Sound Lab access, selected-track animation, Dancer and tape-effect issues drove this revision. Prior release and rollback assets remain available. This is an experimental hardware-testing revision.

**Firmware** `FM-1_75000130`  
**Exact source** [6bcbe45](https://github.com/CodingSlapfu/SLAP-5/commit/6bcbe45985f86e3ffe584bd781f5cf6a2fe58247)  
**SHA-256** `e12921897b297d3811a100175285d885d1141a1f139900cb43195053178f1c54`

| # | Requirement | Status | Evidence and remaining verification |
| --- | --- | --- | --- |
| 1 | SLAP Nexus Display | COMPLETE | Original cap and numeral grid in `device/tools/nexus_master.py`, Display SVG and TTF generated independently from the prior face. |
| 2 | SLAP Nexus Regular | COMPLETE | Companion weight generated from the same original geometry. OFL Barlow supplies fallback characters outside the custom set. |
| 3 | Tight spacing | PARTIAL | Narrow outlines and individual advances deployed. Web TTF includes specified pair kerning. Firmware bitmap layout uses integer advances with no run-time pair kerning. |
| 4 | Native readability | PARTIAL | Native glyph sizes 10, 12, 15, 23 and 31 px. 240×240 screenshots and width fixture passed. Panel readability is still an owner test. |
| 5 | Numerals and punctuation | COMPLETE | Tabular digits, distinctive 0 and 1, original requested punctuation, arrows and sharp glyph in the vectors/TTF. Firmware currently rasterizes its ASCII range. |
| 6 | Larger knobs overall | PARTIAL | Shared rotary widget expanded to a 25-pixel body and larger titles/values across the pages that use it. Some remaining pages still use text-based controls. |
| 7 | Matching HOME title sizes | COMPLETE | HOME patch names and fitting knob titles use the same 15 px Display asset. Verified names and photographed software captures. |
| 8 | Stopped track pulse | COMPLETE | Independent UI clock changes only the selected lane while stopped. A steady colored stripe persists. Captured both states. |
| 9 | Saturation | PARTIAL | Strong RGB565 accents and true black are in the firmware. Studio preview uses the matching track colors. Physical panel calibration remains open. |
| 10 | Sound Lab graphs | COMPLETE | One ALGO detent right from Sound shows the current engine graph. Input fixture and captures cover main, FM6 and wavetable navigation. |
| 11 | FM6 operator pages | PARTIAL | Routing, operators, envelopes and feedback accessible on successive ALGO detents. Existing state displays preserved. Full physical edit and routing audit is still open. |
| 12 | Wavetable visualization | COMPLETE | First graph detent exposes live waveform plus Morph and Scan controls. Host capture verified. |
| 13 | Dancer transport behavior | COMPLETE | Active frames 0–23 are transport-driven. Stopped animation uses existing recovery frames 24–29. Host captures show both states. |
| 14 | Beat Repeat | COMPLETE | Captures and loops a tempo-derived sixteenth. Track and master fixtures prove playback of the captured signal across multiple repeats. |
| 15 | Vinyl Stop | COMPLETE | Slower deceleration using larger bounded master history. Host stereo pitch fixture measured decreasing positive crossings (30, 22, 14). |
| 16 | Rewind | COMPLETE | Recent capture read backward at unit speed with a dry edge blend. Host ramp fixture verifies descending output from rising input. |
| 17 | Expressive pitches | PARTIAL | Manual glide extended to two lead patches. Three flute patches use shallow delayed vibrato when user LFO is off. Musical hardware review remains. |
| 18 | Shorter factory names | PARTIAL | Long sample-kit labels and one long patch label shortened. Factory index order and sample assignments are preserved. Ordered manifest exports from firmware sources. |
| 19 | Mixer filters | COMPLETE | Existing routing/filter implementation retained. Transport/mixer fixture verified filter independence, neutral position and persistence. |
| 20 | Project and recording | COMPLETE | Host save/load, held recording, wrapped holds and V12 tests passed. WASM Studio import, songs and recording passed. |
| 21 | DSP and resources | PARTIAL | Native compile and limits passed. Two host trials per BPM and mode; device timing, XRUNs, LCD transfer and input latency are still unmeasured. |
| 22 | Website and Studio identity | COMPLETE | Versioned Studio/website preview and experimental installer use Nexus derivatives. Prior installer routes remain separate. |
| 23 | Acceptance evidence | COMPLETE | This owner-feedback supplement accompanies the existing 28-item matrix. |
| 24 | Backup | COMPLETE | GitHub tree verified and package, font specimens, captures and test logs backed up. |
| 25 | Experimental release | COMPLETE | Native integrity, browser OTA simulation, and interrupted v0.1.29 update and rollback gates passed. Hardware acceptance is open. |

## Software and flashing qualification

31 host fixtures passed, including recording, storage, mixer controls and navigation. DSP tests verified bounded effects, reverse direction and repeat capture. Studio browser CI and local WASM project/recording tests passed. The package allows only the application region `[0x4000, 0x93000)` and uses the dedicated update-record sector for recovery. Interrupted OTA staging and application rollback preserved the protected areas and saved data in simulation.

Native usage is **553,780 / 581,564 bytes application**, **87,864 / 98,304 static RAM**, and **335,568 / 344,064 pool**. Versus v0.1.29, application size fell 10,620 bytes, static RAM rose 112 bytes and pool usage rose 6,920 bytes. The pool retains 8,496 bytes of headroom.

| BPM | Inline-worker host P95 v0.1.29 → v0.1.30 | P99 | Worst |
| --- | ---: | ---: | ---: |
| 112 | 163.37 → 164.60 µs | 207.13 → 202.95 µs | 3408.03 → 2630.50 µs |
| 137 | 164.59 → 172.71 µs | 203.59 → 222.41 µs | 13913.36 → 2470.80 µs |
| 160 | 169.19 → 168.13 µs | 211.93 → 201.97 µs | 3860.94 → 2068.14 µs |

The revised build recorded zero host callbacks over 5,805 µs in this test. These are callback CPU timings on the host, including synchronous worker execution. They do not measure device deadline misses or sustained Core 1 behavior.

## Navigation and owner test

Tap EDIT to open Sound Lab. PRESETS selects patches. On FM6, turn ALGO right one detent for routing, two for operators, three for envelopes and four for feedback. SELECT picks the operator on its edit pages. Hold SEL while turning SELECT to change tracks there. On a wavetable patch, turn ALGO right once for the waveform and use K1 Morph/K2 Scan. The final ALGO page is Patch Browser.

[Experimental installer](https://codingslapfu.github.io/SLAP-5/flash-testing-0.1.30.html) · [On-device test checklist](https://codingslapfu.github.io/SLAP-5/hardware-test-0.1.30.html) · [Verified prior rollback](https://codingslapfu.github.io/SLAP-5/SLAP-5-0.1.29.fwsc)

[Engineering backup and font deliverables](https://chatgpt.com/library?file_id=libfile_ea40297ab61c8191bf5e84d66da7ad37&account_id=personal)
