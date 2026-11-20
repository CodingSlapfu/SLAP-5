# SLAP-5 real-time audio stability

Baseline source is v0.1.14-testing (6a402b176ebf14949d13035fd21a8fea96902934).
The verified publication checkpoint is fc3f4cf507c8009964656971ea7632e6661f746a.
The next-release acceptance record retains the earlier recording, migration,
Studio, installer and rollback criteria. This file records the newest
physical failure and the corresponding engineering gate.

## Owner hardware baseline

Dense five-track recording failed both with FX off and with heavy FX/slicer.
The owner supplied the following GLOBAL readings for v0.1.14. The counters
are cumulative. The page-one peak and the breakdown peak are from different
capture times.

| Reading | Page 1 | Peak breakdown |
|---|---:|---:|
| BPM | 112 | |
| Average DSP | 92% | |
| Peak callback | 8,508 µs | 8,703 µs |
| Budget | 5,804 µs | 5,804 µs |
| P95 / P99 | 6,400 / 7,200 µs | |
| Deadline misses | 8,279 | |
| DMA XRUNs | 8,320 | 8,898 |
| Completed worker jobs | 260,282 | 268,128 |
| Fallback display cap | 9,999 | 9,999 |
| Active voices | 16 | 16 |
| Voice batch wall time | | 3,451 µs |
| FX batch wall time | | 0 µs |
| Successful Core 1 work | | 0 µs |
| Worker wait | | 120 µs |
| Unclassified time | | 5,252 µs |

FX BATCH only recorded a worker-oriented code path. When late voice output
was discarded, the FX dispatch could be skipped even while serial FX still
ran. The two waits of up to 60 µs could explain 120 µs of waiting, but the
screenshot does not identify their cause. Confirmed software defects are
missing serial breakdown and capped fallback counts. The elapsed time of
specific DSP stages on the physical FM-1 remains unmeasured.

## Engineering changes

- Always render eligible local voice blocks from a compact active slot list,
  including blocks where the worker is unavailable or its admission window
  is closed. Preserve slot order and the shared sample-and-hold RNG order.
- A late voice result lowers the cost assigned to Core 1. After 64 successful
  jobs the quota recovers by one increment. Late output stays isolated and
  local rerender runs once in slot order. Busy and rejected attempts, voice
  late completions, FX late rollback and total fallbacks have readable counts.
  Native concurrent timing and repeated late-FX recovery remain open.
- GLO cycles through summary, non-intrusive peak breakdown, opt-in serial
  profiling and worker counts. The serial page measures input/setup,
  sequencing and arpeggio, direct voices, serial FX/FX output reads, mixing,
  and master/output/telemetry at the worst profiled block. Its per-sample
  timer reads add overhead. Use the ordinary summary for deadline qualification.
  These categories do not yet separately isolate drum allocation, metronome,
  LCD blocking or the complete native interrupt overhead.
- The normal mixer avoids unity automation work; release from automation returns
  smoothly to unity. FX state and save formats remain compatible.
- Visual stereo/spectrum telemetry reads the limited mix before master gain.
  Per-track energy still honors track gain, mute and solo. Spectrum analysis
  stays bounded in the foreground. New XRUNs or elevated average load slow
  visual drawing, with a one-second recovery period.

SLOOP 3.4 source at 2228313 uses independent core-local buses, then shared
post-render mixing/FX, and a compact audio feed for foreground graphics.
AQUORA's experimental FM worker uses frozen voice inputs and separate output
buffers, and its display policy runs outside the audio callback. SLAP-5
retains original implementations. A clean-sheet, persistent worker pipeline
and shared wet buses are not qualified by this checkpoint.

## Reproducible acceptance

- Run device/tests/visual-master.py after building the host simulator.
  All eight rendered frames match exactly at 100%, 50%, 10% and 0% master
  gain for the same notes and drum pattern. At 0% the WAV is silent.
- Run device/tests/dual-core.sh. This includes forced voice and FX result
  rejection, a simulated late voice followed by recovery, and comparisons of
  single-core versus worker PCM.
- Run device/tests/dense-recording.py for the 112 BPM dry case, the 137 BPM
  wet visual scenario (seed 220507) and the 32-bar 144 BPM case (seed 78893).
- Run the scripted host regressions, native package audit, Studio/WASM and OTA
  parser/pull checks. The host simulates mailbox completion synchronously.
  It cannot measure live CPU overlap, actual DMA underruns or voice fallback
  rates on the FM-1.

### Host comparison

Three 10,000-block x86 WASM passes used the same preloaded dense dry pattern.
Median run means were 74.85 µs on released v0.1.14 and 76.70 µs on the
candidate. Median run P99 values were 278.49 and 342.15 µs. Median run
maxima were 2,259.69 and 2,358.00 µs. The passes are noisy and give no
consistent speedup. WASM does not execute the native dual-core scheduling
changes. These measurements cannot establish an FM-1 improvement.

Candidate identity FM-1_75000115. FWSC SHA-256
a39f7408eff209e675a98c9f86aa7d4abc6a9b1d64df25b2434b553f405adbe3.
Audited app 153,716 bytes; .data+.bss 58,980 / 98,304 and pool
328,612 / 344,064 bytes.

## Gate and resume

The archive is an engineering checkpoint. The website remains on the
verified v0.1.14 TESTING release. The owner requested no further incremental
publication while the core remains unqualified. Native timing hardware is
not available in this workspace. Zero new deadline misses/XRUNs, actual
P99/peak headroom, and a complete architecture qualification remain open.

Resume from the v0.1.15 backup branch. Keep all prior releases and the V7
bank reader. Use the serial profile and worker counts to guide the next
design comparison. Compare a bounded persistent worker/FX pipeline against
this candidate under the same dry and wet musical workloads, then qualify
the worst callback, new XRUNs and fallback cost on an FM-1 before promotion.
