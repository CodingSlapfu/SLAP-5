import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { validateProject, encodeNote, withPatternLength } from '../../site/project.mjs';

let Processor;
globalThis.currentTime = 0;
const frames = [];
globalThis.AudioWorkletProcessor = class {
  port = { postMessage(message) { if (['studio-project', 'studio-generated', 'studio-take', 'studio-take-project', 'song-position', 'song-stopped'].includes(message.type)) frames.push(message); } };
};
globalThis.registerProcessor = (name, type) => { Processor = type; };

const build = new URL('../build/emu/', import.meta.url);
await import(new URL('app_worklet.js', build));
await import(new URL('worklet.js', build));
const app = new Processor();
await app.onMessage({ type: 'load', wasm: await readFile(new URL('app.wasm', build)) });
const out = [new Float32Array(128), new Float32Array(128)];

function render(blocks) {
  let peak = 0;
  for (let i = 0; i < blocks; i++) {
    app.process([], [out]);
    globalThis.currentTime += 128 / 44100;
    for (const channel of out) {
      for (const sample of channel) {
        assert.ok(Number.isFinite(sample));
        peak = Math.max(Math.abs(sample), peak);
      }
    }
  }
  return peak;
}

const startup = render(160);
assert.ok(startup > 0.01 && startup < 0.06, `unexpected startup peak ${startup}`);
assert.equal(render(2000), 0);
await app.onMessage({ type: 'buttons', mask: 1 << 10 });
const peak = render(100);
assert.equal(peak, 0, "blank project should play silently");
console.log(`WebAssembly boot, PLAY input and unclipped stereo audio passed (peak ${peak.toFixed(4)}).`);

await app.onMessage({ type: 'app', command: 'read' });
assert.equal(frames[0].names.length, 136);
assert.ok(frames[0].project.tracks.every(t => [...t.steps, ...t.extra.flat()].every(n => n === 0)));
const next = structuredClone(validateProject(frames[0].project));
next.bpm = 135;
next.metronome = 1;
next.tracks[2].fx[5] |= 93;
next.tracks[0].patch = 95;
next.tracks[0].steps[1] = encodeNote(51, 92, 160, 85);
next.tracks[0].extra[0][1] = encodeNote(55, 83, 120, 100);
next.tracks[4].extra[0][1] = encodeNote(36, 110, 180, 100);
next.tracks[4].extra[2][1] = encodeNote(42, 86, 130, 50);
next.tracks[0].mix = withPatternLength(next.tracks[0].mix, 512);
next.tracks[0].steps[400] = encodeNote(63, 100, 120, 100);
next.tracks[0].offsets[0][400] = 13;
next.tracks[0].offsets[1][1] = 4;
next.tracks[4].offsets[3][1] = 2;
next.drumFilters = 1 << 2;
await app.onMessage({ type: 'app', command: 'project', project: next });
await app.onMessage({ type: 'app', command: 'read' });
assert.deepEqual(validateProject(frames[1].project), next);
const bad = structuredClone(next);
bad.tracks[0].steps[1] = 0xffffffff;
assert.throws(() => validateProject(bad));
await app.onMessage({ type: 'app', command: 'transport', playing: false });
render(800);
await app.onMessage({ type: 'app', command: 'audition', track: 0, pitch: 53 });
assert.ok(render(80) > 0.002);
console.log('SLAP STUDIO project round-trip, validation and patch audition passed through the compiled DSP.');

const blank = structuredClone(validateProject(frames[0].project));
const peaks = [];
for (let patch = 0; patch < frames[0].names.length; patch++) {
  blank.tracks[0].patch = patch;
  await app.onMessage({ type: 'app', command: 'project', project: blank });
  await app.onMessage({ type: 'app', command: 'audition', track: 0, pitch: 60 });
  const peak = render(170);
  assert.ok(peak > 0.0002 && peak < 0.95, `patch ${patch}: unexpected peak ${peak}`);
  peaks.push(peak);
  render(220);
}
console.log(`${peaks.length} patch auditions: peak range ${Math.min(...peaks).toFixed(4)}–${Math.max(...peaks).toFixed(4)}.`);

blank.bpm = 200;
for (let track = 0; track < 4; track++) {
  const part = blank.tracks[track];
  part.patch = [5, 39, 81, 127][track];
  for (const page of [5, 6, 7, 8]) part.fx[page] |= 95;
  for (let step = 0; step < 16; step++) {
    part.steps[step] = encodeNote(39 + track * 7 + step % 4, 122, 240, 100);
    part.extra[0][step] = encodeNote(46 + track * 7 + step % 5, 111, 192, 100);
    part.offsets[1][step] = step % 4;
  }
}
for (let lane = 0; lane < 6; lane++) {
  const pitch = [36, 38, 42, 37, 39, 49][lane];
  for (let step = lane % 4; step < 16; step += 4)
    blank.tracks[4].extra[lane][step] = encodeNote(pitch, 118, 190, 100);
}
await app.onMessage({ type: 'app', command: 'project', project: blank });
await app.onMessage({ type: 'app', command: 'transport', playing: true });
const densePeak = render(1600);
assert.ok(densePeak > 0.01 && densePeak < 0.95, `dense arrangement peak ${densePeak}`);
console.log(`Dense five-track overlapping render: peak ${densePeak.toFixed(4)}.`);

await app.onMessage({ type: 'app', command: 'transport', playing: false });
render(900);
const kitProject = structuredClone(validateProject(frames[0].project));
const kitPeaks = [];
const { kitNames } = frames[0];
assert.equal(kitNames.length, 81);
assert.deepEqual(kitNames.slice(32, 56), [
  'STUDIO', 'BRUSH', 'VINTAGE', 'DEEP 808', 'PUNCH 909', 'ACID JAZZ', 'HOUSE',
  'DEEP HOUSE', 'RUFF HOUSE', 'TECHNO', 'ELECTRO', 'HIP-HOP', 'BOOM BAP', 'TRAP',
  'JUNGLE', 'HARD DNB', 'BREAKS', 'DUBSTEP', 'SYNTHWAVE', 'IDM LAB', 'FM LAB',
  'LO-FI', 'AMBIENT', 'CINEMATIC'
]);
assert.equal(new Set(kitNames.slice(56)).size, 25);
const samplePitches = [36, 35, 38, 40, 42, 46, 44, 39, 37, 45, 47, 50, 49, 51, 70, 81];
let sampleAssignments = 0;
for (let kit = 0; kit < kitNames.length; kit++) {
  kitProject.tracks[4].patch = kit;
  await app.onMessage({ type: 'app', command: 'project', project: kitProject });
  for (const pitch of kit < 56 ? [36, 38, 42, 37, 39, 49] : samplePitches) {
    await app.onMessage({ type: 'app', command: 'audition', track: 4, pitch });
    const peak = render(120);
    if (kit >= 56) ++sampleAssignments;
    assert.ok(peak > 0.0001 && peak < 0.95, `kit ${kit}, drum ${pitch}: unexpected peak ${peak}`);
    kitPeaks.push(peak);
    render(210);
  }
}
assert.equal(sampleAssignments, 400);
console.log(`56 legacy kits and ${sampleAssignments} sample-backed assignments: peak range ${Math.min(...kitPeaks).toFixed(4)}–${Math.max(...kitPeaks).toFixed(4)}.`);

const rapid = structuredClone(validateProject(frames[0].project));
rapid.bpm = 240;
for (let lane = 0; lane < 6; lane++) {
  for (let step = 0; step < 16; step++) {
    rapid.tracks[4].extra[lane][step] = encodeNote([36, 38, 42, 37, 39, 49][lane], 117, 180, 100);
    rapid.tracks[4].offsets[lane + 1][step] = (lane * 2 + step) % 16;
  }
}
const rapidPeaks = [];
for (let kit = 0; kit < kitNames.length; kit++) {
  rapid.tracks[4].patch = kit;
  await app.onMessage({ type: 'app', command: 'project', project: rapid });
  await app.onMessage({ type: 'app', command: 'transport', playing: true });
  const peak = render(760);
  assert.ok(peak > 0.001 && peak < 0.95, `rapid kit ${kit}: unexpected peak ${peak}`);
  rapidPeaks.push(peak);
  await app.onMessage({ type: 'app', command: 'transport', playing: false });
  render(350);
}
console.log(`${rapidPeaks.length} rapid-hit kits at 240 BPM: peak range ${Math.min(...rapidPeaks).toFixed(4)}–${Math.max(...rapidPeaks).toFixed(4)}.`);

const signatures = new Set();
for (let style = 0; style < 16; style++) {
  await app.onMessage({ type: 'app', command: 'gen-drums', id: style, style, hit: 73, groove: 70, variation: 40 });
  const generated = frames.at(-1);
  assert.equal(generated.id, style);
  assert.ok(generated.drums.extra.flat().some(Boolean));
  kitProject.tracks[4] = { ...kitProject.tracks[4], ...generated.drums };
  validateProject(kitProject);
  signatures.add(JSON.stringify(generated.drums.extra));
}
assert.equal(signatures.size, 16);
await app.onMessage({ type: 'app', command: 'transport', playing: true });
await app.onMessage({ type: 'app', command: 'gen-drums', id: 16, style: 0, hit: 73, groove: 70, variation: 40, bars: 3 });
render(1500);
assert.equal(frames.at(-1).id, 16);
assert.equal(frames.at(-1).drums?.length, 48);
assert.equal(app.ex.studio_is_playing(), 1);
console.log('Studio generated 16 distinct patterns and applied a 48-step pattern during playback.');

await app.onMessage({ type: 'app', command: 'transport', playing: false });
await app.onMessage({ type: 'app', command: 'project', project: structuredClone(validateProject(frames[0].project)) });
await app.onMessage({ type: 'app', command: 'record-start', track: 0, bars: 5, countIn: 0, unquantized: 1, replace: 0 });
assert.equal(frames.at(-1).state, 6);
render(10);
for (const pitch of [60, 64, 67, 71])
  await app.onMessage({ type: 'app', command: 'record-note', pitch, velocity: 100 });
render(10);
for (const pitch of [60, 64, 67, 71])
  await app.onMessage({ type: 'app', command: 'record-note', pitch, velocity: 0 });
render(10);
await app.onMessage({ type: 'app', command: 'record-status' });
assert.equal(frames.at(-1).state, 5);
await app.onMessage({ type: 'app', command: 'record-stop' });
assert.equal(frames.at(-1).state, 0);
await app.onMessage({ type: 'app', command: 'transport', playing: false });
await app.onMessage({ type: 'app', command: 'read-take' });
const take = validateProject(frames.at(-1).project);
assert.equal(take.tracks[0].mix, withPatternLength(frames[0].project.tracks[0].mix, 512));
assert.equal(take.tracks[0].steps[0] & 127, 60);
assert.deepEqual(take.tracks[0].extra.map(lane => lane[0] & 127), [64, 67, 71]);
const previousFormat = {
  ...take, version: 3, tracks: take.tracks.map(({ modes, ...part }, track) =>
    track === 4 ? { ...part, extra: part.extra.slice(0, 6), offsets: part.offsets.slice(0, 7) } : { ...part, extra: part.extra.slice(0, 1), offsets: part.offsets.slice(0, 2) })
};
const migrated = validateProject(previousFormat);
assert.equal(migrated.tracks[0].extra[0][0] & 127, 64);
assert.ok(migrated.tracks[0].extra[1].every(word => word === 0));
assert.ok(migrated.tracks[0].extra[2].every(word => word === 0));
assert.ok((take.tracks[0].steps[0] >>> 16 & 255) > 1);
const modeProject = structuredClone(take);
modeProject.tracks[0].modes[0][0] = 1;
modeProject.tracks[0].modes[1][0] = 2;
await app.onMessage({ type: 'app', command: 'project', project: validateProject(modeProject) });
await app.onMessage({ type: 'app', command: 'read' });
const restoredModes = validateProject(frames.at(-1).project);
assert.equal(restoredModes.tracks[0].modes[0][0], 1);
assert.equal(restoredModes.tracks[0].modes[1][0], 2);
console.log('Studio recorded four simultaneous notes in a 32-bar take and migrated a two-lane project.');

await app.onMessage({ type: 'app', command: 'transport', playing: true });
render(20);
await app.onMessage({ type: 'app', command: 'record-start', track: 1, quick: true, unquantized: 0 });
render(4);
await app.onMessage({ type: 'app', command: 'record-status' });
assert.equal(frames.at(-1).state, 5);
await app.onMessage({ type: 'app', command: 'record-stop' });
assert.equal(frames.at(-1).state, 0);
assert.equal(frames.at(-1).playing, true);
console.log('Studio Quick REC started during playback and stopping the take preserved transport.');

const timingCases = [];
for (const quantization of [0, 5, 6, 7, 8]) {
  await app.onMessage({ type: 'app', command: 'transport', playing: false });
  await app.onMessage({ type: 'app', command: 'project', project: frames[0].project });
  await app.onMessage({ type: 'app', command: 'record-start', track: 0, bars: 5, countIn: 0, quantization, replace: 0 });
  await app.onMessage({ type: 'app', command: 'record-note', pitch: 60, velocity: 100 });
  render(12);
  await app.onMessage({ type: 'app', command: 'record-note', pitch: 62, velocity: 85 });
  render(172);
  await app.onMessage({ type: 'app', command: 'record-note', pitch: 62, velocity: 0 });
  render(2);
  await app.onMessage({ type: 'app', command: 'record-stop' });
  await app.onMessage({ type: 'app', command: 'read-take' });
  const part = validateProject(frames.at(-1).project).tracks[0];
  const lanes = [part.steps, ...part.extra];
  const lane = lanes.findIndex(events => (events[0] & 127) === 62);
  assert.ok(lane >= 0);
  assert.equal(part.durations[lane][0], 172);
  timingCases.push(part.offsets[lane][0]);
}
assert.equal(new Set(timingCases).size, 5);
console.log('Five quantization strengths changed onset independently of the recorded hold duration.');


const songs = [2, 1].map((repeats, scene) => {
  const project = structuredClone(validateProject(frames[0].project));
  project.bpm = 240;
  for (let track = 0; track < 5; track++) {
    const part = project.tracks[track];
    part.mix = withPatternLength(part.mix, 16 * (track % 4 + 1));
    part.steps[0] = encodeNote(track === 4 ? 36 : 55 + track + scene, 95, 150, 100);
  }
  return { bars: 1, repeats, project };
});
const songStart = frames.length;
await app.onMessage({ type: 'app', command: 'song-start', song: { scenes: songs, index: 0, loop: 'off' } });
assert.ok(render(1080) > 0.005);
assert.deepEqual(frames.slice(songStart).filter(x => x.type === 'song-position').map(x => [x.scene, x.repeat]),
  [[0, 0], [0, 1], [1, 0]]);
assert.equal(frames.at(-1).type, 'song-stopped');
assert.equal(app.ex.studio_is_playing(), 0);
console.log('Song mode advanced two audible scenes with 1–4 bar clips and exact scene repeats.');
