SLAP-5 0.1.12 engineering TESTING for the compatible M-VAVE FM-1. Copyright Ziao.

- Core 1 now runs selected independent track FX chains alongside Core 0. Bounded wait, checkpoint rollback and single-core fallback protect the delay, chorus, room and slicer state. The slicer advances without dividing on each sample.
- Track 5's drum sequencer now shows **SEQ: GEN DRUMS**. Press SEQ to enter and to return. All 32 kit IDs are preserved. Twenty-eight kits gained new combinations of kick, snare, clap and cymbal synthesis layers.
- Solar System orbital speeds respond independently to the music. CRT Plasma reacts to bass and high-frequency bands. The SLAP Reactor and intro lettering blend with the animation over true black.
- Eight factory patches were renamed to avoid SLOOP 3 name collisions. IDs and synthesis parameters are unchanged.

The native package, host regressions, five-track FX parity and fallback, WebAssembly Studio, Chromium integration and simulated OTA passed. The FM-1 still needs to establish callback timing, DMA stability, LCD appearance and musical acceptance. Keep v0.1.11-testing available for rollback.

Firmware identity `FM-1_75000112`. FWSC SHA-256 `fde204437fcbb6fa1bd855e0e78cad8eae0ea69aded2db0100cb6233b148b297`.
