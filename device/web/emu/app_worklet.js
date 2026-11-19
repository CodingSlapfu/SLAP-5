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
          version: 2,
          drumFilters: ex.studio_drum_filters(),
          bpm: ex.studio_bpm(),
          metronome: ex.studio_metronome(),
          swing: ex.studio_swing(),
          tracks: Array.from({ length: 5 }, (_, t) => ({
            mix: ex.studio_mix(t) >>> 0,
            patch: ex.studio_patch(t),
            fx: Array.from({ length: 9 }, (_, page) => ex.studio_fx(t, page) >>> 0),
            steps: Array.from({ length: 64 }, (_, s) => ex.studio_event(t, s, 0) >>> 0),
            extra: Array.from({ length: t === 4 ? 6 : 1 }, (_, lane) =>
              Array.from({ length: 64 }, (_, s) => ex.studio_event(t, s, lane + 1) >>> 0))
          }))
        }
      });
    } else if (m.command === 'project') {
      const { bpm, swing, tracks, drumFilters, metronome } = m.project;
      ex.studio_set_metronome(metronome ?? 0);
      ex.studio_set_drum_filters(drumFilters ?? 0);
      ex.studio_set_clock(bpm, swing);
      tracks.forEach(({ mix, patch, steps, extra, fx }, t) => {
        ex.studio_set_track(t, mix, patch);
        fx?.forEach((word, page) => ex.studio_set_fx(t, page, word));
        steps.forEach((word, s) => ex.studio_set_event(t, s, 0, word));
        for (let lane = 1; lane <= (t === 4 ? 6 : 1); lane++) {
          for (let s = 0; s < 64; s++) ex.studio_set_event(t, s, lane, extra?.[lane-1]?.[s] ?? 0);
        }
      });
    } else if (m.command === 'transport') {
      ex.studio_transport(m.playing ? 1 : 0);
    } else if (m.command === 'audition') {
      ex.studio_audition(m.track, m.pitch);
    }
  }
};
