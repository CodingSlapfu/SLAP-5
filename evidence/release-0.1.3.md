# SLAP-5 0.1.3 engineering TESTING

This release contains real native FM-1 update package bytes. Physical installation is pending a supervised device-specific preflight. Keep the public installer disabled. The application and loader binaries are inspection assets; the FWSC is the complete OTA package.

- Identity: FM-1_75000103.
- Added REC-armed keyboard and MIDI note-on overdubbing during PLAY, quantized to the nearest step. REC captures independent melodic notes up to the existing two-note capacity and writes drum hits into their respective lanes. A full melodic step is preserved. Stopping PLAY disarms REC.
- Host groovebox, page, storage, arpeggiator, polyphonic and recording tests passed. Native package reproduction, WebAssembly smoke and simulated installer verification passed. See the versioned evidence in the source tree.
- The target uses the existing AC791N firmware layout, hardware chip key 980F, JEDEC flash 856014, and a compatible V15-style app partition. Verify the actual bootloader and source firmware identity first. Source labels alone do not establish compatibility.
- Remaining work includes recording count-in and replace modes, arrangement, ties, full LIVE FX, MIDI clock and project transfer, expanded editing, and dual-core timing validation.

## AQUORA migration

The app-only loader writes [0x4000,0x93000). Both apps use a staged loader above the application; the loader also clears recognized update records. On the reviewed AQUORA source, regular banks begin at 0x97000, favorites at 0xA0000, and 12 favorite pairs occupy through 0xB7FFF. SLAP-5 song storage occupies 0xC0000–0xCFFFF. The two apps share the panel-calibration region. Confirm the actual installed AQUORA build and its storage map before testing; keep the verified same-device backup and its known-good firmware package available.

The first physical test can be supervised once the baseline identity, chip, bootloader, backup and usable recovery path are confirmed. Do not deliberately interrupt an update on the only unit. Use the USB-only read-only steps in [hardware qualification](https://github.com/CodingSlapfu/SLAP-5/blob/main/evidence/hardware-qualification.md) and share the identifying results without sending the raw flash dump.
