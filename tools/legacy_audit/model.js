/* From SMF bytes to the audit's model: sections, lanes and drum groups (README.md, "The model").
   Integer arithmetic only. */
'use strict';

const smf = require('./smf.js');
const LEGACY_SECTION_KINDS = require('./legacy_sections.json').kinds;

const PPQ = 480, STEP = 120, BEAT = 480, BAR = 1920;

// floor(a / b) for b > 0, also for negative a.
const floorDiv = (a, b) => Math.floor(a / b);
const mod = (a, b) => ((a % b) + b) % b;

// The 16th step an onset belongs to: the nearest one, a tie (60 ticks) going to the later step.
const step16 = t => floorDiv(t + 60, STEP);

/* ---------- section kinds ---------- */
// Generator v2 markers start with one of these lowercase words (README, "Sections"). Legacy markers are
// capitalized names, looked up in legacy_sections.json (taken from core.js's own section kinds).
const NEW_KINDS = ['intro', 'groove', 'build', 'drop', 'drop2', 'break', 'outro'];
function sectionKind(text) {
  const first = text.split(' ')[0];
  if (NEW_KINDS.includes(first)) return first;
  if (Object.prototype.hasOwnProperty.call(LEGACY_SECTION_KINDS, text)) return LEGACY_SECTION_KINDS[text];
  return 'unknown';
}

/* ---------- lane roles ---------- */
// Channel 10 (index 9) tracks are drums. Every other track's role comes from the first word of its
// name, lowercased (letters only). The legacy part files (one track) take their role from the slot.
const ROLE_WORDS = {
  bass: 'bass', sub: 'bass', reese: 'bass', acid: 'bass', donk: 'bass',
  chords: 'chords', stabs: 'chords', stab: 'chords', pad: 'chords', organ: 'chords', strings: 'chords', piano: 'chords',
  brass: 'chords', orchestra: 'chords', choir: 'chords',
  melody: 'lead', lead: 'lead', pluck: 'lead', arp: 'lead', counter: 'lead', talkbox: 'lead', hoover: 'lead',
  vocal: 'vox', vox: 'vox', chops: 'vox', shouts: 'vox',
  riser: 'fx', fx: 'fx', impact: 'fx', impacts: 'fx', dub: 'fx'
};
const SLOT_ROLES = { Drums: 'drums', Bass: 'bass', Chords: 'chords', Melody: 'lead' };
function firstWord(name) {
  const m = /^[A-Za-z]+/.exec(name);
  return m ? m[0].toLowerCase() : '';
}
function roleOf(name, channel9, slot) {
  if (firstWord(name) === 'sidechain') return 'ignore';
  if (channel9) return 'drums';
  if (slot && SLOT_ROLES[slot.split('-')[0]] && !/^(FullArrangement|FullSong|ExtendedMix)$/.test(slot)) return SLOT_ROLES[slot.split('-')[0]];
  return ROLE_WORDS[firstWord(name)] || 'other';
}

/* ---------- drum groups (by GM key, channel 10 only) ---------- */
const GROUP_OF_KEY = {};
const setGroup = (g, keys) => keys.forEach(k => { GROUP_OF_KEY[k] = g; });
setGroup('kick', [35, 36]);
setGroup('snare', [38, 40]);
setGroup('clap', [39]);
setGroup('rim', [37]);
setGroup('hats', [42, 44, 46, 51, 53, 59, 54, 69, 70, 82]);
setGroup('cymbal', [49, 52, 55, 57]);
setGroup('tom', [41, 43, 45, 47, 48, 50]);
const drumGroup = key => GROUP_OF_KEY[key] || 'perc';

/* ---------- a whole file ---------- */
// relPath: "<KIT>/<KIT>_<slot>.mid" for the legacy baseline; any name for other files.
function loadSong(bytes, relPath) {
  const file = smf.readSmf(bytes);
  if (file.ppq !== PPQ) throw new Error(relPath + ': ' + file.ppq + ' ticks per quarter note; the audit reads 480 only');
  const m = /^([A-Z]+[0-9]+)\/\1_(.+)\.mid$/.exec(relPath);
  const kit = m ? m[1] : '', slot = m ? m[2] : '';
  const endTick = Math.max(0, ...file.tracks.map(t => t.endTick));
  const bars = floorDiv(endTick + BAR - 1, BAR);

  const keySigs = smf.keySignaturesOf(file);
  const markers = smf.markersOf(file);
  const sections = markers.map((mk, i) => {
    const end = i + 1 < markers.length ? markers[i + 1].tick : endTick;
    return { tick: mk.tick, end, text: mk.text, kind: sectionKind(mk.text), onBarLine: mk.tick % BAR === 0 };
  }).filter(s => s.end > s.tick);
  sections.forEach(s => { s.bar = floorDiv(s.tick, BAR); s.bars = floorDiv(s.end - s.tick + BAR - 1, BAR); });

  const tracks = [];
  file.tracks.forEach((t, index) => {
    const notes = smf.notesOf(t);
    if (!notes.length) return;
    const name = smf.trackName(t);
    const channel9 = notes.every(n => n.channel === 9);
    const role = roleOf(name, channel9, slot);
    tracks.push({ index, name, role, notes });
  });
  const drumNotes = [];
  tracks.forEach(tr => { if (tr.role === 'drums') tr.notes.forEach(n => drumNotes.push(Object.assign({ track: tr.index, group: drumGroup(n.key) }, n))); });
  drumNotes.sort((a, b) => a.tick - b.tick || a.track - b.track || a.key - b.key);
  return { path: relPath, kit, slot, format: file.format, endTick, bars, keySigs, sections, tracks, drumNotes };
}

module.exports = { PPQ, STEP, BEAT, BAR, floorDiv, mod, step16, sectionKind, roleOf, drumGroup, GROUP_OF_KEY, NEW_KINDS, loadSong };
