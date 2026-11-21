/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
const assert = require('node:assert/strict');
const { chromium } = require('playwright');

async function test() {
  const browser = await chromium.launch({
    executablePath: process.env.CHROMIUM,
    args: ['--autoplay-policy=no-user-gesture-required', '--no-sandbox']
  });
  try {
    const page = await browser.newPage({ viewport: { width: 1160, height: 800 } });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto((process.env.STUDIO_URL || 'http://127.0.0.1:8764') + '/studio.html');
    await page.locator('#start').click();
    await page.waitForFunction(() => !document.getElementById('play').disabled);
    assert.equal(await page.locator('#patch option').count(), 136);
    await page.locator('#metro').selectOption('2');
    await page.locator('#fx-page').selectOption('5');
    const wet = page.getByLabel('Wet', { exact: true });
    await wet.fill('71');
    await wet.press('Tab');
    assert.equal(await wet.inputValue(), '71');
    await page.locator('#play').click();
    await page.getByRole('button', { name: '5 · Drums' }).click();
    assert.equal(await page.locator('h1').textContent(), 'Drum sequencer');
    assert.equal(await wet.inputValue(), '0');
    assert.equal(await page.locator('#gen-style option').count(), 16);
    assert.equal(await page.locator('#drum-lane option').count(), 16);
    assert.equal(await page.locator('#patch option').count(), 25);
    assert.equal(await page.locator('#patch').inputValue(), '56');
    await page.locator('#patch').selectOption({ label: 'CINEMATIC HYBRID' });
    assert.ok(await page.locator('#gen-apply').isEnabled());
    const roll = page.locator('#roll');
    const box = await roll.boundingBox();
    const gridLeft = box.width - Number(await page.locator("#length").inputValue()) * 40;
    await page.mouse.click(box.x + gridLeft + 5 * 40 + 12, box.y + 27 + 2 * 23 + 10);
    assert.match(await page.locator('#position').textContent(), /Step 6/);
    assert.equal(await page.locator('#velocity').inputValue(), '100');
    await page.locator('#velocity').fill('88');
    await page.locator('#velocity').press('Tab');
    await page.locator('#undo').click();
    await page.locator('#redo').click();
    const downloadPromise = page.waitForEvent('download');
    await page.locator('#export').click();
    const download = await downloadPromise;
    const fs = require('node:fs/promises');
    const filename = await download.path();
    const project = JSON.parse(await fs.readFile(filename, 'utf8'));
    assert.equal(project.tracks.length, 5);
    assert.equal(project.metronome, 2);
    assert.equal(project.tracks[0].fx[5] & 255, 71);
    assert.equal(project.tracks[4].fx[5] & 255, 0);
    assert.equal(project.tracks[4].patch, 80);
    project.tracks[4].patch = 31;
    assert.equal((project.tracks[4].extra[2][5] >>> 8) & 127, 88);
    await page.locator('#import').setInputFiles({ name: 'song.json', mimeType: 'application/json',
      buffer: Buffer.from(JSON.stringify({ ...project, bpm: 138 })) });
    await page.waitForFunction(() => document.getElementById('bpm').value === '138');
    assert.equal(await page.locator('#patch').inputValue(), '31');
    const importedKitOptions = await page.locator('#patch option').evaluateAll(options =>
      options.map(option => ({ id: Number(option.value), label: option.textContent })));
    assert.equal(importedKitOptions.length, 26);
    assert.deepEqual(importedKitOptions.map(({ id }) => id), [31, ...Array.from({ length: 25 }, (_, i) => 56 + i)]);
    assert.match(importedKitOptions[0].label, /legacy/);
    assert.equal(importedKitOptions.filter(({ label }) => label.includes('(legacy)')).length, 1);
    await page.locator('#play').click();
    page.once('dialog', dialog => dialog.accept());
    await page.locator('#gen-apply').click();
    await page.waitForFunction(() => !document.getElementById('gen-apply').disabled);
    const generatedPromise = page.waitForEvent('download');
    await page.locator('#export').click();
    const generatedFile = await (await generatedPromise).path();
    const generated = JSON.parse(await fs.readFile(generatedFile, 'utf8'));
    assert.equal(generated.tracks[4].patch, 31);
    assert.notDeepEqual(generated.tracks[4].extra, project.tracks[4].extra);
    assert.ok(generated.tracks[4].extra.flat().some(Boolean));
    await page.locator('#undo').click();
    await page.getByRole('button', { name: 'Track 1', exact: true }).click();
    await page.locator('#take-bars').selectOption({ label: '4' });
    await page.locator('#take').click();
    await page.waitForFunction(() => document.getElementById('take-state').textContent === 'Play a note');
    const key = page.locator('#take-keys button').first();
    await key.focus();
    await page.keyboard.down('Space');
    await page.waitForFunction(() => document.getElementById('take-state').textContent === 'Recording');
    await page.keyboard.up('Space');
    await page.locator('#take').click();
    await page.waitForFunction(() => document.getElementById('take-state').textContent === 'Take captured');
    assert.equal(await page.locator('#length').inputValue(), '64');
    assert.ok(await page.locator('#next-steps').isDisabled());
    const legacyLength = structuredClone(project);
    legacyLength.tracks[0].mix = (legacyLength.tracks[0].mix & ~0x060000ff) | 128;
    await page.locator('#import').setInputFiles({ name: 'legacy-length.json', mimeType: 'application/json',
      buffer: Buffer.from(JSON.stringify({ ...legacyLength, bpm: 138 })) });
    await page.waitForFunction(() => document.getElementById('length').value === '128');
    await page.locator('#next-steps').click();
    assert.match(await page.locator('#position').textContent(), /Step 65/);
    await page.reload();
    await page.locator('#start').click();
    await page.waitForFunction(() => !document.getElementById('play').disabled);
    assert.equal(await page.locator('#bpm').inputValue(), '138');
    await page.locator('#scene-bars').fill('1');
    await page.locator('#scene-bars').press('Tab');
    await page.locator('#scene-copy').click();
    assert.equal(await page.locator('.song-row').first().locator('button').count(), 2);
    await page.locator('#scene-name').fill('Second');
    await page.locator('#scene-name').press('Tab');
    await page.locator('#scene-earlier').click();
    await page.locator('#song-loop').selectOption('song');
    await page.locator('#song-play').click();
    await page.waitForFunction(() => document.getElementById('song-position').textContent.includes('Second'));
    await page.waitForFunction(() => document.getElementById('song-position').textContent.includes('Scene 1'));
    await page.locator('#song-play').click();
    await page.waitForFunction(() => document.getElementById('song-position').textContent === 'Song stopped');
    await page.locator('#studio-mixer summary').click();
    assert.equal(await page.locator('#mixer-channels meter').count(), 5);
    await page.locator('#mix-auto').check();
    assert.ok(await page.locator('#mix-auto').isChecked());
    assert.deepEqual(errors, []);
    console.log('Chromium loaded DSP, 136 patches, transport, drum edits, undo/redo and project round-trip.');
  } finally {
    await browser.close();
  }
}

test().catch(error => { console.error(error); process.exitCode = 1; });
