/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
globalThis.fm1App = {
  sendDrums(ex, port, id, ok) {
      port.postMessage({ type: 'studio-generated', id, drums: !ok ? null : {
        length: (ex.studio_mix(4) & 255) | (ex.studio_mix(4) >>> 17 & 768),
        steps: Array.from({ length: 512 }, (_, s) => (ex.studio_event(4, s, 0) & 0x7fff7fff) >>> 0),
        offsets: Array.from({ length: 17 }, (_, lane) =>
          Array.from({ length: 512 }, (_, s) => ex.studio_note_offset(4, s, lane))),
        extra: Array.from({ length: 16 }, (_, lane) =>
          Array.from({ length: 512 }, (_, s) => (ex.studio_event(4, s, lane + 1) & 0x7fff7fff) >>> 0))
      } });
  },
  process({ ex, port }) {
    if (this.meter && (++this.meterFrame % 25 === 0))
      port.postMessage({ type: 'mixer-meters', levels: Array.from({ length: 5 }, (_, track) => ex.studio_track_energy(track)) });
    if (!this.generation || ex.studio_drum_generation_pending()) return;
    this.finishDrums(ex, port);
  },
  finishDrums(ex, port, flush = false) {
    if (!this.generation) return;
    const { id, before, scene } = this.generation;
    if (flush) ex.studio_apply_queued_drums();
    if (ex.studio_drum_generation_pending()) return;
    this.generation = undefined;
    this.sendDrums(ex, { postMessage: message => {
      if (scene !== undefined && message.drums && this.song) {
        const { length, ...drums } = message.drums;
        const part = this.song.scenes[scene].project.tracks[4];
        part.mix = ((part.mix & ~0x060000ff) | length) >>> 0;
        Object.assign(part, drums);
        part.modes = part.modes.map(lane => lane.map((mode, step) => step < length ? 0 : mode));
      }
      port.postMessage({ ...message, scene });
    } }, id, ex.studio_drum_generation() !== before);
  },
  finishSceneTake(ex, mem, port) {
    if (!this.sceneTake) return;
    const { scene, track } = this.sceneTake;
    this.sceneTake = undefined;
    ex.studio_end_take();
    this.message({ ex, mem, port: { postMessage: message => {
      const part = message.project.tracks[track];
      const previous = this.song.scenes[scene].project.tracks[track];
      part.mix = ((part.mix & ~0x00ffff00) | (previous.mix & 0x00ffff00)) >>> 0;
      this.song.scenes[scene].project.tracks[track] = part;
      port.postMessage({ type: 'scene-take', scene, track, part });
    } } }, { command: 'read-take' });
  },
  setPerformanceFilters(ex, mixer) {
      for (let track = 0; track < 5; track++)
        ex.studio_set_performance_filter(track, mixer?.trackFilter?.[track] ?? 0);
      ex.studio_set_performance_filter(5, mixer?.allFilter ?? 0);
  },
  loadProject(ex, project) {
      const { bpm, swing, tracks, drumFilters, metronome, mixer } = project;
      const ducks = mixer?.duck?.reduce((word, depth, track) => word | (depth << (track * 8)), 0) ?? 0;
      ex.studio_set_mixer(mixer?.solo ?? 0, ducks, (mixer?.autoLevel === false ? 0 : 1) | (mixer?.autoMix ? 2 : 0));
      this.setPerformanceFilters(ex, mixer);
      ex.studio_set_metronome(metronome ?? 0);
      ex.studio_set_drum_filters(drumFilters ?? 0);
      ex.studio_set_clock(bpm, swing);
      tracks.forEach(({ mix, patch, steps, extra, fx, offsets, durations, modes, timbre, fmEnvelopes }, t) => {
        ex.studio_set_track(t, mix, patch);
        ex.studio_set_timbre(t, timbre ?? 0);
        if (t < 4) for (let slot = 0; slot < 7; slot++)
          ex.studio_set_fm_envelope(t, slot, fmEnvelopes?.[slot] ?? 0);
        fx?.forEach((word, page) => ex.studio_set_fx(t, page, word));
        for (let lane = 0; lane <= (t === 4 ? 16 : 3); lane++) {
          const events = lane ? extra?.[lane - 1] : steps;
          for (let s = 0; s < 512; s++) {
            const word = events?.[s] ?? 0;
            ex.studio_set_event(t, s, lane, word);
            if (word && offsets) ex.studio_set_note_offset(t, s, lane, offsets[lane][s]);
            if (word && modes?.[lane]?.[s]) ex.studio_set_step_mode(t, s, lane, modes[lane][s]);
            if (word && durations?.[lane]?.[s]) ex.studio_set_note_duration(t, lane, s, durations[lane][s]);
          }
        }
      });
  },
  render({ ex, mem, port }, outputs, frames) {
    if (!this.song) return false;
    let written = 0;
    while (written < frames) {
      const song = this.song;
      if (song && !song.remaining) {
        if (song.samples) {
          this.finishSceneTake(ex, mem, port);
          this.finishDrums(ex, port, true);
          const scene = song.scenes[song.index];
          if (song.queued !== undefined) {
            song.index = song.queued;
            song.queued = undefined;
            song.repeat = 0;
          } else if (++song.repeat >= scene.repeats) {
            song.repeat = 0;
            if (song.loop !== 'scene') ++song.index;
            if (song.index >= song.scenes.length && song.loop === 'song') song.index = 0;
          }
        }
        const scene = song.scenes[song.index];
        if (!scene) {
          this.song = undefined;
          ex.studio_transport(0);
          port.postMessage({ type: 'song-stopped' });
        } else {
          this.loadProject(ex, scene.project);
          ex.studio_transport(1);
          song.barSamples = Math.floor(44100 * 15 / scene.project.bpm) * 16;
          song.remaining = song.barSamples * scene.bars;
          song.samples = 0;
          song.lastBar = -1;
        }
      }
      const active = this.song;
      const count = Math.min(frames - written, active ?
        Math.min(active.remaining, active.barSamples - active.samples % active.barSamples) : frames);
      if (active) {
        const bar = Math.floor(active.samples / active.barSamples);
        if (bar !== active.lastBar) {
          active.lastBar = bar;
          for (const point of active.scenes[active.index].automation ?? []) {
            if (point.bar !== bar) continue;
            ex.studio_automate_mix(point.track, point.level ?? 256, point.pan ?? 256);
          }
          port.postMessage({ type: 'song-position', scene: active.index, repeat: active.repeat, bar });
        }
        active.samples += count;
        active.remaining -= count;
      }
      ex.web_render(count);
      outputs[0].set(new Float32Array(mem.buffer, ex.web_out_l(), count), written);
      if (outputs[1]) outputs[1].set(new Float32Array(mem.buffer, ex.web_out_r(), count), written);
      written += count;
    }
    return true;
  },
  message({ ex, mem, port }, m) {
    if (m.command === 'meters') {
      this.meter = !!m.enabled;
      this.meterFrame = 0;
    } else if (m.command === 'read' || m.command === 'read-take') {
      const byte = new Uint8Array(mem.buffer);
      const namesFor = (count, nameAt) => Array.from({ length: count }, (_, i) => {
        const start = nameAt(i);
        return String.fromCharCode(...byte.subarray(start, byte.indexOf(0, start)));
      });
      port.postMessage({
        type: m.command === 'read-take' ? 'studio-take-project' : 'studio-project',
        names: namesFor(ex.studio_patch_count(), ex.studio_patch_name),
        engines: Array.from({ length: ex.studio_patch_count() }, (_, patch) => ex.studio_patch_engine(patch)),
        kitNames: namesFor(ex.studio_kit_count(), ex.studio_kit_name),
        project: {
          version: 6,
          drumFilters: ex.studio_drum_filters() >>> 0,
          bpm: ex.studio_bpm(),
          metronome: ex.studio_metronome(),
          swing: ex.studio_swing(),
          tracks: Array.from({ length: 5 }, (_, t) => ({
            mix: ex.studio_mix(t) >>> 0,
            patch: ex.studio_patch(t),
            ...(ex.studio_timbre(t) ? { timbre: ex.studio_timbre(t) >>> 0 } : {}),
            ...(t < 4 ? { fmEnvelopes: Array.from({ length: 7 }, (_, slot) => ex.studio_fm_envelope(t, slot)) } : {}),
            fx: Array.from({ length: 9 }, (_, page) => ex.studio_fx(t, page) >>> 0),
            steps: Array.from({ length: 512 }, (_, s) => (ex.studio_event(t, s, 0) & 0x7fff7fff) >>> 0),
            offsets: Array.from({ length: t === 4 ? 17 : 4 }, (_, lane) =>
              Array.from({ length: 512 }, (_, s) => ex.studio_note_offset(t, s, lane))),
            modes: Array.from({ length: t === 4 ? 17 : 4 }, (_, lane) =>
              Array.from({ length: 512 }, (_, s) => ex.studio_step_mode(t, s, lane))),
            durations: Array.from({ length: t === 4 ? 17 : 4 }, (_, lane) =>
              Array.from({ length: 512 }, (_, s) => ex.studio_note_duration(t, lane, s))),
            extra: Array.from({ length: t === 4 ? 16 : 3 }, (_, lane) =>
              Array.from({ length: 512 }, (_, s) => (ex.studio_event(t, s, lane + 1) & 0x7fff7fff) >>> 0))
          }))
        }
      });
    } else if (m.command === 'performance-filters') {
      this.setPerformanceFilters(ex, m.mixer);
    } else if (m.command === 'project') {
      if (!this.song) this.loadProject(ex, m.project);
    } else if (m.command === 'song-start') {
      this.song = { ...m.song, remaining: 0, repeat: 0, samples: 0, lastBar: -1 };
    } else if (m.command === 'song-queue') {
      if (this.song) this.song.queued = m.scene;
    } else if (m.command === 'song-stop') {
      this.finishSceneTake(ex, mem, port);
      this.finishDrums(ex, port, true);
      this.song = undefined;
      ex.studio_transport(0);
      port.postMessage({ type: 'song-stopped' });
    } else if (m.command === 'gen-drums') {
      const before = ex.studio_drum_generation();
      const result = ex.studio_generate_drums(m.style, m.hit, m.groove, m.variation, m.bars ?? 0);
      if (result === 2) this.generation = { id: m.id, before, scene: this.song?.index };
      else this.sendDrums(ex, port, m.id, result);
    } else if (m.command === 'record-start') {
      const ok = m.quick ? ex.studio_quick_take(m.track, m.unquantized) :
        ex.studio_begin_take(m.track, m.bars, m.countIn, m.unquantized, m.replace);
      if (ok && this.song) this.sceneTake = { scene: this.song.index, track: m.track };
      if (ok && Number.isInteger(m.quantization)) ex.studio_take_quantization(m.quantization);
      port.postMessage({ type: 'studio-take', state: ok ? ex.studio_record_state() : 0 });
    } else if (m.command === 'record-stop' || m.command === 'record-status') {
      if (m.command === 'record-stop' && this.sceneTake) {
        this.finishSceneTake(ex, mem, port);
        return;
      }
      if (m.command === 'record-stop') ex.studio_end_take();
      port.postMessage({ type: 'studio-take', state: ex.studio_record_state(), playing: !!ex.studio_is_playing() });
    } else if (m.command === 'record-note') {
      ex.studio_take_note(m.pitch, m.velocity);
    } else if (m.command === 'fill') {
      ex.studio_set_fill(m.enabled ? 1 : 0);
    } else if (m.command === 'transport') {
      ex.studio_transport(m.playing ? 1 : 0);
    } else if (m.command === 'audition') {
      ex.studio_audition(m.track, m.pitch);
    }
  }
};
