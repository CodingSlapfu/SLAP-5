# SLAP-5 0.1.22 performance hardware test

**Experimental prerelease. The new performance controls and renderer still need verification on a physical FM-1.**

- **Versioned firmware** `FM-1_75000122` · 611,488 bytes.
- **Firmware SHA-256** `a5fed987e30938862f78dbfbcc6a5b512900879f910930c77089028a78464c79`
- **Studio ZIP SHA-256** `cf56b31afa251d28e1d2a77791530eb4e333790ae4b167e8069ed3f874455116`
- **Verified source** [b0646de](https://github.com/CodingSlapfu/SLAP-5/commit/b0646de221264190f91ebab729859ccd547663f0) · [Hosted Studio tests](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44868387750)

The verified controls are documented in the [manual](https://github.com/CodingSlapfu/SLAP-5/blob/b0646de221264190f91ebab729859ccd547663f0/README.md#performance-controls). Hold EDIT for two seconds to open MIXER. Tap GLO for SETTINGS or hold GLO for five seconds for DIAGNOSTICS. REC actions are available directly from HOME and SELECT hands off an active take to the next track. The new full-mix LIVE FX, assignable knobs, 16/32/64 loops, step modes, six visualizers and shortened Reactor intro are in this candidate.

## Install and rollback

Use M-Flasher with the supported V15-style FM-1 layout, 1 MiB `0x856014` flash and chip key `0x980F`. Keep the same unit's verified backup and independent project exports. Confirm that M-Flasher's plan is app-only `0x4000–0x92FFF` and preserves user data and the bootloader. Do not proceed past the read-only screen on an unrecognized layout.

Restore the original same-unit backup if needed, or use the original [v0.1.21 hardware-test package](https://github.com/CodingSlapfu/SLAP-5/releases/tag/v0.1.21-hardware-testing) for firmware-only rollback. Song slots saved with ratchet or Fill Only metadata use format V9. Keep separate backups for older firmware. The primary website installer remains v0.1.18 and its recovery paths remain available.

The Studio ZIP includes the matching Studio and emulator, without an updater. Unzip it, serve the directory with a local HTTP server such as `python3 -m http.server 8764`, and open `http://localhost:8764/`.

## Hardware checklist

1. Record continuously while moving SELECT through all five tracks. Exercise REC undo, clear loop, clear track and cancel while older tracks continue to play. Hold EDIT during the take, verify the mixer white-key LEDs and mutes, then return to HOME.
2. At **112, 137, and 160 BPM**, run 16 voices, all 16 drum lanes, five slicers and five inserts. Exercise all eleven LIVE FX in each scope, switch scope while a key is held, and change the four assignments.
3. Compare HOME and all six visualizers. Check note highlights, beat/bar counters and PLAY LED timing. Measure LCD frame time and input delay if instrumentation is available. Reset SYSTEM statistics for each pass and record callback peak/P95/P99, DMA XRUNs, missed deadlines, worker fallbacks and the Core 1 stack high-water.
4. Verify an older project, a new V9 song with step modes, a spare-slot save and reboot, and the supported recovery path. Full public promotion and a new official browser-install manifest depend on these physical results.
