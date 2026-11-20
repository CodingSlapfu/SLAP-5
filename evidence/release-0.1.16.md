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
mapped to the SRAM text region fits the linker budget.

Package size is 611,488 bytes. SHA-256 is
`ad54f3b64f7ab730b6f63584908b11b713f610fda6e3f93395bdfe3234dec6e1`.

The software suite covers dense dry and heavy-FX playback, polyphonic
recording and replay through 32 bars, sustained notes, quantization,
arpeggiator boundaries, worker rejection, late voice and FX recovery,
all 128 patches, all 56 available kit IDs, all eight visualizers at four
master levels, Studio projects and the direct updater transaction.

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
   DMA XRUNs and stack high-water. The audio deadline for each 256-frame DMA
   half is about 5.8 ms. Success requires zero new XRUNs and deadline misses
   during the stress and soak runs, with audible notes and feedback intact.
4. Check that master zero silences the output while all eight visualizers
   continue moving. Reload a backup of a legacy song bank before recording
   over it. Keep the backups for rollback because banks using the new
   synthesis or kit IDs require this firmware.
