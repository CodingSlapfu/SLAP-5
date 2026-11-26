# SLAP-5 v0.1.43 experimental qualification

## Hardware report

The owner’s v0.1.42 photos show the saved five-track JUNGLE project at 155 BPM and 60% swing. The SYSTEM frame reads 109% average DSP, 6,902 µs peak, 6,700 µs P95, 6,900 µs P99, 3,450 deadline misses, a 5,804 µs budget and 16 active voices. The XRUN counter is roughly 6.7 thousand in that frame. CORE TIMING shows 118% peak and zero overlap and wait. The engine frame shows NONE for Core 1 and ACOUSTIC PIANO (engine 16) on Core 0. SPIKE PROFILE reads 4,668 µs local voice work, 1,198 µs maximum voice span and 6,893 µs callback. The separate DMA half trace shows 6,452 µs half, 6,449 µs callback and 421 µs nested Timer 5. These captures were taken on different callbacks and must not be summed into a single profile.

The job, overlap and fallback counters can all be cleared with K3. The photos establish no observed Core 1 contribution in those windows. They do not include START RESULT, mailbox lifetime counts or RETIRE REASON, so they cannot distinguish a startup guard, a prior retirement or an online worker without eligible work. The current GLOBAL footer exposes live Core 1 state alongside the resettable job count. The CORE 1 page retains startup result, sent count, jobs since boot and retirement reason. An offline worker and the resulting local voice load are the leading explanation for sustained overload. The initial trigger remains open.

## Audio and platform

- A runtime timeout without a mailbox error or CPU fault still holds Core 1 immediately. A single retry can run on a later clean boot before the audio and timer interrupts start. The retry is consumed before the startup handshake, including if the handshake fails. A second hard timeout or any fault/misread remains barred. No runtime ROM restart or automatic project interruption was introduced.
- Modern acoustic piano caches eight color/motion coefficients at voice start. It reuses the space reserved for FM operator state and keeps oscillator, envelopes, noise and strike behavior. A lone sample-and-hold voice takes the normal block path. Multiple sample-and-hold voices retain sample/slot order for the shared RNG.
- The existing voice and FX fallback PCM tests, five-track dense recording and JUNGLE reload checks still apply. The clean-boot retry must be validated on the FM-1. Deeper ATS, ASP, FX sleeping and Timer 5 scheduling remain open.

## HOME and controls

HOME follows the reference proportions with a 34-pixel status band, five 34-pixel lanes and a 36-pixel control footer. It uses 22-pixel glyphs rasterized from the bundled OFL Barlow Semi Condensed Bold face, lane-colored lettering, shadows, saturated stepped gradients, fine grids, bright note bars, a continuous playhead with head marker and segmented meters. The selected lane pulses through lighter shades of its own hue. Meter-only changes retain strip updates. Actual patches and drum kits, live sequence rendering and the four controls are represented, so the generated frame differs from the reference’s illustrative role names, note arrangement and footer text. The FM-1 framebuffer uses RGB565 and an intentionally bounded gradient with no full-screen backbuffer.

MIXER expands the shared dial ring from a nominal 28 to 42 pixels and adds two-tone analog bevels and higher-contrast pointers. HOME uses labeled numeric cells to preserve the reference composition. Compact subpanels still use the shared compact dial size. GLOBAL status and diagnostics received targeted readability improvements. Knob enlargement and text review across all remaining layouts are open.

## Native-resolution comparison

[Approved artwork](https://chatgpt.com/library?file_id=libfile_89ec94165cfc8191ace2f4fb8e141b09&account_id=personal) and [actual 240×240 firmware framebuffer and four MIXER styles](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.43-hardware-testing/SLAP-5-0.1.43-framebuffer.zip). Open `looper-jungle-240.png` for the playing fixture and `looper-jungle-960.png` for its nearest-neighbor enlargement.

The playing framebuffer shows all five target hues, tall tinted patch lettering, fine vertical divisions, lit meter segments and a continuous playhead. Relative to the reference, the typography has rounded Barlow corners and less slant, the gradients use fewer shades, and the patch names occupy different widths. The status band fits BPM and bar/beat in addition to the USB and MIDI indicators, which makes those indicators substantially smaller than in the artwork. The reference uses a fixed illustrative sequence and the firmware shows the actual project's held notes and scroll position. The footer uses two rows to display all four live values. These differences are visible at native resolution, so reference and readability acceptance remain open.

## Software qualification and release

[Host, native, framebuffer, backup digests and rollback qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45891168607), [browser tests](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45891168497), [pinned 155 BPM host comparison](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45891168586), and [versioned build, publication and live installer byte verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/45891460507) passed.

| Host MIDI mode | Wall P95 µs | Wall P99 µs | Wall peak µs |
| --- | ---: | ---: | ---: |
| Receive | 148.68 → 148.96 | 155.79 → 156.41 | 195.38 → 217.78 |
| Transmit | 143.70 → 144.87 | 163.67 → 165.52 | 282.65 → 219.94 |
| Receive + transmit | 144.06 → 143.52 | 157.78 → 158.29 | 215.79 → 218.06 |

Values are same-run medians, v0.1.42 → v0.1.43. Median host deadline misses were zero. These measurements do not establish native headroom or DMA XRUN status.

- Application source `720683d2e7974274d67163670bd9f9d6fdd0ad53`. Identity `FM-1_75000143`.
- Versioned application 571,560 / 581,564 bytes (+2,536). Static RAM 89,208 / 98,304 and pool 335,568 / 344,064 are unchanged from v0.1.42.
- [Experimental installer](https://codingslapfu.github.io/SLAP-5/flash-testing.html) and [standalone firmware](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.43-hardware-testing/SLAP-5-0.1.43.fwsc), SHA-256 `387a7a96b30e6234411a17eb8c6a7deec11c3b3e7c335f4ef1755ec8170f774f`.
- [Verified source and firmware archive](https://github.com/CodingSlapfu/SLAP-5/releases/download/v0.1.43-hardware-testing/SLAP-5-0.1.43-engineering-backup.tar.gz), SHA-256 `77ab14a61502b7eac335d7dece0c8499662c3352bc67edf5b7cae63581cdcd80`.

## FM-1 acceptance

1. Preserve the exact v0.1.42 project and test on a copy. Keep an untouched legacy copy for v0.1.31. Use v0.1.42 as the immediate rollback if a schema 13 project needs to stay readable.
2. Before clearing measurements, capture GLOBAL → CORE 1 STATUS on the new firmware. Record START RESULT, RUNTIME, MAILBOX SENT, JOBS SINCE BOOT and RETIRE REASON. The job count reset by K3 is separate.
3. Reset stats once and run the same five-track recording at 155 BPM and 60% swing. Use dense notes, overdub, SLICER, LIVE FX, muting, and stop/restart. Capture GLOBAL, SPIKE PROFILE, DMA TRACE and CORE 1 before rebooting if the problem returns.
4. Require continuing mailbox progress, clean recording, peak below 5,804 µs with room to spare and zero new deadline misses or DMA XRUNs. Verify the LOOPER status band, full-lane same-hue pulse and the four MIXER styles on the LCD.
5. If a hard timeout occurs, preserve the project and photos first. A later clean restart can exercise the single retry. Capture its START RESULT and observe mailbox progress after reopening the copy. A CPU fault or mailbox misread must stay barred.
