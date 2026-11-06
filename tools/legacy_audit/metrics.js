/* The metric suite of docs/plans/S1-futuristic-slice.md section 5.1, on files loaded by model.js.
   README.md defines every metric; the comments here only point at the definition. Every decision is
   integer arithmetic. Doubles appear only in reported statistics (DFA alpha), with the rounding stated
   in the README. Each metric takes the list of "seed" files it is measured on and returns plain objects
   of integers and strings. */
'use strict';

const M = require('./model.js');
const { BAR, STEP, floorDiv, mod, step16 } = M;

/* ---------- small integer helpers ---------- */
const popcount = m => { let c = 0; for (let i = 0; i < 12; i++) if (m & (1 << i)) c++; return c; };
const pcMask = notes => notes.reduce((m, n) => m | (1 << (n.key % 12)), 0);
const rotate = (m, r) => { let o = 0; for (let i = 0; i < 12; i++) if (m & (1 << i)) o |= 1 << mod(i - r, 12); return o; };
const maskHex = m => m.toString(16).padStart(3, '0');
const byNum = (a, b) => a - b;
const byStr = (a, b) => (a < b ? -1 : a > b ? 1 : 0);

// The lower median of a list of integers: the element at index floor((n - 1) / 2) once sorted.
function lowerMedian(values) {
  if (!values.length) return null;
  const s = values.slice().sort(byNum);
  return s[floorDiv(s.length - 1, 2)];
}
// a / b in permille, rounded half up, for a >= 0 and b > 0. null when b is 0.
function permille(a, b) {
  if (!b) return null;
  return floorDiv(2000 * a + b, 2 * b);
}
// BigInt version for signed numerators: round half up (toward +infinity) of 1000 * num / den, den > 0.
function permilleBig(num, den) {
  const n = 2000n * num + den, d = 2n * den;
  const q = n / d;
  return Number(n % d !== 0n && (n < 0n) !== (d < 0n) ? q - 1n : q);
}
// Population standard deviation of integers, in thousandths, rounded half up:
// floor(1000 * sqrt(n * S2 - S1^2) / n + 0.5), computed in IEEE doubles in exactly this order.
function sdMilli(values) {
  const n = values.length;
  if (!n) return null;
  let s1 = 0, s2 = 0;
  for (const v of values) { s1 += v; s2 += v * v; }
  const v = n * s2 - s1 * s1; // exact: every term is an integer below 2^53
  return Math.floor((1000 * Math.sqrt(v)) / n + 0.5);
}
const countDistinct = values => new Set(values).size;
function histogram(values) {
  const h = new Map();
  for (const v of values) h.set(v, (h.get(v) || 0) + 1);
  return h;
}
// The most common value: the highest count, ties to the smallest key in byte order of its string form.
function topOf(hist) {
  let best = null;
  for (const [k, c] of hist) {
    if (!best || c > best[1] || (c === best[1] && byStr(String(k), String(best[0])) < 0)) best = [k, c];
  }
  return best;
}
const sortedObject = map => Object.fromEntries([...map.entries()].map(([k, v]) => [String(k), v]).sort((a, b) => byStr(a[0], b[0])));
// Histograms as lists, so JSON key order never depends on JavaScript's integer-key rule: numeric keys
// sorted by value, string keys in byte order.
const numHist = map => [...map.entries()].sort((a, b) => a[0] - b[0]).map(([value, count]) => ({ value, count }));
const strHist = (map, name) => [...map.entries()].sort((a, b) => byStr(a[0], b[0])).map(([k, count]) => ({ [name]: k, count }));

/* ---------- song structure ---------- */
// A file's sections; a file without markers is one section of kind "none" over the whole file.
function sectionsOf(song) {
  if (song.sections.length) return song.sections;
  return [{ tick: 0, end: song.bars * BAR, text: '', kind: 'none', bar: 0, bars: song.bars, onBarLine: true }];
}
const tracksWithRole = (song, role) => song.tracks.filter(t => t.role === role);
const notesWithRole = (song, role) => {
  const out = [];
  tracksWithRole(song, role).forEach(t => t.notes.forEach(n => out.push(Object.assign({ track: t.index }, n))));
  out.sort((a, b) => a.tick - b.tick || a.track - b.track || a.key - b.key);
  return out;
};
const GROUPS = {
  hats: ['hats'],
  perc: ['perc', 'tom', 'rim'],
  snareFamily: ['snare', 'clap']
};
const drumNotesIn = (song, groups) => song.drumNotes.filter(n => groups.includes(n.group));

// Notes of a list (sorted by tick) whose onset is in [t0, t1).
function between(notes, t0, t1) {
  let lo = 0, hi = notes.length;
  while (lo < hi) { const mid = (lo + hi) >> 1; if (notes[mid].tick < t0) lo = mid + 1; else hi = mid; }
  const out = [];
  for (let i = lo; i < notes.length && notes[i].tick < t1; i++) out.push(notes[i]);
  return out;
}

/* ---------- stacks and harmony slots (README: "Stacks", "Harmony slots") ---------- */
// Notes sorted by tick; a stack is a run of notes each within 10 ticks of the run's first onset.
function stacksOf(notes) {
  const out = [];
  for (const n of notes) {
    const cur = out[out.length - 1];
    if (cur && n.tick - cur.tick <= 10) cur.notes.push(n);
    else out.push({ tick: n.tick, notes: [n] });
  }
  return out;
}
// The harmony stacks of a song: stacks of all chords-role notes merged, with 2 or more pitch classes.
function harmonyStacks(song) {
  return stacksOf(notesWithRole(song, 'chords'))
    .map(s => ({ tick: s.tick, mask: pcMask(s.notes), low: Math.min(...s.notes.map(n => n.key)), keys: s.notes.map(n => n.key).sort(byNum) }))
    .filter(s => popcount(s.mask) >= 2);
}
// The harmony of nSlots half-bar slots from tick w0: per slot the stack nearest the slot's start among
// those with onset in [start - 250, start + 710), ties to the later; a slot with none carries the previous
// slot's; leading empty slots take the first filled slot's. null if no slot has a stack.
function harmonySlots(stacks, w0, nSlots) {
  const out = [];
  let k = 0;
  for (let j = 0; j < nSlots; j++) {
    const start = w0 + 960 * j, lo = start - 250, hi = start + 710;
    while (k < stacks.length && stacks[k].tick < lo) k++;
    let best = null;
    for (let q = k; q < stacks.length && stacks[q].tick < hi; q++) {
      if (!best || Math.abs(stacks[q].tick - start) <= Math.abs(best.tick - start)) best = stacks[q];
    }
    out.push(best);
  }
  const first = out.findIndex(x => x);
  if (first < 0) return null;
  for (let j = 0; j < nSlots; j++) if (!out[j]) out[j] = j < first ? out[first] : out[j - 1];
  return out;
}
const firstSectionOf = (song, kind) => song.sections.find(s => s.kind === kind) || null;

/* ---------- G1 progression-shape diversity ---------- */
function progressionShape(song, kind) {
  const sec = firstSectionOf(song, kind);
  if (!sec) return null;
  const slots = harmonySlots(harmonyStacks(song), sec.tick, 16);
  if (!slots) return null;
  const r = slots[0].low % 12;
  return slots.map(s => maskHex(rotate(s.mask, r))).join('.');
}
function diversity(values, n) {
  const measured = values.filter(v => v !== null);
  const hist = histogram(measured);
  const top = topOf(hist);
  return {
    seeds: n, measured: measured.length, distinct: hist.size, top: top ? top[1] : 0,
    distinct_pm: permille(hist.size, measured.length), top_pm: permille(top ? top[1] : 0, measured.length),
    top_shape: top ? String(top[0]) : ''
  };
}
function g1(songs) {
  const perFile = {}, res = {};
  for (const kind of ['break', 'drop']) {
    const shapes = songs.map(s => progressionShape(s, kind));
    songs.forEach((s, i) => { (perFile[s.path] = perFile[s.path] || {})[kind] = shapes[i] === null ? '' : shapes[i]; });
    const d = diversity(shapes, songs.length);
    d.pass = d.measured === d.seeds && 2 * d.distinct >= d.measured && 10 * d.top <= d.measured ? 1 : 0;
    res[kind] = d;
  }
  res.pass = res.break.pass && res.drop.pass ? 1 : 0;
  res.per_file = perFile;
  return res;
}

/* ---------- G2 harmonic rhythm ---------- */
function harmonicRhythm(song) {
  const sec = firstSectionOf(song, 'drop') || (song.sections.length ? null : sectionsOf(song)[0]);
  if (!sec) return null;
  const slots = harmonySlots(harmonyStacks(song), sec.tick, 8);
  if (!slots) return null;
  const runs = [];
  let run = 1;
  for (let j = 1; j < 8; j++) {
    if (slots[j].mask === slots[j - 1].mask) run++;
    else { runs.push(run); run = 1; }
  }
  runs.push(run);
  return runs.join(',');
}
function g2(songs) {
  const patterns = songs.map(harmonicRhythm);
  const measured = patterns.filter(p => p !== null);
  const hist = histogram(measured);
  const fourByOne = hist.get('2,2,2,2') || 0;
  const wholeBars = measured.filter(p => p.split(',').every(x => +x % 2 === 0)).length;
  return {
    seeds: songs.length, measured: measured.length, four_by_one_bar: fourByOne, four_by_one_bar_pm: permille(fourByOne, measured.length),
    patterns: hist.size, changes_on_bar_lines_only: wholeBars,
    pass: measured.length === songs.length && 10 * fourByOne <= 3 * measured.length && hist.size >= 4 ? 1 : 0,
    histogram: strHist(hist, 'pattern'),
    per_file: Object.fromEntries(songs.map((s, i) => [s.path, patterns[i] === null ? '' : patterns[i]]))
  };
}

/* ---------- G3 one-bar repetition in 4-bar windows ---------- */
// Exact positions (quantized files): a bar's content is the notes with floor(tick / 1920) == bar.
function barsOf(notes, bar0, nBars) {
  const bars = Array.from({ length: nBars }, () => []);
  for (const n of between(notes, bar0 * BAR, (bar0 + nBars) * BAR)) bars[floorDiv(n.tick, BAR) - bar0].push(n);
  return bars;
}
/* ---------- the gating bands (plan §5.1) as pure functions of each gate's aggregate ---------- */
// G3, G5, G6 and G7 decide pass with these. fixtures/expected.json "bands" pins every bound at its edge,
// for this file and for core/metrics/gating.cpp's copies (core::metrics::bands), so neither drifts.
const bands = {
  // G3: drum lanes repeat in at most 40% of their 4-bar windows, the bass in at most 30%
  g3: (drumWindows, drumRepeated, bassWindows, bassRepeated) =>
    drumWindows > 0 && bassWindows > 0 && 10 * drumRepeated <= 4 * drumWindows && 10 * bassRepeated <= 3 * bassWindows,
  // G5: a drum group's median 8-bar window has 10 or more velocities and an SD of 6 to 22; a melodic one 6 or more and 8 or more
  g5Drums: (windows, medianDistinct, medianSdMilli) => windows > 0 && medianDistinct >= 10 && medianSdMilli >= 6000 && medianSdMilli <= 22000,
  g5Melodic: (windows, medianDistinct, medianSdMilli) => windows > 0 && medianDistinct >= 6 && medianSdMilli >= 8000,
  // G6: 70% or more of the chord stacks have two or more velocities, and none spreads over more than 4 ticks
  g6: (stacks, twoVelocities, maxSpreadTicks) => stacks > 0 && 10 * twoVelocities >= 7 * stacks && maxSpreadTicks <= 4,
  // G7, per feel lane: the median file's residual SD 1.5 to 6 ticks, lag-1 0.2 or more, DFA alpha 0.65 to 1.1
  g7: (files, medianSdMilli, medianLag1Pm, medianAlphaMilli) => files > 0 && medianSdMilli >= 1500 && medianSdMilli <= 6000 && medianLag1Pm >= 200 &&
    medianAlphaMilli >= 650 && medianAlphaMilli <= 1100
};

const fourOnTheFloor = bar => [0, 480, 960, 1440].every(beat => bar.some(n => Math.abs(mod(n.tick, BAR) - beat) <= 2));
function g3Windows(song) {
  const out = { drums: [0, 0, 0], bass: [0, 0] }; // drums: windows, repeated, exempt; bass: windows, repeated
  const lanes = [];
  song.tracks.filter(t => t.role === 'drums').forEach(t => {
    const byKey = new Map();
    t.notes.forEach(n => { if (!byKey.has(n.key)) byKey.set(n.key, []); byKey.get(n.key).push(n); });
    [...byKey.keys()].sort(byNum).forEach(k => lanes.push({ kind: 'drums', key: k, notes: byKey.get(k) }));
  });
  tracksWithRole(song, 'bass').forEach(t => lanes.push({ kind: 'bass', notes: t.notes }));
  for (const sec of sectionsOf(song)) {
    for (let w = 0; w + 4 <= sec.bars; w += 4) {
      const b0 = sec.bar + w;
      if ((b0 + 4) * BAR > song.endTick) continue;
      for (const lane of lanes) {
        const bars = barsOf(lane.notes, b0, 4);
        if (bars.every(b => !b.length)) continue;
        const fp = bars.map(b => lane.kind === 'drums'
          ? b.map(n => mod(n.tick, BAR) + ':' + n.velocity).sort().join(',')
          : [...new Set(b.map(n => mod(n.tick, BAR)))].sort(byNum).join(','));
        const repeated = fp[0] !== '' && fp.every(f => f === fp[0]) ? 1 : 0;
        if (lane.kind === 'drums') {
          if (M.drumGroup(lane.key) === 'kick' && bars.every(fourOnTheFloor)) { out.drums[2]++; continue; }
          out.drums[0]++; out.drums[1] += repeated;
        } else { out.bass[0]++; out.bass[1] += repeated; }
      }
    }
  }
  return out;
}
function g3(songs) {
  const perFile = {};
  let dw = 0, dr = 0, de = 0, bw = 0, br = 0;
  for (const s of songs) {
    const w = g3Windows(s);
    perFile[s.path] = { drum_windows: w.drums[0], drum_repeated: w.drums[1], kick_4x4_exempt: w.drums[2], bass_windows: w.bass[0], bass_repeated: w.bass[1] };
    dw += w.drums[0]; dr += w.drums[1]; de += w.drums[2]; bw += w.bass[0]; br += w.bass[1];
  }
  return {
    drum_windows: dw, drum_repeated: dr, drum_repeated_pm: permille(dr, dw), kick_4x4_exempt: de,
    bass_windows: bw, bass_repeated: br, bass_repeated_pm: permille(br, bw),
    pass: bands.g3(dw, dr, bw, br) ? 1 : 0,
    per_file: perFile
  };
}

/* ---------- G4 macro changes on phrase-start or phrase-end bars ---------- */
// Bar fingerprint (step attribution, no velocity): sorted distinct "step:key" of the bar's notes.
function stepBars(notes, bar0, nBars) {
  const bars = Array.from({ length: nBars }, () => new Set());
  for (const n of notes) {
    const s = step16(n.tick), b = floorDiv(s, 16) - bar0;
    if (b >= 0 && b < nBars) bars[b].add(mod(s, 16) * 128 + n.key);
  }
  return bars.map(set => [...set].sort(byNum).join(','));
}
// Macro changes of one lane in one section: section-relative bar indices i >= 8 where the bar differs
// from bar i - 8 and bar i - 1 did not (bar 7 counts as not differing).
function macroChanges(fps) {
  const out = [];
  let prev = false;
  for (let i = 8; i < fps.length; i++) {
    const dev = fps[i] !== fps[i - 8];
    if (dev && !prev) out.push(i);
    prev = dev;
  }
  return out;
}
const G4_ROLES = ['bass', 'chords', 'lead'];
function macroChangeBars(song) {
  const res = { bass: [], chords: [], lead: [] }; // absolute bars, with the section-relative index
  for (const role of G4_ROLES) {
    for (const t of tracksWithRole(song, role)) {
      for (const sec of sectionsOf(song)) {
        if (sec.bars < 9) continue;
        const fps = stepBars(t.notes, sec.bar, sec.bars);
        for (const i of macroChanges(fps)) res[role].push({ bar: sec.bar + i, pos: i % 8 });
      }
    }
  }
  return res;
}
function g4(songs) {
  const tot = { bass: [0, 0], chords: [0, 0], lead: [0, 0] };
  const perFile = {};
  for (const s of songs) {
    const ch = macroChangeBars(s), pf = {};
    for (const role of G4_ROLES) {
      const all = ch[role].length, phrase = ch[role].filter(c => c.pos === 0 || c.pos === 7).length;
      tot[role][0] += all; tot[role][1] += phrase;
      pf[role + '_changes'] = all; pf[role + '_on_phrase'] = phrase;
    }
    perFile[s.path] = pf;
  }
  const res = {};
  for (const role of G4_ROLES) res[role] = { changes: tot[role][0], on_phrase: tot[role][1], on_phrase_pm: permille(tot[role][1], tot[role][0]) };
  res.pass = G4_ROLES.every(r => tot[r][0] > 0 && 2 * tot[r][1] >= tot[r][0]) ? 1 : 0;
  res.per_file = perFile;
  return res;
}

/* ---------- G5 velocity resolution per 8 bars ---------- */
const isBackbeat = t => { const p = mod(t, BAR); return (p >= 450 && p <= 510) || (p >= 1410 && p <= 1470); };
const G5_GROUPS = {
  hats: { min: 16, notes: s => drumNotesIn(s, GROUPS.hats) },
  perc: { min: 16, notes: s => drumNotesIn(s, GROUPS.perc) },
  ghosts: { min: 10, notes: s => drumNotesIn(s, GROUPS.snareFamily).filter(n => !isBackbeat(n.tick)) },
  bass: { min: 8, notes: s => notesWithRole(s, 'bass') },
  lead: { min: 8, notes: s => notesWithRole(s, 'lead') }
};
function g5Windows(song) {
  const out = {};
  for (const [g, def] of Object.entries(G5_GROUPS)) {
    const notes = def.notes(song).slice().sort((a, b) => a.tick - b.tick);
    const wins = [];
    for (const sec of sectionsOf(song)) {
      if (sec.kind === 'build') continue;
      for (let w = 0; w + 8 <= sec.bars; w += 8) {
        const t0 = (sec.bar + w) * BAR;
        const v = between(notes, t0, t0 + 8 * BAR).map(n => n.velocity);
        if (v.length >= def.min) wins.push({ n: v.length, distinct: countDistinct(v), sd_milli: sdMilli(v) });
      }
    }
    out[g] = wins;
  }
  return out;
}
function g5(songs) {
  const all = {}; Object.keys(G5_GROUPS).forEach(g => { all[g] = []; });
  const perFile = {};
  for (const s of songs) {
    const w = g5Windows(s), pf = {};
    for (const g of Object.keys(G5_GROUPS)) {
      all[g].push(...w[g]);
      pf[g] = { windows: w[g].length, median_distinct: lowerMedian(w[g].map(x => x.distinct)) ?? -1, median_sd_milli: lowerMedian(w[g].map(x => x.sd_milli)) ?? -1 };
    }
    perFile[s.path] = pf;
  }
  const res = {};
  for (const g of Object.keys(G5_GROUPS)) {
    res[g] = { windows: all[g].length, median_distinct: lowerMedian(all[g].map(x => x.distinct)) ?? -1, median_sd_milli: lowerMedian(all[g].map(x => x.sd_milli)) ?? -1 };
  }
  const drumOk = g => bands.g5Drums(res[g].windows, res[g].median_distinct, res[g].median_sd_milli);
  const melOk = g => bands.g5Melodic(res[g].windows, res[g].median_distinct, res[g].median_sd_milli);
  res.pass = drumOk('hats') && drumOk('perc') && drumOk('ghosts') && melOk('bass') && melOk('lead') ? 1 : 0;
  res.per_file = perFile;
  return res;
}
// The plan's legacy figure: velocity values a lane uses across every kit's 4-bar parts.
function partLevels(songs) {
  const groups = { hats: new Set(), perc: new Set(), snareFamily: new Set(), kick: new Set(), bass: new Set(), chords: new Set(), lead: new Set() };
  let maxDrumLevels = 0;
  for (const s of songs) {
    const kitDrum = new Set();
    s.drumNotes.forEach(n => {
      kitDrum.add(n.velocity);
      if (GROUPS.hats.includes(n.group)) groups.hats.add(n.velocity);
      else if (GROUPS.perc.includes(n.group)) groups.perc.add(n.velocity);
      else if (GROUPS.snareFamily.includes(n.group)) groups.snareFamily.add(n.velocity);
      else if (n.group === 'kick') groups.kick.add(n.velocity);
    });
    maxDrumLevels = Math.max(maxDrumLevels, kitDrum.size);
    ['bass', 'chords', 'lead'].forEach(r => notesWithRole(s, r).forEach(n => groups[r].add(n.velocity)));
  }
  return Object.assign({ most_levels_in_one_drum_file: maxDrumLevels },
    Object.fromEntries(Object.entries(groups).map(([g, set]) => [g, [...set].sort(byNum).join(',')])));
}

/* ---------- G6 chord stacks with 2 or more velocities ---------- */
function g6(songs) {
  let stacks = 0, twoVel = 0, maxSpread = 0;
  const spreads = new Map(), perFile = {};
  for (const s of songs) {
    let fs = 0, ft = 0;
    for (const t of tracksWithRole(s, 'chords')) {
      for (const st of stacksOf(t.notes)) {
        if (st.notes.length < 2) continue;
        const spread = st.notes[st.notes.length - 1].tick - st.tick;
        fs++; if (countDistinct(st.notes.map(n => n.velocity)) >= 2) ft++;
        maxSpread = Math.max(maxSpread, spread);
        spreads.set(spread, (spreads.get(spread) || 0) + 1);
      }
    }
    stacks += fs; twoVel += ft;
    perFile[s.path] = { stacks: fs, two_velocities: ft };
  }
  return {
    stacks, two_velocities: twoVel, two_velocities_pm: permille(twoVel, stacks), max_spread_ticks: maxSpread,
    spread_histogram: numHist(spreads),
    pass: bands.g6(stacks, twoVel, maxSpread) ? 1 : 0,
    per_file: perFile
  };
}

/* ---------- swing estimate, residuals (G7, G8) ---------- */
// Odd-16th candidates: onsets t with t mod 240 in [60, 180); offset (t mod 240) - 120.
const oddOffsets = notes => notes.map(n => mod(n.tick, 240)).filter(r => r >= 60 && r < 180).map(r => r - 120);
// The lane's swing delay in ticks: the lower median of its odd-16th offsets (null if fewer than min).
function swingDelay(notes, min) {
  const o = oddOffsets(notes);
  return o.length >= min ? lowerMedian(o) : null;
}
function residuals(song, notes) {
  const out = [];
  for (const sec of sectionsOf(song)) {
    const inSec = between(notes, sec.tick, sec.end);
    const d = swingDelay(inSec, 1) ?? 0;
    for (const n of inSec) {
      const g1 = 240 * floorDiv(n.tick, 240);
      let best = n.tick - g1;
      for (const g of [g1 + 120 + d, g1 + 240]) { const x = n.tick - g; if (Math.abs(x) < Math.abs(best)) best = x; }
      if (Math.abs(best) <= 30) out.push(best);
    }
  }
  return out;
}
// Lag-1 autocorrelation in permille, exact: [n^2 P - n S1 (A + B) + (n - 1) S1^2] / [n (n S2 - S1^2)].
function lag1Permille(x) {
  const n = BigInt(x.length);
  if (x.length < 2) return null;
  let s1 = 0n, s2 = 0n, p = 0n, a = 0n, b = 0n;
  x.forEach((v, i) => {
    const V = BigInt(v);
    s1 += V; s2 += V * V;
    if (i >= 1) { p += V * BigInt(x[i - 1]); a += V; }
    if (i <= x.length - 2) b += V;
  });
  const den = n * (n * s2 - s1 * s1);
  if (den === 0n) return null;
  const num = n * n * p - n * s1 * (a + b) + (n - 1n) * s1 * s1;
  return permilleBig(num, den);
}
// DFA-1 alpha in thousandths (README, "G7"): double arithmetic in this order; null if undefined.
function dfaAlphaMilli(x) {
  const n = x.length;
  if (n < 64) return null;
  let mean = 0; for (const v of x) mean += v; mean /= n;
  const y = new Array(n); let acc = 0;
  for (let i = 0; i < n; i++) { acc += x[i] - mean; y[i] = acc; }
  const ls = [], lf = [];
  for (let s = 4; s <= Math.floor(n / 4); s *= 2) {
    const nb = Math.floor(n / s);
    const kbar = (s - 1) / 2;
    let skk = 0; for (let k = 0; k < s; k++) skk += (k - kbar) * (k - kbar);
    let total = 0;
    for (let j = 0; j < nb; j++) {
      let vbar = 0; for (let k = 0; k < s; k++) vbar += y[j * s + k]; vbar /= s;
      let skv = 0; for (let k = 0; k < s; k++) skv += (k - kbar) * (y[j * s + k] - vbar);
      const slope = skv / skk;
      let rss = 0; for (let k = 0; k < s; k++) { const r = y[j * s + k] - vbar - slope * (k - kbar); rss += r * r; }
      total += rss / s;
    }
    const f2 = total / nb;
    if (!(f2 > 0)) return null;
    ls.push(Math.log(s)); lf.push(0.5 * Math.log(f2));
  }
  if (ls.length < 3) return null;
  const m = ls.length;
  let mx = 0, my = 0; for (let i = 0; i < m; i++) { mx += ls[i]; my += lf[i]; } mx /= m; my /= m;
  let sxy = 0, sxx = 0; for (let i = 0; i < m; i++) { sxy += (ls[i] - mx) * (lf[i] - my); sxx += (ls[i] - mx) * (ls[i] - mx); }
  return Math.floor(1000 * (sxy / sxx) + 0.5);
}

/* ---------- G7 microtiming on feel lanes ---------- */
const G7_LANES = { hats: GROUPS.hats, perc: GROUPS.perc };
function g7(songs) {
  const perFile = {}, all = { hats: [], perc: [] };
  for (const s of songs) {
    const pf = {};
    for (const [lane, groups] of Object.entries(G7_LANES)) {
      const x = residuals(s, drumNotesIn(s, groups));
      const r = { n: x.length, sd_milli: sdMilli(x) ?? -1, lag1_pm: x.length >= 64 ? (lag1Permille(x) ?? -9999) : -9999, alpha_milli: dfaAlphaMilli(x) ?? -9999 };
      pf[lane] = r;
      if (x.length >= 64) all[lane].push(r);
    }
    perFile[s.path] = pf;
  }
  const res = {};
  for (const lane of Object.keys(G7_LANES)) {
    const L = all[lane];
    res[lane] = {
      files: L.length,
      median_sd_milli: lowerMedian(L.map(r => r.sd_milli)) ?? -1,
      median_lag1_pm: lowerMedian(L.map(r => r.lag1_pm).filter(v => v !== -9999)) ?? -9999,
      median_alpha_milli: lowerMedian(L.map(r => r.alpha_milli).filter(v => v !== -9999)) ?? -9999,
      alpha_undefined: L.filter(r => r.alpha_milli === -9999).length
    };
  }
  const ok = l => bands.g7(res[l].files, res[l].median_sd_milli, res[l].median_lag1_pm, res[l].median_alpha_milli);
  res.pass = ok('hats') && ok('perc') ? 1 : 0;
  res.per_file = perFile;
  return res;
}

/* ---------- G8 per-lane swing ---------- */
function laneSwings(song) {
  const sec = firstSectionOf(song, 'drop');
  if (!sec) return null;
  const lanes = [];
  for (const [name, groups] of [['snare', GROUPS.snareFamily], ['hats', GROUPS.hats], ['perc', GROUPS.perc]]) {
    lanes.push({ lane: name, notes: between(drumNotesIn(song, groups), sec.tick, sec.end) });
  }
  for (const role of ['bass', 'chords', 'lead', 'vox']) {
    tracksWithRole(song, role).forEach(t => lanes.push({ lane: role + ':' + t.index, notes: between(t.notes, sec.tick, sec.end) }));
  }
  const out = [];
  for (const l of lanes) { const d = swingDelay(l.notes, 16); if (d !== null) out.push({ lane: l.lane, delay: d }); }
  return out;
}
// Distinct swing values: sort, then each cluster takes every value within 2 ticks of its smallest.
function distinctSwings(delays) {
  const d = delays.slice().sort(byNum);
  let count = 0;
  for (let i = 0; i < d.length;) { const start = d[i]; count++; while (i < d.length && d[i] <= start + 2) i++; }
  return count;
}
function g8(songs) {
  const perFile = {}, hist = new Map();
  let measured = 0, passing = 0;
  for (const s of songs) {
    const sw = laneSwings(s);
    if (sw === null) { perFile[s.path] = { lanes: '', distinct: -1 }; continue; }
    const k = distinctSwings(sw.map(x => x.delay));
    measured++; if (k >= 3) passing++;
    hist.set(k, (hist.get(k) || 0) + 1);
    perFile[s.path] = { lanes: sw.map(x => x.lane + '=' + x.delay).join(' '), distinct: k };
  }
  return { seeds: songs.length, measured, with_3_or_more: passing, distinct_histogram: numHist(hist), pass: measured === songs.length && passing === measured ? 1 : 0, per_file: perFile };
}

/* ---------- G9 fills ---------- */
const FILL_ADD = ['snare', 'clap', 'rim', 'tom', 'perc'];
function drumStepSets(song, bar0, nBars) {
  const bars = Array.from({ length: nBars }, () => ({ add: new Set(), kick: new Set(), all: [] }));
  for (const n of song.drumNotes) {
    const s = step16(n.tick), b = floorDiv(s, 16) - bar0;
    if (b < 0 || b >= nBars) continue;
    const st = mod(s, 16);
    if (FILL_ADD.includes(n.group)) bars[b].add.add(n.group + ':' + st);
    if (n.group === 'kick') bars[b].kick.add(st);
    if (n.group !== 'hats') bars[b].all.push(st * 128 + n.key);
  }
  return bars;
}
// Fill bars of one section (section-relative indices), against the section's own profile. "Rare" steps:
// snare/clap/rim/tom/perc steps that occur in at most an eighth of the section's bars; "missing" steps:
// kick steps that occur in at least three quarters of them but not in this bar. A bar is a fill if
// missing >= 2, or rare + missing >= 3. Sections under 4 bars have no fills.
function fillBarsOf(song, sec) {
  if (sec.bars < 4) return [];
  const bars = drumStepSets(song, sec.bar, sec.bars), out = [];
  const addCount = new Map(), kickCount = new Map();
  bars.forEach(b => { b.add.forEach(x => addCount.set(x, (addCount.get(x) || 0) + 1)); b.kick.forEach(x => kickCount.set(x, (kickCount.get(x) || 0) + 1)); });
  const commonKicks = [...kickCount.entries()].filter(([, c]) => 4 * c >= 3 * sec.bars).map(([s]) => s);
  for (let i = 0; i < sec.bars; i++) {
    let rare = 0; for (const x of bars[i].add) if (8 * addCount.get(x) <= sec.bars) rare++;
    let missing = 0; for (const s of commonKicks) if (!bars[i].kick.has(s)) missing++;
    if (missing >= 2 || rare + missing >= 3) out.push({ i, fp: [...new Set(bars[i].all)].sort(byNum).join(',') });
  }
  return out;
}
function boundaryGestures(song, fillsBySection) {
  const fx = notesWithRole(song, 'fx'), cym = song.drumNotes.filter(n => n.group === 'cymbal');
  const out = [];
  song.sections.forEach((sec, k) => {
    if (k === 0) return;
    const s = sec.tick, prev = song.sections[k - 1];
    const lastBar = floorDiv(s, BAR) - 1 - prev.bar;
    const kinds = [];
    if (fillsBySection[k - 1].some(f => f.i === lastBar)) kinds.push('fill');
    if (between(fx, s - 2 * BAR, s + 480).length) kinds.push('fx');
    if (cym.some(n => Math.abs(n.tick - s) <= 60)) kinds.push('cymbal');
    if (between(song.drumNotes, s - 240, s).length === 0 && between(song.drumNotes, s - BAR, s - 240).length > 0) kinds.push('gap');
    out.push({ bar: floorDiv(s, BAR), kinds });
  });
  return out;
}
function g9(songs) {
  let boundaries = 0, withGesture = 0;
  const pos = new Map(), perFile = {}, distinctPm = [];
  for (const s of songs) {
    const fills = s.sections.map(sec => fillBarsOf(s, sec));
    const gestures = boundaryGestures(s, fills);
    boundaries += gestures.length; const gw = gestures.filter(g => g.kinds.length).length; withGesture += gw;
    const songFills = [];
    s.sections.forEach((sec, k) => { if (sec.kind !== 'build') fills[k].forEach(f => { songFills.push(f); const p = f.i % 8; pos.set(p, (pos.get(p) || 0) + 1); }); });
    const distinct = countDistinct(songFills.map(f => f.fp));
    if (songFills.length >= 2) distinctPm.push(permille(distinct, songFills.length));
    perFile[s.path] = {
      boundaries: gestures.length, with_gesture: gw, fills: songFills.length, distinct_fills: distinct,
      fill_positions: songFills.map(f => f.i % 8).join(','),
      missing_gesture_bars: gestures.filter(g => !g.kinds.length).map(g => g.bar).join(',')
    };
  }
  const fillsTotal = [...pos.values()].reduce((a, b) => a + b, 0);
  const top = topOf(pos);
  const medDistinct = lowerMedian(distinctPm) ?? -1;
  return {
    boundaries, with_gesture: withGesture, with_gesture_pm: permille(withGesture, boundaries),
    fills: fillsTotal, position_histogram: numHist(pos), top_position: top ? top[0] : -1, top_position_pm: permille(top ? top[1] : 0, fillsTotal),
    songs_with_2_fills: distinctPm.length, median_distinct_pm: medDistinct,
    pass: boundaries > 0 && withGesture === boundaries && fillsTotal > 0 && 10 * (top ? top[1] : 0) <= 6 * fillsTotal && medDistinct >= 700 ? 1 : 0,
    per_file: perFile
  };
}

/* ---------- G10 form diversity ---------- */
const formShape = song => song.sections.map(s => s.kind + s.bars).join(' ');
function salientIntervals(song) {
  const bars = new Set(song.sections.map(s => s.bar));
  bars.add(0);
  const ch = macroChangeBars(song);
  G4_ROLES.forEach(r => ch[r].forEach(c => bars.add(c.bar)));
  const sorted = [...bars].sort(byNum), iv = [];
  for (let i = 1; i < sorted.length; i++) iv.push(sorted[i] - sorted[i - 1]);
  return iv;
}
function g10(songs) {
  const forms = songs.map(s => (s.sections.length ? formShape(s) : null));
  const d = diversity(forms, songs.length);
  const perFile = {};
  let allTwo = true;
  songs.forEach((s, i) => {
    const iv = salientIntervals(s), k = countDistinct(iv);
    if (k < 2) allTwo = false;
    perFile[s.path] = { form: forms[i] === null ? '' : forms[i], interval_values: k };
  });
  return {
    seeds: d.seeds, measured: d.measured, distinct_forms: d.distinct, top_form_count: d.top, top_form_pm: d.top_pm, top_form: d.top_shape,
    every_song_two_interval_values: allTwo ? 1 : 0,
    pass: d.measured === d.seeds && d.distinct >= 8 && 4 * d.top <= d.measured && allTwo ? 1 : 0,
    per_file: perFile
  };
}

/* ========== reported metrics (README: "Reported metrics") ========== */

// Lanes for the bar-level reported metrics: the kick group, and every bass, chords and lead track.
function reportLanes(song) {
  const lanes = [];
  const kick = song.drumNotes.filter(n => n.group === 'kick');
  if (kick.length) lanes.push({ role: 'kick', notes: kick });
  for (const role of ['bass', 'chords', 'lead']) tracksWithRole(song, role).forEach(t => lanes.push({ role, notes: t.notes }));
  return lanes;
}
const R_ROLES = ['kick', 'bass', 'chords', 'lead'];

// R1: share of bars equal to the bar 4 before, inside a section, both non-empty (step attribution, no velocity).
function rRepetition(songs) {
  const tot = Object.fromEntries(R_ROLES.map(r => [r, [0, 0]]));
  for (const s of songs) for (const lane of reportLanes(s)) for (const sec of sectionsOf(s)) {
    const fps = stepBars(lane.notes, sec.bar, sec.bars);
    for (let i = 4; i < fps.length; i++) if (fps[i] !== '' && fps[i - 4] !== '') { tot[lane.role][0]++; if (fps[i] === fps[i - 4]) tot[lane.role][1]++; }
  }
  return Object.fromEntries(R_ROLES.map(r => [r, { pairs: tot[r][0], equal: tot[r][1], equal_pm: permille(tot[r][1], tot[r][0]) }]));
}

// R2: longest run of consecutive identical non-empty 4-bar blocks (blocks from bar 0), in bars.
function rLoopTiling(songs) {
  const runs = Object.fromEntries(R_ROLES.map(r => [r, []]));
  for (const s of songs) {
    const best = {};
    for (const lane of reportLanes(s)) {
      const fps = stepBars(lane.notes, 0, s.bars);
      let longest = 0, run = 0, prev = null;
      for (let b = 0; b + 4 <= s.bars; b += 4) {
        const blk = fps.slice(b, b + 4).join('|'), empty = fps.slice(b, b + 4).every(f => f === '');
        if (!empty && blk === prev) run += 4; else run = empty ? 0 : 4;
        prev = empty ? null : blk;
        longest = Math.max(longest, run);
      }
      best[lane.role] = Math.max(best[lane.role] || 0, longest);
    }
    R_ROLES.forEach(r => { if (best[r] !== undefined) runs[r].push(best[r]); });
  }
  return Object.fromEntries(R_ROLES.map(r => [r, { files: runs[r].length, median_bars: lowerMedian(runs[r]) ?? -1, max_bars: runs[r].length ? Math.max(...runs[r]) : -1 }]));
}

// R3: closed-hat (keys 42, 44) accent period per bar with 4 or more onsets.
function accentPeriod(vel) {
  for (const p of [1, 2, 4, 8]) { let ok = true; for (let s = p; s < 16; s++) if (vel[s] !== vel[s - p]) { ok = false; break; } if (ok) return p; }
  return 16;
}
function rHatAccent(songs) {
  let bars = 0, short = 0;
  const per = new Map();
  for (const s of songs) {
    const hats = s.drumNotes.filter(n => n.key === 42 || n.key === 44);
    const byBar = new Map();
    for (const n of hats) { const st = step16(n.tick), b = floorDiv(st, 16); if (!byBar.has(b)) byBar.set(b, new Array(16).fill(0)); const v = byBar.get(b); v[mod(st, 16)] = Math.max(v[mod(st, 16)], n.velocity); }
    for (const v of byBar.values()) {
      if (v.filter(x => x > 0).length < 4) continue;
      const p = accentPeriod(v);
      bars++; if (p <= 4) short++;
      per.set(p, (per.get(p) || 0) + 1);
    }
  }
  return { bars, period_4_or_less: short, period_4_or_less_pm: permille(short, bars), period_histogram: numHist(per) };
}

// Swing grid of a file (for R4 and I2): the lower median of every note's odd-16th offset, 0 if none.
const fileSwing = song => lowerMedian(oddOffsets(song.tracks.flatMap(t => t.notes))) ?? 0;
const onSwungGrid = (t, d, tol) => { const r = mod(t, 240); return Math.abs(r) <= tol || Math.abs(r - 240) <= tol || Math.abs(r - 120 - d) <= tol; };
const onStraightGrid = (t, tol) => { const r = mod(t, 120); return r <= tol || 120 - r <= tol; };

// R4: melodic notes (bass, chords, lead) that end exactly 14 ticks before a swung 16th line, or 30 after
// one, or on the file's end tick.
function rGateRule(songs) {
  let notes = 0, rule = 0;
  for (const s of songs) {
    const dFile = fileSwing(s);
    for (const role of ['bass', 'chords', 'lead']) for (const t of tracksWithRole(s, role)) {
      const d = swingDelay(t.notes, 16) ?? dFile;
      for (const n of t.notes) {
        const e = n.tick + n.length;
        notes++;
        if (e === s.endTick || onSwungGrid(e + 14, d, 0) || onSwungGrid(e - 30, d, 0)) rule++;
      }
    }
  }
  return { notes, gate_rule: rule, gate_rule_pm: permille(rule, notes) };
}

// Harmony at tick t: the latest harmony stack with onset <= t; before the first, the last one (a loop).
function harmonyAt(stacks, t) {
  if (!stacks.length) return null;
  let lo = 0, hi = stacks.length;
  while (lo < hi) { const mid = (lo + hi) >> 1; if (stacks[mid].tick <= t) lo = mid + 1; else hi = mid; }
  return lo === 0 ? stacks[stacks.length - 1] : stacks[lo - 1];
}
// R5: share of lead and bass onsets whose pitch class is in the current harmony, on strong steps
// (step mod 4 == 0) and weak steps.
function rChordTones(songs) {
  const t = { lead: [0, 0, 0, 0], bass: [0, 0, 0, 0] }; // strong n, strong in, weak n, weak in
  for (const s of songs) {
    const st = harmonyStacks(s);
    if (!st.length) continue;
    for (const role of ['lead', 'bass']) for (const n of notesWithRole(s, role)) {
      const h = harmonyAt(st, n.tick), strong = mod(step16(n.tick), 4) === 0, inChord = (h.mask >> (n.key % 12)) & 1;
      if (strong) { t[role][0]++; t[role][1] += inChord; } else { t[role][2]++; t[role][3] += inChord; }
    }
  }
  return Object.fromEntries(Object.entries(t).map(([r, a]) => [r, { strong: a[0], strong_chord_tones: a[1], strong_pm: permille(a[1], a[0]), weak: a[2], weak_chord_tones: a[3], weak_pm: permille(a[3], a[2]) }]));
}

// R6: breath (lead rest share per 4-bar window with notes) and lead/vox complementarity per half-bar.
function rBreath(songs) {
  const rests = [];
  let units = 0, oneOnly = 0;
  for (const s of songs) {
    const lead = notesWithRole(s, 'lead'), vox = notesWithRole(s, 'vox');
    for (const sec of sectionsOf(s)) for (let w = 0; w + 4 <= sec.bars; w += 4) {
      const t0 = (sec.bar + w) * BAR, win = lead.filter(n => n.tick < t0 + 4 * BAR && n.tick + n.length > t0);
      if (!between(lead, t0, t0 + 4 * BAR).length) continue;
      let covered = 0;
      for (let k = 0; k < 64; k++) { const at = t0 + k * STEP; if (win.some(n => n.tick <= at && at < n.tick + n.length)) covered++; }
      rests.push(permille(64 - covered, 64));
    }
    if (!vox.length || !lead.length) continue;
    for (let h = 0; h < 2 * s.bars; h++) {
      const a = between(lead, h * 960, h * 960 + 960).length > 0, b = between(vox, h * 960, h * 960 + 960).length > 0;
      if (a || b) { units++; if (a !== b) oneOnly++; }
    }
  }
  return { lead_windows: rests.length, median_rest_pm: lowerMedian(rests) ?? -1, half_bars_with_lead_or_vox: units, only_one_pm: permille(oneOnly, units) ?? -1 };
}

// R7: lead shape: step ratio (consecutive top-note intervals of 1 or 2 semitones) and bar 1 = bar 3 rhythm.
function rLeadShape(songs) {
  let intervals = 0, steps = 0, repeats = 0, windows = 0, same13 = 0;
  for (const s of songs) for (const t of tracksWithRole(s, 'lead')) {
    const tops = stacksOf(t.notes).map(st => Math.max(...st.notes.map(n => n.key)));
    for (let i = 1; i < tops.length; i++) { const d = Math.abs(tops[i] - tops[i - 1]); intervals++; if (d === 1 || d === 2) steps++; if (d === 0) repeats++; }
    for (const sec of sectionsOf(s)) for (let w = 0; w + 4 <= sec.bars; w += 4) {
      const fps = stepBars(t.notes, sec.bar + w, 4).map(f => [...new Set(f.split(',').filter(x => x !== '').map(x => floorDiv(+x, 128)))].join(','));
      if (fps[0] === '' || fps[2] === '') continue;
      windows++; if (fps[0] === fps[2]) same13++;
    }
  }
  return { intervals, steps, step_pm: permille(steps, intervals), repeats_pm: permille(repeats, intervals), windows, bar1_eq_bar3: same13, bar1_eq_bar3_pm: permille(same13, windows) };
}

// R8: voicing texture of harmony stacks (per track, 2 or more notes).
function voicingType(keys) {
  const pcs = new Set(keys.map(k => k % 12)).size;
  const sorted = [...new Set(keys)].sort(byNum);
  if (sorted.length >= 3 && sorted.every((k, i) => i === 0 || k - sorted[i - 1] === 5)) return 'quartal';
  return pcs <= 1 ? 'octave' : pcs === 2 ? 'dyad' : pcs === 3 ? 'triad' : 'four_plus';
}
function rVoicing(songs) {
  const typesPerSong = [], types = new Map();
  let stacks = 0, fourPlus = 0, recurring = 0, revoiced = 0;
  for (const s of songs) {
    const seen = new Set(), last = new Map();
    for (const t of tracksWithRole(s, 'chords')) for (const st of stacksOf(t.notes)) {
      if (st.notes.length < 2) continue;
      const keys = st.notes.map(n => n.key).sort(byNum), ty = voicingType(keys), mask = pcMask(st.notes), kk = keys.join(',');
      stacks++; if (ty === 'four_plus') fourPlus++;
      seen.add(ty); types.set(ty, (types.get(ty) || 0) + 1);
      if (last.has(mask)) { recurring++; if (last.get(mask) !== kk) revoiced++; }
      last.set(mask, kk);
    }
    if (seen.size) typesPerSong.push(seen.size);
  }
  return {
    stacks, type_histogram: strHist(types, 'type'), median_types_per_song: lowerMedian(typesPerSong) ?? -1,
    four_plus_pm: permille(fourPlus, stacks), recurring, revoiced, revoiced_pm: permille(revoiced, recurring)
  };
}

// R9: odd cycles: drum lanes (track, key) per 8-bar window whose onset sequence has a period that
// neither divides 16 nor is a multiple of it.
function rOddCycles(songs) {
  let windows = 0, odd = 0;
  for (const s of songs) for (const t of tracksWithRole(s, 'drums')) {
    const keys = [...new Set(t.notes.map(n => n.key))].sort(byNum);
    for (const key of keys) {
      const notes = t.notes.filter(n => n.key === key);
      for (const sec of sectionsOf(s)) for (let w = 0; w + 8 <= sec.bars; w += 8) {
        const b0 = sec.bar + w, seq = new Array(128).fill(0);
        for (const n of notes) { const st = step16(n.tick) - 16 * b0; if (st >= 0 && st < 128) seq[st] = 1; }
        if (!seq.some(x => x)) continue;
        windows++;
        let period = 0;
        for (let p = 1; p <= 64 && !period; p++) { let ok = true; for (let i = p; i < 128; i++) if (seq[i] !== seq[i - p]) { ok = false; break; } if (ok) period = p; }
        if (period && 16 % period !== 0 && period % 16 !== 0) odd++;
      }
    }
  }
  return { lane_windows: windows, odd_cycles: odd, odd_cycles_pm: permille(odd, windows) };
}

// R10: cross-seed duplicate 2-bar motifs: 2-bar windows (even section-relative bars) of a role, as
// (step 0-31, key minus the window's first key); duplicated if another file has the same.
function rDuplicates(songs, role) {
  const owners = new Map(), all = [];
  for (const s of songs) for (const sec of sectionsOf(s)) for (let w = 0; w + 2 <= sec.bars; w += 2) {
    const t0 = (sec.bar + w) * BAR, ns = notesWithRole(s, role).filter(n => { const b = floorDiv(step16(n.tick), 16); return b >= sec.bar + w && b < sec.bar + w + 2; });
    if (!ns.length) continue;
    const first = ns[0].key, fp = [...new Set(ns.map(n => (step16(n.tick) - floorDiv(t0, STEP)) + ':' + (n.key - first)))].sort().join(',');
    all.push({ file: s.path, fp });
    if (!owners.has(fp)) owners.set(fp, new Set());
    owners.get(fp).add(s.path);
  }
  const dup = all.filter(x => owners.get(x.fp).size > 1).length;
  return { windows: all.length, duplicated: dup, duplicated_pm: permille(dup, all.length) };
}

// R11: LHL syncopation per bar, cyclic bar, weights per 16th step.
const LHL_W = [0, -4, -3, -4, -2, -4, -3, -4, -1, -4, -3, -4, -2, -4, -3, -4];
function lhl(onsets) { // onsets: sorted distinct steps 0-15
  let total = 0;
  onsets.forEach((i, k) => {
    const j = k + 1 < onsets.length ? onsets[k + 1] : onsets[0] + 16;
    let best = null;
    for (let x = i + 1; x < j; x++) { const w = LHL_W[x % 16]; if (best === null || w > best) best = w; }
    if (best !== null && best > LHL_W[i]) total += best - LHL_W[i];
  });
  return total;
}
function rSyncopation(songs) {
  const vals = { kick: [], snare: [], hats: [], bass: [], chords: [], lead: [] };
  for (const s of songs) {
    const lanes = [
      ['kick', s.drumNotes.filter(n => n.group === 'kick')], ['snare', drumNotesIn(s, GROUPS.snareFamily)], ['hats', drumNotesIn(s, GROUPS.hats)],
      ['bass', notesWithRole(s, 'bass')], ['chords', notesWithRole(s, 'chords')], ['lead', notesWithRole(s, 'lead')]
    ];
    for (const [name, notes] of lanes) {
      const byBar = new Map();
      for (const n of notes) { const st = step16(n.tick), b = floorDiv(st, 16); if (!byBar.has(b)) byBar.set(b, new Set()); byBar.get(b).add(mod(st, 16)); }
      for (const set of byBar.values()) vals[name].push(lhl([...set].sort(byNum)));
    }
  }
  return Object.fromEntries(Object.entries(vals).map(([k, v]) => {
    const sum = v.reduce((a, b) => a + b, 0);
    return [k, { bars: v.length, mean_milli: v.length ? floorDiv(2000 * sum + v.length, 2 * v.length) : -1, sd_milli: sdMilli(v) ?? -1, distinct: countDistinct(v) }];
  }));
}

/* ========== invariants measured on legacy (README: "Invariants") ========== */
const MINOR = [0, 2, 3, 5, 7, 8, 10], MAJOR = [0, 2, 4, 5, 7, 9, 11];
const tonicOf = ks => mod((ks.minor ? 9 : 0) + 7 * ks.sharps, 12);
function iInKey(songs) {
  const t = {};
  for (const s of songs) {
    if (!s.keySigs.length) continue;
    for (const tr of s.tracks) {
      if (tr.role === 'drums' || tr.role === 'ignore') continue;
      const a = t[tr.role] = t[tr.role] || [0, 0];
      for (const n of tr.notes) {
        let ks = s.keySigs[0]; for (const k of s.keySigs) if (k.tick <= n.tick) ks = k;
        a[0]++; if ((ks.minor ? MINOR : MAJOR).includes(mod(n.key - tonicOf(ks), 12))) a[1]++;
      }
    }
  }
  return sortedObject(new Map(Object.entries(t).map(([r, a]) => [r, { notes: a[0], in_key: a[1], in_key_pm: permille(a[1], a[0]) }])));
}
function iSwungGrid(songs) {
  const t = { riser: [0, 0, 0], build_snare: [0, 0, 0] }; // n, on the swung grid, straight only
  for (const s of songs) {
    const d = fileSwing(s);
    const classify = (a, tick) => { a[0]++; if (onSwungGrid(tick, d, 2)) a[1]++; else if (onStraightGrid(tick, 2)) a[2]++; };
    notesWithRole(s, 'fx').forEach(n => classify(t.riser, n.tick));
    s.sections.filter(x => x.kind === 'build').forEach(sec => between(drumNotesIn(s, GROUPS.snareFamily), sec.tick, sec.end).forEach(n => classify(t.build_snare, n.tick)));
  }
  return Object.fromEntries(Object.entries(t).map(([k, a]) => [k, { notes: a[0], swung: a[1], straight_only: a[2], straight_only_pm: permille(a[2], a[0]) }]));
}
function iKickBass(songs) {
  let pairs = 0, tight = 0;
  for (const s of songs) {
    const kick = s.drumNotes.filter(n => n.group === 'kick');
    for (const n of notesWithRole(s, 'bass')) {
      const near = between(kick, n.tick - 30, n.tick + 31);
      if (!near.length) continue;
      const d = Math.min(...near.map(k => Math.abs(k.tick - n.tick)));
      pairs++; if (d <= 2) tight++;
    }
  }
  return { pairs, within_2_ticks: tight, within_2_ticks_pm: permille(tight, pairs) };
}
function iSections(songs) {
  let n = 0, on8 = 0;
  for (const s of songs) for (const sec of s.sections) { n++; if (sec.tick % (8 * BAR) === 0) on8++; }
  return { sections: n, on_8_bar_lines: on8, on_8_bar_lines_pm: permille(on8, n) };
}
function iAnchors(songs) {
  let n = 0, tight = 0;
  for (const s of songs) for (const k of s.drumNotes.filter(x => x.group === 'kick')) {
    const r = mod(k.tick, 480), d = Math.min(r, 480 - r);
    if (d > 30) continue;
    n++; if (d <= 2) tight++;
  }
  return { kicks_near_beats: n, within_2_ticks: tight, within_2_ticks_pm: permille(tight, n) };
}

module.exports = {
  helpers: { popcount, pcMask, rotate, lowerMedian, permille, permilleBig, sdMilli, stacksOf, harmonyStacks, harmonySlots, between, residuals, swingDelay, distinctSwings, lag1Permille, dfaAlphaMilli, macroChanges, stepBars, fillBarsOf, sectionsOf, notesWithRole, drumNotesIn, GROUPS, sortedObject, numHist, strHist, histogram, topOf },
  bands, g1, g2, g3, g4, g5, partLevels, g6, g7, g8, g9, g10, progressionShape, harmonicRhythm, macroChangeBars, laneSwings, formShape,
  rRepetition, rLoopTiling, rHatAccent, accentPeriod, rGateRule, rChordTones, rBreath, rLeadShape, voicingType, rVoicing, rOddCycles, rDuplicates, lhl, rSyncopation,
  iInKey, iSwungGrid, iKickBass, iSections, iAnchors, fileSwing
};
