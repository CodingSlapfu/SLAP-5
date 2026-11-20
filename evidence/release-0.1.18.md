# SLAP-5 0.1.18 corrective TESTING

Firmware identity `FM-1_75000118`. Physical acceptance remains pending. The owner confirmed that 0.1.17 started Core 1 successfully and recorded 133,214 completions out of 133,215 mailbox requests. During dense chord playback one voice job missed its result window. Core 0 recovered the job and barred Core 1. Subsequent stress readings showed a 6,761 µs peak, a 6,075 µs voice batch, and thousands of misses and underruns against a 5,804 µs budget. That establishes the retirement sequence. The duration and source of the first missed job were unavailable from 0.1.17.

## Correction

- Core 0 rerenders an isolated late voice job into its independent track buffers using the live voice state. The mailbox and private scratch remain untouched until completion is verified. The obsolete result is discarded. Voice and FX dispatch resume after the mailbox is safe.
- The 240 µs voice wait cap and aggregate cutoff are retained. A worker still busy one callback budget after timeout, or late again before 16 successful voice jobs, is retired. Startup, hardware fault, bad-mailbox and FX rollback protections are retained. The bounded recovery allows the single late-job case to avoid permanent retirement without restoring adaptive assignment.
- Fixed cost buckets select the largest eligible voices that fit the Core 1 budget. Track-contiguous buffers reduce the stride in the inner voice loop. A sustain loop for an eligible unmodulated synth voice keeps the envelope in registers and hands the exact release frame back to the general renderer. Voice count, note timing, patch IDs, chord behavior, drum recipes and saved-bank formats are preserved.

## Software verification

Forced isolated lateness resumed at 245 completed voice jobs after the test's fourth job. Two closely spaced misses retired the worker at four completed jobs. Both recordings passed PCM parity. All scripted host cases passed. The 112 BPM chord and 137 BPM FX captures matched 0.1.17 byte for byte in the single-core comparison. Real desktop threads completed 32,053 dry jobs and 54,166 wet jobs with measured aggregate overlaps of 171,877 µs and 219,990 µs respectively. Threaded and single-core PCM matched within the harness tolerance. Those host timings include OS scheduling effects. A separate threaded dry trace drained five late completions and retired after a sixth missed window. This exercises the sustained-overload protection alongside the isolated-miss regression.

The 611,488-byte package SHA-256 is `7b617de2c653e0507eeec342a1564d479b8dee9f4271da421910ef1889170c7e`. The pinned SDK inputs were recovered from the verified rollback archive and matched all three reference hashes. The audit rebuilt the package byte for byte. Native data/BSS and pool usage were 55,492/98,304 and 312,420/344,064 bytes.

### v0.1.17 versus v0.1.18 desktop chord benchmark

Five runs per mode. Values are median run statistics in microseconds. Worker mode here performs mailbox work synchronously on the host for reproducibility. The separate threaded tests above verify actual overlap and PCM.

| Load | Mean old → new | P99 old → new | Peak old → new |
|---|---:|---:|---:|
| Dry, worker | 52.99 → 51.63 | 157.14 → 128.61 | 546.67 → 569.56 |
| Dry, single | 55.74 → 53.26 | 172.74 → 145.16 | 1,169.73 → 996.65 |
| Wet, worker | 87.17 → 83.98 | 204.37 → 190.64 | 398.69 → 675.03 |
| Wet, single | 86.59 → 85.41 | 198.41 → 216.59 | 2,378.26 → 1,202.27 |

Reproduce with `python3 device/tests/compare-dsp-timing.py v0.1.17-testing v0.1.18-testing --runs 5 --chords`. [Chromium CI](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44706798056)

## Physical acceptance

After a cold boot, confirm CORE 1 STATUS reports PASSED and ONLINE. Reset diagnostics and repeat the 112 BPM chord recording, then the 137 BPM wet playback. In DISPATCH, capture **VOICE JOB MAX US** and **LATE DRAINED**. In CORE TIMING, capture overlap, wait and recovery. Also record callback P99 and maximum, deadline misses and DMA XRUNs. A single drained job does not pass the audio deadline gate by itself. Acceptance requires clean 16-voice audio, sustained Core 1 participation, zero new misses and XRUNs, and a worst callback below 5,804 µs. Peak and P99 timing on the device, including service after a hardware retirement, remain unverified.
