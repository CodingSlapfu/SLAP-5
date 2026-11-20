/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { decodeNote, drumNotes, encodeNote, eventLanes, fxLimit, fxPanels, maxSteps, patternLength, validateProject, withPatternLength, within } from './project.mjs';

const $ = id => document.getElementById(id);
const colors = ['#1ce2ff', '#ab70ff', '#ff59b0', '#ffb43a', '#3ef289'];
const ctx2d = $('roll').getContext('2d');
const piano = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
const drums = ['Kick', 'Snare', 'Hat', 'Rim', 'Clap', 'Cymbal'];
const drumStyles = ['IDM', 'Acid Jazz', 'Drum & Bass', 'Jungle', 'Dubstep', 'Synthwave', 'House', 'Deep House',
  'Ruff House', 'Hip-Hop', 'Boom Bap', 'Trap', 'Breakbeat', 'Techno', 'Electro', 'Ambient / Downtempo'];
$('gen-style').replaceChildren(...drumStyles.map((name, value) => new Option(name, value)));
let generation, generationId = 0, take, midiAccess;
const drumRow = pitch => {
  const row = drumNotes.indexOf(pitch);
  return row < 0 ? (pitch + 2) % 6 : row;
};
const left = 69, width = 40, top = 27, height = 23;
let context, worklet, project, stateBefore = [], stateAfter = [];
let track = 0, step = 0, lane = 0, playing = false, drag, patches = [], drumKits = [], displayedKit = false;

function send(command, fields = {}) {
  worklet.port.postMessage({ type: 'app', command, ...fields });
}

const projectKey = 'slap-5-project';
function sync() {
  try {
    localStorage.setItem(projectKey, JSON.stringify(project));
  } catch {
    $('status').textContent = 'Local project storage is unavailable. Export to keep a copy.';
  }
  send('project', { project });
  draw();
}

function edit(change) {
  if (take) return;
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
const pageStart = () => Math.floor(step / 64) * 64;
const noteAt = (part, lane, step) => ({ ...decodeNote(eventLanes(part)[lane][step]), timing: part.offsets[lane][step] });
function setNote(part, lane, step, note) {
  eventLanes(part)[lane][step] = note ? encodeNote(note.pitch, note.velocity, note.gate, note.probability) : 0;
  part.offsets[lane][step] = note?.timing ?? 0;
}
const pitchAt = row => track === 4 ? drumNotes[row] : 84 - row;
const noteLabel = (pitch) => track === 4 ? drums[drumRow(pitch)] :
  piano[pitch % 12] + (Math.floor(pitch / 12) - 1);

function draw() {
  if (!project) return;
  const current = selected(), steps = patternLength(current.mix), first = pageStart(), last = Math.min(steps, first + 64);
  const rows = track === 4 ? 6 : 49;
  document.documentElement.style.setProperty('--accent', colors[track]);
  $('roll').width = left + (last - first) * width;
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
  $('prev-steps').disabled = first === 0;
  $('next-steps').disabled = last === steps;
  $('pan').value = current.mix >>> 8 & 255;
  $('level').value = current.mix >>> 16 & 255;
  $('mute').checked = (current.mix >>> 24) & 1;
  $('bpm').value = project.bpm;
  $('swing').value = project.swing;
  $('metro').value = project.metronome;
  const page = Number($('fx-page').value);
  $('fx-knobs').replaceChildren(...fxPanels[page].map((name, knob) => {
    const label = document.createElement('label');
    const control = document.createElement('input');
    const shift = 8 * knob;
    control.type = 'number';
    control.min = 0;
    control.max = fxLimit(page, knob);
    control.value = current.fx[page] >>> shift & 255;
    control.addEventListener('change', () => {
      const value = control.valueAsNumber;
      if (within(value, 0, Number(control.max))) edit(p => {
        p.tracks[track].fx[page] = (p.tracks[track].fx[page] & ~(255 << shift) | value << shift) >>> 0;
      });
      else draw();
    });
    label.append(name, control);
    return label;
  }));
  $('undo').disabled = !stateBefore.length;
  $('redo').disabled = !stateAfter.length;
  $('tracks').querySelectorAll('button').forEach((b, i) => b.setAttribute('aria-selected', String(i === track)));
  $('drum-controls').hidden = track !== 4;
  $('generator').hidden = track !== 4;
  $('gen-apply').disabled = playing || !!generation || !!take;
  $('take').disabled = (playing && !take) || !!generation;
  $('quick-take').disabled = !!generation;
  const activeLane = lane ? lane - 1 : 0;
  $('drum-lane').value = activeLane;
  $('drum-mute').checked = !!(project.drumFilters & (1 << activeLane));
  $('drum-solo').checked = !!(project.drumFilters & (1 << (activeLane + 8)));
  const word = eventLanes(current)[lane][step];
  const note = noteAt(current, lane, step);
  $('position').textContent = word ? `Step ${step + 1} · ${noteLabel(note.pitch)}` : `Step ${step + 1}`;
  for (const [id, value] of Object.entries({ velocity: note.velocity, gate: note.gate, prob: note.probability, timing: note.timing })) {
    $(id).value = word ? value : '';
    $(id).disabled = !word;
  }
  $('clear').disabled = !word;
  const g = ctx2d;
  g.font = '12px "SLAP Block", system-ui';
  g.textBaseline = 'middle';
  for (let r = 0; r < rows; r++) {
    const pitch = pitchAt(r), y = top + height * r;
    g.fillStyle = track !== 4 && piano[pitch % 12].includes('♯') ? '#0e1320' : '#141a28';
    g.fillRect(0, y, $('roll').width, height - 1);
    g.fillStyle = '#c4c9d6';
    g.fillText(noteLabel(pitch), 12, y + height / 2);
  }
  for (let s = first; s < last; s++) {
    const x = left + (s - first) * width;
    g.fillStyle = s === step ? colors[track] : s % 4 === 0 ? '#424b64' : '#293247';
    g.fillRect(x, top, s === step ? 2 : 1, rows * height);
    g.fillStyle = '#c4c9d6';
    g.fillText(s + 1, x + 5, top / 2);
    eventLanes(current).forEach((events, j) => {
      const cell = events[s];
      if (cell && !(s === drag?.from && j === drag.lane)) paintNote(s, noteAt(current, j, s));
    });
  }
  if (drag) paintNote(drag.to, drag.note);
}

function paintNote(s, note) {
  const row = track === 4 ? drumRow(note.pitch) : 84 - note.pitch;
  if (row < 0 || row >= (track === 4 ? 6 : 49)) return;
  const x = left + (s - pageStart()) * width + 2, y = top + row * height + 2;
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
  const cell = pageStart() + Math.floor(x / width);
  if (cell >= patternLength(selected().mix)) return;
  return { cell, pitch: pitchAt(Math.floor(y / height)), x };
}

$('roll').addEventListener('pointerdown', e => {
  if (!project || e.button !== 0) return;
  const point = hit(e);
  if (!point) return;
  step = point.cell;
  const event = eventLanes(selected());
  const matches = (word, pitch) => word && (track === 4 ? drumRow(decodeNote(word).pitch) === drumRow(pitch) : decodeNote(word).pitch === pitch);
  const hitLane = event.findIndex(row => matches(row[step], point.pitch));
  lane = hitLane >= 0 ? hitLane : track === 4 ? drumRow(point.pitch) + 1 : event.findIndex(row => !row[step]);
  if (lane < 0) { lane = 0; $('status').textContent = 'Both note slots are occupied at this step.'; draw(); return; }
  const word = event[lane][step], note = noteAt(selected(), lane, step);
  if (word && (track === 4 ? drumRow(note.pitch) === drumRow(point.pitch) : note.pitch === point.pitch)) {
    const end = (step - pageStart()) * width + 2 + Math.max(10, Math.min(width * 1.33, note.gate * width / 192));
    const resize = point.x > end - 7 && point.x < end + 2;
    drag = { from: step, to: step, lane, note, initial: note.gate, resize, start: point.x };
    $('roll').setPointerCapture(e.pointerId);
    draw();
  } else {
    edit(p => setNote(p.tracks[track], lane, step, { pitch: point.pitch, velocity: 100, gate: 140, probability: 100, timing: 0 }));
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
  const { from, to, lane: oldLane, note } = drag;
  drag = undefined;
  step = to;
  const events = eventLanes(selected());
  const destination = track === 4 ? drumRow(note.pitch) + 1 :
    events.findIndex((row, index) => !row[to] || (index === oldLane && from === to));
  if (destination < 0 || (events[destination][to] && !(destination === oldLane && from === to))) {
    $('status').textContent = 'The destination already contains a note.';
    draw();
    return;
  }
  lane = destination;
  edit(p => {
    setNote(p.tracks[track], oldLane, from, null);
    setNote(p.tracks[track], destination, to, note);
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
    if (!project || take) return;
    track = i; step = 0; lane = 0; draw();
    $('view').scrollTop = i === 4 ? 0 : 10 * height;
    drawTakeKeys();
  });
  $('tracks').append(button);
}

function takeNote(pitch, velocity) {
  if (take && !take.finishing) send('record-note', { pitch, velocity });
}

function drawTakeKeys() {
  const pitches = track === 4 ? drumNotes : Array.from({ length: 12 }, (_, n) => 60 + n);
  $('take-keys').replaceChildren(...pitches.map(pitch => {
    const key = document.createElement('button');
    key.type = 'button';
    key.textContent = noteLabel(pitch);
    key.addEventListener('pointerdown', e => {
      if (!take) return;
      key.setPointerCapture(e.pointerId);
      takeNote(pitch, 100);
    });
    key.addEventListener('lostpointercapture', () => takeNote(pitch, 0));
    key.addEventListener('keydown', e => {
      if (take && !e.repeat && (e.key === ' ' || e.key === 'Enter')) takeNote(pitch, 100);
    });
    key.addEventListener('keyup', e => {
      if (e.key === ' ' || e.key === 'Enter') takeNote(pitch, 0);
    });
    return key;
  }));
}

function attachMidiInputs() {
  for (const input of midiAccess.inputs.values()) input.onmidimessage = ({ data }) => {
    const [status, pitch, velocity] = data;
    if ((status & 0xf0) === 0x90) takeNote(pitch, velocity);
    else if ((status & 0xf0) === 0x80) takeNote(pitch, 0);
  };
}

$('use-midi').addEventListener('click', async () => {
  try {
    midiAccess = await navigator.requestMIDIAccess();
    midiAccess.onstatechange = attachMidiInputs;
    attachMidiInputs();
    $('use-midi').textContent = 'MIDI enabled';
    $('use-midi').disabled = true;
  } catch (error) {
    $('status').textContent = `MIDI unavailable. ${error.message}`;
  }
});

function startTake(quick) {
  if (take) {
    send('record-stop');
    return;
  }
  if (!project || (playing && !quick) || generation) return;
  const replace = quick ? 0 : Number($('take-mode').value);
  if (replace && eventLanes(selected()).some(events => events.some(Boolean)) &&
      !window.confirm('Replace events in the selected recording range? You can undo the take.')) return;
  take = { before: project, quick, timer: setInterval(() => send('record-status'), 150) };
  $('take').textContent = 'End take';
  $('quick-take').textContent = 'End take';
  $('take-state').textContent = 'Arming';
  send('record-start', { track, quick, bars: $('take-bars').selectedIndex,
    countIn: Number($('take-start').value), unquantized: Number(!$('take-quant').checked), replace });
}
$('take').addEventListener('click', () => startTake(false));
$('quick-take').addEventListener('click', () => startTake(true));

$('gen-apply').addEventListener('click', () => {
  if (!project || track !== 4 || playing || generation || take) return;
  if (eventLanes(project.tracks[4]).some(events => events.some(Boolean)) &&
      !window.confirm('Replace the current drum pattern? You can undo this.')) return;
  const hit = $('gen-hit').valueAsNumber;
  const groove = $('gen-groove').valueAsNumber;
  const variation = $('gen-variation').valueAsNumber;
  if (![hit, groove, variation].every(value => within(value, 0, 100))) return;
  generation = { id: ++generationId, project };
  send('gen-drums', { id: generation.id, style: Number($('gen-style').value), hit, groove, variation });
  draw();
});

numberInput('bpm', 40, 240, (p, v) => { p.bpm = v; });
numberInput('swing', 0, 70, (p, v) => { p.swing = v; });
$('metro').addEventListener('change', () => {
  if (project) edit(p => { p.metronome = Number($('metro').value); });
});
$('fx-page').addEventListener('change', draw);
$('prev-steps').addEventListener('click', () => { step = Math.max(0, pageStart() - 64); draw(); });
$('next-steps').addEventListener('click', () => { step = pageStart() + 64; draw(); });
numberInput('length', 1, maxSteps, (p, value) => {
  step = Math.min(step, value - 1);
  p.tracks[track].mix = withPatternLength(p.tracks[track].mix, value);
});
for (const [id, shift, min, max] of [['pan', 8, 0, 255], ['level', 16, 0, 255]]) {
  numberInput(id, min, max, (p, v) => {
    p.tracks[track].mix = (p.tracks[track].mix & ~(255 << shift) | v << shift) >>> 0;
  });
}
for (const [id, key, max] of [['velocity', 'velocity', 127], ['gate', 'gate', 255], ['prob', 'probability', 100], ['timing', 'timing', 15]]) {
  numberInput(id, id === 'prob' || id === 'timing' ? 0 : 1, max, (p, v) => {
    const note = noteAt(p.tracks[track], lane, step);
    note[key] = v;
    setNote(p.tracks[track], lane, step, note);
  });
}
$('patch').addEventListener('change', () => {
  edit(p => { p.tracks[track].patch = Number($('patch').value); });
  send('audition', { track, pitch: track === 4 ? 36 : 60 });
});
$('mute').addEventListener('change', () => edit(p => {
  p.tracks[track].mix = (p.tracks[track].mix & ~(1 << 24)) | (Number($('mute').checked) << 24);
}));
$('drum-lane').addEventListener('change', () => {
  lane = Number($('drum-lane').value) + 1;
  draw();
});
for (const [id, shift] of [['drum-mute', 0], ['drum-solo', 8]]) {
  $(id).addEventListener('change', () => edit(p => {
    const bit = 1 << (Number($('drum-lane').value) + shift);
    p.drumFilters = $(id).checked ? p.drumFilters | bit : p.drumFilters & ~bit;
  }));
}
$('clear').addEventListener('click', () => edit(p => { setNote(p.tracks[track], lane, step, null); }));
$('undo').addEventListener('click', () => {
  if (take || !stateBefore.length) return;
  stateAfter.push(project); project = stateBefore.pop(); step = 0; sync();
});
$('redo').addEventListener('click', () => {
  if (take || !stateAfter.length) return;
  stateBefore.push(project); project = stateAfter.pop(); step = 0; sync();
});
$('play').addEventListener('click', () => {
  if (take) return;
  playing = !playing;
  send('transport', { playing });
  $('play').textContent = playing ? 'Stop' : 'Play';
  $('gen-apply').disabled = playing || !!generation || !!take;
  $('take').disabled = (playing && !take) || !!generation;
  $('quick-take').disabled = !!generation;
});
$('export').addEventListener('click', () => {
  const url = URL.createObjectURL(new Blob([JSON.stringify(project, null, 2)], { type: 'application/json' }));
  const a = Object.assign(document.createElement('a'), { href: url, download: 'slap-5-project.json' });
  a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
});
$('import').addEventListener('change', async () => {
  if (take) return;
  const file = $('import').files[0];
  if (!file) return;
  try {
    if (file.size > 200000) throw new Error('Project file is too large.');
    const next = validateProject(JSON.parse(await file.text()));
    step = 0; lane = 0;
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
      if (data.type === 'studio-take' && take) {
        if (!data.state && !take.finishing) {
          take.finishing = true;
          clearInterval(take.timer);
          playing = take.quick && !!data.playing;
          if (!take.quick) send('transport', { playing: false });
          $('play').textContent = playing ? 'Stop' : 'Play';
          send('read-take');
        }
        $('take-state').textContent = data.state === 5 ? 'Recording' :
          data.state === 6 ? 'Play a note' : data.state ? `Count-in ${data.state}` : 'Saving take';
        return;
      }
      if (data.type === 'studio-take-project' && take) {
        stateBefore.push(take.before);
        if (stateBefore.length > 100) stateBefore.shift();
        stateAfter = [];
        take = undefined;
        project = validateProject(data.project);
        $('take').textContent = 'Record track';
        $('quick-take').textContent = 'Quick REC';
        $('take-state').textContent = 'Take captured';
        sync();
        return;
      }
      if (data.type === 'studio-generated') {
        if (!generation || data.id !== generation.id) return;
        const valid = generation.project === project && !playing && data.drums;
        generation = undefined;
        if (valid) edit(next => { Object.assign(next.tracks[4], data.drums); });
        else sync();
        return;
      }
      if (data.type !== 'studio-project') return;
      project = validateProject(data.project);
      try {
        const saved = localStorage.getItem(projectKey);
        if (saved) project = validateProject(JSON.parse(saved));
      } catch {
        $('status').textContent = 'The saved project could not be restored.';
      }
      send('project', { project });
      patches = data.names;
      drumKits = data.kitNames;
      for (const id of ['play', 'export', 'import', 'take', 'quick-take', 'use-midi']) $(id).disabled = false;
      $('start').textContent = 'Audio enabled';
      drawTakeKeys();
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
