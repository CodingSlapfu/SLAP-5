/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { validateRelease, verifyPackage } from '../flash-release.mjs';
import { SlapUpdater } from '../flash-transport.mjs';
import { pack7, unpack7 } from '../protocol/fm1ota.js';

const raw = new Uint8Array(await readFile(process.argv[2]));
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
const version = process.argv[3] ?? '0.1.0';
const product = 'FM-1_75' + version.split('.').map(v => v.padStart(2, '0')).join('');
const release = validateRelease({ version, product,
  sha256: sha(raw), bytes: raw.length, sources: ['FM-1_015'] });
const valid = await verifyPackage(release, raw);
const bad = raw.slice();
bad[0x500] ^= 0x80;
await assert.rejects(verifyPackage(release, bad), /SHA-256/);
await assert.rejects(verifyPackage({ ...release, sha256: sha(bad) }, bad), /checksum/);
await assert.rejects(verifyPackage({ ...release, product: 'FM-1_75000200' }, raw), /identity/);
assert.throws(() => validateRelease({ ...release, sources: ['FM-1_016'] }), /manifest/);

const port = { id: 'fm1', state: 'connected', name: 'FM-1' };
const access = { inputs: new Map([['fm1', port]]), outputs: new Map([['fm1', port]]) };
const transport = new SlapUpdater(access);
assert.equal(transport.singlePort(), 'FM-1');
access.inputs.set('other', { ...port, id: 'other' });
assert.throws(() => transport.singlePort(), /Disconnect other/);
access.inputs.delete('other');

function request(addr, len) {
  const bytes = [0, 0x59, 0x30, 8, 0, 0, 0, addr & 255, addr >>> 8 & 255,
    addr >>> 16 & 255, addr >>> 24, len & 255, len >>> 8 & 255, len >>> 16];
  const sum = bytes.slice(6).reduce((a, b) => a + b, 0);
  return [0xf0, ...pack7([...bytes, ~sum & 255]), 0xf7];
}
const replies = [], frames = [request(1024, 512), request(0xf0000000, 8)];
const link = { read: async () => frames.shift(), send: data => { replies.push(data); return true; } };
assert.deepEqual(await transport.serve(link, valid, 0xf0000000, 1000),
  { served: 1, finished: true });
const response = unpack7(replies[0].slice(1, -1));
assert.deepEqual(response.slice(14, -1), [...valid.subarray(1024, 1536)]);
await assert.rejects(transport.serve({ ...link, read: async () => request(valid.length, 1) },
  valid, 0xf0000000, 1000), /bad request/);
console.log('Checked packaged bytes, corruption, source and target identities, ambiguous ports, and simulated pull requests.');

if (['0.1.4', '0.1.7', '0.1.11', '0.1.12'].includes(version)) {
  const manifest = validateRelease(JSON.parse(await readFile(new URL('../release.json', import.meta.url))));
  await verifyPackage(manifest, raw);
  for (const source of ['FM-1_73000738', 'FM-1_75000102', 'FM-1_015'])
    assert.ok(manifest.sources.includes(source));
  assert.ok(!manifest.sources.includes('FM-1_016'));

  async function simulate(source, { loader = 'ota-FM-1_900', disconnect = false, result = manifest.product } = {}) {
    const out = { ...port };
    const midi = { inputs: new Map([['fm1', port]]), outputs: new Map([['fm1', out]]) };
    const updater = new SlapUpdater(midi);
    const stages = [];
    function device(identity, frames = [], lost = false) {
      const model = identity.split('_')[0];
      return { id: { text: identity, model }, link: {
        get lost() { return lost; },
        read: async () => frames.shift(), send: () => true, close() {}
      } };
    }
    const running = device(source, [request(1024, 512), request(0xe0000000, 8)]);
    const ota = device(loader, disconnect ? [] : [request(1536, 128), request(0xf0000000, 8)], disconnect);
    const restarted = device(result);
    let next;
    updater.find = async () => next;
    const operation = updater.stages(running, valid, manifest.product, (stage, count, bytes) => {
      stages.push([stage, bytes]);
      if (stage === 'loader' || stage === 'reboot') {
        midi.outputs.clear();
        assert.equal(updater.singlePort({ allowEmpty: true }), null);
        next = stage === 'loader' ? ota : restarted;
        setTimeout(() => midi.outputs.set('fm1', out), 5);
      }
    });
    if (loader !== 'ota-FM-1_900') return assert.rejects(operation, /Unexpected update loader/);
    if (disconnect) return assert.rejects(operation, /disconnected/);
    if (result !== manifest.product) return assert.rejects(operation, /device reports/);
    assert.equal(await operation, manifest.product);
    assert.deepEqual(stages, [['start', undefined], ['verify', 512], ['loader', undefined],
      ['write', 128], ['reboot', undefined], ['done', undefined]]);
  }
  await Promise.all([
    simulate('FM-1_73000738'), simulate('FM-1_75000102'), simulate('FM-1_75000104'), simulate('FM-1_75000107'), simulate('FM-1_015'),
    ...(version === '0.1.12' ? [simulate('FM-1_75000111')] : []),
    simulate('FM-1_73000738', { loader: 'ota-FM1C_014' }),
    simulate('FM-1_73000738', { disconnect: true }),
    simulate('FM-1_73000738', { result: 'FM-1_73000738' }),
  ]);
  console.log('Simulated full AQUORA, SLAP-5 and stock OTA staging, loader reconnect, writes, restart, and rejection paths.');
}
