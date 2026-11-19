/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
export const within = (v, min, max) => Number.isInteger(v) && v >= min && v <= max;
export const drumNotes = [36, 38, 42, 37, 39, 49];
export const eventLanes = track => [track.steps, ...track.extra];
export function encodeNote(pitch, velocity, gate, probability, timing = 0) {
  return ((pitch | 128) | velocity << 8 | gate << 16 | probability << 24 | (timing & 1) << 15 | (timing & 2) << 30) >>> 0;
}
export function decodeNote(word) {
  return { pitch: word & 127, velocity: word >>> 8 & 127, gate: word >>> 16 & 255, probability: word >>> 24 & 127, timing: (word >>> 15 & 1) | (word >>> 30 & 2) };
}
export const fxPanels = [
  [], ['Low', 'Band', 'High', 'Resonance'], ['Mode', 'Pattern', 'Rate', 'Depth'],
  ['Type', 'Amount', 'Tone', 'Rate'], ['Mix'], ['Wet', 'Decay', 'Size', 'Damping'],
  ['Wet', 'Time', 'Feedback', 'Tone'], ['Wet', 'Depth', 'Rate', 'Tone'], ['Wet', 'Gain', 'Tone', 'Shape']
];
export const fxLimit = (page, knob) => page === 2 && knob < 3 ? [2, 19, 3][knob] :
  page === 3 && knob === 0 ? 7 : page === 6 && knob === 1 ? 5 : 127;
const fxWord = (a, b, c, d) => a | b << 8 | c << 16 | d << 24;
const defaultFx = [0, fxWord(0,0,0,22), fxWord(0,0,1,102), fxWord(0,48,68,52), 100,
  fxWord(0,48,56,65), fxWord(0,2,52,87), fxWord(0,60,52,94), fxWord(0,54,108,39)];

export function validateProject(project) {
  if (![1, 2].includes(project?.version) || !within(project.bpm, 40, 240) || !within(project.swing, 0, 70) ||
      !Array.isArray(project.tracks) || project.tracks.length !== 5) throw new Error('Invalid SLAP-5 project header.');
  if (project.metronome !== undefined && !within(project.metronome, 0, 2))
    throw new Error('Invalid metronome setting.');
  const legacy = project.version === 1;
  if (!legacy && (!within(project.drumFilters, 0, 0x3f3f) || (project.drumFilters & ~0x3f3f)))
    throw new Error('Invalid drum filters.');
  for (const [i, track] of project.tracks.entries()) {
    if (!track || !within(track.mix, 0, 0x1ffffff) || !within(track.mix & 255, 1, 64) ||
        !within(track.patch, 0, i === 4 ? 31 : 127) || !Array.isArray(track.steps) || track.steps.length !== 64) {
      throw new Error('Invalid track data.');
    }
    if (track.fx !== undefined && (!Array.isArray(track.fx) || track.fx.length !== fxPanels.length ||
        track.fx.some((word, page) => !within(word, 0, 0xffffffff) || (word & 0x80808080) ||
          fxPanels[page].some((_, knob) => (word >>> (8 * knob) & 255) > fxLimit(page, knob)))))
      throw new Error('Invalid track effects.');
    if (!legacy && (!Array.isArray(track.extra) || track.extra.length !== (i === 4 ? 6 : 1)))
      throw new Error('Invalid sequencer lane count.');
    for (const [lane, events] of [track.steps, ...(legacy ? [] : track.extra)].entries()) {
      if (!Array.isArray(events) || events.length !== 64) throw new Error('Invalid sequencer lane.');
      for (const word of events) {
        if (word === 0) continue;
        const note = decodeNote(word);
        if (!within(word, 0, 0xffffffff) || !(word & 128) ||
            (legacy && note.timing) || !note.velocity || !note.gate || note.probability > 100 ||
            (i === 4 && lane && note.pitch !== drumNotes[lane - 1]))
          throw new Error('Invalid note data.');
      }
    }
  }
  return { ...project, version: 2, drumFilters: legacy ? 0 : project.drumFilters,
    metronome: project.metronome ?? 0,
    tracks: project.tracks.map((track, i) => ({ ...track, fx: track.fx ?? [...defaultFx],
      extra: legacy ? Array.from({ length: i === 4 ? 6 : 1 }, () => Array(64).fill(0)) : track.extra })) };
}
