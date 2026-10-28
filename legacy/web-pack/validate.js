const P = require('./core.js');
const fs = require('fs');
const path = require('path');

const MINOR_SCALE = [0, 2, 3, 5, 7, 8, 10], MAJOR_SCALE = [0, 2, 4, 5, 7, 9, 11];
const QUAL = { // intervals for chord-name check (rootless voicings must be subsets)
  m9: [0, 3, 7, 10, 2], maj9: [0, 4, 7, 11, 2], m11: [0, 3, 7, 10, 2, 5], m7: [0, 3, 7, 10], maj7: [0, 4, 7, 11],
  '7sus4': [0, 5, 7, 10], '9sus4': [0, 5, 7, 10, 2], '7b9': [0, 4, 7, 10, 1], '': [0, 4, 7], m: [0, 3, 7], '7': [0, 4, 7, 10], '6': [0, 4, 7, 9], '6add11': [0, 4, 5, 7, 9], '69': [0, 4, 7, 9, 2]
};
const PCN = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
let problems = 0;
const warn = (...a) => { problems++; console.log('  !', ...a); };

for (const kit of P.KITS) {
  console.log(kit.id, kit.name, kit.bpm, P.keyInfo(kit.root, kit.mode).label, P.keyInfo(kit.root, kit.mode).camelot);
  if (kit.bpm < 128 || kit.bpm > 140) warn('bpm out of range');
  const scale = (kit.mode === 'major' ? MAJOR_SCALE : MINOR_SCALE).map(i => (i + kit.root) % 12);
  // chord names
  for (const [name, notes] of Object.entries(kit.chords)) {
    const m = /^([A-G])(#|b)?(.*)$/.exec(name);
    const root = (PCN[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0) + 12) % 12;
    const q = QUAL[m[3]];
    if (!q) { warn('unknown quality', name); continue; }
    const allowed = q.map(i => (i + root) % 12);
    notes.forEach(n => { const pc = P.noteNum(n) % 12; if (!allowed.includes(pc)) warn('chord', name, 'has', n); });
  }
  for (const part of ['bass', 'chords', 'lead']) {
    for (const v of ['A', 'B']) {
      const c = P.compilePart(kit, part, v, 0);
      if (c.bars !== 4) warn(part, v, 'bars', c.bars);
      const out = new Set();
      let lo = 999, hi = 0;
      c.events.forEach(e => {
        if (!scale.includes(e.n % 12)) out.add(P.noteLabel(e.n, true));
        lo = Math.min(lo, e.n); hi = Math.max(hi, e.n);
      });
      const range = P.noteLabel(lo, true) + '-' + P.noteLabel(hi, true);
      const ok = { bass: [26, 55], chords: [50, 80], lead: [55, 90] }[part];
      if (lo < ok[0] || hi > ok[1]) warn(part, v, 'range', range);
      // mono overlap check
      if (part !== 'chords') {
        const ev = c.events.slice().sort((a, b) => a.t - b.t);
        for (let i = 1; i < ev.length; i++) if (ev[i].t < ev[i - 1].t + ev[i - 1].d && !ev[i - 1].legato) warn(part, v, 'overlap at', ev[i].t);
      }
      console.log('   ', part.padEnd(6), v, String(c.events.length).padStart(3), 'notes', range.padEnd(9), out.size ? 'chromatic: ' + [...out].join(' ') : '');
    }
  }
  for (const v of ['main', 'var', 'fill']) {
    const d = P.compileDrums(kit, v);
    console.log('    drums ', v.padEnd(5), d.events.length, 'hits');
  }
}
console.log(problems ? problems + ' PROBLEMS' : 'all checks passed');

// write MIDI files
if (process.argv[2]) {
  const outRoot = process.argv[2];
  let count = 0;
  for (const kit of P.KITS) {
    const dir = path.join(outRoot, { ukg: 'UK Garage', mth: 'Minimal Tech House', sg: 'Speed Garage', ch: 'Classic House' }[kit.genre], kit.id + ' ' + kit.name + ' - ' + kit.bpm + ' BPM ' + P.keyInfo(kit.root, kit.mode).label + ' (' + P.keyInfo(kit.root, kit.mode).camelot + ')');
    fs.mkdirSync(dir, { recursive: true });
    for (const f of P.kitMidiFiles(kit, kit.root, kit.bpm)) { fs.writeFileSync(path.join(dir, f.name), f.data); count++; }
  }
  console.log('wrote', count, 'midi files');
}

// ---------- full songs ----------
console.log('\nFULL SONGS');
QUAL['6'] = [0, 4, 7, 9];
let songProblems = 0;
for (const kit of P.KITS) {
  const scale = (kit.mode === 'major' ? MAJOR_SCALE : MINOR_SCALE).map(i => (i + kit.root) % 12);
  for (const [name, voicing] of kit.song.bd) {
    const m = /^([A-G])(#|b)?(.*)$/.exec(name);
    const root = (PCN[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0) + 12) % 12;
    const q = QUAL[m[3]];
    if (!q) { songProblems++; console.log('  ! unknown quality', name); continue; }
    voicing.split(' ').forEach(n => { const pc = P.noteNum(n) % 12; if (!q.map(i => (i + root) % 12).includes(pc)) { songProblems++; console.log('  !', kit.id, name, 'has', n); } });
    voicing.split(' ').forEach(n => { if (!scale.includes(P.noteNum(n) % 12)) { songProblems++; console.log('  !', kit.id, 'breakdown chord', name, 'leaves the key with', n); } });
  }
  for (const pc of [kit.root, (kit.root + 5) % 12]) {
    const sg = P.buildSong(kit, pc);
    const L = sg.lengthTicks;
    let oob = 0, out = {};
    for (const [tk, ev] of Object.entries(sg.T)) {
      ev.forEach(e => { if (e.t < 0 || e.t + e.d > L || e.d <= 0 || e.v < 1 || e.v > 127) oob++; });
      if (tk !== 'drums') {
        const sc2 = scale.map(x => (x + P.transposeFor(kit, pc) + 12) % 12);
        out[tk] = ev.filter(e => !sc2.includes(e.n % 12)).length;
      }
      if (tk === 'bass' || tk === 'lead') for (let i = 1; i < ev.length; i++) if (ev[i].t < ev[i - 1].t + ev[i - 1].d - 1 && !ev[i - 1].legato) { songProblems++; console.log('  !', kit.id, tk, 'overlap at bar', (ev[i].t / 1920 + 1).toFixed(2)); break; }
    }
    if (oob) { songProblems++; console.log('  !', kit.id, 'out-of-bounds events', oob); }
    if (pc === kit.root) console.log(kit.id, kit.name.padEnd(18), sg.bars, 'bars', (sg.bars * 4 * 60 / kit.bpm / 60).toFixed(2), 'min |', sg.sections.map(s => s.name).join(' > '), '| out-of-key notes', JSON.stringify(out));
  }
}
console.log(songProblems ? songProblems + ' SONG PROBLEMS' : 'songs ok');
