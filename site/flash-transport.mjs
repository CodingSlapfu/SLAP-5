/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { Updater } from './protocol/fm1ota.js';
import { livePorts } from './protocol/fm1ports.js';

export class SlapUpdater extends Updater {
  singlePort({ allowEmpty = false } = {}) {
    const inputs = livePorts(this.access.inputs);
    const outputs = livePorts(this.access.outputs);
    if (allowEmpty && inputs.length <= 1 && outputs.length <= 1 &&
        (!inputs.length || !outputs.length)) return null;
    if (inputs.length !== 1 || outputs.length !== 1)
      throw new Error('Disconnect other USB MIDI devices before updating.');
    if (!this.usual(inputs[0].name) || !this.usual(outputs[0].name))
      throw new Error('The USB MIDI ports do not identify as FM-1 ports.');
    return inputs[0].name;
  }

  async waitFor(filter, ms) {
    const deadline = Date.now() + ms;
    while (Date.now() < deadline) {
      const name = this.singlePort({ allowEmpty: true });
      const device = name && await this.find(() => true, name);
      if (device) {
        const { id } = device;
        if (/^ota-/i.test(id.model) && id.text !== 'ota-FM-1_900') {
          device.link.close();
          throw new Error(`Unexpected update loader ${id.text}. Use the recovery guide.`);
        }
        if (filter(id)) return device;
        device.link.close();
      }
      await new Promise(resolve => setTimeout(resolve, 1000));
    }
    return null;
  }
}
