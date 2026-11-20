import { pathToFileURL } from 'node:url';
import { readFile } from 'node:fs/promises';
const [directory, label] = process.argv.slice(2);
let Processor, project;
globalThis.currentTime = 0;
globalThis.AudioWorkletProcessor = class {
  port = { postMessage(message) { if (message.type === 'studio-project') project = message.project; } };
};
globalThis.registerProcessor = (_, type) => { Processor = type; };
for (const path of ['app_worklet.js', 'worklet.js']) await import(pathToFileURL(directory + '/' + path));
const engine = new Processor();
await engine.onMessage({ type: 'load', wasm: await readFile(directory + '/app.wasm') });
const out = [new Float32Array(128), new Float32Array(128)];
function render() { engine.process([], [out]); globalThis.currentTime += 128 / 44100; }
for (let i = 0; i < 2100; i++) render();
await engine.onMessage({ type: 'app', command: 'read' });
const encode = (pitch, velocity) => (pitch | 128 | velocity << 8 | 190 << 16 | 100 << 24) >>> 0;
for (const [t, track] of project.tracks.entries()) {
  track.patch = [0, 28, 86, 116, 21][t];
  for (let step = 0; step < 16; step++) {
    if (t === 4) {
      for (let lane = 0; lane < 6; lane++) {
        if ((step + lane) % 3 !== 0) track.extra[lane][step] = encode([36,38,42,37,39,49][lane], 91);
      }
    } else {
      track.steps[step] = encode(45 + 4 * t + step % 5, 100);
      track.extra[0][step] = encode(52 + 4 * t + step % 5, 92);
    }
  }
}
await engine.onMessage({ type: 'app', command: 'project', project });
await engine.onMessage({ type: 'app', command: 'transport', playing: true });
for (let i = 0; i < 2600; i++) render();
const time = [];
let total = 0;
for (let i = 0; i < 10000; i++) {
  const start = process.hrtime.bigint();
  render();
  const elapsed = Number(process.hrtime.bigint() - start) / 1000;
  time.push(elapsed);
  total += elapsed;
}
time.sort((a,b) => a - b);
console.log(label, 'mean/P95/P99/max µs', [total/10000, time[9499], time[9899], time.at(-1)].map(n => n.toFixed(2)).join(' / '));
