/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
globalThis.fm1App = {
  message({ ex, mem, port }, m) {
    if (m.command === 'read') {
      const byte = new Uint8Array(mem.buffer);
      const names = Array.from({ length: 128 }, (_, i) => {
        const start = ex.studio_patch_name(i);
        return String.fromCharCode(...byte.subarray(start, byte.indexOf(0, start)));
      });
      port.postMessage({
        type: 'studio-project',
        names,
        project: {
          version: 1,
          bpm: ex.studio_bpm(),
          swing: ex.studio_swing(),
          tracks: Array.from({ length: 5 }, (_, t) => ({
            mix: ex.studio_mix(t) >>> 0,
            patch: ex.studio_patch(t),
            steps: Array.from({ length: 64 }, (_, s) => ex.studio_step(t, s) >>> 0)
          }))
        }
      });
    } else if (m.command === 'project') {
      const { bpm, swing, tracks } = m.project;
      ex.studio_set_clock(bpm, swing);
      tracks.forEach(({ mix, patch, steps }, t) => {
        ex.studio_set_track(t, mix, patch);
        steps.forEach((word, s) => ex.studio_set_step(t, s, word));
      });
    } else if (m.command === 'transport') {
      ex.studio_transport(m.playing ? 1 : 0);
    } else if (m.command === 'audition') {
      ex.studio_audition(m.track, m.pitch);
    }
  }
};
