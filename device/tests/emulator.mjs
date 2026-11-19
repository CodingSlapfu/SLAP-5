import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

let Processor;
globalThis.currentTime = 0;
globalThis.AudioWorkletProcessor = class {
  port = { postMessage() {} };
};
globalThis.registerProcessor = (name, type) => { Processor = type; };

const build = new URL('../build/emu/', import.meta.url);
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

assert.equal(render(260), 0);
await app.onMessage({ type: 'buttons', mask: 1 << 10 });
const peak = render(100);
assert.ok(peak > 0.005 && peak < 0.95, `unexpected peak ${peak}`);
console.log(`WebAssembly boot, PLAY input and unclipped stereo audio passed (peak ${peak.toFixed(4)}).`);
