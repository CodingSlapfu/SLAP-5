/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { Updater } from './protocol/fm1ota.js';

const button = document.getElementById('connect');
const status = document.getElementById('result');
button.addEventListener('click', async () => {
  button.disabled = true;
  try {
    if (!navigator.requestMIDIAccess) throw new Error('Use a browser with Web MIDI SysEx support over HTTPS.');
    status.textContent = 'Waiting for an FM-1 identity reply…';
    const access = await navigator.requestMIDIAccess({ sysex: true });
    const device = await new Updater(access).find();
    if (!device) throw new Error('No FM-1 replied. Connect the device directly by USB and retry.');
    device.link.close();
    status.textContent = `Detected ${device.id.text} on ${device.name}.`;
  } catch (error) {
    status.textContent = error.message;
  } finally {
    button.disabled = false;
  }
});
