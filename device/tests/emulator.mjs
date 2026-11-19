import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { validateProject, encodeNote } from '../../site/project.mjs';

let Processor;
globalThis.currentTime = 0;
const frames = [];
globalThis.AudioWorkletProcessor = class {
  port = { postMessage(message) { if (message.type === 'studio-project') frames.push(message); } };
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
assert.equal(render(100), 0);
await app.onMessage({ type: 'buttons', mask: 1 << 10 });
const peak = render(100);
assert.ok(peak > 0.005 && peak < 0.95, `unexpected peak ${peak}`);
console.log(`WebAssembly boot, PLAY input and unclipped stereo audio passed (peak ${peak.toFixed(4)}).`);

await app.onMessage({ type: 'app', command: 'read' });
assert.equal(frames[0].names.length, 128);
const next = structuredClone(validateProject(frames[0].project));
next.bpm = 135;
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
