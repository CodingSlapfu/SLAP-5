/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
export const within = (v, min, max) => Number.isInteger(v) && v >= min && v <= max;
export const maxSteps = 512;
export const legacyDrumKits = 56;
export const primaryDrumKits = 25;
export const factoryPatches = 136;
export const modernSynth = 1 << 27;
export const advancedSynth = 1 << 28;
export const patternLength = mix => (mix & 255) | (mix >>> 17 & 768);
export const withPatternLength = (mix, length) =>
  ((mix & ~0x060000ff) | (length & 255) | ((length & 768) << 17)) >>> 0;
export const drumNotes = [36, 38, 42, 37, 39, 49, 35, 40, 46, 44, 45, 47, 50, 51, 70, 81];
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
const defaultFx = [0, fxWord(0,0,0,22), fxWord(0,0,1,127), fxWord(0,48,68,52), 100,
  fxWord(0,48,56,65), fxWord(0,2,52,87), fxWord(0,60,52,94), fxWord(0,54,108,39)];

export function validateProject(project) {
  if (![1, 2, 3, 4, 5, 6].includes(project?.version) || !within(project.bpm, 40, 240) || !within(project.swing, 0, 70) ||
      !Array.isArray(project.tracks) || project.tracks.length !== 5) throw new Error('Invalid SLAP-5 project header.');
  if (project.metronome !== undefined && !within(project.metronome, 0, 2))
    throw new Error('Invalid metronome setting.');
  const legacy = project.version === 1;
  const extended = project.version >= 3;
  const polyphonic = project.version >= 4;
  const hasDurations = project.version >= 5;
  const expanded = project.version === 6;
  const bankSize = extended ? maxSteps : 64;
  if (!legacy && (!within(project.drumFilters, 0, expanded ? 0xffffffff : 0x3f3f) || (!expanded && (project.drumFilters & ~0x3f3f))))
    throw new Error('Invalid drum filters.');
  for (const [i, track] of project.tracks.entries()) {
    const mixMask = (extended ? 0x7ffffff : 0x1ffffff) | (i === 4 ? 0 : modernSynth | advancedSynth);
    if (track?.timbre !== undefined && (i === 4 || !within(track.timbre, 0, 0xffffffff)))
      throw new Error('Invalid instrument settings.');
    if (track?.fmEnvelopes !== undefined &&
        (i === 4 || !Array.isArray(track.fmEnvelopes) || track.fmEnvelopes.length !== 7 ||
          track.fmEnvelopes.some((value, op) => op === 6 ? !within(value, 0, 16) :
            value !== 0 && !within(value, 65536, 131071))))
      throw new Error('Invalid FM operator envelopes.');
    if (!track || !within(track.mix, 0, mixMask) || (track.mix & ~mixMask) || !within(patternLength(track.mix), 1, bankSize) ||
        !within(track.patch, 0, i === 4 ? legacyDrumKits + primaryDrumKits - 1 : factoryPatches - 1) || !Array.isArray(track.steps) || track.steps.length !== bankSize) {
      throw new Error('Invalid track data.');
    }
    if (track.fx !== undefined && (!Array.isArray(track.fx) || track.fx.length !== fxPanels.length ||
        track.fx.some((word, page) => !within(word, 0, 0xffffffff) || (word & 0x80808080) ||
          fxPanels[page].some((_, knob) => (word >>> (8 * knob) & 255) > fxLimit(page, knob)))))
      throw new Error('Invalid track effects.');
    if (!legacy && (!Array.isArray(track.extra) || track.extra.length !== (i === 4 ? (expanded ? 16 : 6) : polyphonic ? 3 : 1)))
      throw new Error('Invalid sequencer lane count.');
    if (extended && (!Array.isArray(track.offsets) || track.offsets.length !== track.extra.length + 1 ||
        track.offsets.some(offsets => !Array.isArray(offsets) || offsets.length !== bankSize ||
          offsets.some(offset => !within(offset, 0, 15))))) throw new Error('Invalid sixteenth-step offsets.');
    if (hasDurations && (!Array.isArray(track.durations) || track.durations.length !== track.extra.length + 1 ||
        track.durations.some(lane => !Array.isArray(lane) || lane.length !== bankSize ||
          lane.some(units => !within(units, 0, i === 4 ? 0 : 65535)))))
      throw new Error('Invalid note duration.');
    if (track.modes !== undefined &&
        (!expanded || !Array.isArray(track.modes) || track.modes.length !== track.extra.length + 1 ||
         track.modes.some(lane => !Array.isArray(lane) || lane.length !== bankSize ||
           lane.some(mode => !within(mode, 0, 2)))))
      throw new Error('Invalid step mode.');
    for (const [lane, events] of [track.steps, ...(legacy ? [] : track.extra)].entries()) {
      if (!Array.isArray(events) || events.length !== bankSize) throw new Error('Invalid sequencer lane.');
      for (const [index, word] of events.entries()) {
        if (word === 0) {
          if ((extended && track.offsets[lane][index]) || (hasDurations && track.durations[lane][index]) || track.modes?.[lane]?.[index])
            throw new Error('Timing or duration without a note.');
          continue;
        }
        const note = decodeNote(word);
        if (!within(word, 0, 0xffffffff) || !(word & 128) ||
            ((legacy || extended) && note.timing) || !note.velocity || !note.gate || note.probability > 100 ||
            (i === 4 && lane && note.pitch !== drumNotes[lane - 1]))
          throw new Error('Invalid note data.');
      }
    }
  }
  if (project.mixer !== undefined) {
    const { solo, duck, autoLevel, autoMix } = project.mixer ?? {};
    if (!within(solo, 0, 31) || !Array.isArray(duck) || duck.length !== 4 ||
        !duck.every(depth => within(depth, 0, 100)) ||
        typeof autoLevel !== 'boolean' || typeof autoMix !== 'boolean')
      throw new Error('Invalid mixer settings.');
  }
  return { ...project, version: 6, drumFilters: legacy ? 0 : expanded ? project.drumFilters : ((project.drumFilters & 63) | (project.drumFilters & 0x3f00) << 8),
    metronome: project.metronome ?? 0,
    tracks: project.tracks.map((track, i) => {
      const pad = lane => [...lane, ...Array(maxSteps - lane.length).fill(0)];
      const extra = legacy ? Array.from({ length: i === 4 ? 6 : 1 }, () => Array(64).fill(0)) : track.extra;
      const lanes = [track.steps, ...extra].map(pad);
      const offsets = extended ? track.offsets.map(lane => [...lane]) :
        lanes.map(lane => lane.map(word => decodeNote(word).timing * 2));
      if (i !== 4 && !polyphonic) {
        for (let lane = 0; lane < 2; lane++) {
          lanes.push(Array(maxSteps).fill(0));
          offsets.push(Array(maxSteps).fill(0));
        }
      }
      const durations = hasDurations ? track.durations.map(lane => [...lane]) : lanes.map(() => Array(maxSteps).fill(0));
      if (i === 4 && !expanded) {
        while (lanes.length < 17) {
          lanes.push(Array(maxSteps).fill(0));
          offsets.push(Array(maxSteps).fill(0));
          durations.push(Array(maxSteps).fill(0));
        }
      }
      return { ...track, fx: track.fx ?? [...defaultFx],
        steps: lanes[0].map(word => word & 0x7fff7fff),
        extra: lanes.slice(1).map(lane => lane.map(word => word & 0x7fff7fff)), offsets,
        durations, modes: track.modes?.map(lane => [...lane]) ?? lanes.map(() => Array(maxSteps).fill(0)) };
    }) };
}
