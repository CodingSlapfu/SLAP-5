# SLAP-5 0.1.21 experimental hardware test

**Physically unverified. Intended for controlled testing on a compatible FM-1.**

`SLAP-5-0.1.21.fwsc` · identity `FM-1_75000121` · 611,488 bytes

SHA-256 `1cf2b34fa235c0e9d1870429496bdd55b98311d0b466812b51afae9c7483dce9`

## Install and recover

1. Keep the same unit's M-Flasher backup and copies of important projects before installing. Test saves in spare slots. Projects using the new drum lanes or new kit IDs may require this firmware. Preserve the original project data for rollback.
2. Use M-Flasher over direct USB in download mode. Verify a compatible V15-style layout, 1 MiB JEDEC `0x856014` flash, and chip key `0x980F`. Verify the package checksum and the app-only write plan `0x4000–0x92FFF`. Keep the current bootloader and user-data regions. Stop at the read-only screen if any device or map check fails.
3. Let the automatic backup, write, and verification finish before disconnecting. Reboot and check for `FM-1_75000121`. Start with a disposable project before saving edits to anything important.
4. If the unit still enters download mode, use M-Flasher **Restore** with the pre-test backup from this exact unit. For a firmware-only rollback, use the original [v0.1.18 package](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.18-testing) with the same app-only flash map. Use the pre-test backup to recover project data if needed. If download mode is unavailable or the map is unexpected, follow the recovery instructions for the installed bootloader before writing anything else.

[M-Flasher installation and recovery](https://slap-5.jminie66.chatgpt.site/flash#m-flasher) · [v0.1.20 Diagnostic](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.20-diagnostic)

## What to test

- Repeat the same five-track workload at **112, 137, and 160 BPM**. Record rapid notes on all melodic tracks, use all 16 drum lanes, and sustain 16 voices with five slicers and five inserts. Trigger all eleven LIVE FX, loop on CRT, VU, and Reactor, and compare with the visualizer closed.
- While playing, clear and undo a dense drum pattern and cancel a recorded take. Verify timing and held notes after undo. Save a small project in a spare slot, reboot, and verify its steps, effects, and kit assignment.
- Press **GLO** to open SYSTEM. Reset stats with **K3** before each comparable pass. Record peak, P95, P99, budget, deadline misses, DMA XRUNs and fallbacks. Cycle **GLO** to **CORE 1 STATUS** for runtime, mailbox counts, retirement reason and stack high-water. On **DMA TRACE**, freeze with **K2** and change subpages with **K1**. If audio breaks up, include the trace ID, callback/half duration, Core 1 wait and nested Timer 5 time.
- For sample pressure, compare **ENGINE SPAN PEAKS** for the same dense rhythm with sample-backed and legacy kits. That is a timing proxy for drum-engine cost, including sample reads. Compare the regular SYSTEM counters with the same project and LCD visualizer on and off to check for contention. Note any delayed controls or audible note onset. Exact XIP stall cycles, LCD transfer contention, CPU 0 stack watermark, and end-to-end input latency require separate hardware measurement. Profile screens add measurement overhead.
- Listen to all 128 patches and all 25 kits using the level-matched audition clips. Start with HALL FLUTE / ALTO FLUTE, AIR FLUTE / BAMBOO, HALL GRAND / LONG GRAND, TRAP / DUBSTEP (70 / 73), and LIVE BREAKS / PUNCH 909 (60 / 62). Compare attack, sustain, articulation, loudness and instrument identity. The acoustic-style kits use synthesized samples.

Report BPM, loaded effects, visualizer, indications of delayed controls, and whether you heard clicks or dropouts. The [engineering record](https://github.com/CodingSlapfu/SLAP-5/blob/slap-5-red-alert-dsp/evidence/groovebox-overhaul.md) contains the software results.

The native versioned build fits its app slot and memory budgets. Package reconstruction, the host updater checks, and simulated install/rollback in both directions passed. Firmware timing, USB recovery, save behavior, and listening still need physical acceptance. The website remains on v0.1.18.
