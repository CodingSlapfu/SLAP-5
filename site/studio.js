/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { decodeNote, encodeNote, validateProject, within } from './project.mjs';

const $ = id => document.getElementById(id);
const colors = ['#1ce2ff', '#ab70ff', '#ff59b0', '#ffb43a', '#3ef289'];
const ctx2d = $('roll').getContext('2d');
const piano = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
const drums = ['Kick', 'Snare', 'Hat', 'Rim', 'Clap', 'Cymbal'];
const drumNotes = [36, 38, 42, 37, 39, 49];
const drumKits = ['Studio Kit', 'Brush Kit', 'FM Kit', 'Industrial'];
const drumRow = pitch => {
  const row = drumNotes.indexOf(pitch);
  return row < 0 ? (pitch + 2) % 6 : row;
};
const left = 69, width = 40, top = 27, height = 23;
let context, worklet, project, stateBefore = [], stateAfter = [];
let track = 0, step = 0, playing = false, drag, patches = [], displayedKit = false;

function send(command, fields = {}) {
  worklet.port.postMessage({ type: 'app', command, ...fields });
}

function sync() {
  send('project', { project });
  draw();
}

function edit(change) {
  const next = structuredClone(project);
  change(next);
  validateProject(next);
  stateBefore.push(project);
  if (stateBefore.length > 100) stateBefore.shift();
  stateAfter = [];
  project = next;
  sync();
}

const selected = () => project.tracks[track];
const pitchAt = row => track === 4 ? drumNotes[row] : 84 - row;
const noteLabel = (pitch) => track === 4 ? drums[drumRow(pitch)] :
  piano[pitch % 12] + (Math.floor(pitch / 12) - 1);

function draw() {
  if (!project) return;
  const current = selected(), steps = current.mix & 255;
  const rows = track === 4 ? 6 : 49;
  document.documentElement.style.setProperty('--accent', colors[track]);
  $('roll').width = left + steps * width;
  $('roll').height = top + rows * height;
  $('track-number').textContent = `TRACK ${track + 1}`;
  $('title').textContent = track === 4 ? 'Drum sequencer' : 'Piano roll';
  $('hint').textContent = 'Click to add. Drag a note to move it or drag its right edge to change the gate.';
  if (displayedKit !== (track === 4) || !$('patch').options.length) {
    displayedKit = track === 4;
    $('patch').replaceChildren(...(displayedKit ? drumKits : patches).map((name, i) => new Option(name, i)));
  }
  $('patch').value = current.patch;
  $('length').value = steps;
  $('pan').value = current.mix >>> 8 & 255;
  $('level').value = current.mix >>> 16 & 255;
  $('mute').checked = current.mix >>> 24;
  $('bpm').value = project.bpm;
  $('swing').value = project.swing;
  $('undo').disabled = !stateBefore.length;
  $('redo').disabled = !stateAfter.length;
  $('tracks').querySelectorAll('button').forEach((b, i) => b.setAttribute('aria-selected', String(i === track)));
  const word = current.steps[step];
  const note = decodeNote(word);
  $('position').textContent = word ? `Step ${step + 1} · ${noteLabel(note.pitch)}` : `Step ${step + 1}`;
  for (const [id, value] of Object.entries({ velocity: note.velocity, gate: note.gate, prob: note.probability })) {
    $(id).value = word ? value : '';
    $(id).disabled = !word;
  }
  $('clear').disabled = !word;
  const g = ctx2d;
  g.font = '12px system-ui';
  g.textBaseline = 'middle';
  for (let r = 0; r < rows; r++) {
    const pitch = pitchAt(r), y = top + height * r;
    g.fillStyle = track !== 4 && piano[pitch % 12].includes('♯') ? '#0e1320' : '#141a28';
    g.fillRect(0, y, $('roll').width, height - 1);
    g.fillStyle = '#c4c9d6';
    g.fillText(noteLabel(pitch), 12, y + height / 2);
  }
  for (let s = 0; s < steps; s++) {
    const x = left + s * width;
    g.fillStyle = s === step ? colors[track] : s % 4 === 0 ? '#424b64' : '#293247';
    g.fillRect(x, top, s === step ? 2 : 1, rows * height);
    g.fillStyle = '#c4c9d6';
    g.fillText(s + 1, x + 5, top / 2);
    const cell = current.steps[s];
    if (!cell || s === drag?.from) continue;
    paintNote(s, decodeNote(cell));
  }
  if (drag) paintNote(drag.to, drag.note);
}

function paintNote(s, note) {
  const row = track === 4 ? drumRow(note.pitch) : 84 - note.pitch;
  if (row < 0 || row >= (track === 4 ? 6 : 49)) return;
  const x = left + s * width + 2, y = top + row * height + 2;
  const size = Math.max(10, Math.min(width * 1.33, note.gate * width / 192));
  ctx2d.globalAlpha = 0.34 + note.velocity / 190;
  ctx2d.fillStyle = colors[track];
  ctx2d.fillRect(x, y, size, height - 5);
  ctx2d.fillStyle = '#fff';
  ctx2d.fillRect(x + size - 2, y + 3, 1, height - 11);
  ctx2d.globalAlpha = 1;
}

function hit(e) {
  const rect = $('roll').getBoundingClientRect(), x = e.clientX - rect.left - left, y = e.clientY - rect.top - top;
  const maxRow = track === 4 ? 5 : 48;
  if (x < 0 || y < 0 || y >= (maxRow + 1) * height) return;
  const cell = Math.floor(x / width);
  if (cell >= (selected().mix & 255)) return;
  return { cell, pitch: pitchAt(Math.floor(y / height)), x };
}

$('roll').addEventListener('pointerdown', e => {
  if (!project || e.button !== 0) return;
  const point = hit(e);
  if (!point) return;
  step = point.cell;
  const word = selected().steps[step], note = decodeNote(word);
  if (word && (track === 4 ? drumRow(note.pitch) === drumRow(point.pitch) : note.pitch === point.pitch)) {
    const end = step * width + 2 + Math.max(10, Math.min(width * 1.33, note.gate * width / 192));
    const resize = point.x > end - 7 && point.x < end + 2;
    drag = { from: step, to: step, note, initial: note.gate, resize, start: point.x };
    $('roll').setPointerCapture(e.pointerId);
    draw();
  } else {
    edit(p => { p.tracks[track].steps[step] = encodeNote(point.pitch, 100, 140, 100); });
    send('audition', { track, pitch: point.pitch });
  }
});
$('roll').addEventListener('pointermove', e => {
  if (!drag) return;
  const point = hit(e);
  if (!point) return;
  if (drag.resize) {
    drag.note.gate = Math.max(1, Math.min(255, Math.round(drag.initial + (point.x - drag.start) * 192 / width)));
  } else {
    drag.to = point.cell;
    drag.note.pitch = point.pitch;
  }
  draw();
});
$('roll').addEventListener('pointerup', () => {
  if (!drag) return;
  const { from, to, note } = drag;
  drag = undefined;
  step = to;
  edit(p => {
    p.tracks[track].steps[from] = 0;
    p.tracks[track].steps[to] = encodeNote(note.pitch, note.velocity, note.gate, note.probability);
  });
});
$('roll').addEventListener('pointercancel', () => { drag = undefined; draw(); });

function numberInput(id, min, max, change) {
  $(id).addEventListener('change', () => {
    const value = $(id).valueAsNumber;
    if (project && within(value, min, max)) edit(p => change(p, value));
    else draw();
  });
}

for (let i = 0; i < 5; i++) {
  const button = document.createElement('button');
  button.textContent = i === 4 ? '5 · Drums' : `Track ${i + 1}`;
  button.className = 'track';
  button.style.setProperty('--track', colors[i]);
  button.addEventListener('click', () => {
    if (!project) return;
    track = i; step = 0; draw();
    $('view').scrollTop = i === 4 ? 0 : 10 * height;
  });
  $('tracks').append(button);
}

numberInput('bpm', 40, 240, (p, v) => { p.bpm = v; });
numberInput('swing', 0, 70, (p, v) => { p.swing = v; });
for (const [id, shift, min, max] of [['length', 0, 1, 64], ['pan', 8, 0, 255], ['level', 16, 0, 255]]) {
  numberInput(id, min, max, (p, v) => {
    if (id === 'length') step = Math.min(step, v - 1);
    p.tracks[track].mix = (p.tracks[track].mix & ~(255 << shift) | v << shift) >>> 0;
  });
}
for (const [id, key, max] of [['velocity', 'velocity', 127], ['gate', 'gate', 255], ['prob', 'probability', 100]]) {
  numberInput(id, id === 'prob' ? 0 : 1, max, (p, v) => {
    const note = decodeNote(p.tracks[track].steps[step]);
    note[key] = v;
    p.tracks[track].steps[step] = encodeNote(note.pitch, note.velocity, note.gate, note.probability);
  });
}
$('patch').addEventListener('change', () => {
  edit(p => { p.tracks[track].patch = Number($('patch').value); });
  send('audition', { track, pitch: track === 4 ? 36 : 60 });
});
$('mute').addEventListener('change', () => edit(p => {
  p.tracks[track].mix = (p.tracks[track].mix & 0xffffff) | (Number($('mute').checked) << 24);
}));
$('clear').addEventListener('click', () => edit(p => { p.tracks[track].steps[step] = 0; }));
$('undo').addEventListener('click', () => {
  if (!stateBefore.length) return;
  stateAfter.push(project); project = stateBefore.pop(); step = 0; sync();
});
$('redo').addEventListener('click', () => {
  if (!stateAfter.length) return;
  stateBefore.push(project); project = stateAfter.pop(); step = 0; sync();
});
$('play').addEventListener('click', () => {
  playing = !playing;
  send('transport', { playing });
  $('play').textContent = playing ? 'Stop' : 'Play';
});
$('export').addEventListener('click', () => {
  const url = URL.createObjectURL(new Blob([JSON.stringify(project, null, 2)], { type: 'application/json' }));
  const a = Object.assign(document.createElement('a'), { href: url, download: 'slap-5-project.json' });
  a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
});
$('import').addEventListener('change', async () => {
  const file = $('import').files[0];
  if (!file) return;
  try {
    if (file.size > 200000) throw new Error('Project file is too large.');
    const next = validateProject(JSON.parse(await file.text()));
    edit(p => Object.assign(p, next));
    $('status').textContent = 'Project imported.';
  } catch (error) {
    $('status').textContent = error.message;
  } finally { $('import').value = ''; }
});
$('start').addEventListener('click', async () => {
  $('start').disabled = true;
  try {
    context = new AudioContext({ sampleRate: 44100 });
    await context.resume();
    await context.audioWorklet.addModule('./emulator/app_worklet.js');
    await context.audioWorklet.addModule('./emulator/worklet.js');
    const response = await fetch('./emulator/app.wasm');
    if (!response.ok) throw new Error(`Could not load the emulator (${response.status}).`);
    worklet = new AudioWorkletNode(context, 'fm1', { outputChannelCount: [2] });
    worklet.port.onmessage = ({ data }) => {
      if (data.type === 'ready') send('read');
      if (data.type !== 'studio-project') return;
      project = validateProject(data.project);
      patches = data.names;
      for (const id of ['play', 'export', 'import']) $(id).disabled = false;
      $('start').textContent = 'Audio enabled';
      draw();
      $('view').scrollTop = 10 * height;
    };
    worklet.onprocessorerror = () => {
      $('status').textContent = 'The audio processor stopped. Reload to reopen the emulator.';
      $('play').disabled = true;
    };
    worklet.connect(context.destination);
    worklet.port.postMessage({ type: 'load', wasm: await response.arrayBuffer() });
  } catch (error) {
    $('status').textContent = error.message;
    $('start').disabled = false;
    if (context) await context.close();
  }
});
