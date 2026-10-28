(function () {
'use strict';
const P = window.PACK;
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const ITEMS = MANIFEST.items;
const BY_ID = Object.fromEntries(ITEMS.map(i => [i.id, i]));
const GENRE = { ukg: 'UK Garage', gh: 'Garage House', mth: 'Minimal Tech House', sg: 'Speed Garage', ch: 'Classic House', gv: 'Groove House', th: 'Tech House' };
const GSHORT = { ukg: 'UKG', gh: 'Garage house', mth: 'Minimal', sg: 'Speed garage', ch: 'House', gv: 'Groove house', th: 'Tech house', all: 'All genres' };
const GENRES = ['ukg', 'mth', 'sg', 'ch'];
const KIT_FAMS = ['ukg', 'gh', 'mth', 'sg', 'ch', 'gv', 'th'];   // kit families: garage house (Disclosure-style) plays as UK garage but is listed on its own
const STYLE_OF = { gh: 'Disclosure-style', gv: 'Chris Stussy-style', th: 'Chris Lake & Riordan-style' };   // families written in the spirit of an artist (all original material)
const NEW_FAMS = new Set(['gv', 'th']);   // families whose sounds carry .fam and stay out of the other genres' packs
const famTitle = g => GENRE[g] + (STYLE_OF[g] ? ' (' + STYLE_OF[g] + ')' : '');
const famOf = k => k.fam || k.genre;
/* genre skins: the selected genre themes the page and focuses every list; 'all' shows everything in the current kit's skin */
const GLABEL = { ukg: 'UK garage', gh: 'Garage house', mth: 'Minimal', sg: 'Speed garage', ch: 'House', gv: 'Groove house', th: 'Tech house', all: 'All genres' };
const GPACK = { ukg: 'UK garage', gh: 'garage house', mth: 'minimal', sg: 'speed garage', ch: 'house', gv: 'groove house', th: 'tech house' };
const GDESC = {
  ukg: 'Two-step garage the way the pirate stations played it: skipping kicks, shuffled hats, organ stabs, bouncing basslines and chopped, pitched vocals.',
  gh: 'Garage house in the Disclosure mould: four-to-the-floor with 2-step switches, snaps, chord plucks, glide bass and soulful hooks.',
  mth: 'Minimal and tech house for long sets: dry clicks and rims, offbeat hats, dub chords, bleeps and micro-edits that roll for hours.',
  sg: 'Late-90s speed garage: Reese wobbles, sirens, airhorns, offbeat hats and MC shouts over a heavy four-four.',
  ch: 'Classic house from Chicago and New York: piano, organ bass, strings, brass and gospel hooks over a jacking drum machine.',
  gv: 'Groove house in the spirit of Chris Stussy: a bouncy kick, a clap in a big reverb, swung vintage hats, an FM rubber bass, glowing saw chords whose reverb swells in the gaps, and a Juno lead in fifths.',
  th: 'Tech house in the spirit of Chris Lake and Riordan: a fat kick and a hard snare, offbeat toms and rims, a plucky bounce bass, a talking bass, a Moog-style fifth bass, rave stabs, M1-style organ and vocal shots.',
  all: 'Construction kits across seven genres, each with drum, bass, chord and melody MIDI, a full song and an extended mix, plus vocals, bass, keys, loops, FX and drum hits.',
  hw: 'A seasonal set for the party: spooky drops, laughs, a witches\' chant and sung hooks, a set for Murder at Cameron Castle, thunder, a dinner gong, a haunted organ, wolves and bats, with loops in A minor at 128 BPM.'
};
const isGH = it => it.fam === 'gh' || /^UK Garage\/Garage House/.test(it.pack || '');
const itemGenre = it => (it.fam && GENRE[it.fam] ? it.fam : isGH(it) ? 'gh' : it.genre);
const ICON = {
  play: '<svg viewBox="0 0 16 16"><path d="M4 2l10 6-10 6z"/></svg>',
  stop: '<svg viewBox="0 0 16 16"><path d="M3 3h10v10H3z"/></svg>',
  save: '<svg viewBox="0 0 16 16"><path d="M7 1h2v8.2l2.6-2.6 1.4 1.4L8 13 3 8l1.4-1.4L7 9.2z"/><path d="M2 14h12v2H2z"/></svg>',
  star: '<svg viewBox="0 0 16 16"><path d="M8 1.2l2 4.4 4.8.5-3.6 3.2 1 4.7L8 11.6 3.8 14l1-4.7L1.2 6.1 6 5.6z"/></svg>',
  search: '<svg viewBox="0 0 16 16"><path d="M6.8 1.5a5.3 5.3 0 014.2 8.6l3.6 3.6-1.3 1.3-3.6-3.6A5.3 5.3 0 116.8 1.5zm0 1.9a3.4 3.4 0 100 6.8 3.4 3.4 0 000-6.8z"/></svg>',
  spark: '<svg viewBox="0 0 16 16"><path d="M8 0l1.6 4.8L14.4 6.4 9.6 8 8 12.8 6.4 8 1.6 6.4l4.8-1.6zM13 10l.8 2.2 2.2.8-2.2.8L13 16l-.8-2.2-2.2-.8 2.2-.8z"/></svg>',
  comment: '<svg viewBox="0 0 16 16"><path d="M2 2h12a1 1 0 011 1v8a1 1 0 01-1 1H7l-4 3v-3H2a1 1 0 01-1-1V3a1 1 0 011-1zm1 2v6h1.5v1.6L6.6 10H13V4z"/></svg>',
  sliders: '<svg viewBox="0 0 16 16"><path d="M3 1h1.6v3.1a2 2 0 010 3.8V15H3V7.9a2 2 0 010-3.8zM7.2 1h1.6v7.1a2 2 0 010 3.8V15H7.2v-3.1a2 2 0 010-3.8zM11.4 1H13v2.1a2 2 0 010 3.8V15h-1.6V6.9a2 2 0 010-3.8z"/></svg>'
};

/* ---------- small persistence (per-viewer convenience only) ---------- */
const store = {
  get(k, d) { try { const v = localStorage.getItem('ukgmin.' + k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } },
  set(k, v) { try { localStorage.setItem('ukgmin.' + k, JSON.stringify(v)); } catch (e) {} }
};

const S = {
  view: 'kits', kitId: store.get('kit', 'UKG01'), voxFilter: store.get('voxFilter', 'all'), fxFilter: store.get('fxFilter', 'all'),
  hideExplicit: store.get('hideExplicit', false), sel: { type: 'kit' }, playing: false, ks: {},
  genre: store.get('genre', 'all'), lastKit: store.get('lastKit', {}), bkFilter: store.get('bkFilter', 'all'),
  songV: store.get('songV', 'ext') === 'full' ? 'full' : 'ext',
  q: '', fits: store.get('fits', false), crate: new Set(store.get('crate', []))
};
if (S.genre !== 'all' && !KIT_FAMS.includes(S.genre)) S.genre = 'all';
if (!S.lastKit || typeof S.lastKit !== 'object') S.lastKit = {};
const inGenre = it => S.genre === 'all' || it.genre === 'all' || itemGenre(it) === S.genre;
const famTag = () => (S.genre === 'all' ? '' : '_' + GENRE[S.genre].replace(/ /g, '-'));   // "Save these" zips carry the family they were saved from
const songCache = new Map();
function songOf(kit, st, v) {   // full song or extended mix, cached per kit + key
  v = v || S.songV;
  const k = v + kit.id + ':' + st.pc;
  if (!songCache.has(k)) songCache.set(k, v === 'ext' ? P.buildExtended(kit, st.pc) : P.buildSong(kit, st.pc));
  return songCache.get(k);
}
if (!P.KITS.find(k => k.id === S.kitId)) S.kitId = 'UKG01';
const kitOf = id => P.KITS.find(k => k.id === id);
const curKit = () => kitOf(S.kitId);
function ks(kit) {
  kit = kit || curKit();
  if (!S.ks[kit.id]) S.ks[kit.id] = { bpm: kit.bpm, pc: kit.root, vars: { drums: 'main', bass: 'A', chords: 'A', lead: 'A' }, mute: {}, fill: false };
  return S.ks[kit.id];
}

/* ---------- toast ---------- */
let toastT;
function toast(msg, ms = 2600) {
  const t = $('#toast'); t.textContent = msg; t.classList.add('show');
  clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove('show'), ms);
}

/* ---------- downloads capability ---------- */
let DL = null;
(async () => {
  try { if (window.claude && typeof window.claude.use === 'function') DL = await window.claude.use('downloads'); } catch (e) { DL = null; }
  document.documentElement.classList.toggle('can-save', !!DL);
  $('#nosave').classList.add('ready');
})();
let saving = false;
async function offer(filename, parts) {
  if (!DL) { toast("Saving isn't available in this view."); return; }
  try {
    const r = await DL.save({ filename, data: new Blob(parts, { type: 'application/zip' }) });
    if (!r || r.status !== 'delivered') toast('Saved ' + filename);
  } catch (e) {
    const c = e && e.code;
    if (c === 'declined') toast('Save cancelled.');
    else if (c === 'rate_limited') toast('A save prompt is already open. Finish that one first.');
    else if (c === 'rejected_extension' || c === 'extension_not_enabled') toast("This view can't save .zip files.");
    else if (c === 'too_large') toast('Too large for this device. Save the sections one at a time.');
    else if (['unavailable', 'not_granted', 'capability_disabled', 'capability_removed'].includes(c)) { DL = null; document.documentElement.classList.remove('can-save'); toast("Saving isn't available in this view."); }
    else toast("Couldn't save the file.");
  }
}
/* vocals and the wave-3/4 samples live in FLAC files (vocals: wet left, wet right, dry; most files hold several sounds, side by
   side in channels or one after another in time): an item's file is 'flac:<path>?ch=<channels>&n=<frames>[&o=<first frame>]', decoded by flacDecode
   (bit-exact) and handed out as a standalone 16-bit WAV. The last few decoded files stay in memory, since a vocal's wet and dry,
   and the sounds packed beside it, come from the same file. */
const flacDec = new Map();
function flacOf(url) { const m = /^flac:([^?]+)\?ch=([\d,]+)&n=(\d+)(?:&o=(\d+))?$/.exec(url || ''); return m ? { path: m[1], ch: m[2].split(',').map(Number), n: +m[3], o: +(m[4] || 0) } : null; }
function flacPcm(path) {
  if (flacDec.has(path)) { const p = flacDec.get(path); flacDec.delete(path); flacDec.set(path, p); return p; }
  const p = fetch(path).then(r => { if (!r.ok) throw new Error('HTTP ' + r.status + ' for ' + path); return r.arrayBuffer(); })
    .then(ab => { const d = flacDecode(new Uint8Array(ab)); return { sr: d.sr, n: d.n, data: d.data.map(a => Int16Array.from(a)) }; });
  p.catch(() => flacDec.delete(path));
  flacDec.set(path, p);
  while (flacDec.size > 12) flacDec.delete(flacDec.keys().next().value);
  return p;
}
async function flacWav(url) {
  const f = flacOf(url), d = await flacPcm(f.path), c = f.ch.length, n = Math.max(0, Math.min(f.n, d.n - f.o)), len = n * 2 * c;
  const out = new Uint8Array(44 + len), v = new DataView(out.buffer), w4 = (o, t) => { for (let i = 0; i < 4; i++) out[o + i] = t.charCodeAt(i); };
  w4(0, 'RIFF'); v.setUint32(4, 36 + len, true); w4(8, 'WAVE'); w4(12, 'fmt '); v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, c, true);
  v.setUint32(24, d.sr, true); v.setUint32(28, d.sr * 2 * c, true); v.setUint16(32, 2 * c, true); v.setUint16(34, 16, true); w4(36, 'data'); v.setUint32(40, len, true);
  const chans = f.ch.map(k => d.data[k]);
  for (let i = 0, o = 44, s0 = f.o; i < n; i++) for (let j = 0; j < c; j++, o += 2) v.setInt16(o, chans[j][s0 + i], true);
  return out;
}
/* decode the shared files behind a set of sounds ahead of time, one after another, so the first tap on a pad plays at once */
const warmQ = [];
let warming = false;
function warmFiles(items) {
  items.forEach(it => { const f = it && flacOf(it.file); if (f && !flacDec.has(f.path) && !warmQ.includes(f.path)) warmQ.push(f.path); });
  if (warming) return;
  warming = true;
  const next = () => { const p = warmQ.shift(); if (!p) { warming = false; return; } (flacDec.has(p) ? Promise.resolve() : flacPcm(p)).catch(() => {}).then(() => setTimeout(next, 120)); };
  setTimeout(next, 60);
}
async function fetchBytes(url) {
  if (String(url).startsWith('flac:')) return flacWav(url);
  const r = await fetch(url);
  if (!r.ok) throw new Error('HTTP ' + r.status + ' for ' + url);
  return new Uint8Array(await r.arrayBuffer());
}
async function packFiles(list, label) { // list: [{name, url}|{name, data}]
  const out = new Array(list.length);
  let done = 0, i = 0;
  const work = async () => {
    while (i < list.length) {
      const k = i++;
      const f = list[k];
      out[k] = { name: f.name, data: f.data || await fetchBytes(f.url) };
      done++;
      if (list.length > 6 && (done % 4 === 0 || done === list.length)) toast(label + ' ' + done + ' of ' + list.length + '…', 4000);
    }
  };
  await Promise.all([work(), work(), work(), work(), work(), work()]);
  return P.makeZipParts(out);
}
async function guarded(fn) {
  if (saving) { toast('Still packing the last archive…'); return; }
  saving = true;
  try { await fn(); } catch (e) { console.error(e); toast("Couldn't build that archive. Try again."); }
  finally { saving = false; }
}

/* ---------- naming helpers ---------- */
const SHARP = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const FLAT = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];
const PCN = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
function trChord(name, semis, flats) {
  const m = /^([A-G])(#|b)?(.*)$/.exec(name);
  const pc = (PCN[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0) + semis + 24) % 12;
  return (flats ? FLAT : SHARP)[pc] + (m[3] === '69' ? '6/9' : m[3]);
}
function progression(kit, variation) {
  const bars = kit.chordsPart[variation];
  let carry = null;
  return bars.map(b => {
    const toks = b.trim().split(/\s+/);
    const names = [];
    if (toks[0] === '-' && carry) names.push(carry);
    toks.forEach(t => { if (t[0] === '@') { const n = t.slice(1).split(/[*,~:]/)[0]; if (names[names.length - 1] !== n) names.push(n); carry = n; } });
    if (!names.length && carry) names.push(carry);   // a bar with no new chord (stabs every other bar) still sits on the last one
    return names;
  });
}
function kitFolder(kit, key, bpm) { return kit.id + ' ' + kit.name + ' - ' + bpm + ' BPM ' + key.label + ' (' + key.camelot + ')'; }
function keyText(it) {
  if (it.cat === 'vox' && !it.key) return 'Any key (spoken)';
  if (!it.key) return it.cat === 'loops' || it.cat === 'fills' || it.cat === 'hits' ? 'No key (drums)' : 'No key';
  return it.key;
}

/* =====================================================================
   AUDIO ENGINE
   ===================================================================== */
let ctx = null, master, comp, seqOut, drumBus, musicBus, duck, revIn, dlyIn, dlyNode, sampleBus, noiseBuf, organWave, vocalBus, analyser = null;
const laneOut = {};
function ensureAudio() {
  if (ctx) return ctx;
  const AC = window.AudioContext || window.webkitAudioContext;
  ctx = new AC();
  comp = ctx.createDynamicsCompressor();
  comp.threshold.value = -12; comp.knee.value = 8; comp.ratio.value = 3.5; comp.attack.value = 0.004; comp.release.value = 0.2;
  master = ctx.createGain(); master.gain.value = 0.85;
  master.connect(comp);
  if (typeof ctx.createAnalyser === 'function') {   // the stage visual listens to the master output
    analyser = ctx.createAnalyser(); analyser.fftSize = 2048; analyser.smoothingTimeConstant = 0.7; analyser.minDecibels = -92; analyser.maxDecibels = -16;
    comp.connect(analyser); analyser.connect(ctx.destination);
  } else comp.connect(ctx.destination);
  seqOut = ctx.createGain(); seqOut.connect(master);
  sampleBus = ctx.createGain(); sampleBus.gain.value = 0.9; sampleBus.connect(master);
  drumBus = ctx.createGain(); drumBus.gain.value = 0.85;   // goes to seqOut through its sound-shaping chain (below)
  duck = ctx.createGain(); duck.connect(seqOut);
  vocalBus = ctx.createGain(); vocalBus.gain.value = 0.8; vocalBus.connect(seqOut);
  musicBus = ctx.createGain(); musicBus.gain.value = 0.9; musicBus.connect(duck);
  // reverb
  const conv = ctx.createConvolver(); conv.buffer = makeIR(2.4);
  revIn = ctx.createGain(); const revOut = ctx.createGain(); revOut.gain.value = 0.32;
  revIn.connect(conv); conv.connect(revOut); revOut.connect(duck);
  // dotted-eighth delay
  dlyNode = ctx.createDelay(2.0); const fb = ctx.createGain(); fb.gain.value = 0.38;
  const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 2600;
  const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 300;
  dlyIn = ctx.createGain(); const dlyOut = ctx.createGain(); dlyOut.gain.value = 0.42;
  dlyIn.connect(dlyNode); dlyNode.connect(lp); lp.connect(hp); hp.connect(fb); fb.connect(dlyNode); hp.connect(dlyOut); dlyOut.connect(duck);
  laneFx.drums = fxChain(drumBus, seqOut);
  ['bass', 'chords', 'lead'].forEach(l => { laneOut[l] = ctx.createGain(); laneFx[l] = fxChain(laneOut[l], musicBus); });
  laneOut.bass.gain.value = 0.85; laneOut.chords.gain.value = 0.42; laneOut.lead.gain.value = 0.4;
  // shared noise + organ wave
  noiseBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);
  const nd = noiseBuf.getChannelData(0); for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;
  const real = new Float32Array([0, 1, 0.72, 0.45, 0.3, 0, 0.12, 0, 0.06]);
  organWave = ctx.createPeriodicWave(real, new Float32Array(real.length));
  applyAllLaneFx();
  return ctx;
}
function makeIR(sec) {
  const n = Math.floor(ctx.sampleRate * sec), b = ctx.createBuffer(2, n, ctx.sampleRate);
  for (let c = 0; c < 2; c++) {
    const d = b.getChannelData(c); let lp = 0;
    for (let i = 0; i < n; i++) { lp = lp * 0.55 + (Math.random() * 2 - 1) * 0.45; d[i] = lp * Math.pow(1 - i / n, 2.6); }
  }
  return b;
}
const mtof = m => 440 * Math.pow(2, (m - 69) / 12);
const vg = v => Math.pow(v / 127, 1.35);
function env(p, t, peak, a, d, s, end, r) {
  p.setValueAtTime(0.0001, t);
  p.linearRampToValueAtTime(peak, t + a);
  if (s < 1) p.setTargetAtTime(Math.max(0.0001, peak * s), t + a, Math.max(0.005, d / 3));
  p.setTargetAtTime(0.0001, Math.max(end, t + a + 0.002), Math.max(0.005, r / 3));
}
let OSC_SINK = null;   // audio-rate oscillators of the note being built (pitch bends are applied to their detune)
function O(type, f, t, stop, det) {
  const o = ctx.createOscillator();
  if (type === 'organ') o.setPeriodicWave(organWave); else o.type = type;
  o.frequency.setValueAtTime(f, t); if (det) o.detune.value = det;
  o.start(t); o.stop(stop);
  if (OSC_SINK && f >= 20) OSC_SINK.push([o.detune, det || 0]);
  return o;
}
function applyBends(list, b, t) {   // b: {c0: cents at the note start, pts: [[seconds after start, cents]]}
  list.forEach(([p, base]) => { p.setValueAtTime(base + b.c0, t); b.pts.forEach(([dt, c]) => p.linearRampToValueAtTime(base + c, t + dt)); });
}
function G(v, dst) { const g = ctx.createGain(); g.gain.value = v; if (dst) g.connect(dst); return g; }
function F(type, f, q, dst) { const b = ctx.createBiquadFilter(); b.type = type; b.frequency.value = f; if (q != null) b.Q.value = q; if (dst) b.connect(dst); return b; }
function sends(node, rev, dly) { if (rev) node.connect(G(rev, revIn)); if (dly) node.connect(G(dly, dlyIn)); }
let crushCurve;
function crusher() {   // amplitude bit-crush: a 4-bit staircase
  if (!crushCurve) { crushCurve = new Float32Array(2048); for (let i = 0; i < 2048; i++) { const x = i / 1023.5 - 1; crushCurve[i] = Math.round(x * 7) / 7; } }
  const w = ctx.createWaveShaper(); w.curve = crushCurve; return w;
}
let shaper;
function sat() { if (!shaper) { shaper = new Float32Array(1024); for (let i = 0; i < 1024; i++) { const x = i / 511.5 - 1; shaper[i] = Math.tanh(2.2 * x) / Math.tanh(2.2); } } const w = ctx.createWaveShaper(); w.curve = shaper; return w; }

const glide = { bass: null, lead: null };
function synth(kind, ev, t, out) {
  const f = mtof(ev.note), dur = ev.dur, end = t + dur, v = vg(ev.vel), acc = ev.vel >= 112;
  const g = G(0); g.connect(out);
  let stop = end + 0.6;
  OSC_SINK = ev.bends ? [] : null;
  switch (kind) {
    case 'donk': {   // a short sine that drops into pitch, with a little square bite
      const o = O('sine', f * 2, t, stop); o.frequency.exponentialRampToValueAtTime(f, t + 0.03); o.connect(g);
      O('square', f, t, stop).connect(F('lowpass', 1400, 1, G(0.12, g)));
      env(g.gain, t, 0.75 * v, 0.002, 0.12, 0.0001, Math.min(end, t + 0.12), 0.04); sends(g, 0.12, 0.18); stop = t + 0.5; break;
    }
    case 'brass': {
      const lp = F('lowpass', 700, 1.2, g);
      lp.frequency.setValueAtTime(700, t); lp.frequency.linearRampToValueAtTime(acc ? 4200 : 3200, t + 0.04); lp.frequency.setTargetAtTime(1800, t + 0.05, 0.12);
      [-7, 0, 7].forEach(d => O('sawtooth', f, t, stop, d).connect(lp));
      env(g.gain, t, 0.34 * v, 0.018, 0.25, 0.75, end, 0.08); sends(g, 0.22, 0.08); break;
    }
    case 'orch': {   // orchestra hit: bright stacked saws and a noise burst, fast decay
      const lp = F('lowpass', 7000, 0.8, g);
      lp.frequency.setValueAtTime(7000, t); lp.frequency.setTargetAtTime(1600, t + 0.02, 0.12);
      [-10, 0, 10].forEach(d => O('sawtooth', f, t, stop, d).connect(lp)); O('square', f / 2, t, stop).connect(G(0.3, lp));
      const n = ctx.createBufferSource(); n.buffer = noiseBuf; n.start(t); n.stop(t + 0.08); n.connect(F('bandpass', 2500, 0.8, G(0.22 * v, g)));
      env(g.gain, t, 0.3 * v, 0.002, 0.3, 0.0001, Math.min(end, t + 0.45), 0.12); sends(g, 0.4, 0.05); stop = t + 1.2; break;
    }
    case 'choir': {   // 'aah': detuned saws through three vowel formants, slow attack
      const mix = G(1), l = O('sine', 5, t, stop + 0.8), lg = G(5); l.connect(lg);
      [-9, 0, 9].forEach(d => { const o = O('sawtooth', f, t, stop + 0.8, d); lg.connect(o.detune); o.connect(mix); });
      [[800, 5, 1], [1150, 7, 0.55], [2900, 9, 0.3]].forEach(([fr, q, a]) => mix.connect(F('bandpass', fr, q, G(a * 1.5, g))));
      env(g.gain, t, 0.5 * v, dur < 0.25 ? 0.02 : 0.22, 0.4, 0.9, end, dur < 0.25 ? 0.12 : 0.4); sends(g, 0.5, 0.08); stop = end + 1.2; break;
    }
    case 'talkbox': {   // a saw through a moving vowel (wah), gliding between legato notes
      const o = O('sawtooth', f, t, stop), prev = glide.talk;
      if (prev && prev.legato && Math.abs(prev.end - t) < 0.08) { o.frequency.setValueAtTime(prev.f, t); o.frequency.exponentialRampToValueAtTime(f, t + 0.05); }
      glide.talk = { f, legato: ev.legato, end };
      const a = Math.min(0.12, dur / 2), b1 = F('bandpass', 450, 4, g), b2 = F('bandpass', 1000, 6, G(0.6, g));
      b1.frequency.setValueAtTime(450, t); b1.frequency.linearRampToValueAtTime(1100, t + a); b1.frequency.linearRampToValueAtTime(700, Math.max(t + a + 0.01, end));
      b2.frequency.setValueAtTime(1000, t); b2.frequency.linearRampToValueAtTime(1900, t + a); b2.frequency.linearRampToValueAtTime(1300, Math.max(t + a + 0.01, end));
      o.connect(b1); o.connect(b2);
      env(g.gain, t, 0.95 * v, 0.008, 0.2, 0.85, end, 0.05); sends(g, 0.2, 0.2); break;
    }
    case 'hoover': {   // a spread of detuned saws plus the octave below
      const lp = F('lowpass', 3200, 1, g);
      [-28, -12, 0, 12, 28].forEach(d => O('sawtooth', f, t, stop, d).connect(G(0.3, lp)));
      O('sawtooth', f / 2, t, stop).connect(G(0.25, lp));
      env(g.gain, t, 0.36 * v, 0.01, 0.3, 0.8, end, 0.12); sends(g, 0.3, 0.15); break;
    }
    case 'siren': {
      O('sine', f, t, stop).connect(g); O('square', f, t, stop).connect(F('lowpass', 2400, 1, G(0.18, g)));
      env(g.gain, t, 0.3 * v, 0.02, 0.2, 0.9, end, 0.1); sends(g, 0.45, 0.45); break;
    }
    case 'strings2': {   // the strings layer: short notes are pizzicato
      if (dur < 0.2) {
        const lp = F('lowpass', 2600, 0.8, g);
        [-6, 6].forEach(d => O('triangle', f, t, stop, d).connect(lp)); O('sawtooth', f, t, stop).connect(G(0.25, lp));
        env(g.gain, t, 0.5 * v, 0.003, 0.12, 0.0001, t + 0.14, 0.06); sends(g, 0.35, 0.05); stop = t + 0.8; break;
      }
      const lp = F('lowpass', 4200, 0.7, g);
      [-11, -4, 3, 10].forEach(d => O('sawtooth', f, t, stop + 0.6, d).connect(lp));
      env(g.gain, t, 0.22 * v, dur < 0.5 ? 0.02 : 0.07, 0.4, 0.85, end, 0.35); sends(g, 0.45, 0.12); stop = end + 1.2; break;
    }
    case 'sub': {
      O('sine', f, t, stop).connect(g); O('triangle', f * 2, t, stop).connect(G(0.1, g));
      env(g.gain, t, 0.95 * v, 0.006, 0.3, 0.85, end, 0.05); break;
    }
    case 'reese': {
      const lp = F('lowpass', 480, 3.5, g);
      O('sawtooth', f, t, stop, -14).connect(lp); O('sawtooth', f, t, stop, 14).connect(lp);
      const l = O('sine', 4.3, t, stop); l.connect(G(260, lp.frequency));
      O('sine', f, t, stop).connect(G(0.7, g));
      env(g.gain, t, 0.7 * v, 0.008, 0.3, 0.9, end, 0.06); break;
    }
    case 'square': {
      const lp = F('lowpass', 1500, 2, g);
      lp.frequency.setValueAtTime(1500, t); lp.frequency.setTargetAtTime(420, t + 0.005, 0.05);
      O('square', f, t, stop).connect(G(0.45, lp)); O('sine', f, t, stop).connect(G(0.6, g));
      env(g.gain, t, 0.8 * v, 0.005, 0.25, 0.7, end, 0.05); break;
    }
    case 'ghsq': {   // garage-house square lead: two squares a few cents apart through a plucky low-pass
      const lp = F('lowpass', 2400, 1.4, g);
      lp.frequency.setValueAtTime(acc ? 5600 : 4400, t); lp.frequency.setTargetAtTime(1250, t + 0.004, 0.1);
      O('square', f, t, stop, -7).connect(G(0.5, lp)); O('square', f, t, stop, 7).connect(G(0.5, lp));
      env(g.gain, t, 0.42 * v, 0.003, 0.2, 0.5, end, 0.07); sends(g, 0.24, 0.3); break;
    }
    case 'pluck': {
      const lp = F('lowpass', 1800, 2.5, g);
      lp.frequency.setValueAtTime(acc ? 2600 : 1700, t); lp.frequency.setTargetAtTime(200, t + 0.004, 0.035);
      O('sawtooth', f, t, stop).connect(G(0.55, lp)); O('sine', f, t, stop).connect(G(0.55, g));
      env(g.gain, t, 0.85 * v, 0.004, 0.22, 0.55, end, 0.04); break;
    }
    case 'rubber': {
      const lp = F('lowpass', 900, 1, null); const w = sat(); lp.connect(w); w.connect(g);
      const o = O('sine', f * 2.2, t, stop); o.frequency.setValueAtTime(f * 2.2, t); o.frequency.exponentialRampToValueAtTime(f, t + 0.035); o.connect(lp);
      O('square', f, t, stop).connect(G(0.12, lp));
      env(g.gain, t, 0.75 * v, 0.003, 0.2, 0.6, end, 0.05); break;
    }
    case 'acid': case 'acidhi': {
      const lp = F('lowpass', 300, kind === 'acid' ? 13 : 10, null); const w = sat(); lp.connect(w); w.connect(g);
      const o = ctx.createOscillator(); o.type = 'sawtooth';
      const prev = glide[ev.lane];
      if (prev && prev.legato && Math.abs(prev.end - t) < 0.08) { o.frequency.setValueAtTime(prev.f, t); o.frequency.exponentialRampToValueAtTime(f, t + 0.06); }
      else o.frequency.setValueAtTime(f, t);
      o.start(t); o.stop(stop); o.connect(lp);
      const base = kind === 'acid' ? 260 : 600, peak = acc ? 3400 : 1700;
      lp.frequency.setValueAtTime(base + peak, t); lp.frequency.setTargetAtTime(base, t + 0.003, acc ? 0.09 : 0.06);
      env(g.gain, t, (acc ? 0.62 : 0.48) * v * 1.4, 0.003, 0.2, 0.85, end, 0.03);
      if (kind === 'acidhi') sends(g, 0.15, 0.25);
      glide[ev.lane] = { f, legato: ev.legato, end }; break;
    }
    case 'organ': case 'organlead': {
      const o = O('organ', f, t, stop);
      if (kind === 'organlead') { const l = O('sine', 5.3, t, stop); l.connect(G(7, o.detune)); }
      o.connect(g);
      const c = ctx.createBufferSource(); c.buffer = noiseBuf; const cb = F('bandpass', 3500, 1.2, G(0.05 * v, g)); c.connect(cb); c.start(t); c.stop(t + 0.012);
      env(g.gain, t, 0.55 * v, 0.003, 0.22, 0.5, end, 0.05);
      sends(g, 0.18, kind === 'organlead' ? 0.2 : 0.12); break;
    }
    case 'rhodes': {
      const car = O('sine', f, t, stop + 1); const mod = O('sine', f, t, stop + 1);
      const mg = G(0, car.frequency); mod.connect(mg);
      mg.gain.setValueAtTime(f * 2.4 * v, t); mg.gain.setTargetAtTime(f * 0.25, t, 0.25);
      const tine = O('sine', f * 7.02, t, t + 0.25); const tg = G(0, g); tine.connect(tg); env(tg.gain, t, 0.05 * v, 0.001, 0.05, 0.0001, t + 0.05, 0.05);
      car.connect(g); const pan = ctx.createStereoPanner(); pan.pan.value = (Math.random() - 0.5) * 0.5;
      g.disconnect(); g.connect(pan); pan.connect(out);
      env(g.gain, t, 0.7 * v, 0.004, 2.2, 0.35, end, 0.25);
      sends(pan, 0.25, 0.12); stop = end + 1; break;
    }
    case 'stab': case 'dubstab': {
      const lp = F('lowpass', 3000, 1.5, g);
      const hi = kind === 'dubstab' ? 2100 : 3200, lo = kind === 'dubstab' ? 380 : 650;
      lp.frequency.setValueAtTime(hi, t); lp.frequency.setTargetAtTime(lo, t + 0.004, 0.05);
      O('sawtooth', f, t, stop, -9).connect(lp); O('sawtooth', f, t, stop, 9).connect(lp);
      env(g.gain, t, 0.42 * v, 0.003, 0.2, 0.4, end, 0.07);
      sends(g, kind === 'dubstab' ? 0.35 : 0.2, kind === 'dubstab' ? 0.55 : 0.2); break;
    }
    case 'darkstab': {
      const lp = F('lowpass', 1150, 1.6, g);
      O('triangle', f, t, stop + 0.4).connect(lp); O('sawtooth', f, t, stop + 0.4, 6).connect(G(0.35, lp));
      env(g.gain, t, 0.5 * v, 0.005, 0.5, 0.55, end, 0.3); sends(g, 0.45, 0.15); stop = end + 1; break;
    }
    case 'pad': {
      const lp = F('lowpass', 1500, 0.8, g);
      O('sawtooth', f, t, stop + 1, -11).connect(lp); O('sawtooth', f, t, stop + 1, 11).connect(lp);
      env(g.gain, t, 0.32 * v, 0.28, 0.6, 0.85, end, 0.6); sends(g, 0.4, 0.1); stop = end + 1.5; break;
    }
    case 'bell': {
      const car = O('sine', f, t, stop + 0.6); const mod = O('sine', f * 3.5, t, stop + 0.6);
      const mg = G(0, car.frequency); mod.connect(mg); mg.gain.setValueAtTime(f * 3 * v, t); mg.gain.setTargetAtTime(0, t, 0.12);
      car.connect(g); O('sine', f * 2, t, stop).connect(G(0.12, g));
      env(g.gain, t, 0.6 * v, 0.002, 0.9, 0.0001, end + 0.25, 0.25); sends(g, 0.3, 0.3); stop = end + 1; break;
    }
    case 'flute': {
      const o1 = O('sine', f, t, stop), o2 = O('triangle', f, t, stop); const l = O('sine', 5.2, t, stop); const lg = G(6); l.connect(lg); lg.connect(o1.detune); lg.connect(o2.detune);
      o1.connect(g); o2.connect(G(0.3, g));
      const n = ctx.createBufferSource(); n.buffer = noiseBuf; n.loop = true; n.start(t); n.stop(stop); n.connect(F('bandpass', 2200, 1.2, G(0.05, g)));
      env(g.gain, t, 0.6 * v, 0.045, 0.2, 0.8, end, 0.09); sends(g, 0.3, 0.2); break;
    }
    case 'saw': {
      const lp = F('lowpass', 2600, 1, g);
      O('sawtooth', f, t, stop, -9).connect(lp); O('sawtooth', f, t, stop, 9).connect(lp);
      env(g.gain, t, 0.38 * v, 0.006, 0.2, 0.7, end, 0.07); sends(g, 0.2, 0.25); break;
    }
    case 'vox': {
      const o = O('sawtooth', f, t, stop); const l = O('sine', 5.5, t, stop); l.connect(G(9, o.detune));
      [[730, 6, 1], [1090, 8, 0.6], [2440, 10, 0.35]].forEach(([fr, q, a]) => o.connect(F('bandpass', fr, q, G(a * 1.6, g))));
      env(g.gain, t, 0.7 * v, 0.02, 0.2, 0.8, end, 0.08); sends(g, 0.3, 0.22); break;
    }
    case 'strings': {
      const lp = F('lowpass', 4200, 0.7, g);
      [-11, -4, 3, 10].forEach(d => O('sawtooth', f, t, stop + 0.6, d).connect(lp));
      env(g.gain, t, 0.24 * v, 0.07, 0.4, 0.85, end, 0.35); sends(g, 0.45, 0.12); stop = end + 1.2; break;
    }
    case 'fmpluck': {   // FM pluck: a saw modulating a sine at the same pitch, the index falling fast (harder = brighter); an octave-up FM layer
      const car = O('sine', f, t, stop + 0.4), mod = O('sawtooth', f, t, stop + 0.4), mg = G(0, car.frequency); mod.connect(mg);
      mg.gain.setValueAtTime(f * (1.2 + 2.2 * v), t); mg.gain.setTargetAtTime(f * 0.3, t, 0.07);
      const car2 = O('sine', f * 2, t, stop), mod2 = O('sawtooth', f * 2, t, stop), mg2 = G(0, car2.frequency); mod2.connect(mg2);
      mg2.gain.setValueAtTime(f * 2.8 * v, t); mg2.gain.setTargetAtTime(0, t, 0.05);
      car.connect(g); car2.connect(G(0.3, g));
      env(g.gain, t, 0.5 * v, 0.002, 0.3, 0.0001, Math.min(end, t + 0.6), 0.12); sends(g, 0.18, 0.22); stop = end + 0.8; break;
    }
    case 'crushlead': {   // the one-note lead: two saws through a low-pass and a bit-crusher (a staircase shaper), with an echo
      const lp = F('lowpass', acc ? 3600 : 2600, 1.2, null), cr = crusher(); lp.connect(cr); cr.connect(g);
      O('sawtooth', f, t, stop, -6).connect(lp); O('sawtooth', f, t, stop, 6).connect(G(0.7, lp));
      env(g.gain, t, 0.34 * v, 0.004, 0.25, 0.7, end, 0.06); sends(g, 0.16, 0.3); break;
    }
    case 'bleep': {
      O('sine', f, t, stop).connect(g); O('square', f * 2, t, stop).connect(G(0.04, g));
      env(g.gain, t, 0.55 * v, 0.001, 0.12, 0.0001, Math.min(end, t + 0.1), 0.04); sends(g, 0.3, 0.4); stop = t + 0.6; break;
    }
    default: { // pluck lead
      const lp = F('lowpass', 3200, 2, g);
      lp.frequency.setValueAtTime(acc ? 4200 : 3200, t); lp.frequency.setTargetAtTime(420, t + 0.003, 0.05);
      O('sawtooth', f, t, stop).connect(lp); O('square', f, t, stop, 7).connect(G(0.4, lp));
      env(g.gain, t, 0.42 * v, 0.002, 0.15, 0.25, end, 0.05); sends(g, 0.22, 0.25);
    }
  }
  if (OSC_SINK) { applyBends(OSC_SINK, ev.bends, t); OSC_SINK = null; }
  setTimeout(() => { try { g.disconnect(); } catch (e) {} }, (stop - ctx.currentTime + 1.5) * 1000);
}

/* ---------- sampled instruments: the pack's own Reese, organ bass and piano ---------- */
const SAMPLER = {
  reesebig: { gain: 0.85, rel: 0.06, list: () => ITEMS.filter(i => /^reese_pv_\d+$/.test(i.id)).map(i => [+i.id.split('_').pop(), i]) },
  organbass: { gain: 0.8, rel: 0.04, list: () => ITEMS.filter(i => i.set === 'organ_bass').map(i => [P.noteNum(i.noteName.replace('Eb', 'Eb').replace('Ab', 'Ab')), i]) },
  piano: { gain: 0.42, rel: 0.12, list: () => ITEMS.filter(i => i.set === 'piano_ms').map(i => [+i.id.split('_').pop(), i]) },
  ghbass: { gain: 0.95, rel: 0.05, loop: true, filt: [2600, 480, 0.09], list: () => ITEMS.filter(i => i.set === 'gh_bass').map(i => [P.noteNum(i.noteName), i]) },   // seamless one-second loops, plucked by a filter envelope
  ghpluck: { gain: 0.55, rel: 0.2, list: () => ITEMS.filter(i => i.set === 'gh_pluck').map(i => [P.noteNum(i.noteName), i]) },
  gvfm: { gain: 0.95, rel: 0.07, list: setList('gv_fm') },   // groove house (Chris Stussy-style)
  gvglow: { gain: 0.42, rel: 0.4, rev: 0.12, swell: 0.55, list: setList('gv_glow') },   // the reverb ducks under each chord and swells in the gaps
  gvfifth: { gain: 0.48, rel: 0.25, rev: 0.16, dly: 0.14, list: setList('gv_fifth') },
  thbounce: { gain: 1.25, rel: 0.05, loop: true, vfilt: { lo: 380, hi: 4580, curve: 3.2, floor: 140, tau: 0.085, q: 5 }, amp: [0.32, 0.4], list: setList('th_bounce') },   // tech house (Chris Lake & Riordan-style)
  thtalk: { gain: 1.05, rel: 0.06, loop: true, talk: { floor: 170, peak: 700, span: 1800, q: 8 }, amp: [0.45, 0.5], list: setList('th_bounce') },
  thfifth: { gain: 1.1, rel: 0.06, dly: 0.08, list: setList('th_fifth') },
  thrave: { gain: 0.5, rel: 0.12, rev: 0.12, list: setList('th_rave') },
  threese: { gain: 0.74, rel: 0.06, loop: true, list: setList('th_reese') }
};
function setList(id) { return () => ITEMS.filter(i => i.set === id).map(i => [P.noteNum(i.noteName), i]); }
let swellBus = null;
function swellReturn() {   // groove-house chords: their own long reverb, whose return is ducked while a chord sounds
  if (!swellBus) {
    const cv = ctx.createConvolver(); cv.buffer = makeIR(2.6);
    const ret = G(1), send = G(1), lvl = G(0.45, duck); ret.connect(lvl); send.connect(cv); cv.connect(F('highpass', 280, 0.7, ret));   // pumps with the kick, like the main reverb
    swellBus = { send, ret, held: 0 };
  }
  return swellBus;
}
const smpBufs = {};
async function loadSampler(kind) {
  const S_ = SAMPLER[kind]; if (!S_ || smpBufs[kind]) return;
  const list = S_.list();
  const bufs = await Promise.all(list.map(([m, it]) => loadBuffer(it.file).then(b => [m, b])));
  smpBufs[kind] = bufs.sort((a, b) => a[0] - b[0]);
}
function sampler(kind, ev, t, out) {
  const set = smpBufs[kind]; if (!set || !set.length) return;
  let best = set[0];
  set.forEach(r => { if (Math.abs(r[0] - ev.note) < Math.abs(best[0] - ev.note)) best = r; });
  const S_ = SAMPLER[kind], src = ctx.createBufferSource(); src.buffer = best[1];
  src.playbackRate.value = Math.pow(2, (ev.note - best[0]) / 12);
  if (S_.loop) { src.loop = true; src.loopStart = 0; src.loopEnd = best[1].duration; }
  const g = G(0, out);
  if (S_.vfilt) {   // a resonant low-pass envelope that opens much further on hard-hit notes (deep and plucky, bouncy when accented)
    const V_ = S_.vfilt, top = V_.lo + (V_.hi - V_.lo) * Math.pow(ev.vel / 127, V_.curve), lp = F('lowpass', top, V_.q, g);
    lp.frequency.setValueAtTime(top, t); lp.frequency.setTargetAtTime(V_.floor, t + 0.003, V_.tau); src.connect(lp);
  } else if (S_.talk) {   // the talking bass: a high-resonance low-pass that opens like a mouth and closes again
    const T_ = S_.talk, pk = T_.peak + T_.span * Math.pow(ev.vel / 127, 2), lp = F('lowpass', T_.floor, T_.q, g);
    lp.frequency.setValueAtTime(T_.floor, t); lp.frequency.setTargetAtTime(pk, t, 0.03); lp.frequency.setTargetAtTime(T_.floor, t + 0.07, 0.2); src.connect(lp);
  } else if (S_.filt) { const lp = F('lowpass', S_.filt[0], 1.3, g); lp.frequency.setValueAtTime(S_.filt[0] * (ev.vel >= 112 ? 1.5 : 1), t); lp.frequency.setTargetAtTime(S_.filt[1], t + 0.004, S_.filt[2]); src.connect(lp); }
  else src.connect(g);
  const v = vg(ev.vel) * S_.gain, end = t + ev.dur, rel = S_.rel;
  g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(v, t + 0.004);
  if (S_.amp) g.gain.setTargetAtTime(v * S_.amp[0], t + 0.006, S_.amp[1]);
  else g.gain.setValueAtTime(v, Math.max(t + 0.005, end));
  g.gain.setTargetAtTime(0.0001, Math.max(t + 0.005, end), rel / 3);
  src.start(t); src.stop(end + rel * 3 + 0.05);
  if (ev.bends && src.detune) applyBends([[src.detune, 0]], ev.bends, t);
  if (kind === 'piano') sends(g, 0.18, 0.06);
  if (S_.rev || S_.dly) sends(g, S_.rev || 0, S_.dly || 0);
  if (S_.swell) {   // duck the swell reverb while the chord sounds, let it bloom once it stops
    const B = swellReturn(); g.connect(G(S_.swell, B.send));
    B.ret.gain.setTargetAtTime(0.15, t, 0.01); B.ret.gain.setTargetAtTime(1, end + 0.02, 0.12);
  }
}
function kindsFor(kit) { return [...new Set(['bass', 'chords', 'lead'].map(p => kit.sound[p]).concat(Object.values(EXT_VOICE).map(v => v[famOf(kit)])))].filter(k => SAMPLER[k]); }

/* ---------- drums ---------- */
const LANE_HIT = {
  ukg: { K: 'kick_ukg', SN: 'snare_ukg', CL: 'clap_ukg', RIM: 'rim_ukg', CH: 'chat_ukg', OH: 'ohat_ukg' },
  tech: { K: 'kick_tech', SN: 'snare_tech', CL: 'clap_tech', RIM: 'rim_tech', CH: 'chat_tech', OH: 'ohat_tech' },
  sg: { K: 'kick_sg', SN: 'snare_ukg', CL: 'clap_ukg', RIM: 'rim_ukg', CH: 'chat_ukg', OH: 'ohat_ukg' },
  house: { K: 'kick_house', SN: 'snare_house', CL: 'clap_house', RIM: 'rim_tech', CH: 'chat_house', OH: 'ohat_house' },
  gh: { K: 'kick_gh', SN: 'snare_gh', CL: 'clap_gh', RIM: 'rim_gh', CH: 'chat_gh', OH: 'ohat_gh', SH: 'shaker_gh', TB: 'tamb_gh', SNP: 'snap_gh' },
  gv: { K: 'kick_gv', SN: 'snare_gv', CL: 'clap_gv', RIM: 'rim_gv', CH: 'chat_gv', OH: 'ohat_gv', SH: 'shaker_gv', WB: 'perc_gv' },
  th: { K: 'kick_th', SN: 'snare_th', CL: 'clap_th', RIM: 'rim_th', CH: 'chat_th', OH: 'ohat_th', SH: 'shaker_th', WB: 'perc_th', LT: 'tom_th_lo', MT: 'tom_th_mid', HT: 'tom_th_hi' }
};
const SHARED = { SNP: 'snap_gh', SH: 'shaker', TB: 'tamb', COH: 'conga_hi', CLO: 'conga_lo', BH: 'bongo_hi', BL: 'bongo_lo', CLV: 'clave', WB: 'click', RD: 'ride', CR: 'crash', LT: 'tom_lo', MT: 'tom_mid', HT: 'tom_hi', CB: 'cowbell' };
const MIX = { CB: [0.3, 0.25], K: [1, 0], SN: [0.55, 0], CL: [0.62, 0], SNP: [0.42, 0.18], RIM: [0.36, -0.2], CH: [0.26, 0.25], OH: [0.3, 0.3], SH: [0.22, -0.3], TB: [0.22, 0.4], COH: [0.42, -0.35], CLO: [0.46, -0.2], BH: [0.38, 0.3], BL: [0.4, 0.15], CLV: [0.3, 0.35], WB: [0.34, -0.4], RD: [0.2, 0.3], CR: [0.35, 0], LT: [0.62, -0.3], MT: [0.6, 0], HT: [0.58, 0.3] };
const hitName = (kit, lane) => LANE_HIT[kit.sound.kit][lane] || SHARED[lane];
const bufCache = new Map();
function loadBuffer(url) {
  ensureAudio();
  if (!bufCache.has(url)) {
    const u = String(url);
    const p = (u.startsWith('flac:') ? flacWav(url).then(b => { b.buffer._url = u; return b.buffer; })
      : fetch(url).then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.arrayBuffer(); }))
      .then(ab => new Promise((res, rej) => ctx.decodeAudioData(ab, res, rej)));
    p.catch(() => bufCache.delete(url));
    bufCache.set(url, p);
  }
  return bufCache.get(url);
}
const drumBufs = {};
async function loadKitDrums(kit) {
  const lanes = new Set();
  ['main', 'var', 'fill'].forEach(v => Object.keys(kit.drums[v]).forEach(l => lanes.add(l)));
  lanes.add('CR'); lanes.add('SN'); lanes.add('CL');
  await Promise.all([...lanes].map(async l => { const n = hitName(kit, l); drumBufs[n] = await loadBuffer(BY_ID[n].file); }));
}
let openHat = null;
function drum(kit, lane, vel, t) {
  const name = hitName(kit, lane), buf = drumBufs[name];
  if (!buf) return;
  const src = ctx.createBufferSource(); src.buffer = buf;
  const g = G(MIX[lane][0] * Math.pow(vel / 127, 1.6));
  const pan = ctx.createStereoPanner(); pan.pan.value = MIX[lane][1];
  src.connect(g); g.connect(pan); pan.connect(drumBus);
  src.start(t);
  if (lane === 'OH') openHat = { g, t };
  if (lane === 'CH' && openHat && openHat.t < t) { openHat.g.gain.setTargetAtTime(0, t, 0.006); openHat = null; }
  if (lane === 'K') {
    const fm = famOf(kit), depth = fm === 'th' ? 0.38 : fm === 'gv' ? 0.62 : kit.genre === 'mth' ? 0.45 : fm === 'gh' ? 0.55 : 0.7;   // garage house pumps harder and slower; tech house hardest
    duck.gain.setValueAtTime(1, t); duck.gain.linearRampToValueAtTime(depth, t + 0.008); duck.gain.setTargetAtTime(1, t + 0.03, fm === 'gh' ? 0.1 : fm === 'th' ? 0.09 : 0.07);
  }
}

/* ---------- sequencer ---------- */
const seq = { events: [], loopDur: 1, anchor: 0, cursor: 0, timer: null, kitId: null, mode: 'loop', sections: [], bars: 4 };
const SONG_KIND = { pad: 'pad', riser: 'saw', sub: 'sub', vox: 'vox', donk: 'donk', acid: 'acidhi', piano: 'piano', organ: 'organ', strings: 'strings2', brass: 'brass', orch: 'orch', choir: 'choir', talkbox: 'talkbox', hoover: 'hoover', siren: 'siren', voxguide: 'vox' };
const MUTE_OF = { pad: 'chords', arp: 'lead', riser: 'lead', sub: 'bass', stabs: 'chords', counter: 'lead', vox: 'lead', fx: 'drums',
  donk: 'bass', acid: 'bass', piano: 'chords', organ: 'chords', strings: 'chords', brass: 'chords', orch: 'chords', choir: 'chords', talkbox: 'lead', hoover: 'lead', siren: 'lead', voxguide: 'lead', shouts: 'lead', vocal: 'lead' };
// the break-slice track plays the kit's own hits: slice k of a one-bar break -> [lane, level]
const BREAK_HITS = [[['K', 1], ['CH', 0.7]], [['CH', 0.5]], [['K', 0.85], ['CH', 0.6]], [['CH', 0.5]], [['SN', 1], ['CH', 0.6]], [['CH', 0.5], ['SN', 0.35]], [['CH', 0.6]], [['SN', 0.4], ['CH', 0.5]],
  [['K', 0.95], ['CH', 0.7]], [['CH', 0.5]], [['K', 0.8], ['CH', 0.6]], [['CH', 0.5]], [['SN', 1], ['CH', 0.6]], [['CH', 0.5]], [['SN', 0.4]], [['CH', 0.55], ['SN', 0.35]]];
const NO_CC = new Set(['drums', 'kick', 'snare', 'hats', 'perc', 'fills', 'poly', 'micro', 'breaks', 'fx', 'shouts', 'voxguide', 'sidechain']);
const EXT_VOICE = { stabs: { ukg: 'organ', mth: 'dubstab', sg: 'darkstab', ch: 'organ', gh: 'ghpluck', gv: 'gvglow', th: 'stab' }, counter: { ukg: 'lp', mth: 'bleep', sg: 'lp', ch: 'strings', gh: 'rhodes', gv: 'gvfifth', th: 'fmpluck' } };
const EXT_DRUMS = new Set(['kick', 'snare', 'hats', 'perc', 'fills', 'poly', 'micro']);
const FX_SAMPLE = { impact: (g, fm) => (fm === 'th' ? 'fx8_impact' : 'mth_fx_impact'), revcrash: g => (g === 'ukg' ? 'ukg_fx_revcrash' : 'mth_fx_revcrash'), downlifter: g => (g === 'sg' ? 'sg_fx_reesedown' : 'mth_fx_downsweep'), subdrop: () => 'ukg_fx_subdrop', rewind: () => 'ukg_fx_rewind', tapestop: () => 'mth_fx_tapestop' };
const fxBufs = {};
async function loadSongExtras(kit) {   // the extended mix uses more of the kit (toms, congas, cowbell...) and the pack's FX hits
  if (S.songV !== 'ext') return;
  const sg = songOf(kit, ks(kit)), lanes = new Set();
  EXT_DRUMS.forEach(k => (sg.T[k] || []).forEach(e => lanes.add(e.lane)));
  if (sg.T.breaks && sg.T.breaks.length) ['K', 'SN', 'CH'].forEach(l => lanes.add(l));
  await Promise.all([...lanes].map(async l => { const n = hitName(kit, l); if (BY_ID[n] && !drumBufs[n]) drumBufs[n] = await loadBuffer(BY_ID[n].file); }));
  await Promise.all(Object.keys(FX_SAMPLE).map(async f => { const id = FX_SAMPLE[f](kit.genre, famOf(kit)); if (BY_ID[id] && !fxBufs[id]) fxBufs[id] = await loadBuffer(BY_ID[id].file); }));
  const ids = new Set((sg.vocalCues || []).map(c => c.id).concat(sg.shouts || []));   // the pack's own vocals and shouts
  await Promise.all([...ids].map(async id => { if (BY_ID[id] && !fxBufs[id]) fxBufs[id] = await loadBuffer(BY_ID[id].file); }));
  if (sg.T.piano && sg.T.piano.length) await loadSampler('piano');
}
const liveSrc = new Set();
function playSample(ev, t) {   // a sung hook (tempo-matched) or a shout from the pack
  const buf = fxBufs[ev.sample]; if (!buf) return;
  const src = ctx.createBufferSource(); src.buffer = buf; src.playbackRate.value = ev.rate || 1;
  src.connect(G((ev.lane === 'vocal' ? 0.85 : 0.7) * vg(ev.vel), vocalBus));
  src.start(t); liveSrc.add(src); src.onended = () => liveSrc.delete(src);
}
function stopSamples() { liveSrc.forEach(x => { try { x.stop(); } catch (e) {} }); liveSrc.clear(); }
const TO = {};   // per-track output in song mode: CC11 expression gain into a CC74 low-pass
function outFor(lane) {
  const grp = MUTE_OF[lane] || lane;
  if (seq.mode !== 'song' || !laneOut[grp]) return laneOut[grp];
  if (!TO[lane]) { const f = F('lowpass', 19000, 0.5, laneOut[grp]); TO[lane] = { f, g: G(1, f) }; }
  return TO[lane].g;
}
function applyCC(trk, cc, val, t) {
  if (!outFor(trk) || !TO[trk]) return;
  if (cc === 74) TO[trk].f.frequency.setTargetAtTime(Math.min(19000, 120 * Math.pow(2, val / 127 * 7.3)), t, 0.03);
  else TO[trk].g.gain.setTargetAtTime(Math.pow(val / 127, 1.5), t, 0.03);
}
function resetCC(t) { Object.values(TO).forEach(o => { o.f.frequency.cancelScheduledValues(t); o.f.frequency.setTargetAtTime(19000, t, 0.01); o.g.gain.cancelScheduledValues(t); o.g.gain.setTargetAtTime(1, t, 0.01); }); }
function ccStateAt(time) {   // the filter and expression values in force at a song position (for starts and seeks)
  const now = ctx.currentTime, last = {};
  for (const e of seq.events) { if (e.time > time) break; if (e.lane === 'cc') last[e.trk + '/' + e.cc] = e; }
  resetCC(now);
  Object.values(last).forEach(e => applyCC(e.trk, e.cc, e.val, now + 0.012));
}
function fxHit(kit, ev, t) {
  const id = FX_SAMPLE[ev.fx] && FX_SAMPLE[ev.fx](kit.genre, famOf(kit)), buf = fxBufs[id];
  if (!buf) return;
  const when = ev.fx === 'revcrash' ? t + ev.dur - buf.duration : t;   // the reverse swell ends on the downbeat
  const src = ctx.createBufferSource(); src.buffer = buf;
  src.connect(G(0.5 * vg(ev.vel), drumBus));
  const now = ctx.currentTime;
  if (when >= now) src.start(when); else if (now - when < buf.duration) src.start(now, now - when);
}
function compileSong(kit, st) {
  const sg = songOf(kit, st), spb = 60 / st.bpm, sec = t => t / 480 * spb;
  const ev = [];
  // the pack's hooks play as audio when the key is the original and the tempo is close; otherwise the guide melody plays on a synth voice
  const cues = sg.vocalCues || [], wav = cues.length > 0 && cues.every(c => c.shift === 0 && Math.abs(st.bpm / c.bpm - 1) <= 0.035 && BY_ID[c.id]);
  if (wav) cues.forEach(c => ev.push({ time: sec(c.t), lane: 'vocal', sample: c.id, rate: st.bpm / c.bpm, vel: 110 }));
  Object.keys(sg.T).forEach(tk => {
    const B = sg.BEND && sg.BEND[tk] && sg.BEND[tk].length ? sg.BEND[tk] : null;
    let j = -1;
    sg.T[tk].forEach(e => {
      if (tk === 'sidechain' || (tk === 'voxguide' && wav)) return;
      if (tk === 'drums' || EXT_DRUMS.has(tk)) ev.push({ time: sec(e.t), lane: 'drums', drum: e.lane, vel: e.v });
      else if (tk === 'breaks') (BREAK_HITS[e.n - 36] || []).forEach(([l, f]) => ev.push({ time: sec(e.t), lane: 'drums', drum: l, vel: e.v * f * 0.8 }));
      else if (tk === 'fx') ev.push({ time: sec(e.t), dur: sec(e.d), lane: 'fx', fx: e.fx, vel: e.v });
      else if (tk === 'shouts') ev.push({ time: sec(e.t), lane: 'shouts', sample: sg.shouts[e.n - 48], vel: e.v });
      else {
        const o = { time: sec(e.t), dur: sec(e.d), lane: tk, note: e.n, vel: tk === 'voxguide' ? e.v * 0.75 : e.v, legato: e.legato };
        if (B) {   // the bend in force when the note starts, then each change while it sounds
          while (j + 1 < B.length && B[j + 1].t <= e.t + 2) j++;
          const c0 = j >= 0 ? B[j].v * 1200 / 8192 : 0, pts = [];
          for (let k = j + 1; k < B.length && B[k].t < e.t + e.d + 120; k++) pts.push([sec(B[k].t - e.t), B[k].v * 1200 / 8192]);
          if (c0 || pts.length) o.bends = { c0, pts };
        }
        ev.push(o);
      }
    });
  });
  Object.keys(sg.CC || {}).forEach(tk => { if (!NO_CC.has(tk)) sg.CC[tk].forEach(c => { if (c.cc === 74 || c.cc === 11) ev.push({ time: sec(c.t), lane: 'cc', trk: tk, cc: c.cc, val: c.val }); }); });
  ev.sort((a, b) => a.time - b.time || (a.lane === 'drums' ? -1 : 1));
  return { events: ev, loopDur: sg.bars * 4 * spb, sections: sg.sections, bars: sg.bars, song: sg };
}
function compileEvents(kit, st) {
  const tr = P.transposeFor(kit, st.pc), spb = 60 / st.bpm, sec = t => t / 480 * spb;
  const ev = [];
  let d = P.compileDrums(kit, st.vars.drums).events;
  if (st.fill) d = d.filter(e => e.t < 3 * 1920).concat(P.compileDrums(kit, 'fill').events.map(e => Object.assign({}, e, { t: e.t + 3 * 1920 })));
  d.forEach(e => ev.push({ time: sec(e.t), lane: 'drums', drum: e.lane, vel: e.v }));
  ['bass', 'chords', 'lead'].forEach(part => P.compileMelodic(kit, part, st.vars[part], tr).events.forEach(e =>
    ev.push({ time: sec(e.t), dur: sec(e.d), lane: part, note: e.n, vel: e.v, legato: e.legato })));
  ev.sort((a, b) => a.time - b.time || (a.lane === 'drums' ? -1 : 1));
  return { events: ev, loopDur: 16 * spb };
}
function setDelay(bpm) { if (dlyNode) dlyNode.delayTime.setTargetAtTime(3 * 60 / bpm / 4, ctx.currentTime, 0.02); }
function rebuild() {
  const kit = curKit(), st = ks(kit);
  const c = seq.mode === 'song' ? compileSong(kit, st) : compileEvents(kit, st);
  seq.sections = c.sections || []; seq.bars = c.bars || 4;
  if (seq.timer) {
    const now = ctx.currentTime;
    let frac = ((now - seq.anchor) / seq.loopDur) % 1; if (frac < 0) frac += 1;
    if (seq.kitId !== kit.id) frac = 0;
    seq.anchor = seq.kitId !== kit.id ? now + 0.06 : now - frac * c.loopDur;
    seq.events = c.events; seq.loopDur = c.loopDur;
    const rel = now + 0.03 - seq.anchor;
    seq.cursor = c.events.findIndex(e => e.time >= rel);
    if (seq.cursor < 0) { seq.cursor = 0; seq.anchor += c.loopDur; }
    if (seq.mode === 'song') ccStateAt(Math.max(0, rel));
  } else { seq.events = c.events; seq.loopDur = c.loopDur; }
  seq.kitId = kit.id;
  setDelay(st.bpm);
}
function tick() {
  const kit = curKit(), st = ks(kit), ahead = ctx.currentTime + 0.14;
  for (let guard = 0; guard < 4000 && seq.events.length; guard++) {
    if (seq.cursor >= seq.events.length) { seq.cursor = 0; seq.anchor += seq.loopDur; if (seq.mode === 'song') resetCC(seq.anchor); }
    const ev = seq.events[seq.cursor], t = seq.anchor + ev.time;
    if (t > ahead) break;
    if (ev.lane === 'cc') { applyCC(ev.trk, ev.cc, ev.val, Math.max(t, ctx.currentTime)); seq.cursor++; continue; }
    if (t >= ctx.currentTime - 0.015 && !st.mute[MUTE_OF[ev.lane] || ev.lane]) {
      if (ev.lane === 'drums') drum(kit, ev.drum, ev.vel, t);
      else if (ev.lane === 'fx') fxHit(kit, ev, t);
      else if (ev.lane === 'vocal' || ev.lane === 'shouts') playSample(ev, t);
      else if (labOnLane(ev.lane)) labVoice(ev, t, outFor(ev.lane));
      else {
        const kind = ev.lane === 'arp' ? (kit.genre === 'ukg' || kit.genre === 'ch' ? 'bell' : 'bleep') : EXT_VOICE[ev.lane] ? EXT_VOICE[ev.lane][famOf(kit)] : (SONG_KIND[ev.lane] || kit.sound[ev.lane]);
        if (SAMPLER[kind]) sampler(kind, ev, t, outFor(ev.lane));
        else synth(kind, ev, t, outFor(ev.lane));
      }
    }
    seq.cursor++;
  }
}
async function startLoop(mode) {
  seq.mode = mode || 'loop';
  ensureAudio();
  try { await ctx.resume(); } catch (e) {}
  const kit = curKit();
  setPlayUI('loading');
  try { await loadKitDrums(kit); await Promise.all(kindsFor(kit).map(loadSampler)); if (seq.mode === 'song') await loadSongExtras(kit); } catch (e) { toast("Couldn't load the kit's sounds."); setPlayUI(false); return; }
  if (S.playing) return;
  seq.timer = null; rebuild();
  seq.anchor = ctx.currentTime + 0.08; seq.cursor = 0;
  stopSamples(); if (seq.mode === 'song') ccStateAt(0);
  seqOut.gain.cancelScheduledValues(ctx.currentTime); seqOut.gain.setValueAtTime(1, ctx.currentTime);
  seq.timer = setInterval(tick, 25); tick();
  S.playing = true; setPlayUI(true); frame();
}
function seekBar(bar) {
  if (!S.playing || seq.mode !== 'song') return;
  const t = bar * 4 * 60 / ks().bpm, now = ctx.currentTime;
  seq.anchor = now + 0.05 - t;
  seq.cursor = seq.events.findIndex(e => e.time >= t);
  if (seq.cursor < 0) seq.cursor = 0;
  stopSamples(); ccStateAt(t);
}
function stopLoop() {
  if (seq.timer) clearInterval(seq.timer);
  seq.timer = null; S.playing = false;
  stopSamples();
  if (ctx) seqOut.gain.setTargetAtTime(0, ctx.currentTime, 0.012);
  setPlayUI(false); lcd();
}
async function switchKitWhilePlaying(kit) {
  try { await loadKitDrums(kit); await Promise.all(kindsFor(kit).map(loadSampler)); if (seq.mode === 'song') await loadSongExtras(kit); } catch (e) { toast("Couldn't load the kit's sounds."); }
  if (S.playing) rebuild();
  if (S.playing && cur && cur.sync) { const it = cur.it; stopItem(); playItem(it); }   // re-lock a synced loop to the new kit
}
function songTime(st) { const s = Math.round(songOf(curKit(), st).bars * 4 * 60 / st.bpm); return Math.floor(s / 60) + ':' + String(s % 60).padStart(2, '0'); }

/* ---------- sample player: one-shots play once, loops repeat until stopped ---------- */
let cur = null;
// loops repeat: drum, top and percussion loops, breaks, bass and keys loops, sung hooks, chop loops, chants, textures.
// Fills, risers, impacts and spoken vocals play once.
const isLoop = it => !!it.bars && (it.cat === 'loops' || it.cat === 'bass' || it.cat === 'keys' || it.cat === 'vox' || it.kind === 'texture' || !!it.loop);
// unpitched bar-length sounds can follow the kit's tempo by resampling; pitched ones would go out of tune, so they keep theirs
const canSync = it => !!it.bars && !!it.bpm && !it.key;
let toldLoop = false;
async function playItem(it, opts = {}) {
  ensureAudio();
  try { await ctx.resume(); } catch (e) {}
  if (!opts.overlap && cur && cur.it.id === it.id) { stopItem(); return; }
  let buf;
  const pbs = opts.overlap ? [] : $$('[data-play="' + it.id + '"]');
  pbs.forEach(b => b.classList.add('load'));
  try { buf = await loadBuffer(it.file); } catch (e) { pbs.forEach(b => b.classList.remove('load')); toast("Couldn't load " + it.title + '.'); return; }
  pbs.forEach(b => b.classList.remove('load'));
  if (!opts.overlap) stopItem();
  const src = ctx.createBufferSource(); src.buffer = buf;
  const loop = !opts.overlap && isLoop(it), sync = !opts.overlap && S.playing && canSync(it);
  if (loop) { src.loop = true; src.loopStart = 0; src.loopEnd = buf.duration; }
  const rate = sync ? ks().bpm / it.bpm : 1; src.playbackRate.value = rate;
  const g = G(1, sampleBus); src.connect(g);
  let when = ctx.currentTime + 0.01;
  if (S.playing && !opts.overlap) {   // over a playing kit: one-shots start on the next beat, bar-length sounds on the next bar
    const step = 60 / ks().bpm * (it.bars ? 4 : 1), pos = ctx.currentTime - seq.anchor;
    when = Math.max(when, seq.anchor + Math.ceil(pos / step + 0.02) * step);
  }
  src.start(when);
  if (!loop) { const end = when + buf.duration / rate; g.gain.setValueAtTime(1, Math.max(when, end - 0.012)); g.gain.linearRampToValueAtTime(0, end); }
  if (opts.overlap) return;
  cur = { it, src, g, when, dur: buf.duration / rate, loop, sync };
  if (loop && !toldLoop) { toldLoop = true; toast((sync ? 'Looping in time with ' + curKit().id : 'Looping') + '. Tap again to stop.', 2200); }
  src.onended = () => { if (cur && cur.src === src) { cur = null; markRow(); } };
  markRow(); frame();
}
function retimeCur(bpm) {   // a tempo-synced audition stays locked to the kit when its BPM changes
  if (!cur || !cur.sync) return;
  const now = ctx.currentTime, rate = bpm / cur.it.bpm, dur = cur.src.buffer.duration / rate;
  cur.src.playbackRate.setValueAtTime(rate, now);
  if (now > cur.when) { const ph = (now - cur.when) / cur.dur; cur.when = now - (cur.loop ? ph % 1 : ph) * dur; }
  cur.dur = dur;
}
function stopItem() {
  if (!cur) return;
  try { cur.g.gain.setTargetAtTime(0, ctx.currentTime, 0.01); cur.src.stop(ctx.currentTime + 0.05); } catch (e) {}
  cur = null; markRow();
}

/* ---------- animation: playheads, LCD position, row progress ---------- */
let raf = 0;
function frame() {
  if (raf) return;
  const step = () => {
    raf = 0;
    if (!ctx) return;
    if (S.playing) {
      let frac = ((ctx.currentTime - seq.anchor) / seq.loopDur) % 1; if (frac < 0) frac = 0;
      const box = seq.mode === 'song' ? $$('.songbox') : $$('.rollbox');
      box.forEach(b => { const ph = b.querySelector('.playhead'); if (ph) ph.style.transform = 'translateX(' + (frac * b.clientWidth) + 'px)'; });
      lcdPos(frac);
      const p16 = (ctx.currentTime - seq.anchor) / (15 / ks().bpm);
      lightStep(p16 < 0 ? -1 : Math.floor(p16) % 16);
    }
    if (cur) {
      const el = (ctx.currentTime - cur.when) / cur.dur, p = cur.loop ? (el > 0 ? el % 1 : 0) : Math.min(1, Math.max(0, el));
      $$('[data-row="' + cur.it.id + '"] .prc').forEach(r => r.setAttribute('width', (p * 72).toFixed(2)));
    }
    if (S.playing || cur) raf = requestAnimationFrame(step);
  };
  raf = requestAnimationFrame(step);
}

/* =====================================================================
   STAGE VISUAL: one per genre, listening to the master output while anything plays
   ===================================================================== */
const VZ = { cv: null, g: null, w: 0, h: 0, raf: 0, vis: true, last: 0, t0: performance.now(), bins: new Float32Array(48), fbuf: null, ranges: null, col: {}, hazard: null, dead: false };
const REDUCE = window.matchMedia ? matchMedia('(prefers-reduced-motion: reduce)') : { matches: false };
const STAR = (() => {   // the six-pointed star of the Chicago flag, as a unit-radius polygon
  const pts = [];
  for (let k = 0; k < 12; k++) { const a = -Math.PI / 2 + k * Math.PI / 6, r = k % 2 ? 0.48 : 1; pts.push([Math.cos(a) * r, Math.sin(a) * r]); }
  return pts;
})();
const starPath = (cx, cy, R) => 'M' + STAR.map(([x, y]) => (cx + x * R).toFixed(2) + ' ' + (cy + y * R).toFixed(2)).join('L') + 'Z';
function vizColors() {
  if (!VZ.cv) return;
  const css = getComputedStyle(document.documentElement), C = n => css.getPropertyValue(n).trim();
  VZ.col = { v1: C('--viz-1'), v2: C('--viz-2'), v3: C('--viz-3'), stage: C('--stage'), stage2: C('--stage-2'), ink: C('--stage-ink'), muted: C('--stage-muted'), hi: C('--stage-hi'), line: C('--stage-line') };
  VZ.hazard = null;
  vizKick(true);
}
function rgba(hex, a) {   // '#rrggbb' + alpha -> rgba()
  const m = /^#?([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(hex || '');
  return m ? 'rgba(' + parseInt(m[1], 16) + ',' + parseInt(m[2], 16) + ',' + parseInt(m[3], 16) + ',' + a + ')' : hex;
}
function vizSize() {
  const box = VZ.cv.parentNode, w = box.clientWidth, h = box.clientHeight, dpr = Math.min(2, window.devicePixelRatio || 1);
  if (!w || !h) return;
  VZ.w = w; VZ.h = h;
  VZ.cv.width = Math.round(w * dpr); VZ.cv.height = Math.round(h * dpr);
  VZ.g.setTransform(dpr, 0, 0, dpr, 0, 0);
  vizKick(true);
}
function vizInit() {
  const cv = $('#viz');
  if (!cv || !cv.getContext) return;
  let g = null; try { g = cv.getContext('2d'); } catch (e) {}
  if (!g || typeof g.createLinearGradient !== 'function' || !g.createLinearGradient(0, 0, 1, 1)) return;   // no real canvas (tests, very old browsers)
  VZ.cv = cv; VZ.g = g;
  vizColors();
  if (window.ResizeObserver) new ResizeObserver(vizSize).observe(cv.parentNode);
  vizSize();
  if (window.IntersectionObserver) new IntersectionObserver(es => { VZ.vis = es[0].isIntersecting; vizKick(); }).observe($('#stage'));
  document.addEventListener('visibilitychange', () => vizKick());
  try { REDUCE.addEventListener('change', () => vizKick(true)); } catch (e) {}
}
function vizKick(redraw) {
  if (!VZ.cv || VZ.dead) return;
  if (REDUCE.matches) { if (redraw || S.playing || cur) requestAnimationFrame(() => vizDraw(performance.now())); return; }
  if (!VZ.raf && VZ.vis && !document.hidden) VZ.raf = requestAnimationFrame(vizFrame);
}
function vizFrame(now) {
  VZ.raf = 0;
  if (!VZ.vis || document.hidden || VZ.dead) return;
  const live = !!(analyser && (S.playing || cur));
  if (live || now - VZ.last > 30) { VZ.last = now; vizDraw(now); }   // idle motion runs at about 30 fps
  if (!REDUCE.matches) VZ.raf = requestAnimationFrame(vizFrame);
}
/* 48 log-spaced bands from 40 Hz to 16 kHz, each 0..1 with a slow fall; synthetic swell when nothing plays */
function vizBands(now) {
  const B = VZ.bins, n = B.length, live = !!(analyser && (S.playing || cur));
  if (live) {
    if (!VZ.fbuf) {
      VZ.fbuf = new Uint8Array(analyser.frequencyBinCount);
      const hz = ctx.sampleRate / analyser.fftSize;
      VZ.ranges = [];
      for (let i = 0; i < n; i++) {
        const f0 = 40 * Math.pow(400, i / n), f1 = 40 * Math.pow(400, (i + 1) / n);
        VZ.ranges.push([Math.max(1, Math.floor(f0 / hz)), Math.max(Math.floor(f0 / hz) + 1, Math.ceil(f1 / hz))]);
      }
    }
    analyser.getByteFrequencyData(VZ.fbuf);
    for (let i = 0; i < n; i++) {
      const [a, b] = VZ.ranges[i];
      let m = 0; for (let k = a; k < b && k < VZ.fbuf.length; k++) m = Math.max(m, VZ.fbuf[k]);
      const v = Math.pow(m / 255, 1.6) * (1 + i / n * 0.9);
      B[i] = Math.max(Math.min(1, v), B[i] * 0.88);
    }
  } else {
    const t = now / 1000;
    for (let i = 0; i < n; i++) {
      const v = 0.1 + 0.11 * (0.5 + 0.5 * Math.sin(t * 1.1 + i * 0.42)) * (1 - i / (n * 1.4)) + 0.05 * (0.5 + 0.5 * Math.sin(t * 0.47 - i * 0.21));
      B[i] = B[i] * 0.9 + v * 0.1;
    }
  }
  const avg = (a, b) => { let x = 0; for (let i = a; i < b; i++) x += B[i]; return x / (b - a); };
  return { live, bass: avg(0, 8), mid: avg(8, 28), high: avg(28, 48), level: avg(0, 48) };
}
const bandAt = f => { const B = VZ.bins, x = Math.max(0, Math.min(1, f)) * (B.length - 1), i = Math.floor(x), r = x - i; return B[i] * (1 - r) + (B[Math.min(B.length - 1, i + 1)] || 0) * r; };
function vizClock(now) {   // beats since the kit (or the auditioned loop) started; a 128 BPM idle clock otherwise
  if (ctx && S.playing) { const bpm = ks().bpm; return { beat: (ctx.currentTime - seq.anchor) * bpm / 60, bpm, sec: ctx.currentTime - seq.anchor, on: true }; }
  if (ctx && cur) { const bpm = cur.it.bpm || 128; return { beat: (ctx.currentTime - cur.when) * bpm / 60, bpm, sec: ctx.currentTime - cur.when, on: true }; }
  const sec = (now - VZ.t0) / 1000; return { beat: sec * 128 / 60, bpm: 128, sec, on: false };
}
function vizDraw(now) {
  const g = VZ.g, w = VZ.w, h = VZ.h;
  if (!g || !w || !h) return;
  try {
    g.clearRect(0, 0, w, h);
    const E = vizBands(now), K = vizClock(now);
    E.beat = K.beat; E.step = ((Math.floor(K.beat * 4) % 16) + 16) % 16; E.on = K.on; E.sec = K.sec;
    ({ ukg: drawChrome, gh: drawLagoon, mth: drawGrid, sg: drawSiren, ch: drawVinyl, gv: drawCanal, th: drawLaser, hw: drawHaunt })[themeGenre()](g, w, h, E, now);
  } catch (err) { VZ.dead = true; console.warn('stage visual stopped:', err); }
}
function rr(g, x, y, w, h, r) { r = Math.min(r, w / 2, h / 2); g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }
/* UK garage: chrome capsules, mirrored round the centre line, the loudest band in champagne gold; they bounce on the swung 16th */
function drawChrome(g, w, h, E, now) {
  const C = VZ.col, cy = h * 0.5;
  g.save(); g.translate(w * 0.5, cy); g.scale(w / h, 1);
  const glow = g.createRadialGradient(0, 0, 0, 0, 0, h * 0.5);
  glow.addColorStop(0, rgba(C.stage2, 0.95)); glow.addColorStop(1, rgba(C.stage2, 0));
  g.fillStyle = glow; g.fillRect(-h, -h * 0.5, h * 2, h); g.restore();
  const n = Math.max(12, Math.min(30, Math.round(w / 22))), slot = w / n, bw = Math.max(5, slot * 0.58);
  const shift = Math.sin(now / 1700) * h * 0.06;
  const chrome = g.createLinearGradient(0, cy - h * 0.46 + shift, 0, cy + h * 0.46 + shift);
  chrome.addColorStop(0, C.v2); chrome.addColorStop(0.3, C.v1); chrome.addColorStop(0.47, '#ffffff'); chrome.addColorStop(0.53, C.v2); chrome.addColorStop(0.7, C.v1); chrome.addColorStop(1, C.v2);
  const swing = E.on ? Math.max(0, Math.sin(((E.beat * 2) % 1) * Math.PI)) : 0;
  let top = 0, topV = -1;
  const vals = [];
  for (let i = 0; i < n; i++) { const v = bandAt(i / (n - 1)); vals.push(v); if (v > topV) { topV = v; top = i; } }
  for (let i = 0; i < n; i++) {
    const v = vals[i], hh = h * (0.07 + 0.78 * v) * (1 + (i % 2 ? swing : 0) * 0.05);
    g.fillStyle = E.live && i === top ? C.v3 : chrome;
    rr(g, i * slot + (slot - bw) / 2, cy - hh / 2, bw, hh, bw / 2); g.fill();
  }
  g.fillStyle = rgba(C.v1, 0.18); g.fillRect(0, cy - 0.5, w, 1);
}
/* garage house: a low sun over four lagoon waves; the sun swells with the kick */
function drawLagoon(g, w, h, E, now) {
  const C = VZ.col, sx = w * 0.68, sy = h * 0.42, r = Math.min(w, h) * (0.19 + 0.06 * E.bass);
  const halo = g.createRadialGradient(sx, sy, r * 0.4, sx, sy, r * 2.6);
  halo.addColorStop(0, rgba(C.v3, 0.55)); halo.addColorStop(1, rgba(C.v3, 0));
  g.fillStyle = halo; g.fillRect(0, 0, w, h);
  g.fillStyle = C.v3; g.beginPath(); g.arc(sx, sy, r, 0, Math.PI * 2); g.fill();
  const t = now / 1000, sp = E.on ? 1.4 : 0.6;
  [[C.v2, 0.32, 0.56, 0.8, E.high], [C.v1, 0.42, 0.64, 1.15, E.mid], [C.v2, 0.55, 0.74, 0.65, E.bass], [C.v1, 0.82, 0.86, 1.0, E.level]].forEach(([col, a, base, s, en], li) => {
    const amp = h * (0.025 + 0.13 * en), k = (1.6 + li * 0.7) * Math.PI / w;
    g.beginPath(); g.moveTo(0, h);
    for (let x = 0; x <= w + 6; x += 6) g.lineTo(x, h * base + Math.sin(x * k + t * s * sp + li * 1.7) * amp + Math.sin(x * k * 2.3 - t * 0.8 * s) * amp * 0.3);
    g.lineTo(w, h); g.closePath(); g.fillStyle = rgba(col, a); g.fill();
  });
}
/* minimal: a 16-step grid; each row is a frequency band, the running column is the kit's 16th note */
function drawGrid(g, w, h, E) {
  const C = VZ.col, cols = 16, rows = 6, px = Math.max(10, w * 0.04), py = Math.max(12, h * 0.1);
  const cw = (w - px * 2) / cols, rh = (h - py * 2) / rows, rmax = Math.min(cw, rh) * 0.42;
  for (let r = 0; r < rows; r++) {
    const en = bandAt((rows - 1 - r) / (rows - 1) * 0.92);
    for (let c = 0; c < cols; c++) {
      const x = px + cw * (c + 0.5), y = py + rh * (r + 0.5), age = (E.step - c + 16) % 16;
      let rad = rmax * 0.13, col = rgba(C.v1, c % 4 === 0 ? 0.6 : 0.32);
      if (age === 0) { rad = rmax * (0.3 + 0.7 * Math.min(1, en * 1.5)); col = C.v2; }
      else if (E.live && age < 4) { rad = rmax * (0.14 + 0.5 * en * (1 - age / 4)); col = rgba(C.v2, 0.5 - age * 0.1); }
      g.fillStyle = col; g.beginPath(); g.arc(x, y, Math.max(1.3, rad), 0, Math.PI * 2); g.fill();
    }
  }
  g.fillStyle = rgba(C.v1, 0.45);
  for (let c = 0; c <= cols; c += 4) g.fillRect(px + cw * c - 0.5, h - py * 0.55, 1, py * 0.35);
}
/* speed garage: two siren lamps sweeping red and blue, hazard-taped bars along the floor, a flash on every beat */
function drawSiren(g, w, h, E, now) {
  const C = VZ.col, flash = E.on ? Math.pow(1 - ((E.beat % 1) + 1) % 1, 3) : 0, len = Math.hypot(w, h) * 1.15;
  const a = now / 1000 * (E.on ? 2.6 : 0.9);
  [[w * 0.26, C.v2, 0], [w * 0.74, C.v3, Math.PI]].forEach(([lx, col, off]) => {
    const dir = Math.PI / 2 + Math.sin(a + off) * 1.05;
    const ly = 16, beam = g.createRadialGradient(lx, ly, 0, lx, ly, len);
    beam.addColorStop(0, rgba(col, 0.55 + 0.35 * flash)); beam.addColorStop(0.55, rgba(col, 0.1)); beam.addColorStop(1, rgba(col, 0));
    g.fillStyle = beam; g.beginPath(); g.moveTo(lx, ly); g.arc(lx, ly, len, dir - 0.24, dir + 0.24); g.closePath(); g.fill();
    const lamp = g.createRadialGradient(lx, ly, 0, lx, ly, 26 + 10 * flash);
    lamp.addColorStop(0, rgba(col, 0.9)); lamp.addColorStop(1, rgba(col, 0));
    g.fillStyle = lamp; g.beginPath(); g.arc(lx, ly, 26 + 10 * flash, 0, Math.PI * 2); g.fill();
    g.fillStyle = col; g.beginPath(); g.arc(lx, ly, 7 + 3 * flash, 0, Math.PI * 2); g.fill();
  });
  if (!VZ.hazard) {
    const pc = document.createElement('canvas'); pc.width = pc.height = 16;
    const q = pc.getContext('2d'); q.fillStyle = C.stage; q.fillRect(0, 0, 16, 16); q.fillStyle = C.v1;   // 45-degree tape: x + y (mod 16) < 8
    q.beginPath(); q.moveTo(0, 0); q.lineTo(8, 0); q.lineTo(0, 8); q.closePath(); q.fill();
    q.beginPath(); q.moveTo(16, 0); q.lineTo(16, 8); q.lineTo(8, 16); q.lineTo(0, 16); q.closePath(); q.fill();
    VZ.hazard = g.createPattern(pc, 'repeat');
  }
  const n = Math.max(10, Math.min(26, Math.round(w / 26))), slot = w / n, bw = slot * 0.72;
  g.fillStyle = VZ.hazard;
  for (let i = 0; i < n; i++) { const hh = h * (0.04 + 0.5 * bandAt(i / (n - 1))); g.fillRect(i * slot + (slot - bw) / 2, h - hh, bw, hh); }
}
/* classic house: a record at 33 1/3 with a red label and the four stars of the Chicago flag; the rim glows with the bass */
function drawVinyl(g, w, h, E, now) {
  const C = VZ.col, R = Math.max(30, Math.min(h * 0.44, w * 0.34)), cx = w * 0.52, cy = h * 0.5;
  g.strokeStyle = rgba(C.v2, 0.2 + 0.55 * E.bass); g.lineWidth = 2 + 7 * E.bass;
  g.beginPath(); g.arc(cx, cy, R + 7 + 5 * E.bass, 0, Math.PI * 2); g.stroke();
  g.fillStyle = C.v1; g.beginPath(); g.arc(cx, cy, R, 0, Math.PI * 2); g.fill();
  g.lineWidth = 1;
  for (let r = R * 0.4, k = 0; r < R * 0.97; r += 3, k++) { g.strokeStyle = 'rgba(255,255,255,' + (0.03 + (k % 9 === 0 ? 0.05 : 0) + 0.07 * E.high) + ')'; g.beginPath(); g.arc(cx, cy, r, 0, Math.PI * 2); g.stroke(); }
  if (g.createConicGradient) {   // light catching the grooves stays put while the record turns
    const sh = g.createConicGradient(-0.9, cx, cy);
    [[0, 0], [0.06, 0.13], [0.12, 0], [0.5, 0], [0.56, 0.1], [0.62, 0], [1, 0]].forEach(([o, a]) => sh.addColorStop(o, 'rgba(255,255,255,' + a + ')'));
    g.fillStyle = sh; g.beginPath(); g.arc(cx, cy, R * 0.98, 0, Math.PI * 2); g.arc(cx, cy, R * 0.36, 0, Math.PI * 2, true); g.fill();
  }
  const rot = E.on ? E.sec * (100 / 3) / 60 * Math.PI * 2 : now / 9000;
  g.save(); g.translate(cx, cy); g.rotate(rot);
  g.fillStyle = C.v2; g.beginPath(); g.arc(0, 0, R * 0.34, 0, Math.PI * 2); g.fill();
  g.fillStyle = C.v3;
  for (let k = 0; k < 4; k++) g.fill(new Path2D(starPath((k - 1.5) * R * 0.135, 0, R * 0.055)));
  g.fillStyle = C.stage; g.beginPath(); g.arc(0, 0, Math.max(2, R * 0.022), 0, Math.PI * 2); g.fill();
  g.restore();
  const pvx = cx + R * 1.12, pvy = cy - R * 0.78, tip = E.on ? [cx + R * 0.5, cy + R * 0.12] : [cx + R * 1.18, cy + R * 0.32];
  g.strokeStyle = rgba(C.ink, 0.8); g.lineWidth = Math.max(3, R * 0.03); g.lineCap = 'round';
  g.beginPath(); g.moveTo(pvx, pvy); g.lineTo(tip[0], tip[1]); g.stroke();
  g.fillStyle = C.ink; g.beginPath(); g.arc(pvx, pvy, Math.max(5, R * 0.06), 0, Math.PI * 2); g.fill();
}

/* Halloween: a full moon over a graveyard. Each band is a headstone, the loudest lit like a jack-o'-lantern; fog drifts with
   the mids, the moon swells with the kick, and two bats cross, beating their wings on the 8th notes while something plays */
function drawHaunt(g, w, h, E, now) {
  const C = VZ.col, t = now / 1000;
  const mx = w * 0.7, my = h * 0.3, mr = Math.max(14, Math.min(w, h) * (0.16 + 0.025 * E.bass));
  const halo = g.createRadialGradient(mx, my, mr * 0.7, mx, my, mr * 3.4);
  halo.addColorStop(0, rgba(C.v3, 0.3 + 0.25 * E.bass)); halo.addColorStop(1, rgba(C.v3, 0));
  g.fillStyle = halo; g.fillRect(0, 0, w, h);
  g.fillStyle = C.v3; g.beginPath(); g.arc(mx, my, mr, 0, Math.PI * 2); g.fill();
  g.fillStyle = rgba(C.stage2, 0.16);
  [[-0.38, -0.18, 0.2], [0.24, 0.14, 0.26], [-0.08, 0.46, 0.13], [0.44, -0.4, 0.11]].forEach(([dx, dy, r]) => { g.beginPath(); g.arc(mx + dx * mr, my + dy * mr, r * mr, 0, Math.PI * 2); g.fill(); });
  for (let k = 0; k < 2; k++) {
    const p = (t * (0.055 + 0.025 * k) + k * 0.47) % 1, bx = -30 + p * (w + 60), by = h * (0.16 + 0.14 * k) + Math.sin(p * 11 + k * 2) * h * 0.05;
    drawBat(g, bx, by, 8 + 3 * k, Math.sin((E.on ? E.beat * 2 : t * 2.6) * Math.PI * 2 + k * 1.3));
  }
  [[0.74, 0.07, 0.35, E.mid], [0.83, 0.1, -0.25, E.level]].forEach(([base, a, sp, en], li) => {
    g.beginPath(); g.moveTo(0, h);
    for (let x = 0; x <= w + 8; x += 8) g.lineTo(x, h * base + Math.sin(x * 0.011 + t * sp * 2 + li * 2) * h * (0.025 + 0.06 * en));
    g.lineTo(w, h); g.closePath(); g.fillStyle = rgba(C.v2, a); g.fill();
  });
  const n = Math.max(9, Math.min(22, Math.round(w / 34))), slot = w / n, bw = slot * 0.6, vals = [];
  let top = 0, topV = -1;
  for (let i = 0; i < n; i++) { const v = bandAt(i / (n - 1)); vals.push(v); if (v > topV) { topV = v; top = i; } }
  for (let i = 0; i < n; i++) {
    const v = vals[i], hh = Math.max(bw * 0.7, h * (0.06 + 0.52 * v)), x = i * slot + (slot - bw) / 2, y = h - hh;
    g.beginPath(); g.moveTo(x, h); g.lineTo(x, y + bw / 2); g.arc(x + bw / 2, y + bw / 2, bw / 2, Math.PI, 0); g.lineTo(x + bw, h); g.closePath();
    const lit = E.live && i === top;
    g.fillStyle = lit ? C.v1 : rgba(C.ink, 0.14 + 0.24 * v); g.shadowColor = C.v1; g.shadowBlur = lit ? 18 : 0;
    g.fill();
  }
  g.shadowBlur = 0;
}
/* groove house: lamp light on a canal at night; the bars stand on the water and their reflections ripple with the groove */
function drawCanal(g, w, h, E, now) {
  const C = VZ.col, t = now / 1000, water = h * 0.68, n = 24, gap = w / n;
  const glow = g.createRadialGradient(w * 0.18, water, 0, w * 0.18, water, w * 0.7);
  glow.addColorStop(0, rgba(C.v1, 0.22 + 0.25 * E.bass)); glow.addColorStop(1, rgba(C.v1, 0));
  g.fillStyle = glow; g.fillRect(0, 0, w, h);
  for (let i = 0; i < n; i++) {
    const en = bandAt(i / (n - 1) * 0.9), bh = Math.max(3, (water - h * 0.1) * (0.08 + 0.92 * en)), x = i * gap + gap * 0.22, bw = gap * 0.56;
    g.fillStyle = rgba(i % 3 === 0 ? C.v3 : C.v1, 0.85); g.fillRect(x, water - bh, bw, bh);
    for (let k = 0; k < 7; k++) {   // the reflection: broken strokes that sway
      const y = water + 4 + k * (h - water) / 7, sway = Math.sin(t * (E.on ? 2.4 : 1) + i * 0.7 + k * 1.3) * gap * 0.18 * (1 + k * 0.25);
      g.fillStyle = rgba(C.v1, Math.max(0, 0.42 - k * 0.055) * (0.4 + en)); g.fillRect(x + sway, y, bw * (1 - k * 0.07), Math.max(1, (h - water) / 16));
    }
  }
  g.fillStyle = rgba(C.v2, 0.35); g.fillRect(0, water, w, 1);
}
/* tech house: laser fans from the booth sweeping over the floor, a red horizon line and a strobe on the kick */
function drawLaser(g, w, h, E, now) {
  const C = VZ.col, t = now / 1000, beat = E.on ? Math.pow(1 - ((E.beat % 1) + 1) % 1, 4) : 0, ox = w / 2, oy = h * 0.06;
  if (beat > 0.6) { g.fillStyle = rgba(C.v2, 0.06 * beat); g.fillRect(0, 0, w, h); }
  const fan = 9, spread = 0.9 + 0.5 * E.mid, swing = Math.sin(t * (E.on ? 1.3 : 0.4)) * 0.5;
  g.lineWidth = Math.max(1, w / 700);
  for (let k = 0; k < fan; k++) {
    const a = Math.PI / 2 + swing + (k - (fan - 1) / 2) * spread / fan, L = Math.hypot(w, h);
    const grad = g.createLinearGradient(ox, oy, ox + Math.cos(a) * L, oy + Math.sin(a) * L);
    grad.addColorStop(0, rgba(C.v1, 0.75 * (0.35 + E.level))); grad.addColorStop(1, rgba(C.v1, 0));
    g.strokeStyle = grad; g.beginPath(); g.moveTo(ox, oy); g.lineTo(ox + Math.cos(a) * L, oy + Math.sin(a) * L); g.stroke();
  }
  const n = 32, base = h * 0.92;
  for (let i = 0; i < n; i++) {
    const en = bandAt(i / (n - 1) * 0.92), bh = (base - h * 0.2) * en, x = i * w / n + 1;
    g.fillStyle = rgba(i % 8 === 0 ? C.v1 : C.v2, i % 8 === 0 ? 0.9 : 0.55); g.fillRect(x, base - bh, w / n - 2, bh);
  }
  g.fillStyle = C.v1; g.fillRect(0, base, w, Math.max(2, h * 0.012));
  g.fillStyle = rgba(C.v1, 0.25 + 0.5 * beat); g.fillRect(0, base - h * 0.02, w, h * 0.02);
}
function drawBat(g, x, y, s, flap) {
  const wy = flap * s * 0.65;
  g.fillStyle = '#050208';
  g.beginPath(); g.moveTo(x, y);
  g.quadraticCurveTo(x - s * 0.6, y - s * 0.3 - wy, x - s * 1.6, y - wy);
  g.quadraticCurveTo(x - s * 1.1, y + s * 0.1 - wy * 0.3, x - s * 0.9, y + s * 0.38);
  g.quadraticCurveTo(x - s * 0.5, y + s * 0.15, x, y + s * 0.42);
  g.quadraticCurveTo(x + s * 0.5, y + s * 0.15, x + s * 0.9, y + s * 0.38);
  g.quadraticCurveTo(x + s * 1.1, y + s * 0.1 - wy * 0.3, x + s * 1.6, y - wy);
  g.quadraticCurveTo(x + s * 0.6, y - s * 0.3 - wy, x, y);
  g.fill();
  g.beginPath(); g.moveTo(x - s * 0.2, y + s * 0.08); g.lineTo(x - s * 0.13, y - s * 0.3); g.lineTo(x, y + s * 0.02); g.lineTo(x + s * 0.13, y - s * 0.3); g.lineTo(x + s * 0.2, y + s * 0.08); g.closePath(); g.fill();
}

/* =====================================================================
   UI
   ===================================================================== */
function setPlayUI(state) {
  const b = $('#playBtn');
  $('#playIcon').outerHTML = (state === true ? ICON.stop : ICON.play).replace('<svg', '<svg id="playIcon"');
  $('#playTxt').textContent = state === 'loading' ? 'Wait' : state === true ? 'Stop' : 'Play';
  b.setAttribute('aria-label', state === true ? 'Stop' : 'Play kit loop');
  document.documentElement.style.setProperty('--beat-ms', Math.round(60000 / ks().bpm) + 'ms');
  document.body.classList.toggle('playing', state === true && seq.mode === 'loop');
  document.body.classList.toggle('songplaying', state === true && seq.mode === 'song');
  const sp = $('#songPlay');
  if (sp) sp.innerHTML = (state === true && seq.mode === 'song' ? ICON.stop + 'Stop song' : ICON.play + 'Play song');
  if (state !== true) lightStep(-1);
  vizKick(); sendPresence();
}
/* the deck's 16 step lights and the logo's 4 x 4 grid follow the kit's 16th notes */
const MARK_REST = [0, 3, 6, 8, 13];   // the logo at rest: a 2-step kick pattern
let litStep = -2;
function lightStep(s) {
  if (s === litStep) return;
  litStep = s;
  $$('#steps i').forEach((el, i) => el.classList.toggle('on', i === s));
  $$('.mark i').forEach((el, i) => el.classList.toggle('on', s < 0 ? MARK_REST.includes(i) : i === s));
  $$('.kitcard .pat .cur').forEach(c => c.setAttribute('x', s >= 0 && c.closest('.kitcard').dataset.kit === S.kitId ? s * 10 : -20));   // the playing kit's card follows the beat
}
let lastPos = '';
const SEP = '   ';
const lcdKey = key => key.label + ' ' + key.camelot;
function lcdPos(frac) {
  if (S.sel.type !== 'kit') return;
  const st = ks(), key = P.keyInfo(st.pc, curKit().mode);
  let s;
  if (seq.mode === 'song') {
    const barF = frac * seq.bars, bar = Math.floor(barF) + 1;
    const sc = seq.sections.filter(x => x.bar <= barF).pop();
    s = st.bpm + ' BPM' + SEP + lcdKey(key) + SEP + (sc ? sc.name + SEP : '') + 'Bar ' + bar;
    if ($('#lcd1').dataset.song !== S.songV) { $('#lcd1').textContent = (S.songV === 'ext' ? 'Extended mix: ' : 'Full song: ') + curKit().id + ' ' + curKit().name; $('#lcd1').dataset.song = S.songV; }
  } else {
    const bar = Math.floor(frac * 4) + 1, beat = Math.floor(frac * 16) % 4 + 1;
    s = st.bpm + ' BPM' + SEP + lcdKey(key) + SEP + 'Bar ' + bar + '.' + beat;
  }
  if (s !== lastPos) { $('#lcd2').textContent = s; lastPos = s; }
}
function lcd() {
  const l1 = $('#lcd1'), l2 = $('#lcd2');
  l1.dataset.song = '';
  if (S.sel.type === 'kit') {
    const kit = curKit(), st = ks(kit), key = P.keyInfo(st.pc, kit.mode);
    l1.textContent = kit.id + ' ' + kit.name;
    l2.textContent = st.bpm + ' BPM' + SEP + lcdKey(key) + SEP + '4 bars';
  } else {
    const it = BY_ID[S.sel.id];
    l1.textContent = it.cat === 'vox' && it.group !== 'chops' && it.group !== 'pads' ? '“' + it.title + '”' : it.title;
    const k = it.cat === 'vox' && !it.key ? 'Any key' : it.key ? it.key + (it.camelot ? ' ' + it.camelot : '') : 'No key';
    const len = it.bars ? it.bars + (it.bars > 1 ? ' bars' : ' bar') : it.dur.toFixed(1) + ' s';
    l2.textContent = (it.bpm ? it.bpm + ' BPM' + SEP : '') + k + SEP + len;
  }
  lastPos = '';
}

/* ---------- views ---------- */
const VIEWS = ['kits', 'vocals', 'bass', 'fx', 'hits', 'lab', 'crate', 'halloween'];
const skinFor = v => (v === 'halloween' ? 'hw' : S.genre !== 'all' ? S.genre : famOf(curKit()));
function setView(v, push) {
  const was = S.view;
  S.view = v; store.set('view', v); sendPresence();
  $$('.mode').forEach(b => b.setAttribute('aria-selected', String(b.dataset.view === v)));
  VIEWS.forEach(n => { $('#view-' + n).hidden = n !== v; });
  if (document.documentElement.dataset.genre !== themeGenre()) applyTheme(); else if (v === 'halloween' || was === 'halloween') renderStage();
  if (v === 'lab') labShow();
  if (v === 'halloween') renderHw();
  if (v === 'kits') { S.sel = { type: 'kit' }; lcd(); requestAnimationFrame(drawRolls); updateFitCounts(); }
  if (v === 'vocals' || v === 'bass' || v === 'fx') { renderTools(); ({ vocals: renderVox, bass: renderBK, fx: renderFx })[v](); }
  if (v === 'crate') renderCrate();
  if (push) { try { history.replaceState(null, '', '#' + v); } catch (e) {} }
}

/* ---------- kits ---------- */
/* each kit card carries bar 1 of its main beat as 16 steps: kick = tall block, snare or clap = short block */
const patCache = new Map();
function patSVG(k) {
  if (!patCache.has(k.id)) {
    const kick = new Set(), snare = new Set();
    P.compileDrums(k, 'main').events.filter(e => e.t < 1920).forEach(e => {
      const s = Math.round(e.t / 120) % 16;
      if (e.lane === 'K') kick.add(s); else if (e.lane === 'SN' || e.lane === 'CL') snare.add(s);
    });
    let h = '<svg class="pat" viewBox="0 0 160 12" preserveAspectRatio="none" aria-hidden="true"><rect class="cur" x="-20" y="0" width="10" height="12" rx="2"/>';
    for (let s = 0; s < 16; s++) {
      const x = s * 10 + 1;
      h += kick.has(s) ? '<rect class="k" x="' + x + '" y="0" width="8" height="12" rx="2"/>' : snare.has(s) ? '<rect class="s" x="' + x + '" y="4" width="8" height="5" rx="1.5"/>' : '<rect class="o" x="' + (x + 3) + '" y="5" width="2" height="2" rx="1"/>';
    }
    patCache.set(k.id, h + '</svg>');
  }
  return patCache.get(k.id);
}
function kitCardHTML(k) {
  const key = P.keyInfo(k.root, k.mode);
  return '<button class="kitcard" data-kit="' + k.id + '" aria-pressed="' + (k.id === S.kitId) + '">' +
    '<span class="id">' + k.id + '</span><span class="tag t-' + famOf(k) + '">' + esc(k.style) + '</span>' +
    '<span class="nm">' + esc(k.name) + '</span>' +
    '<span class="mt tnum"><span>' + k.bpm + ' BPM</span><span>' + key.label + '</span><span class="cam">' + key.camelot + '</span>' + (NEW_FAMS.has(k.fam) ? '<span class="newtag">New</span>' : k.ext ? '<span class="newtag">New layers</span>' : '') + '</span>' +
    patSVG(k) + '</button>';
}
function renderKitList() {
  const gs = S.genre === 'all' ? KIT_FAMS : [S.genre];
  let html = '';
  gs.forEach(g => {
    if (S.genre === 'all') html += '<div class="kitgroup"><span class="dot g-' + g + '"></span>' + famTitle(g) + '</div>';
    P.KITS.filter(k => famOf(k) === g).forEach(k => { html += kitCardHTML(k); });
  });
  $('#kitlist').innerHTML = html;
}
/* ---------- genre: stage copy, switcher, theme ---------- */
const themeGenre = () => skinFor(S.view);
/* a view change that changes the skin (into or out of Halloween) wipes it in from the tab or button that was pressed */
function goView(v, ev, push) {
  if (v === S.view) return;
  if (skinFor(v) !== skinFor(S.view)) transition(() => setView(v, push !== false), ev); else setView(v, push !== false);
}
const range = a => { const lo = Math.min(...a), hi = Math.max(...a); return lo === hi ? String(lo) : lo + '–' + hi; };
function renderGenres() {
  $('#genres').innerHTML = KIT_FAMS.concat('all').map(g => '<button class="gbtn" data-genre="' + g + '" aria-pressed="' + (S.genre === g) + '"><span class="sw sw-' + g + '"></span>' + GLABEL[g] +
    ' <span class="n">' + (g === 'all' ? P.KITS.length : P.KITS.filter(k => famOf(k) === g).length) + '</span></button>').join('');
}
function renderStage() {
  const g = S.genre, kits = g === 'all' ? P.KITS : P.KITS.filter(k => famOf(k) === g), hw = S.view === 'halloween';
  $('#hwBtnTxt').textContent = hw ? 'Back to the kits' : 'Halloween sounds';
  $('#hwSave').hidden = !hw; $('#shuffleBtn').hidden = hw;
  if (hw) {
    const all = hwItems();
    $('#gName').textContent = 'Halloween';
    $('#gDesc').textContent = GDESC.hw;
    $('#gFacts').innerHTML = [['Vocals', all.filter(i => i.cat === 'vox').length], ['FX & loops', all.filter(i => i.cat === 'fx').length], ['Castle sets', all.filter(i => ['castle', 'supper', 'zf', 'names', 'drama'].includes(i.hsub) || i.castle).length], ['Loops', 'A minor · 128 BPM']]
      .map(([t, d]) => '<div><dt>' + t + '</dt><dd>' + d + '</dd></div>').join('');
    $('#gpackBtn').hidden = true; $('#saveAll').hidden = true;
    return;
  }
  $('#gName').textContent = g === 'all' ? 'All genres' : GENRE[g];
  $('#gDesc').textContent = GDESC[g];
  const facts = [['Kits', kits.length], ['Tempo', range(kits.map(k => k.bpm)) + ' BPM'], g === 'all' ? ['Genres', KIT_FAMS.length] : ['Swing', range(kits.map(k => Math.round(k.swing * 100))) + '%'],
    ['Sounds', ITEMS.filter(i => i.cat !== 'inst' && !i.hidden && inGenre(i)).length]];
  $('#gFacts').innerHTML = facts.map(([t, d]) => '<div><dt>' + t + '</dt><dd>' + d + '</dd></div>').join('');
  $('#shuffleTxt').textContent = g === 'all' ? 'Shuffle any kit' : 'Shuffle a ' + GPACK[g] + ' kit';
  const gp = $('#gpackBtn');
  gp.hidden = g === 'all'; $('#saveAll').hidden = g !== 'all';
  if (g !== 'all') { gp.dataset.gpack = g; $('#gpackTxt').textContent = 'Save the ' + GPACK[g] + ' pack'; }
}
function applyTheme() {
  const g = themeGenre(), root = document.documentElement;
  if (root.dataset.genre !== g) root.dataset.genre = g;
  try { store.set('skin', g); } catch (e) {}
  renderStage(); vizColors();
  drawRolls(); drawSong();
}
/* a genre change wipes the new skin in from the control that was pressed (where the browser supports view transitions) */
function transition(fn, ev) {
  const reduce = window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (!document.startViewTransition || reduce) { fn(); return; }
  const b = ev && ev.target && ev.target.closest ? ev.target.closest('button') : null, r = b ? b.getBoundingClientRect() : null;
  const x = r ? r.left + r.width / 2 : innerWidth / 2, y = r ? r.top + r.height / 2 : 0;
  const st = document.documentElement.style;
  st.setProperty('--vt-x', x + 'px'); st.setProperty('--vt-y', y + 'px');
  st.setProperty('--vt-r', Math.ceil(Math.hypot(Math.max(x, innerWidth - x), Math.max(y, innerHeight - y))) + 'px');
  try { document.startViewTransition(fn); } catch (e) { fn(); }
}
function setGenre(g, ev) {
  if (g === S.genre && S.view !== 'halloween') return;
  transition(() => {
    S.genre = g; store.set('genre', g);
    if (S.view === 'halloween') setView('kits', true);
    if (g !== 'all' && famOf(curKit()) !== g) {
      const want = kitOf(S.lastKit[g]);
      S.kitId = (want && famOf(want) === g ? want : P.KITS.find(k => famOf(k) === g)).id;
      kitChanged(false);
    }
    renderGenres(); renderKitList(); updateCounts(); renderTools(); renderPads();
    if (S.view === 'vocals') renderVox(); else if (S.view === 'bass') renderBK(); else if (S.view === 'fx') renderFx(); else if (S.view === 'kits') updateFitCounts();
    applyTheme();
  }, ev);
}
function shuffleKit(ev) {
  const pool = (S.genre === 'all' ? P.KITS : P.KITS.filter(k => famOf(k) === S.genre)).filter(k => k.id !== S.kitId);
  const k = pool[Math.floor(Math.random() * pool.length)], before = themeGenre();
  S.kitId = k.id;
  const go = () => {
    kitChanged(true);
    if (themeGenre() !== before) applyTheme();
    if (S.view !== 'kits') setView('kits', true);
    const card = $('[data-kit="' + k.id + '"]');
    if (card && card.scrollIntoView) card.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  };
  if (themeGenre() !== before) transition(go, ev); else go();
  if (!S.playing) startLoop('loop');
  toast('Shuffled to ' + k.id + ' ' + k.name + ', ' + k.bpm + ' BPM', 1800);
}
function updateCounts() {
  const n = list => list.filter(inGenre).length;
  $('#tKits').textContent = S.genre === 'all' ? P.KITS.length : P.KITS.filter(k => famOf(k) === S.genre).length;
  $('#tVox').textContent = n(voxItems()); $('#tBK').textContent = n(bkItems()); $('#tFx').textContent = n(fxItems()); $('#tHits').textContent = n(ITEMS.filter(i => i.cat === 'hits'));
  $('#tHw').textContent = hwItems().length;
  $('#tCrate').textContent = S.crate.size || '';
}
function laneHTML(part) {
  const kit = curKit(), st = ks(kit);
  const vars = part === 'drums' ? [['main', 'Main'], ['var', 'Variation']] : [['A', 'A'], ['B', 'B']];
  const names = { drums: 'Drums', bass: 'Bass', chords: 'Chords', lead: 'Melody' };
  return '<div class="lane" data-lane="' + part + '"><div class="lanehead"><span class="ln">' + names[part] + '</span>' +
    '<div class="seg" role="group" aria-label="' + names[part] + ' pattern">' +
    vars.map(([v, l]) => '<button data-var="' + v + '" aria-pressed="' + (st.vars[part] === v) + '">' + l + '</button>').join('') + '</div>' +
    '<span class="sp"></span>' + lfxHTML(part) + '<button class="mute" data-mute="' + part + '" aria-pressed="' + !!st.mute[part] + '">Mute</button>' +
    '<button class="btn ghost save" data-savelane="' + part + '">' + ICON.save + 'MIDI</button></div>' +
    '<div class="rollbox"><canvas aria-label="' + names[part] + ' pattern preview" role="img"></canvas><div class="playhead"></div><span class="rollnote"></span></div></div>';
}
function renderKitDetail() {
  const kit = curKit(), st = ks(kit), key = P.keyInfo(st.pc, kit.mode);
  const opts = [];
  for (let pc = 0; pc < 12; pc++) {
    const k = P.keyInfo(pc, kit.mode);
    opts.push('<option value="' + pc + '"' + (pc === st.pc ? ' selected' : '') + '>' + k.label + ' · ' + k.camelot + (pc === kit.root ? '  (original)' : '') + '</option>');
  }
  $('#kitdetail').innerHTML =
    '<div class="kd-top"><div style="min-width:0"><div class="kd-tags"><span class="tag t-' + famOf(kit) + '">' + GENRE[famOf(kit)] + '</span><span class="style">' + esc(kit.style) + '</span>' +
    (NEW_FAMS.has(kit.fam) ? '<span class="newtag">New</span>' : kit.ext ? '<span class="newtag">New layers</span>' : '') + '</div>' +
    '<h2 class="kd-name display">' + kit.id + ' ' + esc(kit.name) + '</h2><p class="kd-vibe">' + esc(kit.vibe) + '</p></div>' +
    '<div class="kd-act"><button class="btn ghost" id="kitToClaude" hidden>' + ICON.spark + 'Ask Claude about this kit</button><button class="btn ghost" id="kitComment" hidden>' + ICON.comment + 'Comment</button></div></div>' +
    '<div class="prog" id="prog"></div>' +
    '<div class="controls">' +
    '<div class="ctl"><span class="label">Tempo</span><div class="stepper"><button data-bpm="-1" aria-label="Slower">−</button><output id="bpmOut" class="tnum">' + st.bpm + '</output><button data-bpm="1" aria-label="Faster">+</button></div></div>' +
    '<div class="ctl"><label class="label" for="keySel">Key</label><select class="keysel" id="keySel">' + opts.join('') + '</select></div>' +
    '<div class="ctl"><span class="label">Swing</span><span class="readnum">' + Math.round(kit.swing * 100) + '%</span></div>' +
    '<div class="ctl"><span class="label">Bar 4</span><label class="toggle"><input type="checkbox" id="fillTog"' + (st.fill ? ' checked' : '') + '> Drum fill</label></div>' +
    '</div>' +
    '<div class="lanes">' + ['drums', 'bass', 'chords', 'lead'].map(laneHTML).join('') + '</div>' +
    '<div class="songpanel"><div class="lanehead"><div class="seg" role="group" aria-label="Song version"><button data-songv="full" aria-pressed="' + (S.songV === 'full') + '">Full song</button>' +
    '<button data-songv="ext" aria-pressed="' + (S.songV === 'ext') + '">Extended mix</button></div><span class="sm tnum" id="songMeta"></span><span class="sp"></span>' +
    '<button class="btn" id="songPlay">' + ICON.play + 'Play song</button><button class="btn ghost save" id="saveSong">' + ICON.save + 'MIDI</button></div>' +
    '<div class="songbox"><canvas id="songCv" role="img" aria-label="Song arrangement"></canvas><div class="playhead"></div></div>' +
    '<p class="hint" id="songHint"></p></div>' +
    '<div class="kitsave"><button class="btn solid save" id="saveKit">' + ICON.save + 'Save kit (.zip)</button>' +
    '<button class="btn save" id="saveKits">' + ICON.save + 'Save all ' + P.KITS.length + ' kits</button></div>' +
    '<p class="hint" id="kitHint"></p>' +
    '<div class="fitsrow" id="fitsrow"></div>';
  updateKitText(); updateFitCounts(); kitCapsUI();
  requestAnimationFrame(() => { drawRolls(); drawSong(); });
}
const EXT_HINT = {
  UKG07: 'A one-bar break chopped into 16th slices and re-sequenced: straight in the intro, layered under the 2-step, half speed in the half-time switch and as jungle edits in the second drop. A sine sub doubles the Reese, the pack\'s "Hold me closer" hook rides the verse and drops, and a tape stop winds down into drop 2.',
  UKG08: 'Pizzicato, swelling and legato strings, choir oohs, and a sub that dips in pitch at the end of held notes (pitch bend). Two pack vocals: "Stay in bed till the evening", written for this song, and, in the post-hook and drop 2, the brat line "Expensive taste, pretty face".',
  MTH07: 'Rominimal: bongo, clave and click cycles of 3, 5 and 7 steps drifting against the bar, crackle and glitch ratchets, a 16-bar micro-edit section (beat repeats, drop-outs, bass stutters, gated hats), a bass filter that zooms open and retracts, and the whispered "Deeper, deeper" hook.',
  MTH08: 'A bouncing rubber bass (every note drops two semitones into pitch), a 303 acid line, a rave hoover with swoops and dives at the peak, cowbell and ghetto-house claps, ad-lib shouts on their own sampler track, the "Rich bitch energy" chant and a tape stop into the peak.',
  SG07: 'A dub siren on pitch-bend LFOs, a rewind after eight bars of the first drop ("Pull up!", spin-back, "Come again!"), Reese dives at phrase ends, a hoover in the last two drops, MC shouts, and two pack vocals: "Feel the pressure" and "Big bass, big bag".',
  SG08: 'An offbeat donk, the bassline wub (a cutoff LFO whose rate changes every two bars), orchestral string stabs, a 2-step switch, a tape stop into drop 2, MC shouts, "Play that organ loud" (written for this song) in drop 1 and the breakdown, and "All the girls say ooh" in drop 2.',
  CH07: 'Disco strings (pizzicato, legato, scale runs into the bar), brass stabs that fall off by pitch bend and stay out of the singer\'s way, a talkbox answering the vocal, a filtered intro, a filter-sweep switch and a close-out, plus "Glitter on the ceiling" (written for this song) and "Buy me diamonds, baby".',
  CH08: 'A four-part choir with gospel stabs, organ swells and comping, piano runs and broken-chord fills, orchestra hits and a semitone key change for the last chorus. "Hands up higher, one desire" (written for this song) carries the verse and first chorus, "Lift me higher" the last.',
  GH01: 'Garage house in the Disclosure mould: a 32-bar DJ intro with the kick from bar one, string pads and swells, vocal chops that answer the singer, a 2-step switch, and its own hook, "Orange light on your skin", in the verse and both choruses, with "Deeper, deeper" in the switch and "Lift me higher" answering in the breakdown and last chorus.',
  GH02: 'A 2-step verse and drops, a four-to-the-floor switch with octave-popping bass and chops, piano fills, a choir-oohs breakdown on a darker progression, a tape stop into drop 2, its own hook "Missed the last train home" in the verse and drops, and "Hold me closer" answering in the breakdown and drop 2.',
  GH03: 'A filtered intro that opens up, a warbling bass (a cutoff LFO whose rate changes every two bars), its own hook "Swimming in the neon light" in the verse and drops, "You got me dancing" in the breakdown, the brat line "Buy me diamonds, baby" in the post-hook and between the drop-2 hooks, and a filter close-out.',
  GV01: 'Groove house in the Chris Stussy mould: hats first, then the FM bass alone, then the glow chords with their reverb swelling in the gaps (CC91 on the chords track), a Juno-style lead in fifths with a counter answering it, a chord breakdown with pads, a bass-solo switch on the B line and a dusty break under the second drop.',
  GV02: 'Jazzy groove house: a bongo intro, Rhodes stabs and a piano line, piano fills, a dub break that drops to the echo, legato strings in the breakdown and a swell into the build, with a flat-nine turnaround closing every fourth bar.',
  GV03: 'Disco groove house: the octave bass alone, then stuttering glow chords with their reverb swelling in the gaps, a string hook with the fifths lead answering it, a string breakdown and swell, and a disco switch on the variation drums.',
  TH01: 'Tech house in the Chris Lake mould: the intro filter opens up, the bounce bass arrives before the full kit, dry stabs from bar 8, vocal shots on the offbeats in both drops (their own Shouts track), a tom switch, a short 8-bar break and a tape stop into the second drop.',
  TH02: 'Dark tech house: the talking bass alone, the string hook with its Phrygian turn, a string swell into the drop, vocal shots answering the strings, a tom switch without chords, a short string breakdown and harmony on the second drop.',
  TH03: 'UK house in the Riordan mould: a break intro, M1-style organ stabs, the rave-stab riff over chopped breaks, a 2-step switch, an organ breakdown, and a rewind in the second drop ("Pull up!", spin-back, "Come again!") with shots and breaks layered.',
  TH04: 'UK tech house: a break intro, the fifth bass alone, FM pluck chords, the one-note lead (only its length changes) over chopped breaks, two drops with counter-lines and a tape stop into the second.',
  GH04: 'Gospel garage: organ stabs, a choir that swells and then stabs, piano runs, its own hook, "Meet me on the chapel steps" (a male lead), in the verse and choruses, "Sunday morning" in the choir breakdown, "Expensive taste, pretty face" over a 2-step post-hook, and both trading in the final chorus.'
};
function updateKitText() {
  const kit = curKit(), st = ks(kit), key = P.keyInfo(st.pc, kit.mode), tr = P.transposeFor(kit, st.pc);
  const pa = progression(kit, 'A'), pb = progression(kit, 'B');
  const cyc = bars => { const f = []; bars.flat().forEach(n => { if (f[f.length - 1] !== n) f.push(n); }); if (f.length > 1 && f[0] === f[f.length - 1]) f.pop(); return f.join(' '); };
  const same = cyc(pa) === cyc(pb);
  const chip = bars => bars.map(b => '<span>' + b.map(n => trChord(n, tr, key.flats)).join(' → ') + '</span>').join('');
  $('#prog').innerHTML = '<span class="label" style="background:none;border:0;padding:3px 0">Chords' + (same ? '' : ' A') + '</span>' + chip(pa) +
    (same ? '' : '<span class="label" style="background:none;border:0;padding:3px 0">B</span>' + chip(pb));
  $('#bpmOut').textContent = st.bpm;
  const sg = songOf(kit, st), ext = !!sg.extended;
  const nTr = ext ? P.EXT_TRACKS.filter(t => sg.T[t.key].length).length : 7;
  const xs = songOf(kit, st, 'ext'), nExt = P.EXT_TRACKS.filter(t => xs.T[t.key].length).length;
  $('#kitHint').textContent = 'Saves 12 MIDI files in ' + key.label + ' (' + key.camelot + ') at ' + st.bpm + ' BPM: drums main, variation and fill; bass, chords and melody A/B; an 8-bar arrangement; the full song; and the ' + nExt + '-track extended mix' +
    (kit.ext ? ' (with a vocal-guide track carrying the lyrics, pitch-bend moves and a sidechain trigger). The kit zip also carries the pack vocals and shouts the extended mix uses.' : '.');
  $('#songMeta').textContent = sg.bars + ' bars · ' + songTime(st) + ' · ' + nTr + ' tracks';
  $('#songHint').textContent = sg.sections.map(x => x.name).join(' → ') + '. ' + (ext && kit.ext
    ? EXT_HINT[kit.id] + ' Vocals play from the pack in the original key; in another key the vocal guide plays on a synth voice. Tap a section to jump there while it plays.'
    : ext
    ? 'Drums split into kick, snare, hats, percussion and fills, changing every 8 bars; stabs, counter-melody, vocal-chop and FX-hit tracks; the second drop is reharmonised with a V7 turnaround' + (kit.genre === 'ch' ? ', and the final chorus lifts the key a whole step' : '') + '. Tap a section to jump there while it plays.'
    : 'Builds carry a snare roll, a climbing riser and filter automation (CC74), with a silent eighth before each drop. Tap a section to jump there while the song plays.');
}
function drawRolls() {
  if (S.view !== 'kits') return;
  const kit = curKit(), st = ks(kit), tr = P.transposeFor(kit, st.pc), key = P.keyInfo(st.pc, kit.mode);
  const css = getComputedStyle(document.documentElement);
  const C = n => css.getPropertyValue(n).trim();
  const ink = C('--ink'), line = C('--line'), line2 = C('--surface-2'), acc = C('--accent-text'), acc2 = C('--accent-2'), muted = C('--muted');
  const total = 64 * 120;
  $$('.lane').forEach(lane => {
    const part = lane.dataset.lane, cv = lane.querySelector('canvas'), note = lane.querySelector('.rollnote');
    let ev, rows;
    if (part === 'drums') {
      let d = P.compileDrums(kit, st.vars.drums).events;
      if (st.fill) d = d.filter(e => e.t < 3 * 1920).concat(P.compileDrums(kit, 'fill').events.map(e => Object.assign({}, e, { t: e.t + 3 * 1920 })));
      const used = P.DRUM_ORDER.filter(l => d.some(e => e.lane === l));
      rows = used; ev = d;
      note.textContent = used.map(l => P.DRUM[l].label).join(' · ');
    } else {
      ev = P.compileMelodic(kit, part, st.vars[part], tr).events;
      const lo = Math.min(...ev.map(e => e.n)), hi = Math.max(...ev.map(e => e.n));
      rows = [lo, hi];
      note.textContent = P.noteLabel(lo, key.flats) + '–' + P.noteLabel(hi, key.flats);
    }
    const w = cv.clientWidth || cv.parentNode.clientWidth;
    const h = part === 'drums' ? Math.max(54, rows.length * 11 + 8) : part === 'chords' ? 70 : 60;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    cv.style.height = h + 'px'; cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
    const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0); g.clearRect(0, 0, w, h);
    for (let i = 0; i <= 16; i++) { g.fillStyle = i % 4 === 0 ? line : line2; g.fillRect(Math.min(w - 1, Math.round(i / 16 * w)), 0, 1, h); }
    const X = t => t / total * w;
    if (part === 'drums') {
      const rh = (h - 8) / rows.length;
      ev.forEach(e => {
        const r = rows.indexOf(e.lane);
        g.globalAlpha = 0.3 + 0.7 * e.v / 127; g.fillStyle = e.lane === 'K' ? acc : ink;
        g.fillRect(X(e.t), 4 + r * rh + rh * 0.15, Math.max(2, w / 128), rh * 0.7);
      });
    } else {
      const lo = rows[0] - 1, hi = rows[1] + 1, span = hi - lo + 1, rh = (h - 8) / span;
      ev.forEach(e => {
        g.globalAlpha = 0.45 + 0.55 * e.v / 127; g.fillStyle = part === 'bass' ? acc : part === 'chords' ? acc2 : ink;
        g.fillRect(X(e.t), 4 + (hi - e.n) * rh, Math.max(2, X(e.d) - 1), Math.max(2, rh - 1));
      });
    }
    g.globalAlpha = 1;
  });
}
const SEC_STYLE = { intro: ['--surface-2', '--muted'], outro: ['--surface-2', '--muted'], groove: ['--line', '--ink'], build: ['--soft', '--accent-text'], drop: ['--accent', '--accent-ink'], switch: ['--ink', '--surface'], breakdown: ['--stage', '--stage-ink'] };
function drawSong() {
  const cv = $('#songCv'); if (!cv || S.view !== 'kits') return;
  const kit = curKit(), st = ks(kit), sg = songOf(kit, st), ext = !!sg.extended;
  const css = getComputedStyle(document.documentElement), C = n => css.getPropertyValue(n).trim();
  const rows = ext ? P.EXT_TRACKS.map(t => t.key).filter(k => sg.T[k].length && k !== 'sidechain') : ['drums', 'bass', 'chords', 'lead', 'pad', 'arp', 'riser'];
  const w = cv.clientWidth || cv.parentNode.clientWidth, head = 24, rh = ext ? 6 : 9;
  const h = head + rows.length * rh + 6, dpr = Math.min(2, window.devicePixelRatio || 1);
  cv.style.height = h + 'px'; cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
  const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0); g.clearRect(0, 0, w, h);
  const X = bar => bar / sg.bars * w;
  g.font = '600 11px ' + (C('--font-ui') || 'sans-serif');
  sg.sections.forEach(sc => {
    const [bg, fg] = SEC_STYLE[sc.kind] || SEC_STYLE.groove;
    const x0 = X(sc.bar), x1 = X(sc.bar + sc.bars);
    g.fillStyle = C(bg); g.fillRect(x0, 0, x1 - x0 - 1, head - 4);
    g.fillStyle = C(fg);
    const label = x1 - x0 > 60 ? sc.name : x1 - x0 > 26 ? sc.name.split(' ')[0].slice(0, 5) : '';
    if (label) g.fillText(label, x0 + 4, 14);
    g.fillStyle = C('--line'); g.fillRect(x0, head, 1, h - head);
  });
  rows.forEach((tk, i) => {
    const y = head + i * rh;
    g.fillStyle = tk === 'bass' || tk === 'sub' ? C('--accent-text') : tk === 'drums' || EXT_DRUMS.has(tk) ? C('--muted') : C('--ink');
    sg.T[tk].forEach(e => { g.globalAlpha = 0.25 + 0.6 * e.v / 127; g.fillRect(X(e.t / 1920), y + 1, Math.max(1, X(e.d / 1920)), rh - 3); });
  });
  g.globalAlpha = 1;
  cv.dataset.bars = sg.bars;
  cv._sections = sg.sections;
}
function kitChanged(rerenderList) {
  store.set('kit', S.kitId); sendPresence();
  S.lastKit[famOf(curKit())] = S.kitId; store.set('lastKit', S.lastKit);
  if (rerenderList) $$('.kitcard').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.kit === S.kitId)));
  renderKitDetail(); S.sel = { type: 'kit' }; lcd(); applyAllLaneFx();
  if (S.playing) switchKitWhilePlaying(curKit());
  renderTools();
}

/* ---------- sample rows ---------- */
function wavePath(peaks) {
  let d = '';
  peaks.forEach((p, i) => { const hh = Math.max(0.6, p * 30); d += 'M' + i + ' ' + (17 - hh / 2).toFixed(2) + 'h0.72v' + hh.toFixed(2) + 'h-0.72z'; });
  return d;
}
let clipN = 0;
function rowHTML(it) {
  const pk = wavePath(it.peaks), cid = 'cp' + (++clipN);
  const isVox = it.cat === 'vox';
  const title = isVox && it.group !== 'chops' && it.group !== 'pads' ? '“' + esc(it.title) + '”' : esc(it.title);
  const gk = itemGenre(it);
  const gt = gk && GSHORT[gk] && gk !== 'all' && (S.genre === 'all' || gk !== S.genre) ? '<span class="gtag t-' + gk + '">' + GSHORT[gk] + '</span>' : '';
  const sub = gt + (isVox ? esc(it.voice + (it.note ? ' · ' + it.note : '')) : esc(({ fills: 'Fill', fx: 'FX', loops: 'Loop', bass: 'Bass', keys: 'Keys' }[it.cat] || '') + (it.note ? ' · ' + it.note : '')));
  const len = it.bars ? it.bars + (it.bars > 1 ? ' bars' : ' bar') : it.dur.toFixed(1) + ' s';
  const k = keyText(it);
  const nw = it.wave >= (isVox ? 6 : 3) ? '<span class="newtag">New</span>' : '';
  return '<div class="row" data-row="' + it.id + '">' +
    '<button class="pbtn" data-play="' + it.id + '" aria-label="Play ' + esc(it.title) + '">' + ICON.play + '</button>' +
    '<div class="rt"><div class="t' + (isVox ? ' q' : '') + '">' + title + (it.explicit ? '<span class="e" title="Explicit">E</span>' : '') + nw + '</div><div class="s">' + sub + '</div></div>' +
    '<svg class="wave" viewBox="0 0 72 34" preserveAspectRatio="none" aria-hidden="true"><clipPath id="' + cid + '"><rect class="prc" x="0" y="0" width="0" height="34"/></clipPath>' +
    '<path d="' + pk + '" fill="currentColor"/><path class="pr" clip-path="url(#' + cid + ')" d="' + pk + '"/></svg>' +
    '<div class="rm tnum"><span class="b">' + (it.bpm ? it.bpm + ' BPM · ' : '') + len + '</span><span class="k">' + esc(k) + (it.camelot ? ' <span class="cam">' + it.camelot + '</span>' : '') + '</span></div>' +
    '<div class="rowact"><button class="dsn" data-designsrc="' + it.id + '" aria-label="Open ' + esc(it.title) + ' in the Sound lab" title="Open in the Sound lab">' + ICON.sliders + '</button>' + starBtn(it) + '<button class="btn ghost save" data-saveitem="' + it.id + '">' + ICON.save + '<span class="st">Save</span></button></div></div>';
}
/* ---------- search, 'fits this kit', crate ---------- */
function kitKey() { const kit = curKit(), st = ks(kit), k = P.keyInfo(st.pc, kit.mode); return { cam: k.camelot, label: k.label, bpm: st.bpm, kit }; }
function camOK(a, b) {   // Camelot neighbours: same code, one step round the wheel, or the relative major/minor
  const m1 = /^(\d+)([AB])$/.exec(a || ''), m2 = /^(\d+)([AB])$/.exec(b || '');
  if (!m1 || !m2) return true;
  const d = ((+m1[1] - +m2[1]) % 12 + 12) % 12;
  return m1[2] === m2[2] ? d === 0 || d === 1 || d === 11 : d === 0;
}
function fitsKit(it) {
  const k = kitKey();
  if (it.camelot && !camOK(it.camelot, k.cam)) return false;
  if (it.bars && it.bpm) return Math.abs(it.bpm - k.bpm) <= (it.key ? 4 : 8);
  return true;
}
const hay = it => (it.hay || (it.hay = [it.title, it.note, it.voice, it.kind, it.group, it.key, it.camelot, it.bpm ? it.bpm + ' bpm' : '', GSHORT[it.genre], GENRE[it.genre], it.pack].filter(Boolean).join(' ').toLowerCase()));
function shown(it) {
  if (S.fits && !fitsKit(it)) return false;
  const q = S.q.trim().toLowerCase();
  return !q || q.split(/\s+/).every(w => hay(it).includes(w));
}
const fitsLabel = k => 'Fits ' + k.kit.id + ': ' + k.cam + ', ' + k.bpm + ' BPM';
function toolsHTML() {
  const k = kitKey();
  return '<div class="tools"><label class="search">' + ICON.search + '<input type="search" data-q placeholder="Search sounds, keys, BPM" aria-label="Search sounds" value="' + esc(S.q) + '"></label>' +
    '<button class="chip fits" data-fits aria-pressed="' + S.fits + '" title="Only show sounds in a key that mixes with the kit and a tempo near it">' + fitsLabel(k) + '</button>' +
    '<button class="chip scope" data-genre="all" aria-label="Show every genre" hidden></button></div>';
}
function renderTools() {
  const k = kitKey(), label = fitsLabel(k);
  $$('.toolsbox').forEach(b => {
    if (!b.querySelector('[data-q]')) {
      b.innerHTML = toolsHTML();
      if (b.closest('#view-vocals')) b.firstChild.insertAdjacentHTML('beforeend', '<label class="toggle"><input type="checkbox" id="hideE"' + (S.hideExplicit ? ' checked' : '') + '> Hide explicit</label>');
    }
    const f = b.querySelector('[data-fits]'); f.textContent = label; f.setAttribute('aria-pressed', String(S.fits));
    const sc = b.querySelector('.scope');
    sc.hidden = S.genre === 'all';
    if (S.genre !== 'all') sc.innerHTML = '<span class="dot g-' + S.genre + '"></span>' + GLABEL[S.genre] + ' only <span class="x" aria-hidden="true">×</span>';
    const q = b.querySelector('[data-q]'); if (q.value !== S.q) q.value = S.q;
  });
}
function refreshFits() { renderTools(); if (S.view === 'vocals') renderVox(); if (S.view === 'bass') renderBK(); if (S.view === 'fx') renderFx(); if (S.view === 'kits') updateFitCounts(); }
const emptyHTML = () => '<p class="hint empty">Nothing matches' + (S.q ? ' “' + esc(S.q) + '”' : '') + (S.fits ? ' that fits ' + curKit().id : '') + '. ' + (S.fits ? '<button class="btn ghost" data-fitsoff>Show every key and tempo</button>' : '') + '</p>';
function starBtn(it) { const on = S.crate.has(it.id); return '<button class="star" data-star="' + it.id + '" aria-pressed="' + on + '" aria-label="' + (on ? 'Remove from crate' : 'Add to crate') + '">' + ICON.star + '</button>'; }
function toggleStar(id) {
  if (S.crate.has(id)) S.crate.delete(id); else S.crate.add(id);
  store.set('crate', [...S.crate]);
  $$('[data-star="' + id + '"]').forEach(b => { const on = S.crate.has(id); b.setAttribute('aria-pressed', String(on)); b.setAttribute('aria-label', on ? 'Remove from crate' : 'Add to crate'); });
  $('#tCrate').textContent = S.crate.size || '';
  if (S.view === 'crate') renderCrate();
  queueCrate();
}
function renderCrate() {
  const mem = [...S.crate].map(id => BY_ID[id]).filter(Boolean);
  $('#tCrate').textContent = mem.length || '';
  if (!mem.length) { $('#crateRows').innerHTML = '<p class="hint empty">Your crate is empty. Tap the star on any vocal, bass, keys, loop or FX row to collect it here.</p>'; markRow(); return; }
  const groups = [['vox', 'Vocals'], ['bk', 'Bass & keys'], ['fx', 'Loops, fills & FX'], ['hits', 'Drum hits']];
  const gOf = i => i.cat === 'vox' ? 'vox' : i.cat === 'bass' || i.cat === 'keys' ? 'bk' : i.cat === 'hits' ? 'hits' : 'fx';
  $('#crateRows').innerHTML = groups.filter(([g]) => mem.some(i => gOf(i) === g)).map(([g, l]) => groupHead(l) + '<div class="rows">' + mem.filter(i => gOf(i) === g).map(rowHTML).join('') + '</div>').join('');
  markRow();
}
function updateFitCounts() {
  const box = $('#fitsrow'); if (!box) return;
  const k = kitKey(), n = list => list.filter(fitsKit).length;
  box.innerHTML = '<span class="label">Sounds that fit ' + k.kit.id + ' in ' + k.label + ' (' + k.cam + ') at ' + k.bpm + ' BPM' + (S.genre === 'all' ? '' : ', ' + GLABEL[S.genre].toLowerCase() + ' only') + '</span>' +
    [['vocals', 'Vocals', voxItems().filter(i => !(S.hideExplicit && i.explicit)).filter(inGenre)], ['bass', 'Bass & keys', bkItems().filter(inGenre)], ['fx', 'Loops & FX', fxItems().filter(inGenre)]]
      .map(([v, l, list]) => '<button class="btn ghost" data-gofits="' + v + '">' + l + ' <b class="tnum">' + n(list) + '</b></button>').join('');
}
const VOX_FILTERS = [['all', 'All'], ['new', 'New'], ['halloween', 'Halloween'], ['brat', 'Brat'], ['hooks', 'Sung hooks'], ['pads', 'Vocal pads'], ['clean', 'Spoken · clean'], ['explicit', 'Spoken · explicit'], ['tuned', 'Tuned & robot'], ['chops', 'Chop loops'], ['stabs', 'Stabs'], ['adlibs', 'Ad-libs']];
const voxIn = (i, f) => f === 'all' || (f === 'new' ? i.wave >= 6 : i.group === f);
function groupHead(l, gg) {   // 'Name · what it holds' -> bold name and a quieter description; a genre dot when every genre is listed
  const [a, b] = l.split(' · ');
  return '<div class="groupname">' + (gg && S.genre === 'all' ? '<span class="dot g-' + gg + '"></span>' : '') + '<span class="label">' + esc(a) + '</span>' + (b ? '<span class="gsub">' + esc(b) + '</span>' : '') + '</div>';
}
const FX_FILTERS = [['all', 'All'], ['new', 'New'], ['halloween', 'Halloween'], ['drums', 'Drum loops'], ['tops', 'Tops & perc'], ['breaks', 'Breaks'], ['fills', 'Fills'], ['fx', 'FX & textures']];
const FX_GROUPS = [['halloween', 'Halloween · thunder, bells, organ, wolves, theremin & more', 'halloween'], ['dl-ukg', 'Drum loops · UK garage', 'drums', 'ukg'], ['dl-mth', 'Drum loops · minimal tech house', 'drums', 'mth'], ['dl-sg', 'Drum loops · speed garage', 'drums', 'sg'], ['dl-ch', 'Drum loops · classic house', 'drums', 'ch'], ['dl-gh', 'Drum loops · garage house (Disclosure-style)', 'drums', 'gh'], ['dl-gv', 'Drum loops · groove house (Chris Stussy-style)', 'drums', 'gv'], ['dl-th', 'Drum loops · tech house (Chris Lake & Riordan-style)', 'drums', 'th'],
  ['tops', 'Top & percussion loops', 'tops'], ['breaks', 'Breaks · slice-ready and roomy', 'breaks', 'ukg'], ['ukg-fills', 'Fills · UK garage', 'fills', 'ukg'], ['mth-fills', 'Fills · minimal tech house', 'fills', 'mth'], ['th-fills', 'Fills · tech house (snare build)', 'fills', 'th'],
  ['ukg-fx', 'FX · UK garage', 'fx', 'ukg'], ['mth-fx', 'FX · minimal tech house', 'fx', 'mth'], ['sg-fx', 'FX · speed garage', 'fx', 'sg'], ['ch-fx', 'FX · house & textures', 'fx', 'ch'], ['gv-fx', 'FX · groove house (backspin, crash into reverb)', 'fx', 'gv'],
  ['th-fx', 'FX · tech house (endless riser, uplifter, downlifter, impact)', 'fx', 'th']];
const BK_FILTERS = [['all', 'All'], ['new', 'New'], ['reese', 'Reese'], ['synth', 'Subs & synth bass'], ['organ', 'Organ bass'], ['piano', 'Piano'], ['keys', 'Keys & stabs'], ['strings', 'Strings']];
const BK_GROUPS = [['reese', 'Reese bass · one-shots', 'reese', 'sg'], ['reeseloops', 'Reese bass · loops (kit basslines)', 'reese', 'sg'], ['subs', 'Subs & synth bass · one-shots', 'synth'],
  ['bassloops', 'Bass loops & acid · from the new songs', 'synth'], ['ghbass', 'Glide bass · garage house (Disclosure-style)', 'synth', 'gh'], ['organ', 'Organ bass', 'organ', 'ch'], ['piano', 'House piano', 'piano', 'ch'], ['garagekeys', 'Garage keys · organ stabs, Rhodes, bells', 'keys', 'ukg'],
  ['ghkeys', 'Chord pluck · garage house (Disclosure-style)', 'keys', 'gh'],
  ['gvfm', 'FM rubber bass · groove house (Chris Stussy-style)', 'synth', 'gv'], ['gvbassloops', 'Bass loops · groove house', 'synth', 'gv'], ['gvglow', 'Glow saw · groove house', 'keys', 'gv'],
  ['gvfifth', 'Fifths lead · groove house', 'keys', 'gv'], ['gvkeyloops', 'Chord & lead loops · groove house (the reverb swells in the gaps)', 'keys', 'gv'],
  ['thbounce', 'Bounce bass · tech house (Chris Lake & Riordan-style)', 'synth', 'th'], ['thfifth', 'Fifth bass · tech house', 'synth', 'th'], ['threese', 'Crushed Reese · tech house', 'reese', 'th'],
  ['thbassloops', 'Bass loops · tech house (bounce, talking, Reese, fifth)', 'synth', 'th'], ['thrave', 'Rave stab · tech house', 'keys', 'th'], ['thkeyloops', 'Stab, organ & string loops · tech house', 'keys', 'th'], ['dubchords', 'Dub chords & bleeps', 'keys', 'mth'], ['housestabs', 'Brass, orchestra hits & choir', 'keys', 'ch'], ['strings', 'Strings · pads, runs, stabs, loops', 'strings']];
const BK_SETS = { reese: ['reese_classic', 'Reese Classic · chromatic C1–B1', 'sg', '2-second sustained notes, labelled by note'], subs: ['sub_2step', '2-Step Sub · chromatic C1–B1', 'ukg', '808-style sine sub with a long tail, one per note'],
  organ: ['organ_bass', 'Organ Bass · chromatic C1–B2', 'ch', 'M1-style organ bass, 24 notes'],
  ghbass: ['gh_bass', 'Glide Bass · C1–C4, every minor third', 'gh', 'round sine and filtered square; each note loops cleanly for a sampler'],
  ghkeys: ['gh_pluck', 'Chord Pluck · C2–C6, every minor third', 'gh', 'detuned-saw pluck for offbeat minor 7th and 9th chords'],
  gvfm: ['gv_fm', 'FM Rubber Bass · C1–C4, every minor third', 'gv', 'two-operator FM bass: the index falls fast for the pluck, a 25-cent drop at the start, warm drive'],
  gvglow: ['gv_glow', 'Glow Saw · C2–C6, every minor third', 'gv', 'two saws, a breath of noise, a closing ladder filter, a long release and a slow stereo phaser'],
  gvfifth: ['gv_fifth', 'Fifths Lead · C3–C6, every minor third', 'gv', 'Juno-style lead: each key plays the note and its fifth, with PWM, drift and BBD chorus'],
  thbounce: ['th_bounce', 'Bounce Bass · C1–C4, every minor third', 'th', 'bright, driven and sustained (each note loops); add a resonant low-pass envelope that opens with velocity'],
  thfifth: ['th_fifth', 'Fifth Bass · C1–C4, every minor third', 'th', 'Moog-style: saws on the note and its fifth over a square an octave down, a resonant ladder pluck'],
  threese: ['th_reese', 'Crushed Reese · C1–C4, every minor third', 'th', 'two saws either side of the note, a sine a fifth up, driven and 6-bit crushed; each note loops'],
  thrave: ['th_rave', 'Rave Stab · C2–C5, every minor third', 'th', 'chord memory: every key plays a minor chord with the octave, so a one-finger riff becomes a stab line'], piano: ['piano_ms', 'House Piano · multisample C2–C6', 'ch', 'Dry notes every minor third, ready for a sampler'] };
const voxItems = () => ITEMS.filter(i => i.cat === 'vox');
const fxItems = () => ITEMS.filter(i => i.cat === 'fills' || i.cat === 'fx' || i.cat === 'loops');
const fxKey = i => {
  if (i.grp === 'halloween') return 'halloween';
  if (/^(gv|th)-(fx|fills)$/.test(i.grp || '')) return i.grp;
  if (i.grp === 'tops' || i.grp === 'breaks' || (i.grp || '').startsWith('dl-')) return i.grp;
  if (i.grp === 'fx3') return (i.genre === 'all' ? 'ch' : i.genre) + '-fx';
  if (i.cat === 'loops') return i.id.startsWith('drums_ch') ? 'dl-ch' : 'dl-sg';
  return i.id.startsWith('sg_') ? 'sg-fx' : (i.id.startsWith('ukg') ? 'ukg-' : 'mth-') + (i.cat === 'fills' ? 'fills' : 'fx');
};
const bkItems = () => ITEMS.filter(i => (i.cat === 'bass' || i.cat === 'keys') && !i.hidden);
const NEW_BK = new Set(['gvfm', 'gvglow', 'gvfifth', 'gvbassloops', 'gvkeyloops', 'thbounce', 'thfifth', 'threese', 'thrave', 'thbassloops', 'thkeyloops']);
const bkKey = i => NEW_BK.has(i.grp) || i.grp === 'ghbass' || i.grp === 'ghkeys' ? i.grp : i.grp === 'sub' || i.grp === 'synthbass' ? 'subs' : i.grp === 'bassloops' ? 'bassloops' : ['garagekeys', 'dubchords', 'housestabs'].includes(i.grp) ? i.grp : i.grp === 'strings3' ? 'strings' :
  i.set === 'reese_classic' || /^reese_(wobble|growl|dark|dive|stab)$/.test(i.id) ? 'reese' : /^reese_(loop|wob)_/.test(i.id) ? 'reeseloops' :
  i.set === 'organ_bass' || i.id.startsWith('organ_loop') ? 'organ' : (i.set === 'piano_ms' || /^p(stab|loop)_/.test(i.id)) ? 'piano' : 'strings';
if (!FX_FILTERS.some(f => f[0] === S.fxFilter)) S.fxFilter = 'all';
if (!BK_FILTERS.some(f => f[0] === S.bkFilter)) S.bkFilter = 'all';
function renderVox() {
  const all = voxItems().filter(i => !(S.hideExplicit && i.explicit)).filter(inGenre).filter(shown);
  const cnt = f => all.filter(i => voxIn(i, f)).length;
  $('#voxFilters').innerHTML = VOX_FILTERS.filter(([f]) => f === 'all' || f === S.voxFilter || cnt(f)).map(([f, l]) => '<button class="chip" data-voxf="' + f + '" aria-pressed="' + (S.voxFilter === f) + '">' + l + '<span class="n">' + cnt(f) + '</span></button>').join('');
  const list = all.filter(i => voxIn(i, S.voxFilter));
  const groups = [['halloween', 'Halloween · spooky drops, laughs, the witches\' chant & sung hooks'], ['brat', 'Brat · rich-girl luxury talk, keyed chants & ad-libs'], ['hooks', 'Sung hooks · 4-bar loops, keyed, lead + doubles + harmony'], ['pads', 'Vocal pads · choir chords, 4-bar loops, keyed'], ['clean', 'Spoken hooks · clean'], ['explicit', 'Spoken hooks · explicit'], ['tuned', 'Hard-tuned & robot hooks · keyed'], ['chops', 'Sung chop loops · keyed, 2 bars'], ['stabs', 'Vocal stabs · single notes'], ['adlibs', 'Ad-libs']];
  $('#voxRows').innerHTML = groups.filter(([g]) => list.some(i => i.group === g)).map(([g, l]) =>
    groupHead(l) + '<div class="rows">' + list.filter(i => i.group === g).map(rowHTML).join('') + '</div>').join('') ||
    emptyHTML();
  markRow();
}
function renderFx() {
  const all = fxItems().filter(inGenre).filter(shown);
  const inF = (i, f) => f === 'all' || (f === 'new' ? i.wave >= 3 : (FX_GROUPS.find(g => g[0] === fxKey(i)) || [])[2] === f);
  $('#fxFilters').innerHTML = FX_FILTERS.map(([f, l]) => [f, l, all.filter(i => inF(i, f)).length]).filter(([f, , n]) => n || f === 'all' || f === S.fxFilter)
    .map(([f, l, n]) => '<button class="chip" data-fxf="' + f + '" aria-pressed="' + (S.fxFilter === f) + '">' + l + '<span class="n">' + n + '</span></button>').join('');
  const list = all.filter(i => inF(i, S.fxFilter));
  $('#fxRows').innerHTML = FX_GROUPS.filter(([g]) => list.some(i => fxKey(i) === g)).map(([g, l, , gg]) =>
    groupHead(l, gg) + '<div class="rows">' + list.filter(i => fxKey(i) === g).map(rowHTML).join('') + '</div>').join('') || emptyHTML();
  markRow();
}
function setHTML(setId, title, sub, only) {
  const mem = only || ITEMS.filter(i => i.set === setId);
  return '<div class="setrow" data-setrow="' + setId + '"><div class="rt"><div class="t">' + esc(title) + '</div><div class="s">' + sub + '</div></div>' +
    '<div class="rowact"><button class="btn ghost save" data-saveset="' + setId + '">' + ICON.save + 'Save set</button></div>' +
    '<div class="notes8">' + mem.map(i => '<button class="nbtn" data-note="' + i.id + '" aria-label="Play ' + esc(i.title) + '">' + esc(i.noteName) + '</button>').join('') + '</div></div>';
}
function renderBK() {
  const all = bkItems().filter(inGenre).filter(shown);
  const inF = (i, f) => f === 'all' || (f === 'new' ? i.wave >= 3 : (BK_GROUPS.find(g => g[0] === bkKey(i)) || [])[2] === f);
  $('#bkFilters').innerHTML = BK_FILTERS.map(([f, l]) => [f, l, all.filter(i => inF(i, f)).length]).filter(([f, , n]) => n || f === 'all' || f === S.bkFilter)
    .map(([f, l, n]) => '<button class="chip" data-bkf="' + f + '" aria-pressed="' + (S.bkFilter === f) + '">' + l + '<span class="n">' + n + '</span></button>').join('');
  const list = all.filter(i => inF(i, S.bkFilter));
  const h = BK_GROUPS.filter(([g]) => list.some(i => bkKey(i) === g)).map(([g, l, , gg]) => {
    const set = BK_SETS[g], mem = set ? list.filter(i => i.set === set[0]) : [];
    return groupHead(l, gg) +
      (mem.length ? setHTML(set[0], set[1], '<span class="gtag t-' + set[2] + '">' + GSHORT[set[2]] + '</span>' + set[3], mem) : '') +
      '<div class="rows">' + list.filter(i => !i.set && bkKey(i) === g).map(rowHTML).join('') + '</div>';
  }).join('') || emptyHTML();
  $('#bkRows').innerHTML = h;
  markRow();
}
const PAD_KEYS = ['1', '2', '3', '4', 'q', 'w', 'e', 'r', 'a', 's', 'd', 'f', 'z', 'x', 'c', 'v'];
/* long words on the pads get a soft hyphen at their seam, so "Dustweather" breaks as Dust-weather on a phone instead of clipping */
const SHY = new Map(('Bang-croft Black-thorn Brush-wood Crook-shank Dust-weather Evan-geline Fair-weather Floren-tine Hallo-ween Henri-etta Hollo-well ' +
  'Master-piece McGilli-cuddy Mira-belle Moon-shadow Objec-tion Penny-feather Price-well Pudding-ham Scanda-lous Sebas-tian Sera-phina Snod-grass ' +
  "Some-one's Spook-ington Star-light Tambou-rine Thunder-clap Tinc-tures Twinkle-ton Vault-bridge every-thing exclu-sive").split(' ').map(w => [w.replace('-', ''), w.replace('-', '\u00AD')]));
const shy = s => s.replace(/[A-Za-z']{9,}/g, w => SHY.get(w) || w);
const padHTML = (h, i, grp, label) => '<button class="pad" data-pad="' + h.id + '" data-kind="' + h.cat + '" aria-label="' + esc(label || h.title) + (PAD_KEYS[i] ? ', key ' + PAD_KEYS[i].toUpperCase() : '') + '">' +
  (PAD_KEYS[i] ? '<span class="kb" aria-hidden="true">' + PAD_KEYS[i].toUpperCase() + '</span>' : '') + '<span class="pk">' + (h.key && h.cat !== 'vox' ? h.key.replace(' minor', 'm') : '') + '</span><span class="pg">' + grp + '</span><span class="pn">' + esc(shy(label || h.title)) + '</span></button>';
function renderPads() {
  const hits = ITEMS.filter(i => i.cat === 'hits').filter(inGenre);
  const grp = h => h.fam === 'gv' ? 'Groove kit' : h.fam === 'th' ? 'TH kit' : /_gh$/.test(h.id) ? 'GH kit' : h.genre === 'ukg' ? 'UKG kit' : h.genre === 'mth' ? 'Tech kit' : h.genre === 'sg' ? 'SG kit' : h.genre === 'ch' ? 'House kit' : 'Perc';
  $('#pads').innerHTML = hits.map((h, i) => padHTML(h, i, grp(h))).join('');
}
/* ---------- Halloween: a seasonal view with its own skin, pads and rows ---------- */
const hwItems = () => ITEMS.filter(i => i.group === 'halloween' || i.grp === 'halloween');
/* banks of 16 pads, laid out like the keyboard rows (1-4, Q-R, A-F, Z-V); all but the first are for Murder at Cameron Castle */
const HW_BANKS = {
  haunted: ['hw_fx_thunder', 'hw_fx_bell', 'hw_fx_organ', 'hw_fx_stinger', 'hv14', 'hv15', 'hv16', 'hv17', 'hw_fx_door', 'hw_fx_howl', 'hw_fx_chains', 'hw_fx_bats', 'hv18', 'hv21', 'hv22', 'hw_fx_whoosh'],
  castle: ['mc_fx_gong', 'mc01', 'mc02', 'mc_fx_reveal', 'mc12', 'mc13', 'mc03', 'mc15', 'mc05', 'mc06', 'mc07', 'mc04', 'mc14', 'mc08', 'hw_fx_thunder', 'hw_fx_stinger'],
  supper: ['mc22', 'mc18', 'mc36', 'mc37', 'mc19', 'mc20', 'mc21', 'mc30', 'mc23', 'mc24', 'mc25', 'mc26', 'mc27', 'mc28', 'mc29', 'mc34'],
  zf: ['mc39', 'mc40', 'mc41', 'mc42', 'mc43', 'mc44', 'mc45', 'mc46', 'mc_fx_flash', 'mc35', 'mc24', 'mc21', 'mc30', 'hw_fx_whoosh', 'hw_fx_bell', 'mc_fx_reveal'],
  names1: ['mc47', 'mc48', 'mc49', 'mc50', 'mc51', 'mc52', 'mc53', 'mc54', 'mc55', 'mc56', 'mc57', 'mc58', 'mc59', 'mc60', 'mc61', 'mc62'],
  names2: ['mc63', 'mc64', 'mc65', 'mc66', 'mc67', 'mc68', 'mc69', 'mc70', 'mc71', 'mc72', 'mc73', 'mc74', 'mc75', 'mc76', 'mc77', 'mc78'],
  drama: ['mc97', 'mc93', 'mc81', 'mc92', 'mc79', 'mc80', 'mc89', 'mc82', 'mc83', 'mc84', 'mc88', 'mc86', 'mc85', 'mc94', 'mc90', 'mc91']
};
const PAD_SHORT = { mc01: 'Welcome to the castle', mc03: "Someone's a killer", mc04: 'Nobody leaves', mc05: "The butler didn't do it", mc07: 'Dressed to kill', mc08: 'Live from the castle', mc15: "Darling, you're next",
  mc18: 'Old Money, live', mc19: 'Keyhole sees everything', mc20: 'Keyhole exclusive', mc21: 'Restless spirits', mc22: 'Supper is served', mc23: 'House rules', mc24: "You're all evidence", mc25: 'A whole library of secrets',
  mc26: "Who's in the will?", mc29: "Nobody's leaving", mc30: 'Is anybody there?',
  mc39: 'Silence, please: Zebedee', mc40: 'Noise for Zebedee', mc41: 'Zebedee, are you there?', mc42: 'Zebedee chop', mc43: 'Flash, get a picture',
  mc44: 'No photos, Flash', mc45: 'Flash! Flash! Flash!', mc46: 'McGillicuddy chop',
  mc51: 'Countess Fizzwick', mc66: 'Maestro Pricewell', mc70: 'Prof. Pennyfeather', mc90: 'Not at the table', mc91: 'Call the police' };
const HW_PADS = HW_BANKS.haunted;
S.hwBank = HW_BANKS[store.get('hwBank', 'haunted')] ? store.get('hwBank', 'haunted') : 'haunted';
const HW_GROUPS = [['Murder at Cameron Castle · drops for the party, a sung hook, a dinner gong and a reveal sting', i => i.hsub === 'castle' || (!!i.castle && !i.zf)],
  ['The Midnight Supper · drops from around the castle, and a lounge hook', i => i.hsub === 'supper'],
  ['Zebedee & Flash · the medium and the photographer: lines, sung name chops and a camera flash', i => i.hsub === 'zf' || !!i.zf],
  ['Name stabs · every character announced over an orchestral hit', i => i.hsub === 'names'],
  ['Drama stabs · accusations, objections and a dun-dun-DUN', i => i.hsub === 'drama'],
  ['Spoken drops · clean', i => i.hsub === 'spoken' && !i.explicit], ['Spoken drops · explicit', i => i.hsub === 'spoken' && i.explicit],
  ['Ad-libs & laughs', i => i.hsub === 'adlibs'], ['Chants & sung hooks · the witches\' chant is from Macbeth (1606)', i => i.hsub === 'sung'],
  ['FX · one-shots', i => i.cat === 'fx' && !i.bars && !i.castle], ['Loops · A minor (8A), 128 BPM; heartbeat and bubbles fit any key', i => i.cat === 'fx' && i.bars]];
function renderHw() {
  const all = hwItems().filter(i => !(S.hideExplicit && i.explicit));
  $$('[data-hwbank]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.hwbank === S.hwBank)));
  const bank = HW_BANKS[S.hwBank].map(id => BY_ID[id]).filter(Boolean);
  $('#hwPads').innerHTML = bank.map((h, i) => padHTML(h, i, h.cat === 'vox' ? (h.style === 'stab' ? 'Stab' : 'Voice') : 'FX', PAD_SHORT[h.id] || h.title.replace(/[.!]$/, ''))).join('');
  if (S.view === 'halloween') warmFiles(bank);
  $('#hwRows').innerHTML = HW_GROUPS.map(([l, f]) => { const list = all.filter(f); return list.length ? groupHead(l) + '<div class="rows">' + list.map(rowHTML).join('') + '</div>' : ''; }).join('');
  const he = $('#hideE2'); if (he) he.checked = S.hideExplicit;
  markRow();
}
function hitPad(t) {
  const it = BY_ID[t.dataset.pad]; selectItem(it.id); playItem(it, { overlap: true });
  t.classList.remove('hit'); void t.offsetWidth; t.classList.add('hit');
  clearTimeout(t._ht); t._ht = setTimeout(() => t.classList.remove('hit'), 140);
}
function markRow() {
  $$('.row').forEach(r => {
    const on = cur && cur.it.id === r.dataset.row;
    const b = r.querySelector('.pbtn');
    if (b.classList.contains('on') !== !!on) { b.classList.toggle('on', !!on); b.innerHTML = on ? ICON.stop : ICON.play; }
    if (!on) { const p = r.querySelector('.prc'); if (p) p.setAttribute('width', '0'); }
    r.classList.toggle('sel', S.sel.type === 'item' && S.sel.id === r.dataset.row);
  });
}
function selectItem(id) { S.sel = { type: 'item', id }; lcd(); markRow(); }

/* =====================================================================
   SAVING
   ===================================================================== */
const base = p => p.split('/').pop();
function readme() {
  const L = [];
  const pad = (x, n) => String(x).padEnd(n);
  L.push('UKG & MINIMAL PACK  (+ speed garage and classic house)', '=======================================================', '', 'Every file name carries its tempo, key and Camelot code. Everything sits between 128 and 140 BPM.', '');
  L.push('CONSTRUCTION KITS (MIDI)', '');
  KIT_FAMS.forEach(g => {
    L.push('-- ' + famTitle(g));
    P.KITS.filter(k => famOf(k) === g).forEach(k => { const key = P.keyInfo(k.root, k.mode); L.push(k.id + '  ' + pad(k.name, 20) + k.bpm + ' BPM  ' + pad(key.label + ' (' + key.camelot + ')', 18) + progression(k, 'A').map(b => b.map(n => n.replace(/69$/, '6/9')).join(' ')).join(' | ')); });
    L.push('');
  });
  L.push('Each kit: Drums Main / Variation / Fill, Bass A/B, Chords A/B, Melody A/B, an 8-bar arrangement and a FULL SONG');
  L.push('(type 1, seven tracks: Drums, Bass, Chords, Melody, Pad, Arp, Riser) with section markers.');
  L.push('  UK garage (144 bars):    Intro > Groove > Build > Drop 1 > Switch > Breakdown > Build 2 > Drop 2 > 4x4 switch > Outro');
  L.push('  Minimal (128 bars):      Intro (DJ) > Groove > Main > Breakdown > Build > Drop > Switch > Kick + bass switch > Outro (DJ)');
  L.push('  Speed garage (120 bars): Intro > Build > Drop 1 > Reese switch > Breakdown > Bass solo > Drop 2 > Final switch > Outro');
  L.push('  Classic house (128 bars): Intro (DJ) > Bassline > Verse > Chorus > Breakdown > Piano build > Drop > Switch > Outro (DJ)');
  L.push('  Garage house (136 bars): Intro (DJ) > Groove > Verse > Build > Drop 1 > Switch > Breakdown > Build 2 > Drop 2 > Outro (DJ)');
  L.push('  Groove house (120 bars): Intro (DJ, hats first) > Bassline > Chords > Lead > Breakdown > Build > Drop > Bass + drums > Outro (DJ)');
  L.push('  Tech house (120 bars):   Intro (DJ) > Groove (bass before the full kit) > Main > Break > Build > Drop (32 bars) > Kick + bass > Outro (DJ)');
  L.push('Builds: snare/clap roll crescendo (quarters to 32nds), climbing riser, CC74 filter + CC11 expression ramps, silent 8th before each drop.');
  L.push('', 'EXTENDED MIXES (one per kit, 16 tracks, 6 to 8 minutes)', '');
  GENRES.forEach(g => { const k = P.KITS.find(x => x.genre === g), x = P.buildExtended(k, k.root); L.push('  ' + pad(GENRE[g] + ' (' + x.bars + ' bars):', 30) + x.sections.map(s => s.name).join(' > ')); });
  L.push('Tracks: Kick, Snare & Clap, Hats & Ride, Percussion, Fills/Rolls/Cymbals (all GM drum notes on channel 10), Bass, Sub (speed garage', 'and house), Chords, Stabs, Melody, Counter-melody & Harmony, Pad, Arp, Vocal Chops (pitched triggers for a chop sampler),', 'Riser and FX Hits: note 36 impact, 38 reverse cymbal (one bar long, ends on the drop), 40 downlifter, 41 sub drop.');
  L.push('What changes: the drums move every 8 bars (hat patterns, rotating percussion layers, ghost notes, a different fill each phrase);', 'the bass alternates A/B with approach notes, octave pops and runs into the next chord; the second drop is reharmonised', '(a diatonic substitute in bar 3, a V7 turnaround on the last two beats); the melody picks up grace notes, anticipations', 'and a call-and-response sequence; the counter-melody answers in the gaps and the last drop doubles the melody a third below;', 'stabs, chops and arps follow the harmony 16th by 16th. Automation: CC74 filter, CC1 wobble (speed garage), CC11 swells,', 'CC71 resonance (acid section), CC91 reverb throws on the dub stabs. House: the final chorus lifts the key a whole step', '(key-signature change in the file); intro and outro stay in the home key for mixing.');
  L.push('Speed garage bass tracks also carry CC1 (mod wheel) ramps for wobble: map CC1 to your Reese LFO depth.');
  L.push('Breakdowns introduce a new progression per kit on the Pad and Arp tracks with sub roots on the Bass track.');
  L.push('', 'NEW SONGS WITH EXTRA LAYERS (two per genre, plus four garage-house, three groove-house and four tech-house songs, built on current sounds in each scene)', '');
  P.KITS.filter(k => k.ext).forEach(k => {
    const key = P.keyInfo(k.root, k.mode), x = P.buildExtended(k, k.root);
    L.push(k.id + '  ' + k.name + '  (' + k.bpm + ' BPM, ' + key.label + ', ' + key.camelot + ', ' + x.bars + ' bars, ' + P.EXT_TRACKS.filter(t => x.T[t.key].length).length + ' tracks)');
    L.push('  ' + x.sections.map(s => s.name).join(' > '));
    L.push('  ' + EXT_HINT[k.id]);
    const voc = [...new Set(x.vocalCues.map(c => c.id))].map(id => id.toUpperCase() + ' "' + P.VOCALS[id].title + '"');
    if (voc.length) L.push('  Vocals: ' + voc.join(', ') + ' (4-bar loops, each cue marked on the Vocal Guide track)');
    if (x.shouts.length) L.push('  Shouts track: ' + x.shouts.map((id, i) => 'note ' + (48 + i) + ' ' + (P.ADLIBS[id] || id) + ' (' + id.toUpperCase() + ')').join(', '));
    L.push('');
  });
  L.push('How the new layers are written:',
    '  Vocal Guide: the hook melody as sung, with each word as a MIDI lyric (FF 05) and a cue point (FF 07) naming the pack file at the',
    '  start of every 4-bar block. Drop that WAV on the cue. The melody (Melody track) rests while the vocal sings and the vocal chops step around it.',
    '  Break Slices: one bar of break cut into 16 equal slices on MIDI notes 36-51 (C1-D#2 in Ableton and Logic), as a sampler in slice mode lays them out.',
    '  Shouts: one ad-lib per key from MIDI note 48 (C2 in Ableton and Logic) upward; the track name lists them. Load the pack WAVs onto those keys.',
    '  Pitch bends (Reese dives, hoover swoops, brass falls, siren LFOs, bounce and wub dips, tape stops) assume a +/-12 semitone bend range.',
    '  The files set it with RPN 0 on each bent track; if your synth ignores that, set its bend range to 12.',
    '  FX Hits adds note 43 rewind spin-back and 44 tape stop. Sidechain Trigger: note 35 at velocity 1 on every kick, to key a compressor.',
    '  Layer tracks (acid, donk, piano, organ, strings, brass, orchestra hits, choir, talkbox, hoover, siren, shouts) take whichever MIDI channels',
    '  are free; the vocal guide shares channel 12 with the vocal chops (same program).', '');
  L.push('Regions are 4 bars at 480 PPQ with tempo, 4/4 and key signature. Swing is written into the timing.');
  L.push('Drums use General MIDI notes: ' + P.DRUM_ORDER.map(l => P.DRUM[l].label + ' ' + P.DRUM[l].note).join(', ') + '.', '');
  L.push('VOCALS (WAV, processed + Dry folder)', '');
  voxItems().forEach(i => L.push(pad(i.id.toUpperCase(), 6) + (i.explicit ? '[E] ' : '    ') + pad(GSHORT[i.genre] || '', 13) + pad('"' + i.title + '"', 52) + pad(i.bpm + ' BPM', 9) + pad(i.key ? i.key + (i.camelot ? ' (' + i.camelot + ')' : '') : 'any key (spoken)', 20) + i.voice));
  const tag = i => [i.bars ? i.bars + (i.bars > 1 ? ' bars' : ' bar') : '', i.bpm && i.bars ? i.bpm + ' BPM' : '', i.key ? i.key + (i.camelot ? ' (' + i.camelot + ')' : '') : ''].filter(Boolean).join(', ');
  L.push('', 'LOOPS AND ONE-SHOTS: HOW THEY ARE CUT', '',
    'Loops are cut to whole bars at the tempo in the name. Reverb and delay tails are folded back onto the start, so every loop',
    'repeats without a gap or click. Drum loops carry each kit\'s own groove and swing and are named after the kit (UKG01_DrumLoop_134bpm).',
    'Top and percussion loops have no kick, for layering over your own. UKG_Break_SliceReady is one straight bar: cut it into 16 equal slices',
    'and they line up with the Break Slices track in UKG07. The 2-Step Sub, Reese Classic and Organ Bass sets have one file per note,',
    'labelled by note, to load chromatically into a sampler. Bass, acid and keys loops come from the new songs (UKG07/08, MTH07/08,',
    'SG07/08, CH07/08) at the song\'s tempo and key, so they drop straight onto those MIDI kits.', '');
  L.push('GARAGE HOUSE (DISCLOSURE-STYLE)', '',
    'Original sounds in the style of Disclosure\'s early garage-house records, made from scratch here: no audio or melodies from their music.',
    'A round tuned kick, plate clap, crisp hats, snap, shaker and tambourine (GH kit in Drum Hits), four 128 BPM grooves built from that kit,',
    'a detuned-saw Chord Pluck and a sine-and-square Glide Bass, each sampled every minor third. The Glide Bass notes loop cleanly in a sampler.',
    'Kits GH01-GH04 are garage-house songs built on these sounds (MIDI Kits/Garage House): layered clap and snap (the snap is GM note 40),',
    'swung hats, pumping chord plucks, glide or warble basses and square-wave hooks, each with a full song and an extended mix that carries',
    'a hook written for it (H19-H22) plus answering vocals from the pack. The extended mixes add vocal chops through whole sections',
    '(they rest wherever the hook is singing).', '');
  L.push('GROOVE HOUSE (CHRIS STUSSY-STYLE)', '',
    'Original sounds in the spirit of Chris Stussy\'s groove house, made from scratch here: no audio or melodies from his records.',
    'A warm, bouncy kick, a clap into a long dark hall, a snappy snare, a woody rim, vintage hats with a 10-bit edge, a shaker and an FM',
    'glitch tick (Groove kit in Drum Hits); loops from the kits plus a dusty, bit-crushed break filler; the FM Rubber Bass (two-operator FM,',
    'the index falling fast for the pluck), the Glow Saw (two saws, a closing ladder filter, a long release, a slow stereo phaser) and the',
    'Fifths Lead (a Juno-style lead that plays every note with its fifth), each sampled every minor third. In the player the Glow Saw has',
    'its own reverb that ducks while a chord sounds and swells up in the gaps; the extended mixes write the same move as CC91 on the chords.',
    'Kits GV01-GV03 (MIDI Kits/Groove House): heavy swing (58-60%), basslines that start on the roots and turn into their own melody,',
    'chord stabs with room to breathe, rootless voicings that share notes from chord to chord, and leads built on fifths.', '');
  L.push('TECH HOUSE (CHRIS LAKE & RIORDAN-STYLE)', '',
    'Original sounds in the spirit of Chris Lake\'s and Riordan\'s tech house, made from scratch here: no audio or melodies from their records.',
    'A fat, punchy kick with a resonant bump, a hard distorted snare with a short room, a layered clap, a bright rim, driving hats, a two-grain',
    'shaker, three tuned toms and a metal ping (TH kit in Drum Hits); drum, top and skippy UK-house loops and a four-bar snare build; the',
    'Bounce Bass (bright and sustained: the player adds a resonant low-pass envelope that opens much further on hard-hit notes; set your',
    'sampler\'s filter envelope the same way, velocity to cutoff), the Fifth Bass (Moog-style), the Crushed Reese and the Rave Stab (chord',
    'memory: every key plays a minor chord), each sampled every minor third; bass and stab loops from the kits; eight vocal shots',
    '(TS01-TS08: driven, a little bit-crushed, a short room and one slap); an endless Shepard riser, an uplifter, a downlifter and an impact.',
    'Kits TH01-TH04 (MIDI Kits/Tech House): the bassline is the hook, accented on the kick (velocity opens the filter), snare or layered',
    'clap on the backbeat, offbeat toms and rims, short breaks and long drops; TH01 and TH02 place the vocal shots on the offbeats.', '');
  L.push('VOCALS: HOW THEY ARE MADE', '',
    'Sung hooks, chop loops, vocal pads and stabs are sung by a synthesis engine: each line is spoken as a whole phrase, then sung',
    'syllable by syllable onto the melody (consonants land just before the beat, vowels hold the note, with scoops, legato glides,',
    'delayed vibrato and small timing and tuning drift). Every lead is the best of up to ten takes, picked by a speech recogniser, and',
    'hooks add two doubles and a harmony that follows the chords. All lyrics are original. Every vocal comes wet (mixed, in the folder)',
    'and dry (lead only, in Dry/), loops are cut to whole bars with tails folded back onto the start, and the first consonant and breath',
    'of a loop sit at its end, so it repeats cleanly. Vocal pads hold each kit\'s chords in four voices on "ooh" or "aah".',
    'The hard-tuned and robot lines are re-made from the same voices, and the spoken lines and ad-libs carry a synthesised top',
    'octave (12-18 kHz), so every vocal sits as brightly as the sung ones; radio and dark styles stay band-limited on purpose.', '');
  L.push('HALLOWEEN (a seasonal set, in its own Halloween folder)', '',
    'Spoken drops (clean and explicit), ad-libs, an evil laugh and a witch\'s cackle, the witches\' chant from Macbeth (1606, public',
    'domain) sung as a 2-bar coven loop and whispered by three voices, and three original sung hooks on MTH01, CH07 and UKG02.',
    'FX are synthesised from scratch: thunder, a church bell struck on A, a pipe-organ stab in A minor, a creaking door, a wolf howl,',
    'a horror string stinger, chains, bats and a ghost whoosh. The theremin, music box and ghost choir loops are in A minor (8A) at',
    '128 BPM, so they sit on MTH01, MTH07, GH01, CH01 and CH08; the heartbeat and cauldron loops have no key. The laughs are built',
    'syllable by syllable on a vocoder; the evil laugh is slowed like a record for size.',
    'Murder at Cameron Castle (its own folder) is a set for a murder-mystery party: a welcome, the title drop, accusations and',
    'ad-libs (clean and explicit), a sung hook "Murder at Cameron Castle, everybody\'s a suspect" on GH01, a dinner gong on A and a',
    'dun-dun-DUN reveal sting. The Midnight Supper drops come from around the castle: the Old Money podcast, The Keyhole, the',
    'house rules, the medium, the photographer, the will, the art and the car out front, plus a lounge hook "Midnight Supper, every',
    'secret\'s on the menu" on UKG02. Zebedee & Flash: lines for the medium and the photographer, each name sung as a 2-bar chop',
    'loop in A minor at 128 BPM (Zebedee on MTH01, Flash on GH01), and a camera flash. Name Stabs: every character announced',
    'over an orchestral hit on A minor. Drama Stabs: accusations, objections, a dun-dun-DUN and two explicit ones.', '');
  L.push('BASS & KEYS (WAV)', '');
  bkItems().forEach(i => L.push(pad(base(i.pack), 70) + tag(i)));
  L.push('', 'LOOPS, FILLS & FX (WAV)', '');
  fxItems().forEach(i => L.push(pad(base(i.pack), 70) + tag(i)));
  L.push('', 'DRUM HITS (WAV)', '');
  ITEMS.filter(i => i.cat === 'hits').forEach(i => L.push(base(i.pack) + (i.key ? '  tuned to ' + i.key : '')));
  return new TextEncoder().encode(L.join('\r\n') + '\r\n');
}
function allKitFiles(pathPrefix, genre) {
  const files = [];
  P.KITS.filter(k => !genre || (genre === 'gh' || NEW_FAMS.has(genre) ? famOf(k) === genre : k.genre === genre && !NEW_FAMS.has(k.fam))).forEach(k => {
    const key = P.keyInfo(k.root, k.mode);
    const dir = pathPrefix + GENRE[famOf(k)] + '/' + kitFolder(k, key, k.bpm) + '/';
    P.kitMidiFiles(k, k.root, k.bpm).forEach(f => files.push({ name: dir + f.name, data: f.data }));
  });
  return files;
}
function itemFiles(it, prefix) {
  const f = [{ name: prefix + base(it.pack), url: it.file }];
  if (it.dry) f.push({ name: prefix + 'Dry/' + base(it.packDry), url: it.dry });
  return f;
}
async function saveItem(it) {
  await guarded(async () => {
    const parts = await packFiles(itemFiles(it, ''), 'Packing');
    await offer(base(it.pack).replace(/\.wav$/, '') + '.zip', parts);
  });
}
async function saveLane(part) {
  const kit = curKit(), st = ks(kit), key = P.keyInfo(st.pc, kit.mode);
  const files = [P.regionMidi(kit, part, st.vars[part], st.pc, st.bpm)];
  if (part === 'drums') files.push(P.regionMidi(kit, 'drums', 'fill', st.pc, st.bpm));
  await guarded(async () => {
    const name = kit.id + '_' + P.slug(kit.name) + '_' + P.PART_LABEL[part] + '_' + key.short.replace('#', 's') + '-' + key.camelot + '_' + st.bpm + 'bpm.zip';
    await offer(name, P.makeZipParts(files.map(f => ({ name: f.name, data: f.data }))));
  });
}
async function saveSong() {
  const kit = curKit(), st = ks(kit);
  await guarded(async () => {
    const f = S.songV === 'ext' ? P.extendedMidi(kit, st.pc, st.bpm) : P.songMidi(kit, st.pc, st.bpm);
    await offer(f.name.replace(/\.mid$/, '.zip'), P.makeZipParts([{ name: f.name, data: f.data }]));
  });
}
async function saveKit() {
  const kit = curKit(), st = ks(kit), key = P.keyInfo(st.pc, kit.mode);
  const dir = kitFolder(kit, key, st.bpm) + '/';
  await guarded(async () => {
    const files = P.kitMidiFiles(kit, st.pc, st.bpm).map(f => ({ name: dir + f.name, data: f.data }));
    if (kit.ext) {
      const sg = songOf(kit, st, 'ext');
      [...new Set(sg.vocalCues.map(c => c.id).concat(sg.shouts))].forEach(id => { const it = BY_ID[id]; if (it) files.push({ name: dir + 'Vocals used in the extended mix/' + base(it.pack), url: it.file }); });
    }
    const parts = files.some(f => f.url) ? await packFiles(files, 'Packing') : P.makeZipParts(files);
    await offer(kit.id + '_' + P.slug(kit.name) + '_' + key.short.replace('#', 's') + '-' + key.camelot + '_' + st.bpm + 'bpm_' + (kit.ext ? 'MIDI+Vocals' : 'MIDI') + '.zip', parts);
  });
}
async function saveAllKits() {
  await guarded(async () => {
    const files = allKitFiles('MIDI Kits/');
    files.push({ name: 'MIDI Kits/README.txt', data: readme() });
    await offer('All-Genres_MIDI-Kits_' + (files.length - 1) + '-files.zip', P.makeZipParts(files));
  });
}
async function saveGroup(items, zipName, label) {
  await guarded(async () => {
    const list = [];
    items.forEach(it => itemFiles(it, '').forEach(f => { f.name = it.pack.split('/').slice(0, -1).concat(f.name).join('/'); list.push(f); }));
    list.push({ name: 'README.txt', data: readme() });
    const parts = await packFiles(list, label);
    await offer(zipName, parts);
  });
}
async function saveFullPack(genre) {
  await guarded(async () => {
    const root = (genre ? GENRE[genre] + ' Pack' : 'UKG & Minimal Pack') + '/';
    const list = allKitFiles(root + 'MIDI Kits/', genre);
    const ghVox = new Set(P.KITS.filter(k => k.fam === 'gh').flatMap(k => k.ext.form.flatMap(f => [f[3].vocal, f[3].vocal2]).filter(Boolean)));
    const famVox = new Set(P.KITS.filter(k => k.fam === genre && k.ext).flatMap(k => k.ext.form.flatMap(f => [f[3].vocal, f[3].vocal2]).concat(k.ext.shouts || []).filter(Boolean)));
    const inPack = it => genre === 'gh' ? /Garage House \(Disclosure-style\)/.test(it.pack) || ghVox.has(it.id) : NEW_FAMS.has(genre) ? it.fam === genre || famVox.has(it.id) :
      !genre || ((it.genre === genre || it.genre === 'all') && !NEW_FAMS.has(it.fam));
    ITEMS.filter(it => it.pack && inPack(it)).forEach(it => {
      const dir = it.pack.split('/').slice(0, -1).join('/');
      list.push({ name: root + it.pack, url: it.file });
      if (it.dry) list.push({ name: root + dir + '/Dry/' + base(it.packDry), url: it.dry });
    });
    list.push({ name: root + 'README.txt', data: readme() });
    const parts = await packFiles(list, 'Packing');
    await offer(genre ? GENRE[genre].replace(/ /g, '-') + '_Pack.zip' : 'UKG-Minimal-Pack_full_all-genres.zip', parts);
  });
}
async function saveSet(setId) {
  const mem = ITEMS.filter(i => i.set === setId);
  await guarded(async () => {
    const dir = mem[0].pack.split('/').slice(-2, -1)[0] + '/';
    const parts = await packFiles(mem.map(i => ({ name: dir + base(i.pack), url: i.file })), 'Packing');
    await offer(setId + '_' + mem.length + '-notes.zip', parts);
  });
}

/* =====================================================================
   CLAUDE ON THE PAGE: ask for a starting point, a crate saved to the account,
   who else is listening, comments and "ask Claude about this kit".
   Each feature stays hidden until its capability answers for this viewer.
   ===================================================================== */
const CAP = { sample: null, comments: null, room: null, user: null, perms: null, crateDoc: null, dbAsk: false, toClaude: 'off', live: false };
const capUse = n => { try { return window.claude && typeof window.claude.use === 'function' ? Promise.resolve(window.claude.use(n)).catch(() => null) : Promise.resolve(null); } catch (e) { return Promise.resolve(null); } };
S.picks = store.get('picks', null); S.picksOpen = store.get('picksOpen', true) !== false;
async function capsInit() {
  [CAP.sample, CAP.comments, CAP.room, CAP.user, CAP.perms] = await Promise.all(['sample', 'comments', 'room', 'user', 'permissions'].map(capUse));
  if (CAP.sample) $('#askBtn').hidden = false;
  if (CAP.room) { try { CAP.toClaude = await CAP.room.canSendToClaudeSession(); } catch (e) { CAP.toClaude = 'off'; } }
  kitCapsUI();
  let st = {};
  if (CAP.perms) { try { st = (await CAP.perms.state()) || {}; } catch (e) { st = {}; } }
  if (st.db === 'granted') await crateSync();   // no prompt needed: sync straight away
  else if (st.db === 'prompt') CAP.dbAsk = !!(await capUse('db'));   // ask only when the viewer chooses to save it
  renderSyncNote(); labAskUI();
  if (CAP.room && st.room === 'granted') roomLive();
}

/* ---------- the crate, kept in the viewer's own account (data/users/<id>/crate) ---------- */
async function crateSync() {
  const db = await capUse('db'), user = CAP.user || await capUse('user');
  if (!db || !user) return false;
  let uid = null; try { uid = await user.id(); } catch (e) {}
  if (!uid) return false;
  let ref; try { ref = db.doc('data/users/' + uid + '/crate'); } catch (e) { return false; }
  CAP.crateDoc = ref; CAP.dbAsk = false;
  soundsSync(db, uid);
  let first = true;
  ref.onSnapshot(snap => {
    const d = snap && snap.exists ? snap.data() : null;
    const remote = d && Array.isArray(d.ids) ? d.ids.filter(id => typeof id === 'string' && BY_ID[id]) : [];
    if (first) {   // three-way merge: what this browser added or removed since the last sync, applied to the account copy
      first = false;
      const base = new Set(store.get('crateBase', [])), local = S.crate;
      const merged = new Set(remote.concat([...local].filter(i => !base.has(i))));
      [...base].filter(i => !local.has(i)).forEach(i => merged.delete(i));
      setCrate(merged);
      if (merged.size !== remote.length || remote.some(i => !merged.has(i))) pushCrate(); else store.set('crateBase', remote);
      renderSyncNote();
      return;
    }
    if (snap.metadata && snap.metadata.hasPendingWrites) return;
    setCrate(new Set(remote)); store.set('crateBase', remote);
  }, () => { CAP.crateDoc = null; renderSyncNote(); });
  return true;
}
let crateT = 0, crateWriting = false, crateAgain = false;
function queueCrate() { if (!CAP.crateDoc) return; clearTimeout(crateT); crateT = setTimeout(pushCrate, 700); }
async function pushCrate() {
  const ref = CAP.crateDoc;
  if (!ref) return;
  if (crateWriting) { crateAgain = true; return; }
  crateWriting = true;
  const ids = [...S.crate];
  try { await ref.set({ ids, updated: new Date().toISOString() }); store.set('crateBase', ids); }
  catch (e) { if (e && ['invalid_argument', 'revoked', 'not_granted', 'capability_disabled', 'capability_removed'].includes(e.code)) { CAP.crateDoc = null; renderSyncNote(); } }
  finally { crateWriting = false; if (crateAgain) { crateAgain = false; pushCrate(); } }
}
function setCrate(set) {
  S.crate = set; store.set('crate', [...set]);
  $$('[data-star]').forEach(b => { const on = S.crate.has(b.dataset.star); b.setAttribute('aria-pressed', String(on)); b.setAttribute('aria-label', on ? 'Remove from crate' : 'Add to crate'); });
  $('#tCrate').textContent = S.crate.size || '';
  if (S.view === 'crate') renderCrate();
}
function renderSyncNote() {
  const n = $('#syncNote');
  if (!n) return;
  if (CAP.crateDoc) n.textContent = 'Saved to your account, so the crate follows you to your other devices.';
  else if (CAP.dbAsk) n.innerHTML = 'The crate is kept in this browser. <button class="btn ghost" id="syncCrate">Save it to your account</button>';
  else n.textContent = 'The crate is kept in this browser.';
}

/* ---------- who else has the page open, and what they are playing ---------- */
let presT = 0, peersSeq = 0;
function roomLive() {
  const room = CAP.room;
  if (!room || CAP.live) return;
  CAP.live = true;
  room.onPeers(ch => renderPeers(ch.peers), () => { CAP.live = false; renderPeers([]); });
  sendPresence();
}
function sendPresence() {
  if (!CAP.live) return;
  clearTimeout(presT);
  presT = setTimeout(() => {
    CAP.room.presence({ kit: S.kitId, genre: themeGenre(), view: S.view, playing: S.playing ? (seq.mode === 'song' ? S.songV : 'loop') : null }).catch(() => {});
  }, 200);
}
async function renderPeers(peers) {
  const box = $('#live'), mine = ++peersSeq;
  const others = (peers || []).filter(p => p && !p.isMe && p.kind === 'viewer');
  if (!others.length) { box.hidden = true; box.textContent = ''; return; }
  const ids = [...new Set(others.map(p => p.by).filter(Boolean))];
  let ps = {};
  if (ids.length && CAP.user) { try { ps = (await CAP.user.profiles(ids)) || {}; } catch (e) { ps = {}; } }
  if (mine !== peersSeq) return;
  const kitIn = p => { const k = p.presence && p.presence.kit; return typeof k === 'string' && kitOf(k) ? kitOf(k) : null; };
  const nameOf = p => (p.by && ps[p.by] && ps[p.by].name) || 'Someone';
  const wrap = document.createElement('span'); wrap.className = 'peers';
  others.slice(0, 5).forEach(p => {
    const pr = p.by ? ps[p.by] : null, k = kitIn(p), on = !!(p.presence && typeof p.presence.playing === 'string');
    const b = document.createElement('button');
    b.className = 'peer' + (on ? ' on' : ''); b.dataset.peerkit = k ? k.id : '';
    const tip = nameOf(p) + (k ? (on ? ' is playing ' : ' is looking at ') + k.id + ' ' + k.name + '. Open it.' : ' is here.');
    b.title = tip; b.setAttribute('aria-label', tip);
    if (pr && pr.avatarUrl) { const img = document.createElement('img'); img.alt = ''; img.src = pr.avatarUrl; b.append(img); }
    else if (pr && pr.color) b.style.background = pr.color;
    wrap.append(b);
  });
  const lt = document.createElement('span'); lt.className = 'lt';
  const k0 = kitIn(others[0]);
  lt.textContent = others.length === 1 ? nameOf(others[0]) + (k0 ? ' on ' + k0.id : ' is here') : others.length + ' others here';
  box.textContent = ''; box.append(wrap, lt); box.hidden = false;
}
function jumpToKit(k, ev) {
  const genreMoves = S.genre !== 'all' && famOf(k) !== S.genre, skinMoves = genreMoves || (S.genre === 'all' && famOf(k) !== famOf(curKit()));
  const go = () => {
    if (genreMoves) { S.genre = famOf(k); store.set('genre', S.genre); renderGenres(); renderKitList(); renderPads(); updateCounts(); renderTools(); }
    if (S.kitId !== k.id) { S.kitId = k.id; kitChanged(true); }
    applyTheme();
    if (S.view !== 'kits') setView('kits', true);
  };
  if (skinMoves) transition(go, ev); else go();
}

/* ---------- the kit detail's own Claude and comment buttons ---------- */
const CAN_SEND = ['available', 'available_if_summoned', 'available_if_opened'];
function kitCapsUI() {
  const c = $('#kitComment'), s = $('#kitToClaude');
  if (c) c.hidden = !CAP.comments;
  if (s) s.hidden = !(CAP.room && CAN_SEND.includes(CAP.toClaude));
}
function sendKitToClaude() {   // called straight from the click, so the platform sees the viewer's gesture
  const room = CAP.room;
  if (!room) return;
  const kit = curKit(), st = ks(kit), key = P.keyInfo(st.pc, kit.mode), prog = $$('#prog span').map(x => x.textContent.trim()).join(' ');
  const data = { label: kit.id + ' ' + kit.name, kit: kit.id, name: kit.name, genre: GENRE[famOf(kit)], style: kit.style, key: key.label, camelot: key.camelot, bpm: st.bpm,
    swing: Math.round(kit.swing * 100) + '%', chords: prog.slice(0, 300), vibe: kit.vibe, song: S.songV === 'ext' ? 'extended mix' : 'full song' };
  const deliver = CAP.toClaude === 'available_if_summoned' ? 'send' : 'stage';
  room.sendToClaudeSession(data, { deliver }).then(r => {
    toast(r && r.to === 'pane' ? (deliver === 'send' ? 'Sent ' + kit.id + ' to Claude.' : kit.id + ' is in your message to Claude. Ask anything about it.') : 'Taking ' + kit.id + ' to your chat with Claude.', 3200);
  }, e => {
    toast(e && e.code === 'rate_limited' ? 'Give it a moment, then try again.' : "Couldn't reach your chat with Claude from here.");
    room.canSendToClaudeSession().then(v => { CAP.toClaude = v; kitCapsUI(); }, () => { CAP.toClaude = 'off'; kitCapsUI(); });
  });
}
function commentOnKit() {
  const c = CAP.comments, el = $('#kitdetail .kd-top');
  if (!c || !el) return;
  c.openComposer({ element: el }).catch(e => {
    const code = e && e.code;
    if (code === 'rate_limited') toast('Give it a moment, then try again.');
    else if (['unavailable', 'not_granted', 'capability_disabled', 'capability_removed', 'forbidden'].includes(code)) { CAP.comments = null; kitCapsUI(); toast('Commenting from the page is off here.'); }
  });
}

/* ---------- ask Claude for a starting point: a kit, its key and tempo, and sounds that mix with it ---------- */
const ASK_EG = {
  ukg: ['Moody 2-step for a late-night pirate set, with a female hook', 'Bouncy garage with organ stabs and a wobbling sub'],
  gh: ['Sunset garage house with a soulful hook and chord plucks', 'Warm 2-step house for a summer rooftop'],
  mth: ['Hypnotic minimal roller with a whispered vocal', 'Dub chords and bleeps for a 4am afterhours'],
  sg: ['Heavy speed garage with sirens, Reese and MC shouts', 'Ragga-flavoured speed garage for a dubplate'],
  ch: ['Gospel piano house with a big diva hook', 'Filter disco with strings and a jacking groove'],
  gv: ['Rolling groove house with an FM bass and chords that breathe', 'Late-night jazzy house with Rhodes and bongos'],
  th: ['Bouncy tech house with a plucky bass and vocal shots', 'UK house with rave stabs, organ and a crushed Reese'],
  hw: ['A spooky minimal roller for a Halloween party', 'Garage with a witchy chant, thunder and an evil laugh']
};
const ASK_ERR = {
  not_granted: 'Asking Claude is switched off for this page.', sampling_disabled: "Claude isn't available on this account.", not_declared: 'Asking Claude is switched off for this page.',
  capability_disabled: "Asking Claude doesn't work in this view.", capability_removed: "Asking Claude doesn't work in this view.",
  rate_limited: 'Claude is busy for you right now. Try again in a minute.', session_expired: 'Sign in to claude.ai again, then ask.',
  refused: "Claude didn't pick for that request. Describe the sound or mood you want instead.", empty_completion: "Claude didn't answer that one. Try a shorter request.",
  invalid_json: "Claude's answer didn't come back as a pick list. Try again.", prompt_too_large: 'That request is too long. Try a shorter one.', upstream_error: "The request didn't reach Claude. Try again."
};
const ASK_OFF = ['not_granted', 'sampling_disabled', 'not_declared', 'capability_disabled', 'capability_removed'];
const VOX_KIND = { hooks: 'sung hook, 4-bar loop', pads: 'vocal pad, 4-bar loop', brat: 'brat talk or chant', clean: 'spoken line', explicit: 'spoken line, explicit', tuned: 'hard-tuned or robot hook', chops: 'vocal chop loop', stabs: 'vocal stab, one note', adlibs: 'ad-lib' };
const HW_KIND = { spoken: 'Halloween spoken drop', adlibs: 'Halloween ad-lib or laugh', sung: 'Halloween chant or sung hook', castle: 'murder-mystery party drop (Murder at Cameron Castle)', supper: 'murder-mystery party drop (Murder at Cameron Castle)', zf: 'murder-mystery party drop or name chop (Murder at Cameron Castle)', names: "a character's name announced over an orchestral hit (Murder at Cameron Castle)", drama: 'dramatic stab (Murder at Cameron Castle)' };
const kindOf = it => it.group === 'halloween' ? HW_KIND[it.hsub] || 'Halloween vocal' : it.grp === 'halloween' ? 'Halloween FX ' + (it.bars ? 'loop' : 'one-shot') : it.cat === 'vox' ? VOX_KIND[it.group] || 'vocal' : it.cat === 'bass' || it.cat === 'keys' ? it.cat + (it.bars ? ' loop' : ' one-shot') : it.cat === 'loops' ? 'drum loop' : it.cat === 'fills' ? 'drum fill' : 'FX';
function askPrompt(q) {
  const kits = P.KITS.map(k => { const key = P.keyInfo(k.root, k.mode); return [k.id, GENRE[famOf(k)], k.style, k.bpm + ' BPM', key.label + ' (' + key.camelot + ')', 'swing ' + Math.round(k.swing * 100) + '%', k.vibe].join(' | '); });
  const sounds = ITEMS.filter(i => ['vox', 'bass', 'keys', 'loops', 'fills', 'fx'].includes(i.cat) && !i.set && !i.hidden && !(S.hideExplicit && i.explicit)).map(i => [
    i.id, kindOf(i), GENRE[itemGenre(i)] || 'any genre', i.cat === 'vox' && i.group !== 'chops' && i.group !== 'pads' ? '"' + i.title + '"' : i.title,
    i.bpm ? i.bpm + ' BPM' : '', i.key ? i.key + (i.camelot ? ' (' + i.camelot + ')' : '') : i.cat === 'vox' ? 'any key' : '', i.voice || '', (i.note || '').slice(0, 70)].filter(Boolean).join(' | '));
  return 'A music producer is browsing a sample pack of UK garage, garage house, minimal tech house, speed garage and classic house, and asked:\n"' + q + '"\n\n' +
    'The pack also has a Halloween set (spooky vocals, FX and loops); use it when the request is spooky or for a Halloween party.\n' +
    'Choose the best starting point from the pack below.\n' +
    '1. One kit from KITS. You may set its tempo anywhere from 128 to 140 BPM and move it to another key, but its mode (major or minor) stays its own.\n' +
    '2. Six to ten sounds from SOUNDS that suit the request and mix with that kit: keyed sounds in the kit\'s key or a neighbouring Camelot code, loops within about 4 BPM of the tempo. Spoken lines and ad-libs fit any key. Mix the types (vocals, bass or keys, loops or FX) unless the request asks for something narrower.\n' +
    (S.genre !== 'all' ? 'The producer is browsing ' + GENRE[S.genre] + ': prefer it unless the request points to another genre.\n' : '') +
    (S.hideExplicit ? 'Leave out explicit vocals.\n' : '') +
    'Use only ids that appear below.\n\n' +
    'Reply with only a JSON object like:\n{"kit":"UKG04","key":"A minor","bpm":134,"summary":"One or two sentences on why this kit and these sounds fit.","picks":[{"id":"h12","why":"A few words on why it fits."}]}\n\n' +
    'KITS (id | genre | style | tempo | key | swing | sound):\n' + kits.join('\n') + '\n\n' +
    'SOUNDS (id | type | genre | title | tempo | key | voice | notes):\n' + sounds.join('\n');
}
function keyPc(s) {
  const m = /^\s*([A-Ga-g])\s*(#|♯|b|♭)?/.exec(String(s || ''));
  if (!m) return null;
  return (PCN[m[1].toUpperCase()] + (m[2] === '#' || m[2] === '♯' ? 1 : m[2] === 'b' || m[2] === '♭' ? -1 : 0) + 12) % 12;
}
function validatePicks(r) {
  if (!r || typeof r !== 'object' || Array.isArray(r)) return null;
  const kit = kitOf(String(r.kit || '').toUpperCase().trim());
  if (!kit) return null;
  let bpm = Math.round(Number(r.bpm));
  if (!(bpm >= 128 && bpm <= 140)) bpm = kit.bpm;
  const pc = keyPc(r.key), seen = new Set(), picks = [];
  (Array.isArray(r.picks) ? r.picks : []).forEach(x => {
    const id = String((x && x.id) || '').trim(), it = BY_ID[id];
    if (it && !seen.has(id) && ['vox', 'bass', 'keys', 'loops', 'fills', 'fx', 'hits'].includes(it.cat) && !(S.hideExplicit && it.explicit)) { seen.add(id); picks.push({ id, why: String((x && x.why) || '').slice(0, 200) }); }
  });
  return { kit: kit.id, bpm, pc: pc == null ? kit.root : pc, summary: String(r.summary || '').slice(0, 600), picks: picks.slice(0, 12) };
}
let askCtl = null, askTimer = 0;
function askBusy(on) { $('#askGo').disabled = on; $('#askQ').disabled = on; $('#askStop').hidden = !on; $$('#askEg button').forEach(b => { b.disabled = on; }); }
function renderAskExamples() { $('#askEg').innerHTML = (ASK_EG[themeGenre()] || ASK_EG.ukg).map(x => '<button type="button" data-eg="' + esc(x) + '">' + esc(x) + '</button>').join(''); }
async function askClaude(q) {
  q = String(q || '').trim().slice(0, 300);
  const msg = $('#askMsg');
  if (!q) { msg.textContent = 'Describe the track first: a mood, a tempo, a vocal, a club.'; return; }
  if (!CAP.sample || askCtl) return;
  askCtl = new AbortController(); askBusy(true);
  const t0 = Date.now(), tick = () => { msg.innerHTML = 'Claude is listening through the pack<span class="dots"></span> <span class="tnum">' + Math.round((Date.now() - t0) / 1000) + ' s</span>'; };
  tick(); askTimer = setInterval(tick, 1000);
  try {
    const res = validatePicks(await CAP.sample.json(askPrompt(q), { signal: askCtl.signal }));
    if (!res) { msg.textContent = "Claude's answer didn't name a kit from the pack. Try again, or reword the request."; return; }
    msg.textContent = '';
    applyPicks(res, q);
  } catch (e) {
    const code = e && e.code;
    msg.textContent = code === 'cancelled' ? '' : ASK_ERR[code] || ASK_ERR.upstream_error;
    if (ASK_OFF.includes(code)) { CAP.sample = null; $('#askBtn').hidden = true; }
  } finally { clearInterval(askTimer); askCtl = null; askBusy(false); }
}
function applyPicks(res, q) {
  S.picks = { q, kit: res.kit, bpm: res.bpm, pc: res.pc, summary: res.summary, picks: res.picks };
  S.picksOpen = true; store.set('picks', S.picks); store.set('picksOpen', true);
  openPicksKit(null, () => {
    const box = $('#picks');
    if (box && !box.hidden) window.scrollTo({ top: box.getBoundingClientRect().top + scrollY - $('#deck').offsetHeight - 10, behavior: REDUCE.matches ? 'auto' : 'smooth' });
  });
}
function openPicksKit(ev, then) {   // the picked kit at the picked key and tempo
  const p = S.picks, k = p && kitOf(p.kit);
  if (!k) return;
  const st = ks(k); st.bpm = p.bpm; st.pc = p.pc;
  const genreMoves = S.genre !== 'all' && famOf(k) !== S.genre, skinMoves = genreMoves || (S.genre === 'all' && famOf(k) !== famOf(curKit()));
  const go = () => {
    if (genreMoves) { S.genre = famOf(k); store.set('genre', S.genre); renderGenres(); renderKitList(); renderPads(); updateCounts(); }
    S.kitId = k.id; kitChanged(true); applyTheme(); retimeCur(st.bpm);
    if (S.view === 'vocals') renderVox(); else if (S.view === 'bass') renderBK(); else if (S.view === 'fx') renderFx();
    if (ev && S.view !== 'kits') setView('kits', true);
    renderPicks();
    if (then) requestAnimationFrame(then);
  };
  if (skinMoves) transition(go, ev); else go();
}
function renderPicks() {
  const box = $('#picks'), p = S.picks, k = p && typeof p === 'object' && kitOf(p.kit);
  if (!k || !Array.isArray(p.picks)) { box.hidden = true; box.innerHTML = ''; return; }
  const key = P.keyInfo(+p.pc || 0, k.mode), items = p.picks.map(x => ({ it: BY_ID[x.id], why: x.why })).filter(x => x.it);
  box.classList.toggle('closed', !S.picksOpen);
  box.innerHTML = '<div class="picks-in"><div class="picks-head"><div style="min-width:0"><span class="label">Claude picked for “' + esc(p.q || '') + '”</span>' +
    '<h2>' + esc(k.id + ' ' + k.name) + ' at ' + (+p.bpm || k.bpm) + ' BPM in ' + esc(key.label) + ', with ' + items.length + ' sound' + (items.length === 1 ? '' : 's') + '</h2>' +
    (p.summary ? '<p>' + esc(p.summary) + '</p>' : '') + '</div>' +
    '<div class="picks-act"><button class="btn solid" id="picksKit">Open the kit</button><button class="btn ghost" id="picksStar">' + ICON.star + 'Star all</button>' +
    '<button class="btn ghost" id="picksToggle" aria-expanded="' + S.picksOpen + '">' + (S.picksOpen ? 'Hide' : 'Show') + '</button><button class="btn ghost" id="picksClear">Clear</button></div></div>' +
    '<div class="rows">' + items.map(x => rowHTML(x.it) + (x.why ? '<p class="why">' + esc(x.why) + '</p>' : '')).join('') + '</div></div>';
  box.hidden = false;
  markRow();
}

/*__LAB__*/

/* =====================================================================
   EVENTS
   ===================================================================== */
document.addEventListener('click', e => {
  const t = e.target.closest('button, [data-kit]');
  if (!t) return;
  if (t.id === 'playBtn') { S.playing ? stopLoop() : startLoop('loop'); return; }
  if (t.dataset.star) { toggleStar(t.dataset.star); t.classList.remove('pop'); void t.offsetWidth; t.classList.add('pop'); return; }
  if (t.hasAttribute('data-fits') || t.hasAttribute('data-fitsoff')) {
    S.fits = t.hasAttribute('data-fits') ? !S.fits : false; store.set('fits', S.fits); refreshFits();
    if (S.fits) { const k = kitKey(); toast('Showing keys that mix with ' + k.cam + ' and loops near ' + k.bpm + ' BPM.'); }
    return;
  }
  if (t.dataset.gofits) { S.fits = true; store.set('fits', true); S.q = ''; setView(t.dataset.gofits, true); window.scrollTo({ top: 0 }); return; }
  if (t.id === 'saveCrate') { const mem = [...S.crate].map(id => BY_ID[id]).filter(Boolean); if (!mem.length) { toast('Star a few sounds first.'); return; } saveGroup(mem, 'My-Crate_' + mem.length + '-sounds.zip', 'Packing your crate'); return; }
  if (t.id === 'clearCrate') { if (!S.crate.size) return; S.crate.clear(); store.set('crate', []); renderCrate(); $$('[data-star]').forEach(b => b.setAttribute('aria-pressed', 'false')); queueCrate(); toast('Crate cleared.'); return; }
  if (t.id === 'askBtn') { const f = $('#askForm'), open = f.hidden; f.hidden = !open; t.setAttribute('aria-expanded', String(open)); if (open) { renderAskExamples(); $('#askQ').focus(); } return; }
  if (t.id === 'askStop') { if (askCtl) askCtl.abort(); return; }
  if (t.dataset.eg) { $('#askQ').value = t.dataset.eg; askClaude($('#askQ').value); return; }
  if (t.id === 'picksKit') { openPicksKit(e); return; }
  if (t.id === 'picksStar') { const ids = (S.picks && S.picks.picks || []).map(x => x.id).filter(id => BY_ID[id] && !S.crate.has(id)); ids.forEach(id => S.crate.add(id)); setCrate(S.crate); queueCrate(); toast(ids.length ? 'Starred ' + ids.length + ' sound' + (ids.length > 1 ? 's' : '') + '. They are in your crate.' : 'Every pick is already in your crate.'); return; }
  if (t.id === 'picksClear') { S.picks = null; store.set('picks', null); renderPicks(); return; }
  if (t.id === 'picksToggle') { S.picksOpen = !S.picksOpen; store.set('picksOpen', S.picksOpen); renderPicks(); return; }
  if (t.id === 'kitComment') { commentOnKit(); return; }
  if (t.id === 'kitToClaude') { sendKitToClaude(); return; }
  if (t.id === 'syncCrate') { t.disabled = true; crateSync().then(ok => { if (!ok) toast("Your crate can't be saved to your account in this view."); renderSyncNote(); }); return; }
  if (t.dataset.peerkit !== undefined) { const k = kitOf(t.dataset.peerkit); if (k) jumpToKit(k, e); return; }
  if (t.id === 'songPlay') { if (S.playing && seq.mode === 'song') stopLoop(); else { if (S.playing) stopLoop(); S.sel = { type: 'kit' }; startLoop('song'); } return; }
  if (t.id === 'saveSong') { saveSong(); return; }
  if (t.dataset.songv) {
    if (S.songV === t.dataset.songv) return;
    const wasSong = S.playing && seq.mode === 'song';
    if (wasSong) stopLoop();
    S.songV = t.dataset.songv; store.set('songV', S.songV);
    $$('[data-songv]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.songv === S.songV)));
    updateKitText(); drawSong();
    if (wasSong) startLoop('song');
    return;
  }
  if (t.classList.contains('mode')) { goView(t.dataset.view, e); return; }
  if (t.id === 'hwBtn') {
    const into = S.view !== 'halloween';
    goView(into ? 'halloween' : 'kits', e);
    if (into) { const d = $('#deck'); window.scrollTo({ top: d.getBoundingClientRect().top + window.scrollY, behavior: REDUCE.matches ? 'auto' : 'smooth' }); }
    return;
  }
  if (t.dataset.hwbank) { S.hwBank = t.dataset.hwbank; store.set('hwBank', S.hwBank); renderHw(); return; }
  if (t.id === 'saveHw' || t.id === 'hwSave') { saveGroup(hwItems().filter(i => !(S.hideExplicit && i.explicit)), S.hideExplicit ? 'Halloween-Pack_clean.zip' : 'Halloween-Pack.zip', 'Packing the Halloween pack'); return; }
  if (t.dataset.genre) { setGenre(t.dataset.genre, e); return; }
  if (t.id === 'shuffleBtn') { shuffleKit(e); return; }
  if (t.dataset.kit) {
    if (S.kitId === t.dataset.kit) { S.sel = { type: 'kit' }; lcd(); return; }
    const before = themeGenre();
    S.kitId = t.dataset.kit;
    if (themeGenre() !== before) transition(() => { kitChanged(true); applyTheme(); }, e); else kitChanged(true);
    return;
  }
  if (t.dataset.bpm) {
    const st = ks(); const nb = Math.max(128, Math.min(140, st.bpm + +t.dataset.bpm));
    if (nb === st.bpm) { toast('Tempo stays between 128 and 140 BPM.'); return; }
    st.bpm = nb; updateKitText(); lcd(); if (S.playing) rebuild();
    retimeCur(nb);
    refreshFits(); return;
  }
  if (t.dataset.var) {
    const lane = t.closest('.lane').dataset.lane; ks().vars[lane] = t.dataset.var;
    t.parentNode.querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(b === t)));
    updateKitText(); drawRolls(); if (S.playing) rebuild(); return;
  }
  if (t.dataset.mute) { const st = ks(); st.mute[t.dataset.mute] = !st.mute[t.dataset.mute]; t.setAttribute('aria-pressed', String(!!st.mute[t.dataset.mute])); return; }
  if (t.dataset.savelane) { saveLane(t.dataset.savelane); return; }
  if (t.id === 'saveKit') { saveKit(); return; }
  if (t.id === 'saveKits') { saveAllKits(); return; }
  if (t.id === 'saveAll' || t.hasAttribute('data-fullpack')) { saveFullPack(); return; }
  if (t.dataset.gpack) { saveFullPack(t.dataset.gpack); return; }
  if (t.id === 'saveBK') { saveGroup(bkItems().filter(inGenre), 'Bass-and-Keys' + famTag() + '.zip', 'Packing'); return; }
  if (t.dataset.saveset) { saveSet(t.dataset.saveset); return; }
  if (t.dataset.bkf) { S.bkFilter = t.dataset.bkf; store.set('bkFilter', S.bkFilter); renderBK(); return; }
  if (t.dataset.note) {
    const it = BY_ID[t.dataset.note]; selectItem(it.id); playItem(it, { overlap: true });
    t.classList.add('hit'); setTimeout(() => t.classList.remove('hit'), 160); return;
  }
  if (t.id === 'saveVox') { saveGroup(voxItems().filter(inGenre).filter(i => !(S.hideExplicit && i.explicit)), 'Vocals' + famTag() + (S.hideExplicit ? '_clean.zip' : '_all.zip'), 'Packing vocals'); return; }
  if (t.id === 'saveFx') { saveGroup(fxItems().filter(inGenre), 'Loops-Fills-and-FX' + famTag() + '.zip', 'Packing loops & FX'); return; }
  if (t.id === 'saveHits') { saveGroup(ITEMS.filter(i => i.cat === 'hits').filter(inGenre), 'Drum-Hits' + famTag() + '.zip', 'Packing hits'); return; }
  if (t.id === 'deckSave') {
    if (S.sel.type === 'kit' || S.view === 'kits') saveKit(); else saveItem(BY_ID[S.sel.id]);
    return;
  }
  if (t.dataset.voxf) { S.voxFilter = t.dataset.voxf; store.set('voxFilter', S.voxFilter); renderVox(); return; }
  if (t.dataset.fxf) { S.fxFilter = t.dataset.fxf; store.set('fxFilter', S.fxFilter); renderFx(); return; }
  if (t.dataset.play) { const it = BY_ID[t.dataset.play]; selectItem(it.id); playItem(it); return; }
  if (t.dataset.saveitem) { const it = BY_ID[t.dataset.saveitem]; selectItem(it.id); saveItem(it); return; }
  if (t.dataset.pad) { hitPad(t); return; }
});
document.addEventListener('submit', e => { if (e.target && e.target.id === 'askForm') { e.preventDefault(); askClaude($('#askQ').value); } });
document.addEventListener('pointerdown', e => {
  const cv = e.target.closest && e.target.closest('#songCv');
  if (!cv || !S.playing || seq.mode !== 'song') return;
  const r = cv.getBoundingClientRect(), bar = (e.clientX - r.left) / r.width * (+cv.dataset.bars || 1);
  const sc = (cv._sections || []).filter(x => x.bar <= bar).pop();
  if (sc) { seekBar(sc.bar); toast('Jumped to ' + sc.name + ' (bar ' + (sc.bar + 1) + ')', 1400); }
});
let qT;
document.addEventListener('input', e => {
  if (!e.target.hasAttribute || !e.target.hasAttribute('data-q')) return;
  S.q = e.target.value;
  $$('[data-q]').forEach(x => { if (x !== e.target) x.value = S.q; });
  clearTimeout(qT); qT = setTimeout(() => { if (S.view === 'vocals') renderVox(); if (S.view === 'bass') renderBK(); if (S.view === 'fx') renderFx(); }, 120);
});
document.addEventListener('change', e => {
  if (e.target.id === 'keySel') { ks().pc = +e.target.value; updateKitText(); drawRolls(); drawSong(); lcd(); if (S.playing) rebuild(); refreshFits(); }
  if (e.target.id === 'fillTog') { ks().fill = e.target.checked; drawRolls(); if (S.playing) rebuild(); }
  if (e.target.id === 'hideE' || e.target.id === 'hideE2') {
    S.hideExplicit = e.target.checked; store.set('hideExplicit', S.hideExplicit);
    ['#hideE', '#hideE2'].forEach(q => { const x = $(q); if (x) x.checked = S.hideExplicit; });
    renderVox(); if (S.view === 'halloween') renderHw();
  }
});
document.addEventListener('keydown', e => {
  const typing = /INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName);
  if ((S.view === 'hits' || S.view === 'halloween') && !typing && !e.metaKey && !e.ctrlKey && !e.altKey && !e.repeat) {   // the pads play from the keyboard
    const i = PAD_KEYS.indexOf((e.key || '').toLowerCase()), pad = i >= 0 && $$(S.view === 'hits' ? '#pads .pad' : '#hwPads .pad')[i];
    if (pad) { e.preventDefault(); hitPad(pad); return; }
  }
  if (e.code === 'Space' && !typing && document.activeElement.tagName !== 'BUTTON') { e.preventDefault(); S.playing ? stopLoop() : startLoop('loop'); }
});
let rz;
new ResizeObserver(() => { clearTimeout(rz); rz = setTimeout(() => { drawRolls(); drawSong(); }, 60); }).observe(document.body);
try { matchMedia('(prefers-color-scheme: dark)').addEventListener('change', () => { drawRolls(); drawSong(); }); } catch (e) {}
new MutationObserver(() => { drawRolls(); drawSong(); }).observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });

/* ---------- boot ---------- */
$('#gmtable').innerHTML = (() => { const ls = P.DRUM_ORDER; let h = ''; for (let i = 0; i < ls.length; i += 2) { h += '<tr>' + [ls[i], ls[i + 1]].filter(Boolean).map(l => '<td>' + P.DRUM[l].label + '</td><td><b>' + P.DRUM[l].note + '</b> ' + P.noteLabel(P.DRUM[l].note - 12, false).replace(/(\d)$/, '$1') + '</td>').join('') + '</tr>'; } return h; })();
(() => {
  const c = { kits: P.KITS.length, vox: voxItems().length, bk: bkItems().length, fx: fxItems().length, hits: ITEMS.filter(i => i.cat === 'hits').length };
  $('#cKits').textContent = c.kits; $('#cVox').textContent = c.vox; $('#cBK').textContent = c.bk; $('#cFx').textContent = c.fx; $('#cHits').textContent = c.hits;
  const mb = Math.round(MANIFEST.bytes / 1e6);
  $('#packHint').textContent = 'The full pack is about ' + mb + ' MB; genre packs are smaller.';
})();
if (S.genre !== 'all' && famOf(curKit()) !== S.genre) { const want = kitOf(S.lastKit[S.genre]); S.kitId = (want && famOf(want) === S.genre ? want : P.KITS.find(k => famOf(k) === S.genre)).id; }
const hv = (location.hash || '').replace('#', ''), sv = store.get('view', 'kits');
S.view = VIEWS.includes(hv) ? hv : VIEWS.includes(sv) ? sv : 'kits';
document.documentElement.dataset.genre = themeGenre();
$('#stars').innerHTML = [0, 1, 2, 3].map(() => '<svg viewBox="-1 -1 2 2"><path d="' + starPath(0, 0, 1) + '"/></svg>').join('');
renderGenres(); renderTools(); renderKitList(); renderKitDetail(); renderVox(); renderFx(); renderBK(); renderPads(); renderCrate(); updateCounts();
vizInit(); applyTheme();
setView(S.view, false);
lcd(); lightStep(-1); renderPicks();
capsInit();
if (window.ResizeObserver) new ResizeObserver(() => document.documentElement.style.setProperty('--deck-h', $('#deck').offsetHeight + 'px')).observe($('#deck'));
})();
