# SLAP-5 development checkpoint

Copyright (C) 2026 Ziao.

This checkpoint contains a five-track sequencer for the M‑VAVE FM‑1, a host simulator, and an emulator built from the same application source. The native build is a development build. The packager reported SDK reference mismatches and physical compatibility is unverified. Keep this checkpoint off the device.

## Controls

| Control | HOME | SEQ |
| --- | --- | --- |
| SELECT | Select track 1–5 | Select step |
| ALGO | Set BPM | Set BPM |
| Knob 1 | Pan | MIDI pitch |
| Knob 2 | Pattern length, 1–64 steps | Velocity |
| Knob 3 | Level | Note gate |
| Knob 4 | Swing | Hit probability |
| Keys | Audition the selected track | Insert or toggle a note in the selected step |
| PLAY | Start or stop the sequencer | Start or stop the sequencer |

HOME returns to the main sequencer. GLO opens the global view, where SEL toggles mute for the selected track. OCT− and OCT+ transpose the keyboard. The four pitched tracks have fixed voices at this checkpoint. Track 5 has a synthesized drum voice. A demo pattern is loaded at boot and edits are held in memory. USB MIDI channels 1–5 address the five tracks.

## Build and verify

From the source root:

```sh
python3 vendor/fm1-base/tools/build.py device --host
device/build/host/slap-5_host device/tests/groovebox.txt device/build/host
python3 vendor/fm1-base/tools/build.py device --web
node device/tests/emulator.mjs
```

The web build requires Emscripten on PATH. To open it locally, run `python3 -m http.server 8000 --directory device/build/emu` and open `http://localhost:8000`. Start the emulator to enable audio, then press PLAY on the panel.

The native development build requires the JieLi toolchain (`JIELI_TOOLCHAIN`, the directory containing `pi32v2` and `common`) and an AC79 SDK checkout (`AC79_SDK`). Invoke `python3 vendor/fm1-base/tools/build.py device`. The tested toolchain bundle was jieli-linux-toolchains-20260730.1. The SDK assets were the three files identified in `evidence/sdk.sha256` and used by the previous AQUORA workspace. The packager expected different hashes for `uboot.boot` and `cfg/eq_cfg_hw.bin`. SDK assets and the resulting update package are excluded from this checkpoint.

## Provenance and remaining work

The AQUORA v0.7.38 remote source ref was verified at `238a57be426805bbd9292b956315d54a68095bc0`. The board support snapshot came from the local AQUORA integration at `d2c5087`. Its fm1-base and felucca-bsp licenses and notices are vendored. Two AQUORA-specific hooks were removed from the local copy of the board support code. The SLAP-5 app and demo pattern are separate implementations.

The host and WebAssembly logs cover basic transport, track selection, pattern length, step entry, pitch edits and audible playback. The native build log records image size and static checks. On-device timing, compatibility, updates, installation and rollback still require validation.

Remaining scope includes the full synthesis and patch library, advanced melodic and drum sequencing, routing and effects, recording, persistence, wireless MIDI and sync, the custom font and boot animation, visualizers, on-device diagnostics, and SLAP STUDIO, SLAP FLASH and the public site. GitHub returned 404 for CodingSlapfu/SLAP-5 and the repository creation attempt could not authenticate.
