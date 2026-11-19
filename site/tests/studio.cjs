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
    assert.equal(await page.locator('#patch option').count(), 128);
    await page.locator('#play').click();
    await page.getByRole('button', { name: '5 · Drums' }).click();
    assert.equal(await page.locator('h1').textContent(), 'Drum sequencer');
    const roll = page.locator('#roll');
    const box = await roll.boundingBox();
    await page.mouse.click(box.x + 69 + 5 * 40 + 12, box.y + 27 + 2 * 23 + 10);
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
    assert.equal((project.tracks[4].extra[2][5] >>> 8) & 127, 88);
    await page.locator('#import').setInputFiles({ name: 'song.json', mimeType: 'application/json',
      buffer: Buffer.from(JSON.stringify({ ...project, bpm: 138 })) });
    await page.waitForFunction(() => document.getElementById('bpm').value === '138');
    await page.locator('#play').click();
    await page.reload();
    await page.locator('#start').click();
    await page.waitForFunction(() => !document.getElementById('play').disabled);
    assert.equal(await page.locator('#bpm').inputValue(), '138');
    assert.deepEqual(errors, []);
    console.log('Chromium loaded DSP, 128 patches, transport, drum edits, undo/redo and project round-trip.');
  } finally {
    await browser.close();
  }
}

test().catch(error => { console.error(error); process.exitCode = 1; });
