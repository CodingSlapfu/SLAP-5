"""Construct a current-only deployment from the verified firmware package."""
import hashlib
import json
import shutil
import sys
from pathlib import Path

studio, candidate, out = map(Path, sys.argv[1:])
out.mkdir()
for name in ("emulator", "protocol", "fonts"):
    shutil.copytree(studio / name, out / name)
for name in ("studio.html", "studio.css", "studio.js", "project.mjs", "sound-names.json",
             "flash.js", "flash-release.mjs", "flash-transport.mjs"):
    shutil.copy2(studio / name, out / name)
firmware = "SLAP-5-0.1.55.fwsc"
shutil.copy2(candidate / firmware, out / firmware)
data = (out / firmware).read_bytes()
manifest = dict(version="0.1.55", product="FM-1_75000155", bytes=len(data),
                sha256=hashlib.sha256(data).hexdigest(), sources=["FM-1_75000131","FM-1_75000137","FM-1_75000138","FM-1_75000139","FM-1_75000140","FM-1_75000141","FM-1_75000142","FM-1_75000143","FM-1_75000144","FM-1_75000145","FM-1_75000146","FM-1_75000147","FM-1_75000148","FM-1_75000149","FM-1_75000150","FM-1_75000151", "FM-1_75000152", "FM-1_75000153", "FM-1_75000154"])
(out / "release.json").write_text(json.dumps(manifest, indent=2) + "\n")
screens = [
    ("looper-jungle-240.png", "HOME and the five JUNGLE tracks"),
    ("fm6-active.png", "Sound Lab FM6 routing"),
    ("wavetable-active.png", "Sound Lab wavetable"),
    ("drum-sound-lab.png", "Drum Sound Lab and live signal"),
    ("knob-analog-classic.png", "MIXER Analog Classic"),
    ("knob-analog-studio.png", "MIXER Analog Studio"),
    ("knob-digital-neon.png", "MIXER Digital Neon"),
    ("knob-digital-segmented.png", "MIXER Digital Segmented"),
    ("gravity-rings.png", "Gravity Rings"),
    ("digital-vu.png", "Digital VU"),
    ("role-generator.png", "Melodic ROLE GENERATOR"),
    ("filter.png", "FX filter response"),
    ("slicer.png", "Slicer"),
    ("guarded-clear.png", "Track-clear countdown"),
]
import zipfile
with zipfile.ZipFile(candidate / "SLAP-5-0.1.55-framebuffer.zip") as archive:
    (out / "screens").mkdir()
    for file, _ in screens:
        (out / "screens" / file).write_bytes(archive.read(file))
gallery = "".join(
    f'<figure><img src="screens/{file}" width="240" height="240" loading="lazy" '
    f'alt="{title}"><figcaption>{title}</figcaption></figure>' for file, title in screens
)

def page(title, content):
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title} · SLAP-5</title><link rel="stylesheet" href="studio.css">
<link rel="stylesheet" href="fonts/slap-block.css">
<style>figure{{margin:0}}img{{max-width:100%;height:auto;image-rendering:pixelated}}
.gallery{{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,240px),1fr));gap:1.25rem}}
nav{{display:flex;gap:1rem;flex-wrap:wrap}}code{{overflow-wrap:anywhere}}</style></head>
<body><header><a class="brand" href="./">SLAP<span>-5</span></a>
<nav aria-label="Main"><a href="flash.html">Install</a><a href="studio.html">Studio</a><a href="./#controls">Controls</a></nav>
</header><main>{content}</main></body></html>
"""

archive_link = ("https://github.com/CodingSlapfu/SLAP-5/releases/tag/"
                "historical-firmware-through-0.1.48")
(out / "index.html").write_text(page("Five-track groovebox", f"""
<h1>SLAP-5 v0.1.55 hardware testing</h1>
<p>Experimental FM-1 build. Physical JUNGLE stress qualification is pending.</p><p>A five-track groovebox for the M-VAVE FM-1 with four melodic tracks, a drum track and sixteen shared voices.</p>
<p><a href="flash.html">Install on FM-1</a> · <a href="studio.html">Open SLAP Studio</a> · <a href="emulator/">Open the FM-1 simulation</a></p>
<h2 id="controls">Performance controls</h2>
<ul><li>HOME opens the looper. Press again for visualizers. SELECT chooses a track.</li>
<li>HOME K1–K4 adjust level, pattern length, stereo pan and swing.</li>
<li>Tap REC to start or finish a take. Recording preferences set overdub or replace and count-in behavior.</li>
<li>On HOME, hold REC + OCT− for two seconds to clear the selected track. Hold REC + OCT+ for five seconds to reset the working song. Release early to cancel. Saved songs and system preferences remain available.</li>
<li>Tap EDIT for Sound Lab. Hold EDIT for one second for MIXER.</li>
<li>MIXER K1–K4 adjust level, duck, all-track filter and selected-track filter. ARP toggles solo, and SELECT + K2 changes the duck source.</li>
<li>In SETTINGS → DISPLAY, use SELECT to highlight KNOB THEME and turn K1.</li></ul>
<h2>Sound and connectivity</h2>
<p>25 synthesis engines, 128 factory patches, 25 factory drum kits with 16 sounds each, 16/32/64-step patterns, LIVE FX, SLICER and USB audio/MIDI. The firmware uses 256-frame audio callbacks and a 240×240 RGB565 display. Studio runs the matching firmware source in a browser simulation.</p>
<h2>Firmware gallery</h2><p>240×240 captures from the v0.1.55 firmware host renderer.</p>
<div class="gallery">{gallery}</div>
<h2>Using the FM-1</h2><p>Use a USB data cable and close other MIDI apps before connecting. Check the project backup and firmware identity before installing. If the browser cannot connect, verify USB MIDI access and use the download link with the desktop flasher if needed.</p>
<h2>Release notes</h2><p>v0.1.55 reduces repeated sample-bank reads in the drum voice renderer and work in the SLICER and live FX sample paths. FM-1 callback timing remains under test.</p><p>Stop transport, select a melodic track, press SEQ twice and use K1 role, K2 style and K3 bars. SEL generates an editable phrase, and asks for a second press before replacing existing events. The generator offers 22 style presets and follows the track key and scale. HOME lanes are darker, and green battery outline means USB power with charge level unknown.</p><p>v0.1.53 added layered dial surfaces, stronger metallic bevels, cyan/magenta Neon segments and a framed Segmented readout. HOME notes pass behind the track names and scroll off the left edge. Browser updates allow compatible SLAP-5 v0.1.31 and v0.1.37–v0.1.54 directly. Choose under SETTINGS → DISPLAY → KNOB THEME.</p><p>FX inspection now follows active track FX and samples the visible track. Rendering uses a 20 FPS cap with coalescing under load. HOME gains a dotted playhead, vertical battery gauge and a clearer tempo and shortcut header. The Dancer follows the transport beat with a bounded bass accent. K1 toggles bass response, K2 changes the beat division and pose stride, K3 toggles subtle sway, and K4 toggles club lights. HOME K2 queues live length changes for the next measure. GEN DRUM writes finite editable 16-, 32-, or 64-step patterns.</p>
<p><a href="https://github.com/CodingSlapfu/SLAP-5/issues">Support</a> · <a href="{archive_link}">Recovery archive</a></p>
"""), encoding="utf-8")
(out / "flash.html").write_text(page("Install", f"""
<h1>Install SLAP-5 v0.1.55</h1>
<p>For the M-VAVE FM-1. Save untouched project copies and use a data-capable USB cable.</p>
<p>Identity <code>{manifest['product']}</code> · {manifest['bytes']:,} bytes</p>
<p>SHA-256 <code>{manifest['sha256']}</code></p>
<ol><li>Connect your FM-1 over USB.</li><li>Confirm the installed firmware is supported for this update and close other MIDI applications.</li>
<li>Connect below, review the device identity and confirm the update. Keep USB connected until installation finishes.</li></ol>
<p id="availability"></p>
<p><button id="connect">Connect FM-1</button></p>
<p><label><input id="confirmed" type="checkbox"> I backed up my projects and verified the connected device.</label></p>
<p><button id="install" disabled>Install v0.1.55</button></p>
<p id="result" role="status"></p>
<p id="desktop-fallback"><a href="{firmware}">Download the verified firmware for the desktop installer</a></p>
<p><a href="{archive_link}">Recovery instructions and historical archive</a></p>
<script type="module" src="flash.js"></script>
"""), encoding="utf-8")
