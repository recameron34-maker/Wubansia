  /* ---------- extended mixes: longer, more varied, one layer deeper ---------- */
  // Sixteen tracks: the drum kit split into five lanes, bass + sub, chords + stabs, melody + counter-melody/harmony,
  // pad, arp, vocal-chop MIDI, riser and FX hits. Everything is generated from the kit's own parts with a seeded
  // random source, so a kit always gives the same extended mix.
  const EXT_TRACKS = [
    { key: 'kick', name: 'Kick', ch: 9, program: 0, drum: true },
    { key: 'snare', name: 'Snare & Clap', ch: 9, program: 0, drum: true },
    { key: 'hats', name: 'Hats & Ride', ch: 9, program: 0, drum: true },
    { key: 'perc', name: 'Percussion', ch: 9, program: 0, drum: true },
    { key: 'fills', name: 'Fills, Rolls & Cymbals', ch: 9, program: 0, drum: true },
    { key: 'bass', name: 'Bass', ch: 1, program: 38 },
    { key: 'sub', name: 'Sub', ch: 7, program: 39 },
    { key: 'chords', name: 'Chords', ch: 2, program: 4 },
    { key: 'stabs', name: 'Stabs', ch: 8, program: 17 },
    { key: 'lead', name: 'Melody', ch: 3, program: 80 },
    { key: 'counter', name: 'Counter-melody & Harmony', ch: 10, program: 81 },
    { key: 'pad', name: 'Pad', ch: 4, program: 89 },
    { key: 'arp', name: 'Arp', ch: 5, program: 98 },
    { key: 'vox', name: 'Vocal Chops', ch: 11, program: 54 },
    { key: 'riser', name: 'Riser', ch: 6, program: 81 },
    { key: 'fx', name: 'FX Hits', ch: 12, program: 119 }
  ];
  const FX_NOTE = { impact: 36, revcrash: 38, downlifter: 40, subdrop: 41 };
  const DRUM_TRK = { K: 'kick', SN: 'snare', CL: 'snare', CH: 'hats', OH: 'hats', RD: 'hats', CR: 'fills', LT: 'fills', MT: 'fills', HT: 'fills' };
  const PERCSET = new Set(['SH', 'TB', 'RIM', 'CLV', 'WB', 'COH', 'CLO', 'BH', 'BL', 'CB']);
  const QI = {
    '': [0, 4, 7], m: [0, 3, 7], '6': [0, 4, 7, 9], m6: [0, 3, 7, 9], '7': [0, 4, 7, 10], m7: [0, 3, 7, 10], maj7: [0, 4, 7, 11],
    m9: [0, 3, 7, 10, 2], maj9: [0, 4, 7, 11, 2], '9': [0, 4, 7, 10, 2], m11: [0, 3, 7, 10, 2, 5], '7sus4': [0, 5, 7, 10],
    '9sus4': [0, 5, 7, 10, 2], '7b9': [0, 4, 7, 10, 1], m7b5: [0, 3, 6, 10]
  };
  const PERC_LIB = {
    SH: ['3243324332433243', '2.3.2.3.2.3.2.3.', '.3.3.3.3.3.3.3.3', '4232423242324232'],
    TB: ['....6.......6...', '..5...5...5...5.', '....5..3....5..3', '......5.......5.'],
    RIM: ['...5..4....5..4.', '......4..4....3.', '..4...4...4..4..', '.4....4..4...4..'],
    CLV: ['5..5..5...5.5...', '..5.5...5..5....', '5.....5...5.....', '...5..5....5....'],
    WB: ['..4.......4..4..', '.....4....4.....', '...4...4..4.....', '.4.....4....4...'],
    COH: ['......5...5.....', '...5..5.....5...', '..4.4...5.....4.', '..5...5...5...5.'],
    CLO: ['5.......5..5....', '5.....4.....4...', '4..4......5.....', '5...4...5...4...'],
    BH: ['..5...5.....5.5.', '...5.5.....5..5.', '..5.5...5...5...'],
    BL: ['5.....5...5.....', '4..4....4..4....', '5..4..5.....4...'],
    CB: ['..........5.....', '......4.......5.', '..5.......5.....', '....5.......5...']
  };
  const GENRE_PERC = { ukg: ['SH', 'RIM', 'TB', 'BH', 'CLV', 'BL'], mth: ['SH', 'CLV', 'WB', 'RIM', 'COH', 'CB', 'CLO', 'BH'], sg: ['SH', 'TB', 'RIM', 'CLV', 'COH', 'CLO'], ch: ['SH', 'TB', 'COH', 'CLO', 'CB', 'CLV'] };
  const STAB_RHY = {
    ukg: ['..9..7....8..7..', '......8..7....8.', '..8...8...8..7..', '.7..8.....8..7..'],
    mth: ['......8.........', '...........8....', '......8.......6.', '..........8.....'],
    sg: ['..8...8...8...8.', '..8.....8.....7.', '......8.......8.', '..8..8....8.....'],
    ch: ['9..8..8...8..8..', '..8..8..8...8...', '9.....8...8.....', '..8...8...8..8..']
  };
  const VOX_RHY = {
    ukg: ['..x..x.x..x...x.', 'x..x..x...x.x...', '..x.x...x..x....', '......x.x..x..x.'],
    mth: ['x.......x.x.....', '......x.......x.', 'x.....x.........', '..x.......x.....'],
    sg: ['..x...x.x...x...', 'x...x..x..x.....', '..x.x...x.....x.', '....x.x.....x...'],
    ch: ['....x..x....x.x.', 'x..x....x..x....', '..x..x....x.x...', 'x...x...x..x.x..']
  };
  const CTR_RHY = ['x.x.x...', 'x..x..x.', '..x.x.x.', 'x...x.x.', 'x.x...x.', '.x.x..x.', 'x..x.x..'];
  const FILLW = { ukg: ['roll', 'toms', 'flams', 'roll', 'openhat'], mth: ['kicks', 'openhat', 'roll', 'kicks', 'flams'], sg: ['roll', 'toms', 'kicks', 'roll', 'flams'], ch: ['toms', 'roll', 'flams', 'openhat', 'toms'] };
  const HATS = { ukg: ['base', 'base', ['base', 'ride'], 'shuffle'], mth: ['base', 'sixteenths', ['base', 'ride'], 'eighths'], sg: ['base', 'sixteenths', 'base', ['base', 'ride']], ch: ['offbeat', 'base', ['offbeat', 'ride'], 'base'] };
  const PIANO_RHY = ['9..8..8...8..8..', '9.....8...8..8..', '..8..8..8...8...'];

  function chordFromName(name, voicing, tr) {
    const m = /^([A-G])(#|b)?(.*)$/.exec(name);
    const root = ((PC[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0) + tr) % 12 + 12) % 12;
    const v = voicing.map(x => (typeof x === 'string' ? noteNum(x) : x) + tr);
    const iv = QI[m[3]];
    return { name, root, pcs: iv ? iv.map(i => (root + i) % 12) : [...new Set(v.map(n => n % 12))], voicing: v };
  }
  function hash32(s) { let h = 2166136261; for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); } return h >>> 0; }
  function mulberry(a) {
    return () => { a = (a + 0x6D2B79F5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  }

  function buildExtended(kit, pc) {
    const tr = transposeFor(kit, pc), R = mulberry(hash32(kit.id + '/extended'));
    const pick = a => a[Math.floor(R() * a.length)], chance = p => R() < p, jit = a => Math.round((R() * 2 - 1) * a);
    const g = kit.genre, sc = SCALE[kit.mode], home = ((kit.root + tr) % 12 + 12) % 12;
    const scPcs = sc.map(i => (home + i) % 12);
    const P16 = p => Math.round(warp(p, kit.swing) * STEP);
    const D = {}; ['main', 'var', 'fill'].forEach(v => { D[v] = compileDrums(kit, v).events; });
    const M = {}; ['bass', 'chords', 'lead'].forEach(p => ['A', 'B'].forEach(v => { M[p + v] = compileMelodic(kit, p, v, tr).events; }));
    const T = {}, CC = {};
    EXT_TRACKS.forEach(t => { T[t.key] = []; CC[t.key] = []; });
    const sections = [], gaps = [], keyChanges = [];
    let lift = 0;                                   // semitones (the house key lift)
    const PC_ = GENRE_PERC[g];

    /* ---- harmony: the chord on each beat of the 4-bar loop, plus reharmonisations ---- */
    const CHI = {};
    Object.keys(kit.chords).forEach(nm => { CHI[nm] = chordFromName(nm, kit.chords[nm], tr); });
    const starts = [];
    M.chordsA.filter(e => e.chord).sort((a, b) => a.t - b.t).forEach(e => { if (!starts.length || starts[starts.length - 1].t !== e.t) starts.push(e); });
    const beatChord = [];
    for (let k = 0; k < 16; k++) {
      const t = k * 480 + 130;
      let s = starts[starts.length - 1];
      starts.forEach(x => { if (x.t <= t) s = x; });
      beatChord.push(CHI[s.chord]);
    }
    const diat7 = d => [0, 2, 4, 6].map(k => (home + sc[(d + k) % 7]) % 12);
    const subCache = new Map();
    function subOf(c) {   // diatonic third-substitution: root a third lower, shares three notes with the original
      if (subCache.has(c)) return subCache.get(c);
      const d = scPcs.indexOf(c.root);
      const s = d < 0 ? c : { name: c.name + '/sub', root: diat7((d + 5) % 7)[0], pcs: diat7((d + 5) % 7), voicing: null };
      subCache.set(c, s); return s;
    }
    const domRoot = (home + 7) % 12;
    const DOM = { name: 'V7', root: domRoot, pcs: [0, 4, 7, 10].map(i => (domRoot + i) % 12), voicing: null };
    const lb = b => ((b % 4) + 4) % 4;
    function harm(b, beat, rh) {
      let c = beatChord[lb(b) * 4 + beat];
      if (rh && rh.sub && lb(b) === 2) c = subOf(c);
      if (rh && rh.turn && lb(b) === 3 && beat >= 2) c = DOM;
      return c;
    }
    const at = (bs, t) => { const tt = t + 130; return [bs + Math.floor(tt / BAR), Math.floor((tt % BAR) / 480)]; };

    function voice(pcs, prev, lo, hi, max) {
      max = max || 4;
      const use = pcs.length > max ? pcs.slice(1, 1 + max) : pcs.slice();
      const ref = prev && prev.length ? prev.slice().sort((a, b) => a - b) : [(lo + hi) / 2 - 5, (lo + hi) / 2, (lo + hi) / 2 + 5];
      let best = null;
      for (let r = 0; r < use.length; r++) {
        const ord = use.slice(r).concat(use.slice(0, r));
        let b0 = lo; while (((b0 % 12) + 12) % 12 !== ord[0]) b0++;
        const v = [b0];
        for (let i = 1; i < ord.length; i++) { let n = v[i - 1] + 1; while (n % 12 !== ord[i]) n++; v.push(n); }
        for (const sh of [0, 12]) {
          const w = v.map(n => n + sh);
          if (w[w.length - 1] > hi) continue;
          const cost = w.reduce((s, n, i) => s + Math.abs(n - ref[Math.min(i, ref.length - 1)]), 0);
          if (!best || cost < best.cost) best = { w, cost };
        }
      }
      return best ? best.w : use.map(p => lo + (((p - lo) % 12) + 12) % 12);
    }
    function degOf(n) {
      const rel = (((n - home) % 12) + 12) % 12, oct = Math.floor((n - home - rel) / 12);
      let d = 6; while (d > 0 && sc[d] > rel) d--;
      return { oct, d, alt: rel - sc[d] };
    }
    function shiftDeg(n, k) {
      const x = degOf(n); let d = x.d + k, oct = x.oct;
      while (d < 0) { d += 7; oct--; }
      while (d > 6) { d -= 7; oct++; }
      return home + oct * 12 + sc[d] + x.alt;
    }
    function snap(n, pcs, maxd) {
      for (let k = 0; k <= (maxd || 2); k++) for (const s of [-1, 1]) { const m = n + s * k; if (pcs.includes(((m % 12) + 12) % 12)) return m; }
      return n;
    }
    const tonesIn = (c, lo, hi) => { const o = []; for (let m = lo; m <= hi; m++) if (c.pcs.includes(m % 12)) o.push(m); return o; };
    const nearest = (arr, x, avoid) => { const c = arr.filter(y => y !== avoid); return (c.length ? c : arr).reduce((a, y) => (Math.abs(y - x) < Math.abs(a - x) ? y : a)); };

    /* ---- emitters ---- */
    const pushN = (trk, t, d, n, v, extra) => { T[trk].push(Object.assign({ t: Math.round(t), d: Math.max(15, Math.round(d)), n: n + lift, v: clampV(v) }, extra || {})); };
    const pushD = (lane, t, v, trk, d) => { if (!DRUM[lane]) return; T[trk || DRUM_TRK[lane] || 'perc'].push({ t: Math.max(0, Math.round(t)), d: d || 60, n: DRUM[lane].note, v: clampV(v), lane }); };
    const ccSet = (trk, t, cc, val) => CC[trk].push({ t: Math.round(t), cc, val: Math.max(0, Math.min(127, Math.round(val))) });
    function ccRamp(trk, b0, n, cc, from, to, curve) {
      const steps = Math.max(1, Math.round(n * 8));
      for (let s = 0; s <= steps; s++) { const f = s / steps; ccSet(trk, b0 * BAR + Math.min(f * n * BAR, n * BAR - 1), cc, from + (to - from) * (curve ? curve(f) : f)); }
    }
    const fx = (type, t, d, v) => T.fx.push({ t, d: d || 240, n: FX_NOTE[type], v: v || 110, fx: type });
    const gap = b => gaps.push(b * BAR);

    /* ---- drums ---- */
    const barEv = (v, b) => { const i = lb(b); return D[v].filter(e => e.t >= i * BAR && e.t < (i + 1) * BAR).map(e => ({ t: e.t - i * BAR, lane: e.lane, v: e.v })); };
    function hatsGen(mode) {
      const o = [];
      if (mode === 'eighths') for (let s = 0; s < 16; s += 2) o.push({ p: s, lane: 'CH', v: s % 4 ? 96 : 66 });
      if (mode === 'sixteenths') { const acc = [72, 44, 96, 50]; for (let s = 0; s < 16; s++) o.push({ p: s, lane: 'CH', v: acc[s % 4] }); }
      if (mode === 'offbeat') { for (let s = 2; s < 16; s += 4) o.push({ p: s, lane: 'OH', v: 92 }); for (let s = 0; s < 16; s += 4) o.push({ p: s, lane: 'CH', v: 58 }); }
      if (mode === 'shuffle') { const acc = [80, 38, 64, 98, 70, 40, 60, 100, 80, 38, 66, 96, 72, 44, 58, 104]; for (let s = 0; s < 16; s++) if (s % 4 !== 1 || chance(0.5)) o.push({ p: s, lane: 'CH', v: acc[s] }); }
      if (mode === 'ride') for (let s = 0; s < 16; s += 2) o.push({ p: s, lane: 'RD', v: s % 4 ? 84 : 64 });
      return o;
    }
    function libGen(lane, variant) {
      const L = PERC_LIB[lane]; if (!L) return [];
      const s = L[((variant % L.length) + L.length) % L.length], o = [];
      for (let j = 0; j < 16; j++) if (s[j] !== '.') o.push({ p: j, lane, v: Math.round(13 + (+s[j]) * 12.7) });
      return o;
    }
    function fillGen(type) {
      const o = [];
      if (type === 'roll') { for (let k = 0; k < 8; k++) o.push({ p: 12 + k * 0.5, lane: 'SN', v: 52 + k * 8, trk: 'fills', d: 50 }); return { ev: o, clear: 12, lanes: ['SN', 'CL'] }; }
      if (type === 'toms') { [[10, 'HT', 96], [11, 'HT', 88], [12, 'MT', 100], [13, 'MT', 92], [14, 'LT', 108], [15, 'LT', 100]].forEach(([p, l, v]) => o.push({ p, lane: l, v, trk: 'fills' })); return { ev: o, clear: 10, lanes: ['SN', 'CL', 'CH', 'OH', 'RIM'] }; }
      if (type === 'kicks') { [12, 13, 14, 15].forEach((p, k) => o.push({ p, lane: 'K', v: 72 + k * 12, trk: 'fills' })); return { ev: o, clear: 12, lanes: ['K'] }; }
      if (type === 'flams') { [12, 14].forEach(p => { o.push({ p, lane: 'CL', v: 104, trk: 'fills' }); o.push({ p: p + 0.17, lane: 'CL', v: 70, trk: 'fills' }); }); o.push({ p: 15, lane: 'SN', v: 86, trk: 'fills' }); return { ev: o, clear: 12, lanes: ['CL', 'SN'] }; }
      if (type === 'openhat') { o.push({ p: 14, lane: 'OH', v: 104, trk: 'fills' }); return { ev: o, clear: 14, lanes: ['CH', 'OH'] }; }
      if (type === 'big') { D.fill.forEach(e => o.push({ t: e.t, lane: e.lane, v: e.v, trk: e.lane === 'K' ? 'kick' : DRUM_TRK[e.lane] === 'hats' ? 'hats' : 'fills' })); return { ev: o, clear: 0, lanes: null }; }
      return { ev: o, clear: 16, lanes: [] };
    }
    function drumBar(b, o) {
      const t0 = b * BAR, vf = o.vel || 1;
      const fill = o.fill ? fillGen(o.fill) : null;
      const cleared = (lane, p) => !!fill && (fill.lanes === null || fill.lanes.includes(lane)) && p >= fill.clear - 0.01;
      const modes = [].concat(o.hats || 'base');
      const base = barEv(o.base || 'main', b);
      base.forEach(e => {
        const L = e.lane, p = e.t / STEP;
        if (L === 'K') { if (o.kick && o.kick !== 'base' && o.kick !== 'thin') return; if (o.kick === 'thin' && e.t % 480 > 10) return; }
        else if (L === 'SN' || L === 'CL') { if (o.snare === false) return; }
        else if (L === 'CH' || L === 'OH' || L === 'RD') { if (!modes.includes('base')) return; }
        else if (L === 'CR') return;
        else if (!o.percBase) return;
        if (cleared(L, p)) return;
        const loose = DRUM_TRK[L] === 'hats' || PERCSET.has(L);
        pushD(L, t0 + e.t + (loose ? jit(4) : 0), e.v * vf * (1 + (R() - 0.5) * (loose ? 0.14 : 0.06)));
      });
      if (o.kick === 'four') for (let k = 0; k < 4; k++) if (!cleared('K', k * 4)) pushD('K', t0 + k * 480, 116 * vf);
      modes.filter(m => m !== 'base' && m !== 'none').forEach(m => hatsGen(m).forEach(h => { if (!cleared(h.lane, h.p)) pushD(h.lane, t0 + P16(h.p) + jit(4), h.v * vf * (0.92 + R() * 0.16)); }));
      (o.lib || []).forEach(([lane, vi]) => {
        if (o.percBase && base.some(e => e.lane === lane)) return;
        libGen(lane, vi).forEach(h => { if (!cleared(lane, h.p)) pushD(lane, t0 + P16(h.p) + jit(5), h.v * vf * (0.88 + R() * 0.24)); });
      });
      if (o.ghosts) {
        const gl = g === 'mth' ? 'RIM' : 'SN';
        [3, 6, 7, 9, 11, 14, 15].forEach(p => {
          if (!chance(o.ghosts) || cleared(gl, p)) return;
          if (base.some(e => (e.lane === 'SN' || e.lane === 'CL' || e.lane === gl) && Math.abs(e.t / STEP - p) < 0.8)) return;
          pushD(gl, t0 + P16(p) + jit(3), (g === 'mth' ? 50 : 36) * vf * (0.8 + R() * 0.4), gl === 'RIM' ? 'perc' : 'snare');
        });
      }
      if (fill) fill.ev.forEach(f => pushD(f.lane, t0 + (f.t !== undefined ? f.t : P16(f.p)), f.v * vf, f.trk, f.d));
      if (o.crash) pushD('CR', t0, 110, 'fills', 240);
    }
    const drumPhrase = (b0, n, fn) => { for (let i = 0; i < n; i++) drumBar(b0 + i, fn(i)); };
    const fillAt = (ph, i) => (i % 16 === 15 ? (ph % 2 ? 'big' : pick(FILLW[g])) : pick(FILLW[g]));
    const hatsAt = ph => HATS[g][ph % HATS[g].length];
    function grooveDrums(b0, n, energy, o) {
      o = o || {};
      drumPhrase(b0, n, i => {
        const ph = Math.floor(i / 8), last = i % 8 === 7;
        const nLib = Math.max(0, Math.min(3, energy - 1 + (ph % 2)));
        return {
          base: o.base ? o.base(ph, i) : (ph % 2 ? 'var' : 'main'),
          kick: o.kick ? o.kick(i) : 'base', snare: o.snare !== false,
          hats: o.hats ? o.hats(ph, i) : (energy >= 3 ? hatsAt(ph) : 'base'),
          percBase: o.percBase !== false,
          lib: PC_.slice(1, 1 + nLib).map((l, k) => [l, ph + k]),
          ghosts: energy >= 2 ? (g === 'mth' ? 0.12 : g === 'ch' ? 0.06 + 0.02 * energy : 0.1 + 0.05 * energy) : 0,
          fill: last && !(o.noFillLast && i === n - 1) ? (o.fill ? o.fill(i, ph) : fillAt(ph, i)) : null,
          crash: o.crash ? o.crash(i) : (i === 0 && energy >= 3) || (energy >= 4 && i > 0 && i % 16 === 0),
          vel: o.vel ? o.vel(i / n) : 1
        };
      });
    }
    function rollT(b0, n, lane, v0, v1) {   // crescendo: quarters -> 8ths -> 16ths -> 32nds
      const total = n * BAR;
      for (let i = 0; i < n; i++) {
        const f = i / n, sub = f < 0.25 ? 4 : f < 0.5 ? 8 : f < 0.875 ? 16 : 32, step = BAR / sub;
        for (let k = 0; k < sub; k++) { const t = i * BAR + k * step; pushD(lane, b0 * BAR + t, v0 + (v1 - v0) * t / total, 'fills', Math.min(60, step)); }
      }
    }

    /* ---- melodic transforms (on 4-bar loops, ticks relative to the loop) ---- */
    const thin = ev => ev.filter(e => e.t % 480 < 30 || e.v >= 110);
    const octPop = ev => ev.map(e => { const s = Math.round(e.t / STEP) % 4; return (s === 2 || s === 3) && e.d <= 260 && e.n + 12 <= 55 && chance(0.6) ? Object.assign({}, e, { n: e.n + 12 }) : e; });
    const sortT = ev => ev.slice().sort((a, b) => a.t - b.t);
    const orn = ev => {
      const s = sortT(ev), out = [];
      s.forEach((e, i) => {
        const prevEnd = i ? s[i - 1].t + s[i - 1].d : -1e9;
        if (e.d >= 300 && e.t - prevEnd >= 70 && e.t >= 60 && chance(0.4)) out.push({ t: e.t - 50, d: 45, n: shiftDeg(e.n, 1), v: Math.round(e.v * 0.7) });
        out.push(e);
      });
      return out;
    };
    const ANT = BAR - P16(15);
    const push = ev => {
      const s = sortT(ev);
      return s.map((e, i) => {
        if (e.t % BAR > 10 || Math.floor(e.t / BAR) % 2 === 0) return e;
        const prevEnd = i ? s[i - 1].t + s[i - 1].d : -1e9;
        if (e.t - ANT - prevEnd < 20) return e;
        return Object.assign({}, e, { t: e.t - ANT, d: e.d + ANT });
      });
    };
    const snapTo = rh => (ev, bs) => !rh ? ev : ev.map(e => {
      const [bar, beat] = at(bs, e.t - 70);
      const c = harm(bar, beat, rh), o = beatChord[lb(bar) * 4 + beat];
      if (c === o || c.pcs.includes(e.n % 12)) return e;
      if (e.t % 480 >= 40 && e.d < 240) return e;          // passing notes on weak 16ths stay
      return Object.assign({}, e, { n: snap(e.n, c.pcs, 2) });
    });
    const answer = (deg, rh) => (ev, bs) => {
      const first = ev.filter(e => e.t < 2 * BAR);
      if (first.length < 2) return ev;
      const resp = first.map(e => {
        const t = e.t + 2 * BAR; let n = shiftDeg(e.n, deg);
        const [bar, beat] = at(bs, t - 70);
        if (t % 480 < 40 || e.d >= 240) n = snap(n, harm(bar, beat, rh).pcs, 2);
        return Object.assign({}, e, { t, n });
      });
      return first.concat(resp);
    };
    const reharm = rh => (ev, bs) => {
      if (!rh) return ev;
      const groups = new Map();
      ev.forEach(e => { if (!groups.has(e.t)) groups.set(e.t, []); groups.get(e.t).push(e); });
      const out = [];
      groups.forEach((grp, t) => {
        const [bar, beat] = at(bs, t);
        const c = harm(bar, beat, rh), orig = grp[0].chord ? CHI[grp[0].chord] : null;
        if (!orig || c === orig) { out.push(...grp); return; }
        const notes = grp.map(e => e.n).sort((a, b) => a - b);
        const v = voice(c.pcs, notes, notes[0] - 3, notes[notes.length - 1] + 4, notes.length);
        v.forEach((n, i) => out.push(Object.assign({}, grp[Math.min(i, grp.length - 1)], { n })));
      });
      return out;
    };
    const bassFollow = rh => (ev, bs) => (!rh || !rh.turn) ? ev : ev.map(e => {
      const [bar, beat] = at(bs, e.t - 70);
      if (lb(bar) !== 3 || beat < 2) return e;
      let d = (((DOM.root - beatChord[12 + beat].root) % 12) + 12) % 12; if (d > 6) d -= 12;
      let n = e.n + d; if (n < 26) n += 12; if (n > 55) n -= 12;
      return Object.assign({}, e, { n });
    });
    const bassRange = n => { while (n < 26) n += 12; while (n > 52) n -= 12; return n; };
    const bfill = rh => (ev, bs) => {
      const cut = 3 * BAR + P16(12) - 5;
      const keep = ev.filter(e => e.t < cut).map(e => (e.t + e.d > cut ? Object.assign({}, e, { d: Math.max(30, cut - e.t - 10) }) : e));
      const nr = harm(bs + 4, 0, rh).root;
      let tgt = 24 + nr; if (tgt < 31) tgt += 12;
      const last = keep.length ? keep[keep.length - 1].n : tgt;
      let dir = last > tgt ? 1 : -1;
      let run = [4, 3, 2, 1].map(k => shiftDeg(tgt, dir * k));
      if (run.some(n => n < 26 || n > 55)) { dir = -dir; run = [4, 3, 2, 1].map(k => shiftDeg(tgt, dir * k)); }
      run.forEach((n, i) => keep.push({ t: 3 * BAR + P16(12 + i), d: P16(13 + i) - P16(12 + i) - 12, n: bassRange(n), v: 88 + i * 7, legato: g === 'sg' }));
      return keep;
    };
    const approach = rh => (ev, bs) => {
      const out = ev.slice();
      [1, 3].forEach(bi => {
        const t = bi * BAR + P16(15);
        if (ev.some(e => e.t >= t - 30 && e.t < (bi + 1) * BAR)) return;
        const prev = sortT(ev.filter(e => e.t < t)).pop();
        if (!prev || prev.t + prev.d > t - 20) return;
        const nr = harm(bs + bi + 1, 0, rh).root;
        const tgt = nearest([24, 36, 48].map(o => o + nr), prev.n);
        out.push({ t, d: BAR - P16(15) - 10, n: bassRange(tgt - 1), v: 82 });
      });
      return out;
    };
    const acid = ev => {
      const s = sortT(ev);
      return s.map((e, i) => {
        const nx = s[i + 1];
        const leg = !!nx && chance(0.45) && nx.t - (e.t + e.d) < 130 && nx.t > e.t;
        const up = chance(0.22) && e.n + 12 <= 55;
        return Object.assign({}, e, { n: e.n + (up ? 12 : 0), v: chance(0.3) ? 122 : Math.min(e.v, 96), legato: leg, d: leg ? nx.t - e.t + 30 : e.d });
      });
    };
    function place(trk, ev4, b0, n, o) {
      o = o || {};
      const L4 = 4 * BAR, total = n * BAR;
      for (let off = 0, it = 0; off < total; off += L4, it++) {
        let ev = ev4.map(e => Object.assign({}, e));
        const xs = o.xf ? [].concat(o.xf(it) || []) : [];
        xs.forEach(fn => { ev = fn(ev, b0 + off / BAR, it); });
        ev.forEach(e => {
          const t = off + e.t; if (t < 0 || t >= total) return;
          pushN(trk, b0 * BAR + t, Math.min(e.d, total - t), e.n, e.v * (o.vel ? o.vel(t / total) : 1), e.legato ? { legato: true } : null);
        });
      }
    }

    /* ---- generated layers ---- */
    const leadMed = (() => { const a = M.leadA.map(e => e.n).sort((x, y) => x - y); return a[a.length >> 1] || 72; })();
    const ctrCenter = Math.max(60, Math.min(74, leadMed - 8));
    function counterCall(b0, n, rh, vf) {   // short answers in the half-bars the melody leaves open
      const lead = T.lead.filter(e => e.t >= b0 * BAR - 60 && e.t < (b0 + n) * BAR);
      let prev = ctrCenter;
      for (let hb = 0; hb < n * 2; hb++) {
        const bar = b0 + (hb >> 1), half = hb & 1, ts = bar * BAR + half * 960;
        const busy = lead.filter(e => e.t >= ts - 60 && e.t < ts + 960).length;
        if (busy > 1 || (!half && busy > 0) || (!half && chance(0.5))) continue;
        const rhy = pick(CTR_RHY), pos = [];
        for (let s = 0; s < 8; s++) if (rhy[s] === 'x') pos.push(half * 8 + s);
        pos.forEach((p, i) => {
          const t = bar * BAR + P16(p), c = harm(bar, p >> 2, rh);
          const tones = tonesIn(c, ctrCenter - 8, ctrCenter + 8);
          if (!tones.length) return;
          const nn = nearest(tones, prev, prev);
          const end = i + 1 < pos.length ? bar * BAR + P16(pos[i + 1]) : Math.min(bar * BAR + P16(half * 8 + 8), t + 420);
          pushN('counter', t, end - t - 20, nn, (i + 1 === pos.length ? 78 : 66 + R() * 14) * vf);
          prev = nn;
        });
      }
    }
    function harmonyLine(b0, n, rh, vf) {   // a diatonic third under the melody, chord tones on the strong beats
      T.lead.filter(e => e.t >= b0 * BAR && e.t < (b0 + n) * BAR).forEach(e => {
        const [bar, beat] = at(0, e.t - 70);
        let h = shiftDeg(e.n - lift, -2);
        if (e.t % 480 < 40 || e.d >= 240) h = snap(h, harm(bar, beat, rh).pcs, 2);
        pushN('counter', e.t, e.d, h, e.v * 0.72 * vf, e.legato ? { legato: true } : null);
      });
    }
    function stabPhrase(b0, n, rh, vf, set) {
      let prev = null;
      const lo = g === 'mth' ? 55 : 60, hi = g === 'mth' ? 76 : 82;
      for (let i = 0; i < n; i++) {
        const b = b0 + i, rhy = STAB_RHY[g][(set + Math.floor(i / 4)) % STAB_RHY[g].length];
        for (let p = 0; p < 16; p++) {
          if (rhy[p] === '.') continue;
          const c = harm(b, p >> 2, rh), v = voice(c.pcs, prev, lo, hi, 3); prev = v;
          const t = b * BAR + P16(p);
          v.forEach(x => pushN('stabs', t, g === 'mth' ? 220 : 150, x, (13 + (+rhy[p]) * 12.7) * vf * (0.94 + R() * 0.12)));
          if (g === 'mth') { ccSet('stabs', t, 91, 100); ccSet('stabs', t + 600, 91, 35); }
        }
      }
    }
    function voxPhrase(b0, n, rh, vf, set) {
      let last = null;
      for (let i = 0; i < n; i++) {
        const b = b0 + i, rhy = VOX_RHY[g][(set + Math.floor(i / 2)) % VOX_RHY[g].length];
        let k = 0;
        for (let p = 0; p < 16; p++) {
          if (rhy[p] !== 'x') continue;
          const tones = tonesIn(harm(b, p >> 2, rh), 67, 84);
          let nn = tones[(k * 2 + i) % tones.length];
          if (last !== null && Math.abs(nn - last) > 7) nn = nearest(tones, last);
          const nx = rhy.indexOf('x', p + 1), end = nx < 0 ? BAR : P16(nx);
          pushN('vox', b * BAR + P16(p), Math.min(end - P16(p) - 20, 220), nn, (84 + R() * 16) * vf);
          last = nn; k++;
        }
      }
    }
    function sustained(trk, b0, n, rh, vf, lo, hi, max) {   // one voicing per harmonic change, held
      let prev = null, cur = null;
      const flush = () => { if (cur) cur.v.forEach(x => pushN(trk, cur.t, cur.end - cur.t - 30, x, 72 * vf)); cur = null; };
      for (let i = 0; i < n; i++) for (const beat of [0, 2]) {
        const b = b0 + i, c = harm(b, beat, rh), t = b * BAR + beat * 480;
        if (cur && cur.c === c) { cur.end = t + 960; continue; }
        flush();
        const v = voice(c.pcs, prev, lo, hi, max || 4); prev = v;
        cur = { c, t, end: t + 960, v };
      }
      flush();
    }
    function subRoots(b0, n, rh, vf) {
      let cur = null;
      const flush = () => { if (cur) pushN('sub', cur.t, cur.end - cur.t - 30, cur.n, 92 * vf); cur = null; };
      for (let i = 0; i < n; i++) for (const beat of [0, 2]) {
        const b = b0 + i, c = harm(b, beat, rh), t = b * BAR + beat * 480;
        let r = 24 + c.root; if (r < 28) r += 12;
        if (cur && cur.n === r) { cur.end = t + 960; continue; }
        flush(); cur = { t, end: t + 960, n: r };
      }
      flush();
    }
    function arpBar(b, c, rate, pat, vf, lo) {
      const tones = tonesIn(c, lo, lo + 19);
      if (!tones.length) return;
      let seq = tones;
      if (pat === 'down') seq = tones.slice().reverse();
      if (pat === 'updown') seq = tones.concat(tones.slice(1, -1).reverse());
      if (pat === 'broken') { seq = []; for (let i = 0; i + 2 < tones.length; i++) seq.push(tones[i], tones[i + 2]); if (!seq.length) seq = tones; }
      if (pat === 'pedal') { seq = []; tones.slice(1).forEach(x => seq.push(tones[0], x)); if (!seq.length) seq = tones; }
      const per = 16 / rate;
      for (let k = 0; k < rate; k++) {
        const p = k * per, t = b * BAR + P16(p), nx = b * BAR + P16(p + per);
        pushN('arp', t, Math.max(40, (nx - t) * 0.8), seq[k % seq.length], (58 + (k % 4 === 0 ? 26 : 0) + R() * 8) * vf);
      }
    }
    function riser(b0, n) {
      const total = n * BAR;
      let base = 60 + home; if (base > 66) base -= 12;
      for (let i = 0; i < n; i++) {
        const deg = Math.round(i * 7 / Math.max(1, n - 1));
        const note = base + Math.floor(deg / 7) * 12 + sc[deg % 7];
        for (let k = 0; k < 16; k++) { const t = i * BAR + k * 120; pushN('riser', b0 * BAR + t, 100, note + (k % 2 ? 12 : 0), 30 + 92 * t / total); }
      }
      ccRamp('riser', b0, n, 74, 10, 127); ccRamp('riser', b0, n, 11, 70, 127);
    }
    function reeseAuto(b0, n) {   // wobble depth (CC1) and cutoff (CC74) moves, a different one each 4 bars
      for (let ph = 0; ph < n / 4; ph++) {
        const b = b0 + ph * 4, style = pick(['ramp', 'beat', 'half', 'swell']);
        if (style === 'ramp') { ccRamp('bass', b, 2, 1, 0, 110); ccRamp('bass', b + 2, 2, 1, 110, 0); }
        if (style === 'beat') for (let k = 0; k < 16; k++) ccSet('bass', (b + (k >> 2)) * BAR + (k & 3) * 480, 1, k % 2 ? 100 : 10);
        if (style === 'half') for (let k = 0; k < 8; k++) ccSet('bass', (b + (k >> 1)) * BAR + (k & 1) * 960, 1, k % 2 ? 0 : 96);
        if (style === 'swell') ccRamp('bass', b, 4, 1, 0, 120, f => f * f);
        ccRamp('bass', b, 4, 74, 85, 120, f => Math.sin(f * Math.PI));
      }
    }

    /* ---- section types ---- */
    const sec = (name, kind, bars, fn) => { sections.push({ name, bar: B0, bars, kind }); fn(B0, bars); B0 += bars; };
    let B0 = 0;
    function intro(b0, n) {
      const q = n / 4;
      drumPhrase(b0, n, i => {
        const st = Math.floor(i / q), ph = Math.floor(i / 8);
        return {
          base: 'main', kick: st === 0 && (g === 'ukg' || g === 'sg') ? 'none' : 'base', snare: st >= 2,
          hats: st === 0 ? (g === 'ch' ? 'offbeat' : 'eighths') : 'base', percBase: st >= 1,
          lib: st >= 1 ? PC_.slice(0, Math.min(3, st)).map((l, k) => [l, k + ph]) : [],
          ghosts: st >= 3 ? 0.12 : 0, fill: i % 8 === 7 ? (i === n - 1 ? 'big' : pick(FILLW[g])) : null, vel: 0.8 + 0.2 * i / n
        };
      });
      if (n >= 32) {
        place('bass', M.bassA, b0 + 2 * q, 2 * q, { xf: () => [thin], vel: () => 0.85 }); ccRamp('bass', b0 + 2 * q, 2 * q, 74, 30, 85);
        if (g === 'mth') stabPhrase(b0 + 3 * q, q, null, 0.7, 1);
        else { place('chords', M.chordsB, b0 + 3 * q, q, { vel: f => 0.5 + 0.3 * f }); ccRamp('chords', b0 + 3 * q, q, 74, 25, 75); }
      } else {
        place('chords', M.chordsB, b0 + n / 2, n / 2, { vel: f => 0.55 + 0.25 * f }); ccRamp('chords', b0, n, 74, 25, 80);
        if (g === 'sg') { place('bass', M.bassA, b0 + 3 * q, q, { xf: () => [thin] }); ccRamp('bass', b0 + 3 * q, q, 74, 20, 70); }
        voxPhrase(b0 + 3 * q, q, null, 0.7, 2);
      }
    }
    function groove(b0, n, o) {
      grooveDrums(b0, n, o.energy || 2, { crash: i => i === 0 && !!o.crash });
      for (let it = 0; it < n / 4; it++) {
        const xs = [];
        if (it % 2 === 1) xs.push(approach(null));
        if (it === n / 4 - 1) xs.push(bfill(null));
        place('bass', o.bassB && it % 2 ? M.bassB : M.bassA, b0 + it * 4, 4, { xf: () => xs });
      }
      if (o.bassFilter) ccRamp('bass', b0, n, 74, o.bassFilter[0], o.bassFilter[1]);
      if (o.chords !== false) {
        const cf = o.chordsFrom || 0;
        place('chords', M.chordsA, b0 + cf, n - cf, { vel: o.chordsVel });
        ccRamp('chords', b0 + cf, n - cf, 74, o.f0 || 60, o.f1 || 100);
      }
      if (o.lead) place('lead', M.leadB, b0 + (o.leadFrom || 0), n - (o.leadFrom || 0), { vel: () => 0.85, xf: it => (it % 2 ? [orn] : []) });
      if (o.stabs) stabPhrase(b0 + (o.stabsFrom || 0), n - (o.stabsFrom || 0), null, 0.75, o.stabSet || 0);
      if (o.vox) voxPhrase(b0 + n - 8, 8, null, 0.8, 1);
      if (o.counter) counterCall(b0 + n / 2, n / 2, null, 0.8);
    }
    function build(b0, n, o) {
      o = o || {};
      const fourK = g === 'mth' || g === 'ch' || g === 'sg';
      drumPhrase(b0, n, i => ({
        base: 'main', kick: i < n - 2 ? (fourK ? 'four' : 'base') : 'none', snare: false,
        hats: i < n / 2 ? 'base' : 'sixteenths', percBase: i < n - 2, lib: i < n / 2 ? [[PC_[0], 2]] : [], vel: 0.75 + 0.25 * i / n
      }));
      rollT(b0, n, g === 'ch' ? 'CL' : 'SN', 30, 127);
      [[10, 'HT', 100], [11, 'HT', 96], [12, 'MT', 108], [13, 'LT', 116]].forEach(([p, l, v]) => pushD(l, (b0 + n - 1) * BAR + P16(p), v, 'fills'));
      place('chords', M.chordsB, b0, n, { vel: f => 0.5 + 0.5 * f, xf: () => [reharm(o.rh)] }); ccRamp('chords', b0, n, 74, 35, 127);
      place('lead', M.leadA, b0 + n / 2, n / 2, { vel: f => 0.45 + 0.45 * f, xf: () => [thin, snapTo(o.rh)] });
      if (g === 'sg') { place('bass', M.bassB, b0 + n / 2, n / 2, { vel: f => 0.6 + 0.4 * f }); ccRamp('bass', b0 + n / 2, n / 2, 74, 20, 127); ccRamp('bass', b0 + n / 2, n / 2, 1, 0, 100); }
      else if (g === 'mth') place('bass', M.bassA, b0, n / 2, { xf: () => [thin] });
      for (let i = 0; i < n; i++) arpBar(b0 + i, harm(b0 + i, 0, o.rh), i < n / 2 ? 8 : 16, 'up', 0.35 + 0.6 * i / n, 60 + Math.floor(i * 12 / n));
      ccRamp('arp', b0, n, 74, 30, 127);
      const sb = b0 + n - 2, tones = tonesIn(harm(sb, 0, o.rh), 69, 81);
      for (let i = 0; i < 2; i++) for (let p = 0; p < 16; p++) pushN('vox', (sb + i) * BAR + P16(p), 80, tones[(p >> 2) % Math.max(1, tones.length)], 48 + 74 * (i * 16 + p) / 32);
      riser(b0, n);
      fx('revcrash', (b0 + n - 1) * BAR, BAR, 104);
      gap(b0 + n);
    }
    function drop(b0, n, o) {
      fx('impact', b0 * BAR, BAR, 112);
      if (g === 'ukg' || g === 'sg') fx('subdrop', b0 * BAR, 2 * BAR, 104);
      grooveDrums(b0, n, o.energy, {
        base: ph => ((ph + (o.varFirst ? 1 : 0)) % 2 ? 'var' : 'main'), kick: o.four || null, noFillLast: !!o.noFillLast,
        fill: o.lastFill ? (i, ph) => (i === n - 1 ? o.lastFill : fillAt(ph, i)) : null
      });
      for (let it = 0; it < n / 4; it++) {
        const src = Math.floor(it / 2) % 2 === 0 ? M.bassA : M.bassB, xs = [bassFollow(o.rh)];
        if (it % 2 === 1) xs.push(approach(o.rh));
        if (it % 4 === 3 || it === n / 4 - 1) xs.push(bfill(o.rh));
        if (o.energy >= 4 && it % 2 === 0) xs.push(octPop);
        place('bass', src, b0 + it * 4, 4, { xf: () => xs });
      }
      if (g === 'sg') reeseAuto(b0, n); else ccSet('bass', b0 * BAR, 74, 112);
      place('chords', M.chordsA, b0, n, { xf: () => [reharm(o.rh)] }); ccSet('chords', b0 * BAR, 74, 112);
      for (let it = 0; it < n / 4; it++) {
        const xs = [snapTo(o.rh)];
        if (it % 4 === 1) xs.push(orn);
        if (it % 4 === 3) xs.push(answer(2, o.rh));
        if (o.energy >= 4 && it % 2 === 1) xs.push(push);
        place('lead', M.leadA, b0 + it * 4, 4, { xf: () => xs });
      }
      const cf = o.counterFrom === undefined ? n / 2 : o.counterFrom;
      if (o.counter === 'call') counterCall(b0 + cf, n - cf, o.rh, 0.9);
      if (o.counter === 'harmony') harmonyLine(b0, n, o.rh, 1);
      const sf = o.stabsFrom || 0;
      stabPhrase(b0 + sf, n - sf, o.rh, 0.85, o.stabSet || 0);
      voxPhrase(b0 + n / 2, n / 2, o.rh, 0.9, o.voxSet || 0);
      if (g === 'sg' || g === 'ch') subRoots(b0, n, o.rh, 1);
      if (o.pad) sustained('pad', b0, n, o.rh, 0.6, 55, 79);
      if (g === 'mth' && o.energy >= 4) for (let i = 0; i < n; i++) arpBar(b0 + i, harm(b0 + i, 0, o.rh), 16, ['up', 'broken', 'updown', 'pedal'][Math.floor(i / 4) % 4], 0.5, 72);
    }
    function switchSec(b0, n, o) {
      grooveDrums(b0, n, 3, { base: ph => (ph % 2 ? 'main' : 'var'), kick: o.four ? () => 'four' : null, hats: ph => o.hats || hatsAt(ph + 1), crash: i => i === 0 });
      for (let it = 0; it < n / 4; it++) {
        const xs = [approach(null)];
        if (it % 2 === 1) xs.push(bfill(null));
        if (o.octPop) xs.push(octPop);
        place('bass', M.bassB, b0 + it * 4, 4, { xf: () => xs });
      }
      if (g === 'sg') reeseAuto(b0, n);
      place('chords', M.chordsB, b0, n, { vel: () => 0.9 });
      place('lead', M.leadB, b0, n / 2, { xf: () => [orn] });
      place('lead', M.leadA, b0 + n / 2, n / 2, { xf: () => [answer(2, null)] });
      counterCall(b0, n, null, 0.85);
      stabPhrase(b0, n, null, 0.85, 2);
      if (g === 'sg' || g === 'ch') subRoots(b0, n, null, 0.9);
      if (o.toGap) { rollT(b0 + n - 2, 2, 'SN', 40, 127); fx('revcrash', (b0 + n - 1) * BAR, BAR, 104); riser(b0 + n - 4, 4); gap(b0 + n); }
    }
    function breakdownSec(b0, n) {
      const bd = kit.song.bd.map(([nm, v]) => chordFromName(nm, v.split(' '), tr));
      const per = 2, bdAt = b => bd[Math.floor((b - b0) / per) % bd.length];
      fx('downlifter', b0 * BAR, 2 * BAR, 100);
      drumPhrase(b0, n, i => ({ base: 'main', kick: 'none', snare: false, hats: i >= n / 2 ? 'eighths' : 'none', percBase: false, lib: [[PC_[0], 1]], vel: 0.45 + 0.3 * i / n }));
      for (let k = 0; k < n / per; k++) {
        const c = bdAt(b0 + k * per), t = (b0 + k * per) * BAR, len = per * BAR;
        c.voicing.forEach(x => pushN('pad', t, len - 40, x, 74));
        let r = c.voicing[0]; while (r > 40) r -= 12; while (r < 28) r += 12;
        pushN(g === 'sg' || g === 'ch' ? 'sub' : 'bass', t, len - 60, r, 84);
        const top = c.voicing[c.voicing.length - 1], tv = top + 12 <= 86 ? top + 12 : top;
        if (k % 2 === 1 || k >= n / per / 2) pushN('vox', t + P16(2), BAR - P16(2) - 60, tv, 80);
        if (g === 'ch') {   // house piano comping on the breakdown chords
          const v = voice(c.pcs, c.voicing.slice(1), 55, 77, 4);
          for (let bb = 0; bb < per; bb++) {
            const rhy = PIANO_RHY[(k + bb) % PIANO_RHY.length];
            for (let p = 0; p < 16; p++) if (rhy[p] !== '.') v.forEach(x => pushN('chords', t + bb * BAR + P16(p), 170, x, (13 + (+rhy[p]) * 12.7) * (0.55 + 0.4 * k / (n / per))));
          }
        }
      }
      const pats = ['up', 'updown', 'broken', 'down'];
      for (let i = 0; i < n; i++) arpBar(b0 + i, bdAt(b0 + i), i < n / 2 ? 8 : 16, pats[Math.floor(i / per) % pats.length], 0.6 + 0.4 * i / n, 60);
      let prev = 72;
      for (let i = 0; i < n; i++) {
        const b = b0 + i, tones = tonesIn(bdAt(b), 64, 79), nn = nearest(tones, prev, i % 2 ? prev : null);
        if (i >= n / 4) pushN('counter', b * BAR, BAR - 40, nn, 64 + 20 * i / n);
        prev = nn;
      }
      ccRamp('pad', b0, n, 11, 60, 120); ccRamp('arp', b0, n, 74, 40, 115); ccRamp('counter', b0, n, 11, 50, 110);
      if (g === 'ch') ccRamp('chords', b0, n, 74, 60, 110);
    }
    function bridge(b0, n) {   // the main progression reharmonised and held: no kick, chops and an answering line
      const rh = { sub: true, turn: true };
      fx('downlifter', b0 * BAR, 2 * BAR, 96);
      drumPhrase(b0, n, i => ({ base: 'main', kick: 'none', snare: false, hats: 'offbeat', percBase: true, lib: [[PC_[0], 3]], vel: 0.6 + 0.3 * i / n, fill: i === n - 1 ? 'roll' : null }));
      sustained('chords', b0, n, rh, 0.9, 55, 77, 4);
      sustained('pad', b0, n, rh, 0.6, 60, 84, 3);
      for (let i = 0; i < n; i++) for (const beat of [0, 2]) {
        const c = harm(b0 + i, beat, rh); let r = 24 + c.root; if (r < 28) r += 12;
        pushN(g === 'sg' || g === 'ch' ? 'sub' : 'bass', (b0 + i) * BAR + beat * 480, 900, r, 80);
      }
      voxPhrase(b0, n, rh, 0.8, 3);
      counterCall(b0, n, rh, 0.8);
      riser(b0 + n - 4, 4); fx('revcrash', (b0 + n - 1) * BAR, BAR, 104); gap(b0 + n);
    }
    function dubBreak(b0, n, o) {   // kick, bass and dub stabs with long reverb throws
      drumPhrase(b0, n, i => ({ base: 'main', kick: 'base', snare: false, hats: 'none', percBase: true, lib: [[PC_[1], i >> 2]], vel: 0.95, fill: i === n - 1 ? 'roll' : null }));
      place('bass', M.bassA, b0, n, { xf: () => [thin] });
      if (g === 'sg') reeseAuto(b0, n);
      let prev = null;
      for (let i = 0; i < n; i++) {
        const b = b0 + i, rhy = STAB_RHY.mth[i % STAB_RHY.mth.length];
        for (let p = 0; p < 16; p++) if (rhy[p] !== '.') {
          const c = harm(b, p >> 2, null), v = voice(c.pcs, prev, 55, 76, 3); prev = v;
          const t = b * BAR + P16(p);
          v.forEach(x => pushN('stabs', t, 260, x, 96));
          ccSet('stabs', t, 91, 110); ccSet('stabs', t + 720, 91, 30);
        }
      }
      voxPhrase(b0, n, null, 0.75, 3);
      if (o && o.toGap) { riser(b0 + n - 4, 4); fx('revcrash', (b0 + n - 1) * BAR, BAR, 104); gap(b0 + n); }
    }
    function mthSwitch(b0, n) {   // kick, bass and rims for 8, then the full kit returns
      const h = n / 2;
      drumPhrase(b0, h, i => ({ base: 'main', kick: 'base', snare: false, hats: 'none', percBase: true, lib: [['RIM', i >> 2], [PC_[2], 1]], vel: 1, fill: i === h - 1 ? 'kicks' : null }));
      place('bass', M.bassA, b0, h); ccRamp('bass', b0, h, 74, 60, 115);
      grooveDrums(b0 + h, h, 3, { base: () => 'var', crash: i => i === 0 });
      place('bass', M.bassB, b0 + h, h, { xf: () => [approach(null)] });
      stabPhrase(b0 + h, h, null, 0.8, 1);
      place('lead', M.leadB, b0 + h, h, { xf: () => [orn] });
    }
    function acidSec(b0, n) {
      grooveDrums(b0, n, 3, { hats: () => ['base', 'ride'], noFillLast: true });
      place('bass', M.bassB, b0, n, { xf: () => [acid] });
      ccRamp('bass', b0, n / 2, 74, 40, 120, f => f * f); ccRamp('bass', b0 + n / 2, n / 2, 74, 120, 55); ccRamp('bass', b0, n, 71, 60, 115);
      stabPhrase(b0 + n / 2, n / 2, null, 0.7, 3);
      rollT(b0 + n - 2, 2, 'SN', 40, 127); riser(b0 + n - 4, 4); fx('revcrash', (b0 + n - 1) * BAR, BAR, 104); gap(b0 + n);
    }
    function bassSolo(b0, n) {
      drumPhrase(b0, n, i => ({ base: 'main', kick: i < n - 2 ? 'base' : 'none', snare: false, hats: 'base', percBase: false, lib: [], vel: 0.9 }));
      place('bass', M.bassA, b0, n, { xf: it => (it ? [bfill(null)] : []) });
      ccRamp('bass', b0, n, 74, 30, 127); ccRamp('bass', b0, n, 1, 0, 110);
      rollT(b0 + n / 2, n / 2, 'SN', 40, 127);
      riser(b0, n); fx('revcrash', (b0 + n - 1) * BAR, BAR, 104); gap(b0 + n);
    }
    function outro(b0, n) {
      const h = n / 2;
      grooveDrums(b0, h, 3, { crash: i => i === 0 });
      place('bass', M.bassA, b0, h, { vel: f => 1 - 0.25 * f }); ccRamp('bass', b0, h, 74, 110, 50);
      place('chords', M.chordsA, b0, h / 2, { vel: f => 0.9 - 0.4 * f }); ccRamp('chords', b0, h / 2, 74, 100, 25);
      stabPhrase(b0, h / 2, null, 0.7, 0);
      drumPhrase(b0 + h, h, i => ({
        base: 'main', kick: 'base', snare: i < h / 2, hats: 'base', percBase: i < h * 3 / 4, lib: i < h / 2 ? [[PC_[0], 0]] : [],
        vel: 1 - 0.15 * i / h, fill: i % 8 === 7 && i < h - 1 ? 'openhat' : null
      }));
    }

    /* ---- the forms ---- */
    if (g === 'ukg') {
      sec('Intro (DJ)', 'intro', 16, intro);
      sec('Intro 2', 'groove', 16, (b, n) => groove(b, n, { energy: 2, chordsVel: () => 0.8, f0: 60, f1: 95, stabs: true, stabsFrom: 8 }));
      sec('Verse', 'groove', 16, (b, n) => groove(b, n, { energy: 2, lead: true, vox: true, bassB: true, crash: true }));
      sec('Build', 'build', 8, (b, n) => build(b, n));
      sec('Drop 1', 'drop', 32, (b, n) => drop(b, n, { energy: 3, counter: 'call', stabsFrom: 16 }));
      sec('Switch (bass lick)', 'switch', 16, (b, n) => switchSec(b, n, { hats: 'shuffle' }));
      sec('Breakdown', 'breakdown', 16, breakdownSec);
      sec('Build 2', 'build', 8, (b, n) => build(b, n, { rh: { sub: true } }));
      sec('Drop 2 (reharmonised)', 'drop', 32, (b, n) => drop(b, n, { energy: 4, rh: { sub: true, turn: true }, counter: 'call', counterFrom: 8, varFirst: true, stabSet: 1, voxSet: 2 }));
      sec('4x4 switch', 'switch', 16, (b, n) => switchSec(b, n, { four: true, octPop: true }));
      sec('Bridge', 'breakdown', 8, bridge);
      sec('Final drop', 'drop', 16, (b, n) => drop(b, n, { energy: 4, rh: { turn: true }, counter: 'harmony', pad: true, stabSet: 3, voxSet: 1 }));
      sec('Outro (DJ)', 'outro', 32, outro);
    } else if (g === 'mth') {
      sec('Intro (DJ)', 'intro', 32, intro);
      sec('Groove', 'groove', 32, (b, n) => groove(b, n, { energy: 2, chords: false, stabs: true, stabsFrom: 16, vox: true, bassFilter: [40, 105] }));
      sec('Main', 'groove', 32, (b, n) => groove(b, n, { energy: 3, lead: true, leadFrom: 8, counter: true, bassB: true, crash: true, f0: 40, f1: 95 }));
      sec('Breakdown', 'breakdown', 16, breakdownSec);
      sec('Build', 'build', 16, (b, n) => build(b, n));
      sec('Drop', 'drop', 32, (b, n) => drop(b, n, { energy: 4, rh: { sub: true }, counter: 'call' }));
      sec('Kick + bass switch', 'switch', 16, mthSwitch);
      sec('Acid', 'switch', 16, acidSec);
      sec('Peak', 'drop', 32, (b, n) => drop(b, n, { energy: 4, rh: { turn: true }, counter: 'harmony', varFirst: true, stabSet: 2, voxSet: 1 }));
      sec('Outro (DJ)', 'outro', 32, outro);
    } else if (g === 'sg') {
      sec('Intro', 'intro', 16, intro);
      sec('Build', 'build', 8, (b, n) => build(b, n));
      sec('Drop 1', 'drop', 32, (b, n) => drop(b, n, { energy: 3, counter: 'call', stabsFrom: 16 }));
      sec('Reese switch', 'switch', 16, (b, n) => switchSec(b, n, {}));
      sec('Breakdown', 'breakdown', 16, breakdownSec);
      sec('Bass solo', 'build', 8, bassSolo);
      sec('Drop 2 (reharmonised)', 'drop', 32, (b, n) => drop(b, n, { energy: 4, rh: { sub: true, turn: true }, counter: 'call', counterFrom: 8, varFirst: true, stabSet: 1, voxSet: 2 }));
      sec('Dub break', 'switch', 8, (b, n) => dubBreak(b, n, {}));
      sec('Build 2', 'build', 8, (b, n) => build(b, n, { rh: { turn: true } }));
      sec('Final drop', 'drop', 32, (b, n) => drop(b, n, { energy: 4, rh: { turn: true }, counter: 'harmony', pad: true, stabSet: 3, voxSet: 1 }));
      sec('Outro', 'outro', 32, outro);
    } else {
      sec('Intro (DJ)', 'intro', 32, intro);
      sec('Bassline', 'groove', 16, (b, n) => groove(b, n, { energy: 2, chordsFrom: 8, f0: 30, f1: 85, stabs: true, stabsFrom: 8, stabSet: 1 }));
      sec('Verse', 'groove', 16, (b, n) => groove(b, n, { energy: 2, lead: true, vox: true, bassB: true, crash: true }));
      sec('Pre-chorus', 'build', 8, (b, n) => build(b, n));
      sec('Chorus', 'drop', 32, (b, n) => drop(b, n, { energy: 3, counter: 'call' }));
      sec('Piano breakdown', 'breakdown', 16, breakdownSec);
      sec('Piano build', 'build', 8, (b, n) => build(b, n, { rh: { sub: true } }));
      sec('Drop (reharmonised)', 'drop', 32, (b, n) => drop(b, n, { energy: 4, rh: { sub: true, turn: true }, counter: 'call', counterFrom: 8, varFirst: true, stabSet: 2, voxSet: 2 }));
      sec('Switch', 'switch', 16, (b, n) => switchSec(b, n, { hats: ['offbeat', 'ride'], toGap: true }));
      sec('Final chorus (key lift +2)', 'drop', 16, (b, n) => {
        lift = 2; keyChanges.push({ t: b * BAR, pc: (home + 2) % 12 });
        drop(b, n, { energy: 4, rh: { turn: true }, counter: 'harmony', pad: true, stabSet: 3, voxSet: 1 });
        lift = 0; keyChanges.push({ t: (b + n) * BAR, pc: home });
      });
      sec('Outro (DJ)', 'outro', 32, outro);
    }

    /* ---- silent eighth before each drop, then tidy every track ---- */
    gaps.forEach(gp => {
      const g0 = gp - 240;
      Object.keys(T).forEach(k => {
        if (k === 'fx') return;
        T[k] = T[k].filter(e => !(e.t >= g0 && e.t < gp));
        T[k].forEach(e => { if (e.t < g0 && e.t + e.d > g0) e.d = Math.max(15, g0 - e.t); });
      });
    });
    const bars = sections.reduce((m, x) => Math.max(m, x.bar + x.bars), 0), L = bars * BAR;
    const MONO = new Set(['bass', 'sub', 'lead', 'counter']);
    Object.keys(T).forEach(k => {
      const ev = T[k].filter(e => e.t < L).sort((a, b) => a.t - b.t || a.n - b.n), out = [];
      const win = EXT_TRACKS.find(tk => tk.key === k).drum ? 15 : 1;
      ev.forEach(e => {   // merge exact duplicates (and drum double-hits within 15 ticks)
        for (let j = out.length - 1; j >= 0 && out[j].t > e.t - win; j--) {
          if (out[j].n === e.n) { out[j].v = Math.max(out[j].v, e.v); out[j].d = Math.max(out[j].d, e.d); return; }
        }
        out.push(e);
      });
      if (MONO.has(k)) {   // one note at a time (a legato note may overlap the next by 30 ticks for the glide)
        const mono = [];
        out.forEach(e => {
          const p = mono[mono.length - 1];
          if (p && p.t === e.t) { if (e.v > p.v) mono[mono.length - 1] = e; return; }
          if (p && p.t + p.d > e.t) p.d = Math.max(15, p.legato ? Math.min(p.d, e.t - p.t + 30) : e.t - p.t - 2);
          mono.push(e);
        });
        out.length = 0; out.push(...mono);
      }
      const lastByPitch = {};
      out.forEach(e => { const p = lastByPitch[e.n]; if (p && p.t + p.d > e.t - 1) p.d = Math.max(10, e.t - p.t - 2); lastByPitch[e.n] = e; });
      out.forEach(e => { if (e.t + e.d > L) e.d = Math.max(10, L - e.t); });
      T[k] = out;
    });
    Object.keys(CC).forEach(k => { CC[k] = CC[k].filter(c => c.t < L).sort((a, b) => a.t - b.t); });
    return { T, CC, sections, bars, lengthTicks: L, keyChanges, extended: true };
  }

  function extendedMidi(kit, targetPc, bpm) {
    const key = keyInfo(targetPc, kit.mode);
    const sg = buildExtended(kit, targetPc);
    const tracks = EXT_TRACKS.map(tk => ({ name: tk.name, events: sg.T[tk.key], ch: tk.ch, program: tk.program, ccs: sg.CC[tk.key] || [] })).filter(t => t.events.length);
    const markers = sg.sections.map(s => ({ t: s.bar * BAR, text: s.name }));
    const keyChanges = sg.keyChanges.map(k => ({ t: k.t, key: keyInfo(k.pc, kit.mode) }));
    const nm = kit.id + '_' + slug(kit.name) + '_ExtendedMix-' + sg.bars + 'bars_' + key.short.replace('#', 's') + '-' + key.camelot + '_' + bpm + 'bpm.mid';
    return { name: nm, data: midiFile(tracks, { bpm, key, name: kit.name + ' extended mix (' + key.label + ', ' + bpm + ' BPM)', lengthTicks: sg.lengthTicks, markers, keyChanges }) };
  }

