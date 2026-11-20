# SLAP-5 0.1.19 Diagnostic

**EXPERIMENTAL — PHYSICAL PERFORMANCE UNVERIFIED**

Firmware identity `FM-1_75000119`. v0.1.18-testing remains the primary testing installer. This diagnostic package targets the same supported FM-1 configuration as v0.1.18. Back up projects before updating. Use the supported V15-style layout with a 1 MiB `0x856014` flash and chip key `0x980F`; check device identity and layout before writing with M-Flasher. Use the release's FWSC file with M-Flasher and keep the known-good v0.1.18 package available for rollback. The update must stay inside the application window `0x4000–0x92FFF`. [Current installer and recovery instructions](https://slap-5.jminie66.chatgpt.site/flash#m-flasher) · [v0.1.18 rollback](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.18-testing)

## One short hardware capture

1. Open GLOBAL and press GLO until **DMA TRACE** appears. Turn K2 left to resume if frozen. Turn K3 to clear diagnostics.
2. Play the existing 160 BPM arrangement until a miss occurs or for about one minute. Turn K2 right to freeze the most recently captured offending half. Stop playback when convenient.
3. Use K1 to view the four pages. Type the **TRACE ID**, **DMA HALF US**, **DEADLINE MISS**, **HALF XRUN**, **NESTED T5 US**, and **APP CALLBACK US**. If there was a miss, include **LOCAL VOICE US**, **BATCH FX US**, and **WORKER WAIT US** from the same frozen record. No photos are needed. If no miss occurred, report the DMA time and zero counters as an observation for that run.

The DMA IRQ keeps a rolling eight-record buffer. Before a fault it retains successively slower halves. After the first fault it retains the latest eight faults, so later successful playback does not hide the most recent one. The clock surrounds render, acknowledgement and free-half validation. The app measures voice batch, local render, dispatch, note-on, maximum span and batch FX. Remote work, completed jobs, waiting and overlap share the same record. Timer 5 time is measured separately. Several quantities overlap in time. OTHER APP includes serial FX, events, mix and other unassigned time. OUTSIDE APP includes work outside the render call and does not establish the source of that time. The trace does not independently identify other preemption or bus contention. The UI retains the latest record and can freeze its capture without modifying clocks or DSP policy.

The instrumentation is opt-in. The DMA IRQ uses three extra timestamps per traced half and takes an additional timestamp when recording an event. Existing per-span instrumentation is active on the trace page. Normal latency measurements should use the ordinary GLOBAL page. Physical overhead still needs measurement. No USB export is provided in this build.

## Software qualification

Native compilation and structural audits passed. The pinned inputs and reproduced package matched byte for byte. The native image uses 162,696 bytes of application flash, 56,840 of 98,304 bytes of static RAM, and 312,420 of 344,064 bytes of pool memory. Firmware SHA-256 `926a12c82784d88235725045be91f582298538af561d86eca4c35b68232dd53d`.

The current voice scheduler, worker recovery and DMA ownership remain active. Tests cover isolated late recovery, repeated-late retirement, FX rollback, storage, held/polyphonic/extended recording, 16-voice 160 BPM playback, modulated five-track FX at 137 BPM, WASM/Studio project compatibility, patches and kits, plus updater identity and corruption handling. Native package integrity and host/WASM checks do not constitute a physical boot or flash test.

The owner previously observed a 6,053 µs peak, 97 misses and 99 DMA XRUNs with zero physical worker fallbacks. The cause remains open. The physical target is approximately 4,933 µs peak, with zero misses and XRUNs against the 5,804 µs deadline.
