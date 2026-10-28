// Checks the extended mixes: structure, bounds, ranges, monophony, consonance with what the harmony tracks are
// actually playing, and how much more varied each track is than in the full song.
const P = require('./core.js');
const fs = require('fs');
const path = require('path');
const BAR = P.BAR;
const SC = { minor: [0, 2, 3, 5, 7, 8, 10], major: [0, 2, 4, 5, 7, 9, 11] };
const RANGE = { bass: [24, 56], sub: [24, 45], chords: [45, 86], stabs: [50, 88], lead: [55, 98], counter: [48, 90], pad: [40, 88], arp: [48, 100], vox: [55, 92], riser: [48, 96], fx: [36, 44],
  breaks: [36, 51], acid: [43, 74], donk: [36, 64], piano: [45, 96], organ: [43, 88], strings: [48, 91], brass: [50, 86], orch: [36, 86], choir: [45, 84], talkbox: [55, 86], hoover: [40, 72], siren: [60, 96], voxguide: [36, 84], shouts: [48, 63], sidechain: [35, 35] };
const MONO = ['bass', 'sub', 'lead', 'counter', 'breaks', 'acid', 'donk', 'hoover', 'siren', 'talkbox', 'voxguide'];
const HARM = ['chords', 'stabs', 'pad'];
let problems = 0;
const bad = (...a) => { problems++; console.log('  !', ...a); };

function fingerprints(events) {   // distinct bar contents (position + pitch + velocity band)
  const bars = new Map();
  events.forEach(e => { const b = Math.floor(e.t / BAR); if (!bars.has(b)) bars.set(b, []); bars.get(b).push((e.t % BAR) + ':' + e.n + ':' + (e.v >> 4)); });
  return new Set([...bars.values()].map(a => a.sort().join(','))).size;
}
const rows = [];
for (const kit of P.KITS) {
  for (const pc of [kit.root, (kit.root + 5) % 12]) {
    const s = P.buildExtended(kit, pc);
    const tr = P.transposeFor(kit, pc);
    const L = s.lengthTicks;
    // structure
    let b = 0;
    s.sections.forEach(x => { if (x.bar !== b) bad(kit.id, 'section gap/overlap at', x.name); b = x.bar + x.bars; });
    if (b !== s.bars) bad(kit.id, 'sections end at', b, 'not', s.bars);
    for (const [tk, ev] of Object.entries(s.T)) {
      let oob = 0, out = 0;
      ev.forEach(e => {
        if (e.t < 0 || e.t + e.d > L || e.d <= 0 || e.v < 1 || e.v > 127 || e.n < 0 || e.n > 127 || !Number.isInteger(e.t) || !Number.isInteger(e.d)) oob++;
        const r = RANGE[tk]; if (r && (e.n < r[0] || e.n > r[1])) out++;
      });
      if (oob) bad(kit.id, pc, tk, oob, 'events out of bounds');
      if (out) bad(kit.id, pc, tk, out, 'notes out of range', RANGE[tk]);
      if (MONO.includes(tk)) for (let i = 1; i < ev.length; i++) if (ev[i].t < ev[i - 1].t + ev[i - 1].d - 1 && !ev[i - 1].legato) { bad(kit.id, pc, tk, 'overlap at bar', (ev[i].t / BAR + 1).toFixed(2)); break; }
      const last = {};
      for (const e of ev) { const p = last[e.n]; if (p && p.t + p.d > e.t) { bad(kit.id, pc, tk, 'same-pitch overlap at bar', (e.t / BAR + 1).toFixed(2), 'note', e.n); break; } last[e.n] = e; }
    }
    for (const [tk, cc] of Object.entries(s.CC)) cc.forEach(c => { if (c.t < 0 || c.t >= L || c.val < 0 || c.val > 127) bad(kit.id, tk, 'bad CC', JSON.stringify(c)); });
    if (pc !== kit.root) continue;
    // key: notes outside the key (the house key lift counts against its own key)
    const lift = s.keyChanges.length ? [s.keyChanges[0].t, s.keyChanges[1].t] : null;
    const home = ((kit.root + tr) % 12 + 12) % 12, liftBy = lift ? ((s.keyChanges[0].pc - home) % 12 + 12) % 12 : 0;
    const inKey = (n, t) => { const k = lift && t >= lift[0] && t < lift[1] ? home + liftBy : home; return SC[kit.mode].map(i => (k + i) % 12).includes(n % 12); };
    const outKey = {};
    ['bass', 'sub', 'chords', 'stabs', 'lead', 'counter', 'pad', 'arp', 'vox', 'acid', 'donk', 'piano', 'organ', 'strings', 'brass', 'orch', 'choir', 'talkbox', 'hoover', 'voxguide'].forEach(tk => { const ev = s.T[tk]; if (ev.length) outKey[tk] = Math.round(100 * ev.filter(e => !inKey(e.n, e.t)).length / ev.length); });
    // harmony: strong-beat notes against the chord the generator used at that 16th (full chord: root + extensions)
    const P16 = x => Math.round(P.warp(x, kit.swing) * 120);
    const stepOf = t => { const x = (t + 30) % BAR; let st = 0; for (let i = 1; i < 16; i++) if (P16(i) <= x) st = i; return Math.floor((t + 30) / BAR) * 16 + st; };
    const cons = {};
    ['lead', 'counter', 'vox', 'stabs', 'pad', 'arp', 'sub', 'acid', 'piano', 'organ', 'strings', 'brass', 'orch', 'choir', 'talkbox', 'hoover', 'voxguide'].forEach(tk => {
      let n = 0, off = 0;
      s.T[tk].forEach(e => {
        if (e.t % 480 > 40 && e.d < 240) return;
        const h = s.harmLine[stepOf(e.t)]; if (!h) return;
        n++; if (!h.includes(e.n % 12)) off++;
      });
      if (n) cons[tk] = +(100 * off / n).toFixed(1);
    });
    // variety vs the full song
    const full = P.buildSong(kit, pc);
    const drumsExt = ['kick', 'snare', 'hats', 'perc', 'fills'].flatMap(k => s.T[k]);
    const v = {
      drums: [fingerprints(full.T.drums), fingerprints(drumsExt)], bass: [fingerprints(full.T.bass), fingerprints(s.T.bass)],
      chords: [fingerprints(full.T.chords), fingerprints(s.T.chords.concat(s.T.stabs))], lead: [fingerprints(full.T.lead), fingerprints(s.T.lead.concat(s.T.counter))]
    };
    const tracks = P.EXT_TRACKS.filter(t => s.T[t.key].length).length;
    rows.push({ kit, s, tracks, outKey, cons, v });
    console.log(kit.id.padEnd(6), kit.name.padEnd(18), String(s.bars).padStart(3), 'bars', (s.bars * 4 * 60 / kit.bpm / 60).toFixed(2), 'min', tracks, 'tracks |',
      'unique bars full->ext: drums', v.drums.join('->'), 'bass', v.bass.join('->'), 'chords', v.chords.join('->'), 'melody', v.lead.join('->'),
      '| non-chord strong-beat notes %', JSON.stringify(cons), '| out-of-key %', JSON.stringify(outKey));
  }
}
console.log('\nFORMS');
['ukg', 'mth', 'sg', 'ch'].forEach(g => { const r = rows.find(x => x.kit.genre === g); console.log(g, r.s.bars, 'bars:', r.s.sections.map(x => x.name + ' ' + x.bars).join(' > ')); });
console.log(problems ? problems + ' PROBLEMS' : 'extended mixes: all checks passed');

console.log('\nEXT-SPEC KITS: bends, lyrics, channels');
for (const kit of P.KITS.filter(k => k.ext)) {
  const s = P.buildExtended(kit, kit.root), L = s.lengthTicks;
  // bends: values in range, every track ends at zero, no bend held away from zero for long (the build siren may hold 8 bars)
  const bendInfo = [];
  for (const [tk, b] of Object.entries(s.BEND)) {
    if (!b.length) continue;
    if (b.some(x => x.v < -8192 || x.v > 8191 || x.t < 0 || x.t >= L)) bad(kit.id, tk, 'bend out of range');
    if (b[b.length - 1].v !== 0) bad(kit.id, tk, 'bend not reset at the end');
    let since = null, longest = 0;
    b.forEach(x => { if (x.v !== 0 && since === null) since = x.t; if (x.v === 0 && since !== null) { longest = Math.max(longest, x.t - since); since = null; } });
    const lim = tk === 'siren' ? 9 * BAR : 2 * BAR;
    if (longest > lim) bad(kit.id, tk, 'bend held off zero for', (longest / BAR).toFixed(2), 'bars');
    bendInfo.push(tk + ':' + b.length + ' (longest ' + (longest / 480).toFixed(1) + ' beats)');
  }
  // lyrics: one per sung word, all inside the vocal guide
  const lyr = s.META.voxguide.filter(m => m.kind === 'lyric'), cues = s.META.voxguide.filter(m => m.kind === 'cue');
  const words = s.vocalCues.reduce((n, c) => n + P.VOCALS[c.id].mel.filter(m => (P.VOCALS[c.id].talk ? m[2] : m[3]) !== '~').length, 0);
  if (lyr.length !== words) bad(kit.id, 'lyrics', lyr.length, 'expected', words);
  if (cues.length !== s.vocalCues.length) bad(kit.id, 'cues', cues.length, 'expected', s.vocalCues.length);
  // vocal guide notes vs the harmony underneath (strong beats and long notes)
  const P16 = x => Math.round(P.warp(x, kit.swing) * 120);
  const stepOf = t => { const x = (t + 30) % BAR; let st = 0; for (let i = 1; i < 16; i++) if (P16(i) <= x) st = i; return Math.floor((t + 30) / BAR) * 16 + st; };
  let vn = 0, voff = 0; const offs = [];
  s.T.voxguide.forEach(e => { if (e.t % 480 > 40 && e.d < 240) return; const h = s.harmLine[stepOf(e.t)]; if (!h) return; vn++; if (!h.includes(e.n % 12)) { voff++; offs.push('bar ' + (e.t / BAR + 1).toFixed(2) + ' ' + P.noteLabel(e.n, true)); } });
  // lead must be silent under the vocal; chops must not start inside a sung note
  const blocks = s.vocalCues.map(c => [c.t, c.t + 4 * BAR - 130]);
  const leadUnder = s.T.lead.filter(e => blocks.some(([a, b]) => e.t >= a && e.t < b)).length;
  if (leadUnder) bad(kit.id, leadUnder, 'lead notes under the vocal');
  const choppedOver = s.T.vox.filter(e => s.T.voxguide.some(v => e.t >= v.t - 60 && e.t < v.t + v.d)).length;
  if (choppedOver) bad(kit.id, choppedOver, 'vocal chops on top of the sung vocal');
  // channels: layer tracks get free channels; no two pitched tracks with different programs share one
  const f = P.extendedMidi(kit, kit.root, kit.bpm);
  const tr = P.EXT_TRACKS.filter(t => s.T[t.key].length), byCh = {};
  const chans = (() => { const used = new Set(), ch = {}; tr.forEach(t => { if (t.ch != null) { ch[t.key] = t.ch; used.add(t.ch); } }); const free = [0, 13, 14, 15, 7, 5, 6, 4].filter(c => !used.has(c)); tr.forEach(t => { if (t.ch == null) ch[t.key] = free.shift(); }); return ch; })();
  tr.forEach(t => { const c = chans[t.key]; if (c === 9) return; (byCh[c] = byCh[c] || []).push(t); });
  Object.entries(byCh).forEach(([c, ts]) => { if (new Set(ts.map(t => t.program)).size > 1) bad(kit.id, 'channel', c, 'shared by', ts.map(t => t.key).join('+'), 'with different programs'); });
  const layers = tr.filter(t => t.layer).map(t => t.key + '@' + (chans[t.key] + 1));
  console.log(kit.id.padEnd(6), kit.name.padEnd(18), f.data.length, 'bytes |', tr.length, 'tracks | layers', layers.join(' '), '| bends', bendInfo.join(', ') || '-', '| lyrics', lyr.length, '| vocal blocks', s.vocalCues.length,
    '| vocal guide off-chord', vn ? (100 * voff / vn).toFixed(1) + '% of ' + vn : '-', offs.length ? '(' + offs.slice(0, 6).join(', ') + ')' : '');
}
console.log(problems ? problems + ' PROBLEMS' : 'ext-spec checks passed');

if (process.argv[2]) {
  const outRoot = process.argv[2];
  let count = 0;
  for (const kit of P.KITS) {
    const key = P.keyInfo(kit.root, kit.mode);
    const dir = path.join(outRoot, { ukg: 'UK Garage', mth: 'Minimal Tech House', sg: 'Speed Garage', ch: 'Classic House' }[kit.genre], kit.id + ' ' + kit.name + ' - ' + kit.bpm + ' BPM ' + key.label + ' (' + key.camelot + ')');
    fs.mkdirSync(dir, { recursive: true });
    const f = P.extendedMidi(kit, kit.root, kit.bpm);
    fs.writeFileSync(path.join(dir, f.name), f.data); count++;
  }
  console.log('wrote', count, 'extended mixes');
}
