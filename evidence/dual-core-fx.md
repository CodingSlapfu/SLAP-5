# SLAP-5 parallel FX candidate

Source baseline: SLAP-5 main at 0b192665693eea9cf54cd94dc5d2d09ee6122eb1.
Published baseline: v0.1.11-testing at e8172330923058b7e4f8322d5d9e7a972ee29d0c.

## Reference implementations

- AQUORA's v0.7.38-testing experimental path passes immutable oscillator input records to independent local and remote FM job buffers. Its inner FM renderer is RAM-resident. Eligible synthesis runs on both cores before the audio mixer consumes the results. It leaves voice releases local, bounds readiness checks, halts a failed worker and consumes local fallback data. Its visual-worker build and experimental audio-worker build are mutually exclusive.
- SLOOP 3 at 2228313f3abe0ad6c8f28ef0cd9b9d8167c01330 partitions voice slots and accumulates samples in two distinct per-track buses. The audio callback adds the buses, then runs track effects, sends and master processing in order. Its CPU1 mailbox validates the function, argument and request identity, fences publication and halts the worker when completion fails. SLAP-5 uses original DSP and the existing fm1-base platform mailbox.

## SLAP-5 execution and fallback

Core 0 owns DMA, transport, MIDI, melodic voice rendering and master mixing.
Core 1 renders a copied drum voice subset into its own dry bus. For qualifying
effect-heavy blocks, Core 0 and Core 1 then process distinct track FX chains
simultaneously. The dispatcher estimates work from enabled effects and gives
Core 1 up to two tracks while keeping the trace clock on Core 0. Both return
sample-aligned results before the existing wet mix, automation and limiter.
The active-mask weights are engineering estimates pending device profiling.

Core 0 checkpoints the remote FX state and the ring entries the worker may
overwrite. The job owns its tracks through completion. On failure, Core 0
halts Core 1, restores the checkpoint and renders the same samples locally.
This preserves feedback history, gate position and per-track trace writes.
The block does not co-run with visual drawing on the worker. The main-loop
visualizer frame interval increases under measured DSP pressure.

The slicer computes its step index at clock changes and advances across
sample boundaries by comparison and addition. It retains the existing
pattern phase and stutter-buffer addressing. The 32 kit IDs and bank format
are unchanged. The first four kits retain their original signal path.
The other 28 combine six additional synthesis mechanisms for kick, snare,
clap and metallic percussion with their existing circuit and parameter sets.

## Reproduction and limits

- Host base regressions: run all device/tests/*.txt with the host simulator.
- Worker parity: sh device/tests/dual-core.sh from the repository root. The
  host mailbox executes synchronously. Three runs compare PCM across the
  FX worker, single-core path and a discarded completed worker result after
  128 successful blocks. This establishes state and sample parity, without
  measuring on-device overlap or latency.
- Factory names: python3 device/tests/patch-names.py /path/to/sloop3-fm1.
- Emulator and Studio: site/build.sh and node device/tests/emulator.mjs.
- Native and OTA: device/build-native.sh --release 0.1.12 with the verified
  SDK. Then run tools/audit-package.py and the simulated OTA transport test.

The owner-reported v0.1.11 figures are 5,793 µs average and 6,720 µs peak
against a 5,804 µs budget, with 3,012 misses and about 3,110 underruns.
Native measurements on an FM-1 are required to compare the workload.
Test a dense five-track pattern with all five slicers and delay, chorus
and reverb armed. Use the same pattern, BPM, master level and measurement
interval on both builds. Test with visuals closed and with Spectrum Reactor,
Solar System, CRT Plasma and SLAP Reactor. Record average and worst callback
time, deadline misses, DMA underruns and worker fallbacks. Audition the kits
individually and in the mix for timbre, dynamics and clipping.
