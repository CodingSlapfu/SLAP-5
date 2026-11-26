# SLAP-5 v0.1.44 experimental qualification

## Hardware report and audio policy

The owner reports that v0.1.43 passed Core 1 startup and ran online before a later BARRED state with FX LATE, one fallback and continuing DMA XRUNs under the saved five-track JUNGLE project. The supplied images include the Core 1 diagnostics page. Fine numeric readings aren't legible enough in the available copies to quote. The reported retirement reason makes a late native track FX job the leading path to loss of Core 1. The exact effect chain, callback pressure and timing that caused the initial late completion remain open.

v0.1.43 handed up to two mutable track FX chains to Core 1. It checkpointed the state before dispatch, rendered other chains locally, and waited within the callback cutoff. If the worker was still busy, it had to be held before its live state could be restored and rendered again. Earlier dispatch or a larger wait would still leave that ownership problem.

v0.1.44 runs all live track FX on Core 0 in playback order. Native FX batching, dispatch admission, checkpoint, handoff, bounded FX wait and rollback are bypassed entirely. No FX worker owns the effect state in the native callback. The effect graph, sample order and wet mix stay intact. Core 1 continues to render eligible voices with its existing private-scratch recovery and guarded boot retry. In the host test build only, SLAP_TEST_LEGACY_FX_WORKER selects the old dispatcher so forced timeouts and rollback PCM parity remain covered. The default host policy matches the native dispatch policy.

This eliminates the reported FX retirement route by design. It adds Core 0 FX load compared with successful parallel FX execution. The net FM-1 deadline margin and sustained Core 1 operation under the owner's saved project are unverified. Voice timeout, a mailbox misread and platform fault guards remain in force.

## HOME and readability

- Small labels at 10 and 12 pixels use the bundled OFL Barlow Semi Condensed SemiBold face. The existing compact font slots apply throughout HOME, GLOBAL, SETTINGS, MIXER, SOUND LAB and the patch, synth, envelope, LFO, FX, expression, browser and utility panels wherever those slots are used. Display titles retain the existing SLAP Block face.
- HOME displays patch and kit names up to eight visible characters. Short names are unchanged. Long names are packed without spaces, then limited to eight characters. Full names remain in the browsers and project data.
- The selected lane uses a stronger bounded pulse in its own gradient, with nearly white lane-tinted type. The right-edge meter and contrasting note bars stay legible.
- A thin animated strip uses subdivisions to show current bar or measure progress and the selected track's own loop progress. It updates as an independent 240×11 region in two-pixel increments. Transport stop clears the fill. The compact transport and footer panels use dark complementary blue surfaces.
- LEVEL, STEPS, PAN and SWING each have an arc, pointer and readable value. PAN fills from its center. The existing four persistent MIXER styles remain available.

## Software qualification

[Native, host, settings, dense recording, backup digests and rollback](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45897020383), [browser](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45897020417), [pinned DSP comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45897020358), and [release and served installer verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45897199511) passed.

The legacy FX fault injection retires the host worker and preserves PCM through rollback. The same five-wet-chain fixture in the default worker policy produces matching PCM, zero FX worker jobs or fallbacks and more than 128 voice jobs with Core 1 online. The recorded JUNGLE host fixture also passes save/reload and stop/restart. A host schedule cannot reproduce the physical DMA or LCD interrupt timing.

| Host MIDI mode | Median wall P95 µs | Median wall P99 µs | Median wall peak µs |
| --- | ---: | ---: | ---: |
| Receive | 167.33 → 169.70 | 177.08 → 179.34 | 233.99 → 241.90 |
| Transmit | 222.15 → 224.29 | 237.11 → 239.64 | 247.25 → 252.99 |
| Receive and transmit | 166.28 → 168.16 | 179.87 → 182.00 | 244.71 → 250.59 |

Results are v0.1.43 → v0.1.44, with zero median host budget overruns. Versioned application uses 572,888/581,564 bytes (+1,328), static RAM 87,896/98,304 bytes (−1,312), and pool 335,568/344,064 bytes (unchanged).

- [HOME before and after on the same playing JUNGLE fixture](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44-HOME-before-after.png), v0.1.43 left and v0.1.44 right. [Full framebuffer archive](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44-framebuffer.zip).
- [Live experimental installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html). Application source `da17fe35bbde5b41865c896cfbcbd98f5c2786a0`, firmware identity `FM-1_75000144`.
- [Standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44.fwsc) SHA-256 `18ed006e09a75678aca7d9ef968b34d84b9f09956ed45f251538cacf7bbdd553`.
- [Verified engineering backup](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.44-hardware-testing/SLAP-5-0.1.44-engineering-backup.tar.gz) SHA-256 `c2eae900974f384560c098a78e8233b275ac58ba7d5469e186356cde4f9e3a51`.

## Physical acceptance

Use a copy of the saved five-track 155 BPM, 60% swing JUNGLE project. Record GLOBAL Core 1 startup and mailbox counts before K3. Overdub the same dense passage with SLICER and LIVE FX, switch tracks to exercise the pulse and progress display, then stop and restart. Capture GLOBAL, CORE 1, SPIKE PROFILE and DMA TRACE before rebooting if playback degrades. Require ongoing mailbox progress, no unexpected BARRED state, zero new deadline misses and XRUNs, and peaks below 5,804 µs with usable headroom. Verify LCD legibility at playing distance. Preserve the v0.1.43 firmware and project copy as immediate rollback.
