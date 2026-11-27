/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { SlapUpdater } from './flash-transport.mjs';
import { validateRelease, verifyPackage } from './flash-release.mjs';

const connect = document.getElementById('connect');
const install = document.getElementById('install');
const confirmed = document.getElementById('confirmed');
const status = document.getElementById('result');
const availability = document.getElementById('availability');
const desktop = document.getElementById('desktop-fallback');
let release, access, checked, running = false;

async function loadRelease() {
  const response = await fetch('./release-testing-0.1.53.json', { cache: 'no-store', redirect: 'error' });
  if (response.status === 404) throw new Error('No browser release is available. Use M-Flasher below.');
  if (!response.ok) throw new Error('Could not load the release manifest.');
  release = validateRelease(await response.json());
  availability.textContent = `SLAP-5 ${release.version} TESTING · ${release.product}`;
  connect.disabled = false;
}

function updateInstall() {
  confirmed.disabled = !release || !checked || running;
  install.disabled = !release || !checked || !confirmed.checked || running;
}

async function identify() {
  const updater = new SlapUpdater(access);
  const port = updater.singlePort();
  const id = await updater.probe(port);
  if (!id || id.model !== 'FM-1') throw new Error('No running FM-1 replied over Web MIDI.');
  return { updater, id, port };
}

connect.addEventListener('click', async () => {
  connect.disabled = true;
  checked = undefined;
  updateInstall();
  try {
    if (!navigator.requestMIDIAccess) throw new Error('Use an HTTPS browser with Web MIDI SysEx support.');
    access = await navigator.requestMIDIAccess({ sysex: true });
    const { id, port } = await identify();
    confirmed.checked = false;
    checked = !id.open && release?.sources.includes(id.text) ? { identity: id.text, port } : undefined;
    status.textContent = `Detected ${id.text}. ${checked ? 'Browser update supported by this firmware. Confirm your hardware to continue.' :
      'Use M-Flasher for this device.'}`;
    if (!checked) desktop.focus();
  } catch (error) {
    status.textContent = error.message + ' Continue with M-Flasher for USB download mode.';
    desktop.focus();
  } finally {
    connect.disabled = false;
    updateInstall();
  }
});

confirmed.addEventListener('change', updateInstall);
window.addEventListener('beforeunload', event => {
  if (running) event.preventDefault();
});
install.addEventListener('click', async () => {
  if (!release || !checked || !confirmed.checked || running) return;
  running = true;
  connect.disabled = true;
  updateInstall();
  let transferStarted = false, device;
  try {
    status.textContent = 'Downloading and verifying the release package…';
    const response = await fetch(`./${release.file}`, { redirect: 'error' });
    if (!response.ok) throw new Error('Release download failed.');
    const image = await verifyPackage(release, new Uint8Array(await response.arrayBuffer()));
    const { updater, id, port } = await identify();
    if (id.text !== checked.identity || port !== checked.port)
      throw new Error('The selected FM-1 changed. Detect it again.');
    device = await updater.find(candidate => candidate.text === checked.identity && !candidate.open, port);
    if (!device) throw new Error('The verified FM-1 disconnected.');
    transferStarted = true;
    await updater.stages(device, image, release.product, (stage, count, bytes) => {
      const messages = { start: 'Starting update', loader: 'Waiting for the update loader',
        reboot: 'Waiting for the installed firmware identity', done: 'Verified installed identity' };
      status.textContent = bytes ? `${stage}: ${count} requests, ${bytes} bytes transmitted.` :
        `${messages[stage] ?? stage}${count ? ': ' + count : ''}.`;
    });
  } catch (error) {
    status.textContent = error.message + (transferStarted ?
      ' Check the FM-1 display and follow the M-Flasher recovery guidance before retrying.' : ' Transfer did not start.');
  } finally {
    device?.link.close();
    running = false;
    checked = undefined;
    confirmed.checked = false;
    connect.disabled = false;
    updateInstall();
  }
});

loadRelease().catch(error => { availability.textContent = error.message; });
