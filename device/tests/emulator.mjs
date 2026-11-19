import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { validateProject, encodeNote } from '../../site/project.mjs';

let Processor;
globalThis.currentTime = 0;
const frames = [];
globalThis.AudioWorkletProcessor = class {
  port = { postMessage(message) { if (['studio-project', 'studio-generated'].includes(message.type)) frames.push(message); } };
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
assert.equal(frames[0].names.length, 128);
assert.ok(frames[0].project.tracks.every(t => [...t.steps, ...t.extra.flat()].every(n => n === 0)));
const next = structuredClone(validateProject(frames[0].project));
next.bpm = 135;
next.metronome = 1;
next.tracks[2].fx[5] |= 93;
next.tracks[0].patch = 95;
next.tracks[0].steps[1] = encodeNote(51, 92, 160, 85);
next.tracks[0].extra[0][1] = encodeNote(55, 83, 120, 100, 2);
next.tracks[4].extra[0][1] = encodeNote(36, 110, 180, 100);
next.tracks[4].extra[2][1] = encodeNote(42, 86, 130, 50, 1);
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
for (let patch = 0; patch < 128; patch++) {
  blank.tracks[0].patch = patch;
  await app.onMessage({ type: 'app', command: 'project', project: blank });
  await app.onMessage({ type: 'app', command: 'audition', track: 0, pitch: 60 });
  const peak = render(170);
  assert.ok(peak > 0.0002 && peak < 0.95, `patch ${patch}: unexpected peak ${peak}`);
  peaks.push(peak);
  render(220);
}
console.log(`128 patch auditions: peak range ${Math.min(...peaks).toFixed(4)}–${Math.max(...peaks).toFixed(4)}.`);

blank.bpm = 200;
for (let track = 0; track < 4; track++) {
  const part = blank.tracks[track];
  part.patch = [5, 39, 81, 127][track];
  for (const page of [5, 6, 7, 8]) part.fx[page] |= 95;
  for (let step = 0; step < 16; step++) {
    part.steps[step] = encodeNote(39 + track * 7 + step % 4, 122, 240, 100);
    part.extra[0][step] = encodeNote(46 + track * 7 + step % 5, 111, 192, 100, step % 4);
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
for (let kit = 0; kit < 32; kit++) {
  kitProject.tracks[4].patch = kit;
  await app.onMessage({ type: 'app', command: 'project', project: kitProject });
  for (const pitch of [36, 38, 42, 37, 39, 49]) {
    await app.onMessage({ type: 'app', command: 'audition', track: 4, pitch });
    const peak = render(120);
    assert.ok(peak > 0.0001 && peak < 0.95, `kit ${kit}, drum ${pitch}: unexpected peak ${peak}`);
    kitPeaks.push(peak);
    render(210);
  }
}
console.log(`32 kits, six drum lanes each: peak range ${Math.min(...kitPeaks).toFixed(4)}–${Math.max(...kitPeaks).toFixed(4)}.`);

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
await app.onMessage({ type: 'app', command: 'gen-drums', id: 16, style: 0, hit: 73, groove: 70, variation: 40 });
assert.equal(frames.at(-1).drums, null);
console.log('Studio generated 16 distinct editable drum patterns and rejected generation during playback.');
