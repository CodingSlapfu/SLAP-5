/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
globalThis.fm1App = {
  message({ ex, mem, port }, m) {
    if (m.command === 'read' || m.command === 'read-take') {
      const byte = new Uint8Array(mem.buffer);
      const names = Array.from({ length: 128 }, (_, i) => {
        const start = ex.studio_patch_name(i);
        return String.fromCharCode(...byte.subarray(start, byte.indexOf(0, start)));
      });
      port.postMessage({
        type: m.command === 'read-take' ? 'studio-take-project' : 'studio-project',
        names,
        project: {
          version: 3,
          drumFilters: ex.studio_drum_filters(),
          bpm: ex.studio_bpm(),
          metronome: ex.studio_metronome(),
          swing: ex.studio_swing(),
          tracks: Array.from({ length: 5 }, (_, t) => ({
            mix: ex.studio_mix(t) >>> 0,
            patch: ex.studio_patch(t),
            fx: Array.from({ length: 9 }, (_, page) => ex.studio_fx(t, page) >>> 0),
            steps: Array.from({ length: 512 }, (_, s) => (ex.studio_event(t, s, 0) & 0x7fff7fff) >>> 0),
            offsets: Array.from({ length: t === 4 ? 7 : 2 }, (_, lane) =>
              Array.from({ length: 512 }, (_, s) => ex.studio_note_offset(t, s, lane))),
            extra: Array.from({ length: t === 4 ? 6 : 1 }, (_, lane) =>
              Array.from({ length: 512 }, (_, s) => (ex.studio_event(t, s, lane + 1) & 0x7fff7fff) >>> 0))
          }))
        }
      });
    } else if (m.command === 'project') {
      const { bpm, swing, tracks, drumFilters, metronome } = m.project;
      ex.studio_set_metronome(metronome ?? 0);
      ex.studio_set_drum_filters(drumFilters ?? 0);
      ex.studio_set_clock(bpm, swing);
      tracks.forEach(({ mix, patch, steps, extra, fx, offsets }, t) => {
        ex.studio_set_track(t, mix, patch);
        fx?.forEach((word, page) => ex.studio_set_fx(t, page, word));
        for (let lane = 0; lane <= (t === 4 ? 6 : 1); lane++) {
          const events = lane ? extra?.[lane - 1] : steps;
          for (let s = 0; s < 512; s++) {
            const word = events?.[s] ?? 0;
            ex.studio_set_event(t, s, lane, word);
            if (word && offsets) ex.studio_set_note_offset(t, s, lane, offsets[lane][s]);
          }
        }
      });
    } else if (m.command === 'gen-drums') {
      const ok = ex.studio_generate_drums(m.style, m.hit, m.groove, m.variation);
      port.postMessage({ type: 'studio-generated', id: m.id, drums: !ok ? null : {
        steps: Array.from({ length: 512 }, (_, s) => (ex.studio_event(4, s, 0) & 0x7fff7fff) >>> 0),
        offsets: Array.from({ length: 7 }, (_, lane) =>
          Array.from({ length: 512 }, (_, s) => ex.studio_note_offset(4, s, lane))),
        extra: Array.from({ length: 6 }, (_, lane) =>
          Array.from({ length: 512 }, (_, s) => (ex.studio_event(4, s, lane + 1) & 0x7fff7fff) >>> 0))
      } });
    } else if (m.command === 'record-start') {
      const ok = m.quick ? ex.studio_quick_take(m.track, m.unquantized) :
        ex.studio_begin_take(m.track, m.bars, m.countIn, m.unquantized, m.replace);
      port.postMessage({ type: 'studio-take', state: ok ? ex.studio_record_state() : 0 });
    } else if (m.command === 'record-stop' || m.command === 'record-status') {
      if (m.command === 'record-stop') ex.studio_end_take();
      port.postMessage({ type: 'studio-take', state: ex.studio_record_state(), playing: !!ex.studio_is_playing() });
    } else if (m.command === 'record-note') {
      ex.studio_take_note(m.pitch, m.velocity);
    } else if (m.command === 'transport') {
      ex.studio_transport(m.playing ? 1 : 0);
    } else if (m.command === 'audition') {
      ex.studio_audition(m.track, m.pitch);
    }
  }
};
