# SLAP-5 0.1.26 Experimental Hardware Testing

Firmware identity `FM-1_75000126`. This build uses the archived corrective candidate from `d20625dc89b60fb122d1a1cd893f8502cf49bc46`. Physical FM-1 timing and USB recovery remain to be measured.

EDIT opens FM6 and wavetable graphs directly. HOME shows all five sequences and audio meters. The step editor shows note names, and Digital VU shows STEP within the bar. The 256-frame audio path, factory IDs, song compatibility, and eleven momentary black-key effects are preserved.

The native build uses 555,600 of 581,564 application bytes, 87,288 of 98,304 static RAM bytes, and 328,648 of 344,064 pool bytes. See the [candidate evidence](https://github.com/CodingSlapfu/SLAP-5/blob/d968397c6ba31aeb98fc0fa65e34c8d4343aa55b/evidence/continuation-0.1.26.md).

## Physical test

1. Back up projects and retain the known-good 0.1.25 package. Use compatible V15-style hardware with the verified app-only layout.
2. Confirm `FM-1_75000126` after reboot. Load a backed-up song and verify all five tracks.
3. At 112, 137 and 160 BPM, run a dense five-track arrangement while overdubbing. Switch among EUPHORIA, FM SHIMMER and wavetable patches. Exercise slicers, inserts and Vinyl Stop.
4. Record callback peak, P95/P99, deadline misses, XRUNs and worker fallback counts from Diagnostics when available. Note the page, tempo and patch when audio or controls stutter.
5. Check five-lane HOME, FM6 and wavetable screens during playback for tearing and clipping. The full typography audit and higher-detail Dancer remain open.
6. If testing fails, restore the owner-tested [0.1.25 experimental package](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.25-hardware-testing) using the verified device-specific recovery route.
