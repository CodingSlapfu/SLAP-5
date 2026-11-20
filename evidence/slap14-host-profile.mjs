import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { readFile } from 'node:fs/promises';
const [directory, label, mode = "dry"] = process.argv.slice(2);
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
  if (mode !== 'modern') track.mix &= ~(1 << 27);
  if (mode === 'wet') {
    const fx = (a, b, c, d) => (a | b << 8 | c << 16 | d << 24) >>> 0;
    track.fx[2] = fx(2, 3 + t, 1, 102);
    track.fx[5] = fx(45, 58, 56, 65);
    track.fx[6] = fx(30, 2, 45, 87);
    track.fx[7] = fx(30, 60, 52, 73);
  }
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
const audio = createHash('sha256');
for (let i = 0; i < 10000; i++) {
  const start = process.hrtime.bigint();
  render();
  const elapsed = Number(process.hrtime.bigint() - start) / 1000;
  time.push(elapsed);
  total += elapsed;
  for (const channel of out) audio.update(Buffer.from(channel.buffer));
}
time.sort((a,b) => a - b);
console.log(label, mode, 'mean/P95/P99/max µs', [total/10000, time[9499], time[9899], time.at(-1)].map(n => n.toFixed(2)).join(' / '));
console.log(label, mode, 'PCM SHA-256', audio.digest('hex'));
