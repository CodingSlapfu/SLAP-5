# SLAP-5 0.1.16 corrective TESTING

M-VAVE FM-1 firmware identity `FM-1_75000116`. This prerelease is for supported
1 MiB `0x856014` hardware with the established V15-style application layout.
FM-1 callback latency, hardware overlap, DMA underruns, stack high-water
marks and installation recovery still require device validation.

## Audio engine

- Replaced Adaptive Core 1 assignment with deterministic per-span voice
  partitioning and private per-core track buses. The sequencer and arpeggiator
  set sample-accurate span boundaries. The driver retains its 256-frame DMA
  halves, with up to 64 samples per DSP span.
- Retired the worker on a late or invalid result. Core 0 uses its untouched
  voice states to complete that span once and renders subsequent spans via
  a single live bus. Parallel effects use independent track state and
  bounded rollback. Core 0 owns final mixing and DMA.
- Placed the synth kernel and its oscillator dependencies in SRAM. Reduced
  per-sample drum decay calculations and bypassed wet-mix work for dry tracks.
  Kept the existing incremental GATE/SLICER step and sample clock.
- Updated the factory FM, plucked strings, pads, acoustic piano, woodwinds and
  flute models. Synth engine and patch IDs are unchanged. Saved projects
  carry the legacy synthesis when they predate this build. Choosing a
  factory patch on a melodic track selects the new synthesis for that track.
- Added 24 primary drum kits and kept IDs 0–31 for older saved songs. The
  16 GEN DRUMS styles are independent of the selected kit. Studio and the
  emulator read the same kit table as firmware.

## Verification

The native image is 156,344 bytes. Data/BSS is 55,140 of 98,304 bytes.
The pool uses 312,420 of 344,064 bytes, leaving 31,644 bytes. The code
mapped to the SRAM text region uses 14,124 of 32,768 bytes.

Package size is 611,488 bytes. SHA-256 is
`ad54f3b64f7ab730b6f63584908b11b713f610fda6e3f93395bdfe3234dec6e1`.

The software suite covers dense dry and heavy-FX playback, polyphonic
recording and replay through 32 bars, sustained notes, quantization,
arpeggiator boundaries, worker rejection, late voice and FX recovery,
all 128 patches, all 56 available kit IDs, all eight visualizers at four
master levels, Studio projects and the direct updater transaction.

### Desktop audio callback comparison

Pinned host timing compared the v0.1.15 engineering checkpoint and the
0.1.16 candidate using five alternating runs per workload. The numbers are
medians of five runs over the last 1,600 playback callbacks, in microseconds.
The host executes worker calls synchronously. These values do not measure
concurrent hardware execution, flash timing or the FM-1 DMA deadline.

| Workload | Source | Mean | P95 | P99 | Peak |
|---|---|---:|---:|---:|---:|
| Dry, simulated worker | 0.1.15 | 64.15 | 102.54 | 166.51 | 1,246.01 |
| Dry, simulated worker | 0.1.16 | 56.13 | 77.18 | 135.42 | 369.94 |
| Heavy FX, simulated worker | 0.1.15 | 96.06 | 140.43 | 208.86 | 872.60 |
| Heavy FX, simulated worker | 0.1.16 | 88.46 | 124.82 | 174.97 | 370.98 |
| Dry, single core | 0.1.15 | 60.24 | 89.57 | 141.28 | 383.86 |
| Dry, single core | 0.1.16 | 54.11 | 73.48 | 122.22 | 421.98 |
| Heavy FX, single core | 0.1.15 | 89.72 | 121.64 | 166.38 | 371.11 |
| Heavy FX, single core | 0.1.16 | 86.50 | 119.80 | 167.66 | 334.14 |

Reproduce with `python3 device/tests/compare-dsp-timing.py bd43dd9 HEAD`.
Worker and single-core PCM agree within one 16-bit rounding unit in the
dense and forced-failure fixtures. Legacy dry and wet workloads agreed
byte for byte between the old and new WebAssembly engines. All 192
legacy kit and percussion combinations rendered byte-identically.
The 24 new kits were checked for onset RMS, decay and spectral centroid.
The new kick centroids spanned 33–100 Hz. New snares spanned about 4.5–10.6 kHz.
The tests establish acoustic differences. Musical preference remains
subject to listening on the unit.

Implementation references were AQUORA `dc188d7` for bounded frozen-state
worker jobs and SRAM placement, and SLOOP 3.4 `2228313` for independent
core accumulators and fixed-size scheduling. The SLAP-5 implementation
uses original source. Chromium CI passed on source commit `08a1543`.

## Physical FM-1 acceptance

1. Back up the unit and song banks. Confirm support and install with
   [SLAP FLASH](https://slap-5.jminie66.chatgpt.site/flash). Check
   `FM-1_75000116` after restart. Keep the 0.1.14 rollback available.
2. Reset GLOBAL statistics with K3. Run the same dense 112 BPM dry recording
   and 137 BPM wet recording with 16 synth voices, drums and all five
   GATE/SLICER chains. Repeat with a visualizer open. Include held chords,
   overdubbing, rapid parameter changes and playback after disarming REC.
3. Record GLOBAL summary, peak breakdown, worker counters and CORE TIMING
   after each run. Check mean, P95/P99, peak, core overlap, wait, recovery,
   DMA XRUNs. Capture stack high-water separately with hardware instrumentation. The audio deadline for each 256-frame DMA
   half is about 5.8 ms. Success requires zero new XRUNs and deadline misses
   during the stress and soak runs, with audible notes and feedback intact.
4. Check that master zero silences the output while all eight visualizers
   continue moving. Reload a backup of a legacy song bank before recording
   over it. Keep the backups for rollback because banks using the new
   synthesis or kit IDs require this firmware.
