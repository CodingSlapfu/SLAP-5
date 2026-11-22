# SLAP-5 v0.1.31 FM-1 engineering qualification

Candidate FM-1_75000131. Package SHA-256 `36676ae153435f065c99c2106bdccf43917f7cd57f65fb702e0144d0055ef8be`. Resumed from the 0.1.30 source and qualification checkpoint. The owner reports that 0.1.30 plays well overall and that the synthesis graphs were difficult to find and see.

| # | Owner requirement | Status | Evidence |
|---|---|---|---|
| 1 | Large animated FM6 | PARTIAL | EDIT shows six nodes for FM6 immediately. ALGORITHM right once opens the 88-pixel routing canvas. Idle and played 240×240 captures differ. FM-1 confirmation open. |
| 2 | FM6 routing, envelopes, feedback | PARTIAL | Graph edges and carrier roles follow the three modern DSP algorithms. Active edges reflect operator gain and track level. Existing envelope and feedback pages remain reachable. Full feedback and envelope redesign remains open. |
| 3 | Large Wavetable | PARTIAL | 96-pixel canvas with active waveform, adjacent frame traces, grid and scan marker. FM-1 confirmation open. |
| 4 | Morph and scan | PARTIAL | Host capture switches waveforms with K1/K2, and held-note frames differ. Legacy wavetable synthesis uses a different algorithm and remains to be represented separately. |
| 5 | Other engines | PARTIAL | Existing envelope, filter and live-signal families retained. Individual engine graph designs and full per-engine verification remain open. |
| 6 | FILTER curve | PARTIAL | Existing cached response uses the DSP filter transfer function over a log axis. Frequency and dB ticks remain to be labeled and checked on hardware. |
| 7 | SLICER | PARTIAL | Added an actual signal trace alongside the pattern, cursor and gate/capture/replay status. Capture region and replay position remain open. |
| 8 | Navigation | PARTIAL | Graph fixture covers the EDIT entry, one-detent access, FM6 pages, Wavetable and return HOME. Hardware visibility open. |
| 9 | Thicker Nexus | COMPLETE | Original outline generator uses 67 and 104 stroke units, up from 52 and 84. Native bitmap and Studio TrueType builds regenerated. |
| 10 | Firmware-wide type fit | PARTIAL | Current patch and kit name-width checks pass. Full page-by-page maximum-size audit remains open. |
| 11 | Premium rotary controls | PARTIAL | Shared rotary renderer has a larger beveled ring, illuminated pointer and reference ticks. Existing footer geometry limits the size. Full per-widget caching and panel review remain open. |
| 12 | Selected lane highlight | PARTIAL | Persistent lane field and tempo-linked outline, with an independent stopped pulse. Host capture verified. |
| 13 | Patch names in lanes | PARTIAL | Patch names drawn in reserved area of all five existing lanes. Lane height is unchanged in this candidate. |
| 14 | Battery | PARTIAL | HOME reserves space for outline gauge, empty and unknown states, USB and low-battery label. Host full-battery capture verified. Panel states open. |
| 15 | RGB565 colors | PARTIAL | Existing saturated palette retained and used for lane outlines, graph overlays and rotary pointers. Full palette redesign remains open. |
| 16 | ALGORITHM in visualizers | COMPLETE | Host fixture proves 12 detents leave BPM unchanged in VISUAL, and ALGORITHM resumes normal tempo control at HOME. |
| 17 | Gravity Rings | PARTIAL | Tempo-dependent perspective projection added to both main and transient rings. |
| 18 | Ferrofluid | PARTIAL | Bass curvature and transient spikes added. Physical aesthetic review open. |
| 19 | Solar System | PARTIAL | Five assigned planets now grow and leave trails with independent track energy. Physical review open. |
| 20 | Digital VU | PARTIAL | Real transport BAR and BEAT replace STEP across five meters. Physical review open. |
| 21 | Signature visualizer | PARTIAL | Existing SLAP Reactor retained. The preferred historical revision still needs to be identified. |
| 22 | Expressive patches | PARTIAL | Prior lead glide and flute vibrato retained. No new voice/DSP changes in this candidate. |
| 23 | Shorter names | PARTIAL | Short names and factory IDs retained. Current name-width checks pass. Additional renaming remains open. |
| 24 | Resources and timing | PARTIAL | Native uses 555,204/581,564 app bytes, 87,864/98,304 static RAM and 335,568/344,064 pool bytes, leaving 8,496 free. Host regressions passed. Native Core 1, XRUNs, LCD transfer time and latency remain unmeasured. |
| 25 | Website and Studio | PARTIAL | Candidate WASM, fonts, instructions and real 240×240 captures staged for separate experimental pages. |
| 26 | Backup and rollback | PARTIAL | Verified 0.1.30 engineering backup. Interrupted upgrade and rollback simulations retained protected regions and songs/settings byte-for-byte. |
| 27 | Publication | PARTIAL | Candidate package audit and update simulations completed locally. Public release and installer verification are separate gates. |

## Graph acceptance

| Graph | Control route | 240×240 host evidence | Physical FM-1 |
|---|---|---|---|
| FM6 | EDIT, then ALGORITHM right one with FM GLASS selected | Six nodes, routing variants, held-note frame differences | Open |
| Wavetable | EDIT, then ALGORITHM right one with LAMP WAVE selected | Three traces, frame marker, Morph and Scan captures | Open |
| Other engines | EDIT, then ALGORITHM right one | Existing signal and parameter graphs | Open |
| Filter and Slicer | FX, then ALGORITHM to Filter or Slicer | DSP curve and Slicer grid captured | Open |

The original acceptance matrix remains in owner-master-acceptance.md. Host tests execute the software path and do not establish FM-1 Core 1 timing.
