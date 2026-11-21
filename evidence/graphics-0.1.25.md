# SLAP-5 0.1.25 graphics verification

The QG-1 reference was reviewed at commit `6eaa033a2f77e134c85dca709617f1429e22e23c`. The reviewed source included its graph, diagnostic, performance, overview and framebuffer modules. The relevant design principles were parameter-driven curves, explicit routing diagrams and invalidation limited to changed regions. SLAP-5 implements these independently. It retains the original framebuffer and uses cached filter points and a cached HOME title row.

| Page | Visible data and change |
| --- | --- |
| HOME | Five cards in one row, track activity, moving 1/16 roll |
| SEQUENCE | Beat and bar lines, subdivisions, timing, held lengths, edit cursor and playhead |
| GLOBAL | Existing audio, DMA and DSP diagnostics |
| PATCH | FM6 routing and operator bars, new operator envelope graph and feedback value, existing wavetable scan |
| ENV | Shaded ADSR with segment markers |
| FX | Cached, connected filter response with axes, existing slicer and signal traces |
| VISUAL | Black Dancer stage, faster Solar System, stepped Digital VU and sound-driven DNA/Ferrofluid |
| SAVE | Existing slot grid and save progress |
| KEY & CHORDS | Existing scale keyboard |
| LFO | Waveform and rate-driven phase marker |
| ARP | Existing pitch history |
| MIXER | Existing levels and mix state |
| RECORD | Existing take indicators |
| GEN DRUMS | Existing preview and pending state |
| LIVE FX | Turntable and speed meter tied to Vinyl Stop DSP |
| SETTINGS | Named DSP assignments and control values |

The gallery uses host framebuffers from the respective builds. See [HOME comparison](graphics/HOME-0124-0125.png) and the 0.1.25 screenshots in [site/screens](../site/screens). Actual FM-1 panel DMA, latency, XIP timing and long-duration audio measurements remain pending. Further work includes full-resolution dancer masters and a deeper graph pass on mixer, drum generation, the remaining Live FX and Studio's instrument graph editors.
