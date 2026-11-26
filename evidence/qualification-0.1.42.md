# SLAP-5 v0.1.42 experimental qualification

Application source `2d89ffe83b8146917837092698b42136e4add881`. Firmware identity `FM-1_75000142`.

## Owner report

On v0.1.41 the owner reported audio crackling and lag at 155 BPM and 60% swing in a dense five-track JUNGLE project. The physical capture showed approximately 117% DSP, 6,920 µs peak against a 5,804 µs deadline, 3,147 misses, 15,513 XRUNs, BARRED/LATE and one fallback with 4,433 sent and 4,432 completed. Those data do not distinguish a hard FX timeout from an unacknowledged voice job. At either hard timeout the platform stops Core 1. The resulting serial load can explain sustained overload, while the first late job and its trigger still require native hardware evidence.

## Source changes

- After a late voice result, Core 0 renders the current block from live state. Core 1 has private voice scratch until it acknowledges. Acknowledged late output is discarded. Three repeated misses narrow the next remote batch to one voice. Sixteen clean completions restore the normal split. New FX jobs stay local while voice recovery is active.
- An FX batch needs exclusive ownership of live effect state. The dispatcher uses the last matching batch cost and remaining callback time to avoid launching a predicted late job. The cache key reserves six bits per track for the full FX cost range. An acknowledged FX completion within the existing grace window keeps Core 1 active and temporarily runs FX locally. A hard timeout stops the worker before rollback and rerender. The platform's boot-only start guard remains in force.
- The GLOBAL Core 1 page distinguishes VOICE LATE and FX LATE. The reason survives K3 resets, which clear software workload counters.
- SETTINGS → KNOB STYLE adds Analog Classic, Analog Studio, Digital Neon and Digital Segmented. K1 selects the style. The default uses the previous saved-settings encoding. Custom selections use a marker so retired theme bits cannot become a knob preference.
- HOME uses fitted 27-pixel SIGNATURE glyphs, dark shadows and chamfered tips. Rotary rendering uses shared geometry and the existing strip renderer, with a center indication for bipolar values. HOME and MIXER assignments and the single CHROMA palette are retained.

## Hosted verification

[Native, host, framebuffer and rollback run](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45885597354). Deterministic tests exercised one, two and three acknowledged late voice jobs, forced voice hard timeout, FX grace and hard timeout, and PCM equivalence against Core 0. Dense recording and replay matched at 155 BPM. The JUNGLE fixture verified save/reload, 16 voices and stop/restart at 155 BPM and 60% swing. Host MIDI, Sound Lab and settings fixtures ran in the same qualification.

Versioned application 569,024/581,564 bytes (+240). Static RAM 89,208/98,304 (+32). Pool 335,568/344,064 (unchanged). The runner downloaded and verified the v0.1.41 firmware and source/firmware archive, along with the v0.1.31 rollback firmware, by SHA-256. The candidate passed packaged application boundaries and simulated interrupted OTA and v0.1.31 rollback.

[Actual host firmware framebuffer captures](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45885597354/artifacts/15149750592) include four distinct HOME knob states, 240×240 pixels and enlarged views. The reference in this artifact is the historical firmware baseline. A direct comparison with the owner-approved design and physical LCD readability remain open.

## Pinned host timing

Same-run median wall timing in microseconds at 155 BPM. Each row compares v0.1.41 with this candidate under the same host fixture and pinning.

| MIDI traffic | P95 | P99 | Peak |
| --- | ---: | ---: | ---: |
| Receive | 167.91 → 168.51 | 179.01 → 180.39 | 241.95 → 247.26 |
| Transmit | 223.16 → 223.81 | 238.60 → 239.21 | 260.10 → 251.22 |
| Receive + transmit | 166.26 → 166.31 | 180.09 → 180.01 | 251.92 → 244.76 |

[Exact-source host timing](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45885597332). Median host budget overruns were zero in all three cases. Hardware deadline and DMA results remain open.

## Release and recovery assets

- [Experimental v0.1.42](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.42-hardware-testing) and [verified live installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html)
- [Firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.42-hardware-testing/SLAP-5-0.1.42.fwsc) SHA-256 `7a2f7730a99b144c2390d6e42682082358e3c9921efe031cb777de19ac4ab1cc`
- [Engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.42-hardware-testing/SLAP-5-0.1.42-engineering-backup.tar.gz) SHA-256 `d9762dcca9156eb55bc3000eaa21a2181b4bb2f1b39c019621b6c1bde0dba127`
- [Framebuffer archive](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.42-hardware-testing/SLAP-5-0.1.42-framebuffer.zip) and [versioned build and deployment verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45885709203)
- [v0.1.31 rollback package](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.31-hardware-testing/SLAP-5-0.1.31.fwsc)

## Physical test procedure

1. Preserve the exact project used for the v0.1.41 report and work on a copy. Keep an untouched pre-Expression project for v0.1.31. Schema 13 songs require firmware v0.1.41 or newer.
2. On the FM-1, open that copy and verify 155 BPM, 60% swing, five tracks and JUNGLE drums. Reset GLOBAL counters with K3 before measuring. Use the same passage and test duration as the previous build where possible.
3. Play and overdub dense chords. Exercise SLICER, LIVE FX, MIDI, track switching and all four knob styles. Record max and P99 callback time, deadline misses, XRUNs, active voices and the Core 1 mailbox progress. If playback degrades, capture the Core 1 runtime/retire page and the voice/FX counters before rebooting.
4. Mute the active tracks, stop playback, reset with K3, and observe DSP and new misses/XRUNs after tails settle. Restart and repeat on the same project copy.
5. Require a peak below 5,804 µs with headroom, zero new deadline misses and XRUNs, productive Core 1, and clean recording and playback. Report any audio discontinuity or LCD stall with the active page and gesture.

## Remaining work

Physical confirmation of the first late job and native load/latency remains open. A genuine hard timeout still stops the worker for the session. Safe native re-entry after a hard stop requires platform-level work and on-device qualification. Deeper synthesis/FX sleep and shared-polyphony tuning, factory patch listening, firmware-wide type refinement, remaining simultaneous USB audio/MIDI and compatibility checks, and the rest of the consolidated backlog remain open.
