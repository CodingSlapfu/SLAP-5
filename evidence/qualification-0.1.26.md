# SLAP-5 0.1.26 publication and desktop performance follow-up

GitHub prerelease target: `611d0c8afe89c6ef3565a8c244f2346bcbbafb8e`. Firmware and matching Studio were taken from the existing candidate backup at `d968397c6ba31aeb98fc0fa65e34c8d4343aa55b`. GitHub Actions run 44931275518 passed the publication job, updater simulation, package and rollback validation, and re-downloaded asset checksums. The live experimental installer, firmware, Studio ZIP and emulator were checked against the verified files. Primary manifest remains 0.1.18. Previous 0.1.25 remains available for rollback.

Hosted benchmark bootstrap failed because the child Python was resolved from PATH outside the prepared environment. The completed benchmark below ran in the desktop workspace where child Python could import both dependencies. The main run used five alternating trials per build, tempo and mode. The host-only glide control used two alternating trials with the candidate's note-on glide trigger disabled in the comparison build. The control started while the main run was finishing 160 BPM single-core, so that row may include additional shared-host contention. The glide control shows no improvement when the trigger is disabled. It does not identify a causal mechanism.

```
Thread CPU time, steady playback
patch-switch-112 worker baseline   mean/P95/P99/peak µs   190.80 / 350.69 / 552.00 / 2677.66
patch-switch-112 worker redesign   mean/P95/P99/peak µs   186.60 / 332.37 / 500.31 / 1516.04
patch-switch-112 single baseline   mean/P95/P99/peak µs   197.44 / 370.05 / 593.37 / 3192.93
patch-switch-112 single redesign   mean/P95/P99/peak µs   195.19 / 361.86 / 572.98 / 2860.72
patch-switch-137 worker baseline   mean/P95/P99/peak µs   182.36 / 315.14 / 435.10 / 2007.23
patch-switch-137 worker redesign   mean/P95/P99/peak µs   183.78 / 330.21 / 492.25 / 2002.64
patch-switch-137 single baseline   mean/P95/P99/peak µs   180.65 / 321.45 / 432.87 / 1393.88
patch-switch-137 single redesign   mean/P95/P99/peak µs   185.44 / 325.18 / 507.96 / 1958.09
patch-switch-160 worker baseline   mean/P95/P99/peak µs   180.59 / 305.29 / 446.13 / 2355.10
patch-switch-160 worker redesign   mean/P95/P99/peak µs   197.81 / 356.12 / 581.74 / 2147.20
patch-switch-160 single baseline   mean/P95/P99/peak µs   183.77 / 309.89 / 427.33 / 1030.65
patch-switch-160 single redesign   mean/P95/P99/peak µs   180.35 / 303.98 / 445.63 / 1828.32
Thread CPU time, steady playback
patch-switch-137 worker baseline   mean/P95/P99/peak µs   175.63 / 274.32 / 396.02 / 3961.24
patch-switch-137 worker redesign   mean/P95/P99/peak µs   184.30 / 292.61 / 441.75 / 6391.17
patch-switch-137 single baseline   mean/P95/P99/peak µs   171.52 / 285.96 / 361.06 / 1278.56
patch-switch-137 single redesign   mean/P95/P99/peak µs   186.80 / 327.66 / 546.39 / 2144.96
{
  "identity": "FM-1_75000126",
  "packageBytes": 611488,
  "packageSha256": "d21ee062768ecdbb50d12ee162cc18b4c5fcc12860e2fa24ce7e26a41dac8c2c",
  "appBytes": 555600,
  "appSlotBytes": 581564,
  "loaderBytes": 8332,
  "flashImageBytes": 602112,
  "appWriteStart": "0x4000",
  "appWriteEndExclusive": "0x93000"
}
```

The 137 BPM worker tail was higher in both desktop comparisons of 0.1.25 versus 0.1.26. At 160 BPM the five-trial worker P99 also rose despite a lower peak. These measurements cover host callback thread CPU time. CPU1 contention, XIP stalls, display DMA, polling latency, and stack high-water still require separate physical captures. The firmware contains existing callback and worker diagnostics. Hold GLO for five seconds, then capture callback percentiles, deadline misses, XRUNs, worker wait/late/recovery, DMA trace, and C0/C1 engine spans. Capture the same song, page and patch-change sequence on 0.1.25 and 0.1.26. No new physical probes were included in this release.

The existing verified interface changes ship unchanged in this experimental package. The firmware-wide typography and FM6/wavetable graph audit, higher-detail Dancer, and physical tearing checks remain open.
