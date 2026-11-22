# SLAP-5 v0.1.29 — EXPERIMENTAL FM-1 HARDWARE TESTING

This release is for controlled FM-1 testing. Physical audio stability, Core 1 behavior, EDIT distortion, HOME tearing, encoder response and USB flashing still require device results.

- Source `2df799cfa0320bf3c6de97777b41b7b86fe642c3`
- Firmware identity `FM-1_75000129`
- Package SHA-256 `52aa1473d14caea60f0f83245f612579bdb6f05085bb68aea1e39f785c24d745`
- [Separate experimental browser installer](https://codingslapfu.github.io/SLAP-5/flash-testing-0.1.29.html)
- [On-device test checklist](https://codingslapfu.github.io/SLAP-5/hardware-test-0.1.29.html)
- [Verified previous 0.1.26 package for M-Flasher rollback](https://codingslapfu.github.io/SLAP-5/SLAP-5-0.1.26.fwsc)
- [Verified engineering backup and benchmark logs](https://chatgpt.com/library?file_id=libfile_0cdd0abd3ce481919b1a9f79e1c71395&account_id=personal)

The exact native rebuild matched the verified package byte for byte. The browser OTA harness checked package size, SHA-256 and identity, rejected damaged or incompatible packages, simulated a mid-update disconnect and required the expected identity after reboot. Loader simulations covered interrupted installation and rollback through v0.1.26 and verified protection of the boot and song/settings regions. The permitted application write window is `[0x4000, 0x93000)`. The loader also uses its dedicated update-record sector.

The experimental browser installer accepts v0.1.26 on the supported hardware configuration. Other configurations can follow the read-only checks and recovery guidance for M-Flasher. Back up songs first. A V12 save with active performance filters may be unreadable on older firmware. Use a disposable project for save/load testing.

The prior host tests do not measure native Core 1 performance. Inline host tests measure callback CPU use. Threaded host trials entered fallback in both revisions. Physical timing is the purpose of this release.
