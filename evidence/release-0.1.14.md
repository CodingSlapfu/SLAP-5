SLAP-5 v0.1.14 engineering TESTING for compatible M-VAVE FM-1 hardware. Copyright Ziao.

- A seeded 32-bar test exposed voice batches crossing a sequencer step when the countdown reached zero. The corrected batch boundary now gives identical worker and single-core output in that test.
- Voice allocation balances estimated engine costs. Core 0 uses active voice lists and renders independent voices in blocks. Both cores read the same interpolated sine table in SRAM. Tracks with all FX stages off bypass the FX kernel away from the inspection pages.
- Press GLO again for the worst callback stage breakdown. K3 resets the window. The breakdown reports voice and FX batch wall time, successful Core 1 work, worker wait and unclassified remainder.
- Spectrum analysis consumes decimated samples outside the audio callback in bounded chunks. The meter telemetry and all eight visualizers remain available.
- Four independently editable melodic notes per step. Recorded holds have independent durations at 128-sample resolution (about 2.9 ms at 44.1 kHz). Quantization changes onset while preserving the held interval. Replay suppresses overlapping instances of the same sustained event at loop wrap. Studio displays the held length and edits it in milliseconds.
- Firmware reads V2 through V6. A bank containing recorded durations uses V7 with a capacity of 440 sparse notes. Four-lane banks without explicit holds use V6 with a capacity of 587 notes. Back up existing banks before testing. v0.1.13 can open V2–V5 banks. The active song stays in RAM when a bank cannot fit the notes.
- Record quantize offers Off and 25%, 50%, 75% or 100% strength on straight 1/8, 1/16 and 1/32 grids, with 50% at 1/16 as the default. Studio uses the same quantizer and reads earlier project versions.

Host seeded five-track recording and replay, native package audit, simulated OTA and Studio migrations validate the engineering build. The host worker invokes its jobs synchronously. Physical callback improvement and DMA stability require the owner's FM-1 acceptance.

Firmware identity `FM-1_75000114`. SHA-256 `8cf3240a239e92eee2b7fef2c9ef5fec6803a3444ebb035faf5e422484bb2689`.
