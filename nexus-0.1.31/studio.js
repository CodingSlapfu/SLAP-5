/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { advancedSynth, decodeNote, drumNotes, encodeNote, eventLanes, fxLimit, fxPanels, legacyDrumKits, modernSynth, patternLength, validateProject, withPatternLength, within } from './project.mjs';

const $ = id => document.getElementById(id);
const colors = ['#00e1ff', '#9d3eff', '#ff259e', '#ffaf10', '#15ff66'];
const ctx2d = $('roll').getContext('2d');
const piano = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
const drums = ['Kick', 'Snare', 'Closed Hat', 'Rim', 'Clap', 'Crash', 'Sub Kick', 'Alt Snare', 'Open Hat', 'Pedal Hat', 'Low Tom', 'Mid Tom', 'High Tom', 'Ride', 'Shaker', 'Perc FX'];
$('drum-lane').replaceChildren(...drums.map((name, i) => new Option(name, i)));
const drumStyles = ['IDM', 'Acid Jazz', 'Drum & Bass', 'Jungle', 'Dubstep', 'Synthwave', 'House', 'Deep House',
  'Ruff House', 'Hip-Hop', 'Boom Bap', 'Trap', 'Breakbeat', 'Techno', 'Electro', 'Ambient / Downtempo'];
$('gen-style').replaceChildren(...drumStyles.map((name, value) => new Option(name, value)));
let generation, generationId = 0, take, midiAccess;
const drumRow = pitch => {
  const row = drumNotes.indexOf(pitch);
  return row < 0 ? (pitch + 2) % 6 : row;
};
const left = 93, width = 40, top = 27, height = 23;
let context, worklet, project, stateBefore = [], stateAfter = [];
let track = 0, step = 0, lane = 0, playing = false, drag, patches = [], engines = [], drumKits = [];
let displayedKit = false, displayedLegacy = -1;

const songKey = 'slap-5-arrangement-v1';
let arrangement, songPlaying = false, sceneIndex = 0, clipClipboard, activeScene = -1;
const songControls = ['song-play', 'scene-add', 'scene-copy', 'scene-delete', 'scene-earlier',
  'scene-later', 'automation-set', 'automation-clear', 'song-export', 'song-import', 'scene-name', 'scene-bars', 'scene-repeats', 'clip-copy', 'clip-paste'];

function validateSong(value) {
  if (value?.format !== 'slap-5-song' || value.version !== 1 || typeof value.name !== 'string' ||
      value.name.length > 80 || !Array.isArray(value.scenes) || !value.scenes.length || value.scenes.length > 16)
    throw new Error('Invalid SLAP-5 song.');
  return { ...value, scenes: value.scenes.map(scene => {
    if (typeof scene.name !== 'string' || scene.name.length > 80 ||
        !within(scene.bars, 1, 16) || !within(scene.repeats, 1, 16)) throw new Error('Invalid scene.');
    const automation = scene.automation ?? [];
    if (!Array.isArray(automation) || automation.length > 80 ||
        automation.some(point => !point || !within(point.bar, 0, scene.bars - 1) || !within(point.track, 0, 4) ||
          (point.level === undefined && point.pan === undefined) ||
          (point.level !== undefined && !within(point.level, 0, 255)) ||
          (point.pan !== undefined && !within(point.pan, 0, 255)))) throw new Error('Invalid mix automation.');
    return { ...scene, automation, project: validateProject(scene.project) };
  }) };
}

function saveSong() {
  try { localStorage.setItem(songKey, JSON.stringify(arrangement)); }
  catch { $('status').textContent = 'Song storage is unavailable. Export to keep a copy.'; }
}

function drawSong() {
  if (!arrangement) return;
  const scene = arrangement.scenes[sceneIndex];
  $('song-name').value = arrangement.name;
  $('scene-name').value = scene.name;
  $('scene-bars').value = scene.bars;
  $('automation-bar').max = scene.bars;
  $('scene-repeats').value = scene.repeats;
  for (const id of songControls) $(id).disabled = songPlaying && id !== 'song-play';
  $('scene-delete').disabled ||= arrangement.scenes.length === 1;
  $('scene-add').disabled ||= arrangement.scenes.length >= 16;
  $('scene-copy').disabled ||= arrangement.scenes.length >= 16;
  $('scene-earlier').disabled ||= sceneIndex === 0;
  $('scene-later').disabled ||= sceneIndex === arrangement.scenes.length - 1;
  $('clip-undo').disabled = songPlaying || !sceneClipHistory.get(scene)?.length;
  $('clip-paste').disabled ||= !clipClipboard || clipClipboard.drum !== (track === 4);
  $('song-play').textContent = songPlaying ? 'Stop song' : 'Play song';
  const grid = $('song-grid');
  grid.replaceChildren();
  const labels = ['Scene', 'Track 1', 'Track 2', 'Track 3', 'Track 4', 'Drums'];
  let bar = 1;
  const rows = labels.map((label, row) => {
    const element = document.createElement('div');
    element.className = 'song-row';
    const title = document.createElement('strong');
    title.textContent = label;
    element.append(title);
    arrangement.scenes.forEach((item, i) => {
      const button = document.createElement('button');
      const part = row ? item.project.tracks[row - 1] : null;
      button.type = 'button';
      button.className = 'song-clip';
      button.setAttribute('aria-pressed', String(sceneIndex === i && (!row || track === row - 1)));
      if (activeScene === i) button.classList.add('active');
      if (row) {
        button.style.setProperty('--clip-color', colors[row - 1]);
        button.textContent = (row === 5 ? drumKits[part.patch] : patches[part.patch]) ??
          (row === 5 ? 'Drum clip' : 'Melodic clip');
        const meta = document.createElement('small');
        meta.textContent = `${patternLength(part.mix)} steps${part.mix & (1 << 24) ? ' · Muted' : ''}`;
        button.append(meta);
      } else {
        button.textContent = `Bar ${bar} · ${item.name} ×${item.repeats}`;
        bar += item.bars * item.repeats;
      }
      button.addEventListener('click', () => {
        if (songPlaying) {
          if (row) track = row - 1;
          if (activeScene !== i) send('song-queue', { scene: i });
          draw();
          return;
        }
        if (take || generation) return;
        sceneIndex = i;
        if (row) track = row - 1;
        activateScene();
      });
      element.append(button);
    });
    return element;
  });
  grid.append(...rows);
}

function activateScene() {
  project = structuredClone(arrangement.scenes[sceneIndex].project);
  stateBefore = [];
  stateAfter = [];
  step = lane = 0;
  sync();
}

function setMixAutomation(clear) {
  if (!arrangement || songPlaying) return;
  const scene = arrangement.scenes[sceneIndex];
  const bar = $('automation-bar').valueAsNumber - 1;
  const level = $('automation-level').valueAsNumber;
  const pan = $('automation-pan').valueAsNumber;
  if (!within(bar, 0, scene.bars - 1) || (!clear && !within(level, 0, 100) && !within(pan, -100, 100))) return;
  scene.automation = (scene.automation ?? []).filter(point => point.bar !== bar || point.track !== track);
  if (!clear) {
    const point = { bar, track };
    if (within(level, 0, 100)) point.level = Math.round(level * 255 / 100);
    if (within(pan, -100, 100)) point.pan = 128 + Math.round(pan * (pan < 0 ? 128 : 127) / 100);
    scene.automation.push(point);
  }
  saveSong();
  $('status').textContent = `${clear ? 'Cleared' : 'Saved'} automation · Track ${track + 1}, bar ${bar + 1}`;
}
$('automation-set').onclick = () => setMixAutomation(false);
$('automation-clear').onclick = () => setMixAutomation(true);

const sceneClipHistory = new WeakMap();
function rememberSceneClip(scene, track, part) {
  const key = arrangement.scenes[scene];
  const changes = sceneClipHistory.get(key) ?? [];
  changes.push({ track, part: structuredClone(part) });
  if (changes.length > 24) changes.shift();
  sceneClipHistory.set(key, changes);
}
$('clip-undo').onclick = () => {
  if (songPlaying || take || generation) return;
  const scene = arrangement.scenes[sceneIndex];
  const prior = sceneClipHistory.get(scene)?.pop();
  if (!prior) return;
  scene.project.tracks[prior.track] = prior.part;
  track = prior.track;
  activateScene();
};

function changeScenes(action) {
  if (!arrangement || songPlaying || take || generation) return;
  action();
  activateScene();
}

$('song-name').addEventListener('change', () => {
  if (!arrangement) return;
  arrangement.name = $('song-name').value;
  saveSong();
});
for (const [id, field] of [['scene-name', 'name'], ['scene-bars', 'bars'], ['scene-repeats', 'repeats']]) {
  $(id).addEventListener('change', () => {
    if (!arrangement || songPlaying) return;
    const value = field === 'name' ? $(id).value : $(id).valueAsNumber;
    const scene = arrangement.scenes[sceneIndex];
    if (field === 'name' || within(value, 1, 16)) {
      scene[field] = value;
      if (field === 'bars') scene.automation = (scene.automation ?? []).filter(point => point.bar < value);
    }
    saveSong();
    drawSong();
  });
}
$('scene-add').onclick = () => changeScenes(() => {
  const empty = structuredClone(project);
  empty.tracks.forEach(part => {
    for (const row of [...eventLanes(part), ...part.offsets, ...part.modes, ...part.durations]) row.fill(0);
  });
  arrangement.scenes.splice(++sceneIndex, 0, { name: 'New scene', bars: 4, repeats: 1, project: empty });
});
$('scene-copy').onclick = () => changeScenes(() => {
  const scene = structuredClone(arrangement.scenes[sceneIndex]);
  scene.name = (scene.name + ' copy').slice(0, 80);
  arrangement.scenes.splice(++sceneIndex, 0, scene);
});
$('scene-delete').onclick = () => changeScenes(() => {
  arrangement.scenes.splice(sceneIndex, 1);
  sceneIndex = Math.min(sceneIndex, arrangement.scenes.length - 1);
});
for (const [id, delta] of [['scene-earlier', -1], ['scene-later', 1]])
  $(id).onclick = () => changeScenes(() => {
    const [scene] = arrangement.scenes.splice(sceneIndex, 1);
    sceneIndex += delta;
    arrangement.scenes.splice(sceneIndex, 0, scene);
  });
$('clip-copy').onclick = () => {
  clipClipboard = { drum: track === 4, part: structuredClone(project.tracks[track]) };
  drawSong();
};
$('clip-paste').onclick = () => edit(next => { next.tracks[track] = structuredClone(clipClipboard.part); });
$('song-play').onclick = () => {
  if (songPlaying) { send('song-stop'); return; }
  if (playing || take || generation) return;
  songPlaying = true;
  send('song-start', { song: { scenes: arrangement.scenes, index: sceneIndex, loop: $('song-loop').value } });
  draw();
};
$('song-export').onclick = () => {
  const file = URL.createObjectURL(new Blob([JSON.stringify(arrangement)], { type: 'application/json' }));
  const anchor = document.createElement('a');
  anchor.href = file;
  anchor.download = 'slap-5-song.json';
  anchor.click();
  setTimeout(() => URL.revokeObjectURL(file), 2000);
};
$('song-import').onchange = async () => {
  try {
    const input = $('song-import').files[0];
    if (!input) return;
    if (input.size > 16 * 1024 * 1024) throw new Error('Song exceeds the import limit.');
    arrangement = validateSong(JSON.parse(await input.text()));
    sceneIndex = 0;
    activateScene();
  } catch (error) { $('status').textContent = error.message; }
  finally { $('song-import').value = ''; }
};

function send(command, fields = {}) {
  worklet.port.postMessage({ type: 'app', command, ...fields });
}

const projectKey = 'slap-5-project';
function sync({ filtersOnly = false } = {}) {
  try {
    localStorage.setItem(projectKey, JSON.stringify(project));
  } catch {
    $('status').textContent = 'Local project storage is unavailable. Export to keep a copy.';
  }
  if (arrangement) {
    arrangement.scenes[sceneIndex].project = structuredClone(project);
    saveSong();
  }
  if (filtersOnly) send('performance-filters', { mixer: project.mixer });
  else send('project', { project });
  draw();
}

function edit(change, options) {
  if (take || songPlaying || generation) return;
  const next = structuredClone(project);
  change(next);
  validateProject(next);
  stateBefore.push(project);
  if (stateBefore.length > 100) stateBefore.shift();
  stateAfter = [];
  project = next;
  sync(options);
}

const selected = () => project.tracks[track];
const pageStart = () => Math.floor(step / 64) * 64;
const noteAt = (part, lane, step) => ({ ...decodeNote(eventLanes(part)[lane][step]), timing: part.offsets[lane][step], duration: part.durations[lane][step], mode: part.modes[lane][step] });
const durationMs = units => Math.round(units * 128000 / 44100);
const durationUnits = ms => Math.min(65535, Math.round(ms * 44100 / 128000));
const noteStart = (cell, note) => left + (cell - pageStart() + note.timing / 16) * width;
function noteWidth(cell, note) {
  const base = 44100 * 15 / project.bpm;
  const swing = base * project.swing / 200;
  let span = cell % 2 ? base - swing : base + swing;
  const available = span * (1 - note.timing / 16);
  let samples = note.duration ? note.duration * 128 : span * note.gate / 192;
  let steps = Math.min(available, samples) / span;
  samples -= available;
  if (samples > 0) {
    const pairs = Math.floor(samples / (2 * base));
    steps += pairs * 2;
    samples -= pairs * 2 * base;
    span = 2 * base - span;
    steps += Math.min(span, samples) / span + Math.max(0, samples - span) / (2 * base - span);
  }
  return Math.max(2, width * steps);
}
function setNote(part, lane, step, note) {
  eventLanes(part)[lane][step] = note ? encodeNote(note.pitch, note.velocity, note.gate, note.probability) : 0;
  part.offsets[lane][step] = note?.timing ?? 0;
  part.durations[lane][step] = note?.duration ?? 0;
  part.modes[lane][step] = note?.mode ?? 0;
}
const pitchAt = row => track === 4 ? drumNotes[row] : 84 - row;
const noteLabel = (pitch) => track === 4 ? drums[drumRow(pitch)] :
  piano[pitch % 12] + (Math.floor(pitch / 12) - 1);

const defaultMixer = () => ({ solo: 0, duck: [0, 0, 0, 0], autoLevel: true, autoMix: false });
function drawMixer() {
  const mixer = project.mixer ?? defaultMixer();
  $('mix-level').checked = mixer.autoLevel;
  $('mix-auto').checked = mixer.autoMix;
  $('mix-filter').value = mixer.allFilter ?? 0;
  $('mix-filter').disabled = songPlaying;
  const channels = project.tracks.map((part, index) => {
    const channel = document.createElement('div');
    channel.className = 'mixer-channel settings';
    channel.style.setProperty('--clip-color', colors[index]);
    const title = document.createElement('strong');
    title.textContent = index === 4 ? 'Drums' : `Track ${index + 1}`;
    const meter = document.createElement('meter');
    meter.id = `meter-${index}`;
    meter.max = 160;
    meter.setAttribute('aria-label', `${title.textContent} activity`);
    channel.append(title, meter);
    const control = (name, type, value, update, limit = [0, 100], options) => {
      const label = document.createElement('label');
      const input = document.createElement('input');
      input.type = type;
      if (type === 'checkbox') input.checked = !!value;
      else {
        input.min = limit[0]; input.max = limit[1]; input.value = value;
      }
      input.disabled = songPlaying;
      input.addEventListener('change', () => {
        const next = type === 'checkbox' ? input.checked : input.valueAsNumber;
        if (type === 'checkbox' || within(next, ...limit)) edit(p => update(p, next), options);
      });
      label.append(name, input);
      channel.append(label);
    };
    control('Level', 'number', Math.round((part.mix >>> 16 & 255) * 100 / 255), (p, value) => {
      p.tracks[index].mix = (p.tracks[index].mix & ~(255 << 16)) | Math.round(value * 255 / 100) << 16;
    });
    control('Pan', 'number', Math.round(((part.mix >>> 8 & 255) - 128) * 100 / 128), (p, value) => {
      p.tracks[index].mix = (p.tracks[index].mix & ~(255 << 8)) |
        (128 + Math.round(value * (value < 0 ? 128 : 127) / 100)) << 8;
    }, [-100, 100]);
    control('Mute', 'checkbox', part.mix & (1 << 24), (p, value) => {
      p.tracks[index].mix = (p.tracks[index].mix & ~(1 << 24)) | Number(value) << 24;
    });
    control('Solo', 'checkbox', mixer.solo & (1 << index), (p, value) => {
      const state = p.mixer ??= defaultMixer();
      state.solo = state.solo & ~(1 << index) | Number(value) << index;
    });
    if (index < 4) control('Duck', 'number', mixer.duck[index], (p, value) => {
      (p.mixer ??= defaultMixer()).duck[index] = value;
    });
    control('Track filter', 'number', mixer.trackFilter?.[index] ?? 0, (p, value) => {
      const state = p.mixer ??= defaultMixer();
      (state.trackFilter ??= [0, 0, 0, 0, 0])[index] = value;
    }, [-100, 100], { filtersOnly: true });
    return channel;
  });
  $('mixer-channels').replaceChildren(...channels);
}
$('mix-filter').onchange = () => {
  const value = $('mix-filter').valueAsNumber;
  if (project && within(value, -100, 100))
    edit(p => { (p.mixer ??= defaultMixer()).allFilter = value; }, { filtersOnly: true });
};
for (const [id, field] of [['mix-level', 'autoLevel'], ['mix-auto', 'autoMix']])
  $(id).onchange = () => { if (project) edit(p => { (p.mixer ??= defaultMixer())[field] = $(id).checked; }); };
$('studio-mixer').ontoggle = () => { if (worklet) send('meters', { enabled: $('studio-mixer').open }); if (project && $('studio-mixer').open) drawMixer(); };

function drawInstrument(part) {
  const panel = $('instrument-editor');
  const engine = engines[part.patch];
  panel.hidden = track === 4 || (engine !== 0 && engine !== 2);
  if (panel.hidden) return;
  const heading = document.createElement('strong');
  heading.textContent = engine === 0 ? 'FM6' : 'Wavetable';
  panel.replaceChildren(heading);
  const field = (label, options, value, change) => {
    const wrapper = document.createElement('label');
    const select = document.createElement('select');
    select.replaceChildren(...options.map((name, index) => new Option(name, index)));
    select.value = value;
    select.addEventListener('change', () => change(Number(select.value)));
    wrapper.append(label, select);
    panel.append(wrapper);
  };
  const update = config => edit(next => {
    next.tracks[track].timbre = config >>> 0;
    next.tracks[track].mix |= advancedSynth | modernSynth;
  });
  if (engine === 0) {
    const config = part.timbre || 0x3ffda249;
    field('Routing', ['Serial', 'Branch', 'Layer'], config >>> 30, value => update((config & 0x3fffffff) | value << 30));
    const envelopes = part.fmEnvelopes ?? Array(7).fill(0);
    const editEnvelope = (slot, value) => {
      edit(next => {
        const voice = next.tracks[track];
        voice.fmEnvelopes = [...envelopes];
        voice.fmEnvelopes[slot] = value;
        voice.mix |= advancedSynth | modernSynth;
      });
      send('audition', { track, pitch: 60 });
    };
    const values = Array.from({ length: 16 }, (_, i) => String(i));
    field('Feedback', ['Factory', ...values], envelopes[6], value => editEnvelope(6, value));
    for (let op = 0; op < 6; op++) {
      const ratioShift = op * 3, levelShift = 18 + op * 2;
      field(`Op ${op + 1} ratio`, Array.from({ length: 8 }, (_, i) => ((i + 1) / 2).toFixed(1)),
        config >>> ratioShift & 7, value => update(config & ~(7 << ratioShift) | value << ratioShift));
      field(`Op ${op + 1} level`, ['Off', 'Low', 'Mid', 'Full'],
        config >>> levelShift & 3, value => update(config & ~(3 << levelShift) | value << levelShift));
      const envelope = envelopes[op] || 0x14561;
      ['Attack', 'Decay', 'Sustain', 'Release'].forEach((name, index) => {
        const shift = 4 * index;
        field(`Op ${op + 1} ${name}`, values, envelope >>> shift & 15,
          value => editEnvelope(op, envelope & ~(15 << shift) | value << shift));
      });
    }
  } else {
    const config = part.timbre || 0x80002040;
    const values = Array.from({ length: 128 }, (_, i) => String(i));
    field('Morph', values, config & 127, value => update(config & ~127 | value));
    field('Movement', values, config >>> 8 & 127, value => update(config & ~(127 << 8) | value << 8));
  }
}

function draw() {
  if (!project) return;
  drawSong();
  if ($('studio-mixer').open) drawMixer();
  const current = selected(), steps = patternLength(current.mix), first = pageStart(), last = Math.min(steps, first + 64);
  const rows = track === 4 ? drums.length : 49;
  document.documentElement.style.setProperty('--accent', colors[track]);
  $('roll').width = left + (last - first) * width;
  $('roll').height = top + rows * height;
  $('track-number').textContent = `TRACK ${track + 1}`;
  $('title').textContent = track === 4 ? 'Drum sequencer' : 'Piano roll';
  $('hint').textContent = 'Click to add. Drag a note to move it or drag its right edge to change the gate.';
  const drumTrack = track === 4;
  const legacy = drumTrack && current.patch < legacyDrumKits ? current.patch : -1;
  const patchSelect = $('patch');
  if (displayedKit !== drumTrack || displayedLegacy !== legacy || !patchSelect.options.length) {
    displayedKit = drumTrack;
    displayedLegacy = legacy;
    const options = drumTrack
      ? drumKits.slice(legacyDrumKits).map((name, i) => new Option(name, legacyDrumKits + i))
      : patches.map((name, i) => new Option(name, i));
    if (legacy >= 0) options.unshift(new Option(`${drumKits[legacy]} (legacy)`, legacy));
    patchSelect.replaceChildren(...options);
  }
  patchSelect.value = current.patch;
  drawInstrument(current);
  const normalSteps = [16, 32, 48, 64];
  const lengths = $('length');
  lengths.replaceChildren(...normalSteps.map(length => new Option(length, length)));
  if (!normalSteps.includes(steps)) lengths.append(new Option(`${steps} (legacy)`, steps));
  lengths.value = steps;
  $('prev-steps').disabled = first === 0;
  $('next-steps').disabled = last === steps;
  $('pan').value = current.mix >>> 8 & 255;
  $('level').value = Math.round((current.mix >>> 16 & 255) * 100 / 255);
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
  $('gen-apply').disabled = songPlaying && activeScene < 0 || !!generation || !!take;
  $('take').disabled = (playing && !take) || !!generation;
  $('quick-take').disabled = !!generation;
  const activeLane = lane ? lane - 1 : 0;
  $('drum-lane').value = activeLane;
  $('drum-mute').checked = !!(project.drumFilters & (1 << activeLane));
  $('drum-solo').checked = !!(project.drumFilters & (1 << (activeLane + 16)));
  const word = eventLanes(current)[lane][step];
  const note = noteAt(current, lane, step);
  $('position').textContent = word ? `Step ${step + 1} · ${noteLabel(note.pitch)}` : `Step ${step + 1}`;
  for (const [id, value] of Object.entries({ velocity: note.velocity, gate: note.gate, prob: note.probability, timing: note.timing })) {
    $(id).value = word ? value : '';
    $(id).disabled = !word;
  }
  $('step-mode').value = note.mode;
  $('step-mode').disabled = !word;
  $('hold-ms').value = word && track !== 4 ? durationMs(note.duration) : '';
  $('hold-ms').disabled = !word || track === 4;
  $('gate').disabled = !word || !!note.duration;
  $('clear').disabled = !word;
  const g = ctx2d;
  g.font = '12px "SLAP Nexus", system-ui';
  g.textBaseline = 'middle';
  for (let r = 0; r < rows; r++) {
    const pitch = pitchAt(r), y = top + height * r;
    g.fillStyle = track !== 4 && piano[pitch % 12].includes('♯') ? '#0e1320' : '#141a28';
    g.fillRect(0, y, $('roll').width, height - 1);
    g.fillStyle = '#c4c9d6';
    g.fillText(noteLabel(pitch), 12, y + height / 2);
  }
  g.fillStyle = '#c4c9d6';
  g.fillText('1/16', 12, top / 2);
  for (let s = first; s < last; s++) {
    const x = left + (s - first) * width;
    g.fillStyle = s === step ? colors[track] : s % 16 === 0 ? '#7791b8' : s % 4 === 0 ? '#424b64' : '#293247';
    g.fillRect(x, top, s === step ? 2 : 1, rows * height);
    g.fillStyle = '#c4c9d6';
    g.fillText(s % 16 === 0 ? `B${s / 16 + 1}` : s % 16 + 1, x + 5, top / 2);
    eventLanes(current).forEach((events, j) => {
      const cell = events[s];
      if (cell && !(s === drag?.from && j === drag.lane)) paintNote(s, noteAt(current, j, s));
    });
  }
  if (drag) paintNote(drag.to, drag.note);
}

function paintNote(s, note) {
  const row = track === 4 ? drumRow(note.pitch) : 84 - note.pitch;
  if (row < 0 || row >= (track === 4 ? drums.length : 49)) return;
  const x = noteStart(s, note), y = top + row * height + 2;
  const size = noteWidth(s, note);
  ctx2d.globalAlpha = 0.34 + note.velocity / 190;
  ctx2d.fillStyle = colors[track];
  ctx2d.fillRect(x, y, size, height - 5);
  ctx2d.fillStyle = '#fff';
  ctx2d.fillRect(x + size - 2, y + 3, 1, height - 11);
  ctx2d.globalAlpha = 1;
}

function hit(e) {
  const rect = $('roll').getBoundingClientRect(), x = e.clientX - rect.left - left, y = e.clientY - rect.top - top;
  const maxRow = track === 4 ? drums.length - 1 : 48;
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
  let hitLane = event.findIndex(row => matches(row[step], point.pitch));
  if (hitLane < 0 && track !== 4) {
    const first = pageStart();
    for (let start = step - 1; start >= first && hitLane < 0; --start) {
      hitLane = event.findIndex((row, index) => {
        if (!matches(row[start], point.pitch)) return false;
        const previous = noteAt(selected(), index, start);
        return point.x + left <= noteStart(start, previous) + noteWidth(start, previous);
      });
      if (hitLane >= 0) step = start;
    }
  }
  lane = hitLane >= 0 ? hitLane : track === 4 ? drumRow(point.pitch) + 1 : event.findIndex(row => !row[step]);
  if (lane < 0) { lane = 0; $('status').textContent = 'All four note slots are occupied at this step.'; draw(); return; }
  const word = event[lane][step], note = noteAt(selected(), lane, step);
  if (word && (track === 4 ? drumRow(note.pitch) === drumRow(point.pitch) : note.pitch === point.pitch)) {
    const end = noteStart(step, note) + noteWidth(step, note) - left;
    const resize = point.x > end - 7 && point.x < end + 2;
    drag = { from: step, to: step, lane, note, initial: note.gate, initialDuration: note.duration, resize, start: point.x };
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
    if (drag.initialDuration) {
      const deltaMs = (point.x - drag.start) * 15000 / (width * project.bpm);
      drag.note.duration = Math.max(1, durationUnits(durationMs(drag.initialDuration) + deltaMs));
    } else {
      drag.note.gate = Math.max(1, Math.min(255, Math.round(drag.initial + (point.x - drag.start) * 192 / width)));
    }
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
  if (!project || (playing && !quick) || generation || (songPlaying && (!quick || activeScene < 0))) return;
  const replace = quick ? 0 : Number($('take-mode').value);
  if (replace && eventLanes(selected()).some(events => events.some(Boolean)) &&
      !window.confirm('Replace events in the selected recording range? You can undo the take.')) return;
  take = { before: project, quick, scene: songPlaying ? activeScene : undefined, timer: setInterval(() => send('record-status'), 150) };
  $('take').textContent = 'End take';
  $('quick-take').textContent = 'End take';
  $('take-state').textContent = 'Arming';
  send('record-start', { track, quick, bars: $('take-bars').selectedIndex,
    countIn: Number($('take-start').value), quantization: Number($('take-quant').value), replace });
}
$('take').addEventListener('click', () => startTake(false));
$('quick-take').addEventListener('click', () => startTake(true));

$('gen-apply').addEventListener('click', () => {
  if (!project || track !== 4 || generation || take || (songPlaying && activeScene < 0)) return;
  if (eventLanes((songPlaying ? arrangement.scenes[activeScene].project : project).tracks[4]).some(events => events.some(Boolean)) &&
      !window.confirm('Replace the current drum pattern? You can undo this.')) return;
  const hit = $('gen-hit').valueAsNumber;
  const groove = $('gen-groove').valueAsNumber;
  const variation = $('gen-variation').valueAsNumber;
  if (![hit, groove, variation].every(value => within(value, 0, 100))) return;
  generation = { id: ++generationId, project };
  send('gen-drums', { id: generation.id, style: Number($('gen-style').value), hit, groove, variation, bars: Number($('gen-bars').value) });
  $('status').textContent = playing ? 'Drums queued for the next bar' : 'Generating drums';
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
$('length').addEventListener('change', () => {
  if (!project || take) return;
  const value = Number($('length').value);
  if (![16, 32, 48, 64].includes(value)) return;
  step = Math.min(step, value - 1);
  edit(p => { p.tracks[track].mix = withPatternLength(p.tracks[track].mix, value); });
});
for (const [id, shift, min, max] of [['pan', 8, 0, 255], ['level', 16, 0, 100]]) {
  numberInput(id, min, max, (p, v) => {
    const byte = id === 'level' ? Math.round(v * 255 / 100) : v;
    p.tracks[track].mix = (p.tracks[track].mix & ~(255 << shift) | byte << shift) >>> 0;
  });
}
for (const [id, key, max] of [['velocity', 'velocity', 127], ['gate', 'gate', 255], ['prob', 'probability', 100], ['timing', 'timing', 15]]) {
  numberInput(id, id === 'prob' || id === 'timing' ? 0 : 1, max, (p, v) => {
    const note = noteAt(p.tracks[track], lane, step);
    note[key] = v;
    setNote(p.tracks[track], lane, step, note);
  });
}
$('step-mode').addEventListener('change', () => edit(p => {
  const note = noteAt(p.tracks[track], lane, step);
  note.mode = Number($('step-mode').value);
  setNote(p.tracks[track], lane, step, note);
}));
$('fill').addEventListener('click', () => {
  if (!worklet) return;
  const enabled = $('fill').getAttribute('aria-pressed') !== 'true';
  $('fill').setAttribute('aria-pressed', String(enabled));
  send('fill', { enabled });
});
numberInput('hold-ms', 0, 190214, (p, value) => {
  const note = noteAt(p.tracks[track], lane, step);
  note.duration = durationUnits(value);
  setNote(p.tracks[track], lane, step, note);
});
$('patch').addEventListener('change', () => {
  edit(p => {
    p.tracks[track].patch = Number($('patch').value);
    if (track !== 4) {
      delete p.tracks[track].timbre;
      delete p.tracks[track].fmEnvelopes;
      p.tracks[track].mix |= modernSynth;
      if (engines[p.tracks[track].patch] === 0 || engines[p.tracks[track].patch] === 2)
        p.tracks[track].mix |= advancedSynth;
      else p.tracks[track].mix &= ~advancedSynth;
    }
  });
  send('audition', { track, pitch: track === 4 ? 36 : 60 });
});
$('mute').addEventListener('change', () => edit(p => {
  p.tracks[track].mix = (p.tracks[track].mix & ~(1 << 24)) | (Number($('mute').checked) << 24);
}));
$('drum-lane').addEventListener('change', () => {
  lane = Number($('drum-lane').value) + 1;
  draw();
});
for (const [id, shift] of [['drum-mute', 0], ['drum-solo', 16]]) {
  $(id).addEventListener('change', () => edit(p => {
    const bit = 1 << (Number($('drum-lane').value) + shift);
    p.drumFilters = ($(id).checked ? p.drumFilters | bit : p.drumFilters & ~bit) >>> 0;
  }));
}
$('clear').addEventListener('click', () => edit(p => { setNote(p.tracks[track], lane, step, null); }));
$('undo').addEventListener('click', () => {
  if (take || songPlaying || generation || !stateBefore.length) return;
  stateAfter.push(project); project = stateBefore.pop(); step = 0; sync();
});
$('redo').addEventListener('click', () => {
  if (take || songPlaying || generation || !stateAfter.length) return;
  stateBefore.push(project); project = stateAfter.pop(); step = 0; sync();
});
$('play').addEventListener('click', () => {
  if (take || songPlaying) return;
  playing = !playing;
  send('transport', { playing });
  $('play').textContent = playing ? 'Stop' : 'Play';
  $('gen-apply').disabled = songPlaying && activeScene < 0 || !!generation || !!take;
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
      if (data.type === 'mixer-meters') {
        data.levels.forEach((value, track) => {
          const meter = $(`meter-${track}`);
          if (meter) meter.value = value;
        });
        return;
      }
      if (data.type === 'song-position') {
        activeScene = data.scene;
        $('song-position').textContent = `${arrangement.scenes[data.scene].name} · Bar ${data.bar + 1} · Repeat ${data.repeat + 1}`;
        draw();
        return;
      }
      if (data.type === 'song-stopped') {
        songPlaying = false;
        activeScene = -1;
        $('song-position').textContent = 'Song stopped';
        sync();
        return;
      }
      if (data.type === 'scene-take' && take) {
        clearInterval(take.timer);
        take = undefined;
        const scene = arrangement.scenes[data.scene];
        rememberSceneClip(data.scene, data.track, scene.project.tracks[data.track]);
        scene.project.tracks[data.track] = data.part;
        if (data.scene === sceneIndex) project = structuredClone(scene.project);
        saveSong();
        $('take').textContent = 'Record track';
        $('quick-take').textContent = 'Quick REC';
        $('take-state').textContent = `Take captured · ${scene.name}`;
        draw();
        return;
      }
      if (data.type === 'studio-take' && take) {
        if (!data.state && take.scene !== undefined) {
          clearInterval(take.timer);
          take = undefined;
          $('take').textContent = 'Record track';
          $('quick-take').textContent = 'Quick REC';
          $('take-state').textContent = 'Recording unavailable';
          return;
        }
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
        project = validateProject(project.mixer ? { ...data.project, mixer: project.mixer } : data.project);
        $('take').textContent = 'Record track';
        $('quick-take').textContent = 'Quick REC';
        $('take-state').textContent = 'Take captured';
        sync();
        return;
      }
      if (data.type === 'studio-generated') {
        if (!generation || data.id !== generation.id) return;
        if (data.scene !== undefined && data.drums) {
          const scene = arrangement.scenes[data.scene];
          const { length, ...drums } = data.drums;
          const part = scene.project.tracks[4];
          rememberSceneClip(data.scene, 4, part);
          part.mix = withPatternLength(part.mix, length);
          Object.assign(part, drums);
          part.modes = part.modes.map(lane => lane.map((mode, step) => step < length ? 0 : mode));
          if (data.scene === sceneIndex) project = structuredClone(scene.project);
          generation = undefined;
          saveSong();
          $('status').textContent = `Drum pattern applied · ${scene.name}`;
          draw();
          return;
        }
        const valid = generation.project === project && data.drums;
        generation = undefined;
        if (valid) edit(next => {
          const { length, ...drums } = data.drums;
          const previous = next.tracks[4];
          previous.mix = withPatternLength(previous.mix, length);
          Object.assign(previous, drums);
          previous.modes = previous.modes.map(lane => lane.map((mode, step) => step < length ? 0 : mode));
        });
        else sync();
        $('status').textContent = valid ? 'Drum pattern applied' : 'Drum generation canceled';
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
      patches = data.names;
      engines = data.engines;
      drumKits = data.kitNames;
      const initial = { format: 'slap-5-song', version: 1, name: 'Untitled song',
        scenes: [{ name: 'Scene 1', bars: 4, repeats: 1, project: structuredClone(project) }] };
      try {
        const saved = localStorage.getItem(songKey);
        arrangement = saved ? validateSong(JSON.parse(saved)) : initial;
      } catch { arrangement = initial; }
      project = structuredClone(arrangement.scenes[0].project);
      send('project', { project });
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
