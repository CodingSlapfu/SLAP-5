/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
export const within = (v, min, max) => Number.isInteger(v) && v >= min && v <= max;
export function encodeNote(pitch, velocity, gate, probability) {
  return ((pitch | 128) | velocity << 8 | gate << 16 | probability << 24) >>> 0;
}
export function decodeNote(word) {
  return { pitch: word & 127, velocity: word >>> 8 & 127, gate: word >>> 16 & 255, probability: word >>> 24 };
}
export function validateProject(project) {
  if (project?.version !== 1 || !within(project.bpm, 40, 240) || !within(project.swing, 0, 70) ||
      !Array.isArray(project.tracks) || project.tracks.length !== 5) throw new Error('Invalid SLAP-5 project header.');
  for (const [i, track] of project.tracks.entries()) {
    if (!track || !within(track.mix, 0, 0x1ffffff) || !within(track.mix & 255, 1, 64) ||
        !within(track.patch, 0, i === 4 ? 3 : 127) || !Array.isArray(track.steps) || track.steps.length !== 64) {
      throw new Error('Invalid track data.');
    }
    for (const word of track.steps) {
      if (word === 0) continue;
      const note = decodeNote(word);
      if (!within(word, 0, 0xffffffff) || !(word & 128) || (word & 0x8000) ||
          !note.velocity || !note.gate || note.probability > 100) throw new Error('Invalid note data.');
    }
  }
  return project;
}
