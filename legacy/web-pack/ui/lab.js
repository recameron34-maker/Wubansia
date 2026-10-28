/* =====================================================================
   SOUND DESIGN: knobs, live lane macros on the kit, and the Sound lab
   (resample any pack sound or the viewer's own, render offline, so what
   you hear is exactly what you save)
   ===================================================================== */

/* ---------- knobs: drag up or down (Shift for fine), arrow keys, double-click to reset ---------- */
const KN = {};   // id -> {min, max, step, def, fmt, bipolar, get(), set(v)}
function arcPath(cx, cy, r, a0, a1) {   // degrees, 0 at the top, clockwise
  const pt = a => [cx + r * Math.sin(a * Math.PI / 180), cy - r * Math.cos(a * Math.PI / 180)];
  const [x0, y0] = pt(a0), [x1, y1] = pt(a1);
  return 'M' + x0.toFixed(2) + ' ' + y0.toFixed(2) + 'A' + r + ' ' + r + ' 0 ' + (Math.abs(a1 - a0) > 180 ? 1 : 0) + ' ' + (a1 > a0 ? 1 : 0) + ' ' + x1.toFixed(2) + ' ' + y1.toFixed(2);
}
function knobSVG(k, v) {
  const u = (v - k.min) / (k.max - k.min), a = -135 + 270 * u, z = k.bipolar ? -135 + 270 * ((0 - k.min) / (k.max - k.min)) : -135;
  const [p0, p1] = a >= z ? [z, a] : [a, z];
  const tip = [20 + 9 * Math.sin(a * Math.PI / 180), 20 - 9 * Math.cos(a * Math.PI / 180)];
  return '<svg viewBox="0 0 40 40" aria-hidden="true"><path class="tr" d="' + arcPath(20, 20, 16, -135, 135) + '"/>' + (p1 - p0 > 0.5 ? '<path class="arc" d="' + arcPath(20, 20, 16, p0, p1) + '"/>' : '') +
    '<circle class="cap" cx="20" cy="20" r="11"/><line class="ptr" x1="20" y1="20" x2="' + tip[0].toFixed(2) + '" y2="' + tip[1].toFixed(2) + '"/></svg>';
}
function knobHTML(id, label, mini) {
  const k = KN[id], v = k.get();
  return '<div class="knob' + (mini ? ' mini' : '') + '" data-knob="' + id + '"><button type="button" class="kn" role="slider" aria-label="' + esc(label) + '" aria-valuemin="' + k.min + '" aria-valuemax="' + k.max +
    '" aria-valuenow="' + v + '" aria-valuetext="' + esc(k.fmt(v)) + '" title="' + esc(label + ': ' + k.fmt(v)) + '">' + knobSVG(k, v) + '</button><span class="kl">' + esc(label) + '</span>' +
    (mini ? '' : '<span class="kv tnum">' + esc(k.fmt(v)) + '</span>') + '</div>';
}
function knobPaint(id) {
  const k = KN[id], v = k.get();
  $$('[data-knob="' + id + '"]').forEach(el => {
    const b = el.querySelector('.kn');
    b.innerHTML = knobSVG(k, v); b.setAttribute('aria-valuenow', String(v)); b.setAttribute('aria-valuetext', k.fmt(v));
    b.title = b.getAttribute('aria-label') + ': ' + k.fmt(v);
    const t = el.querySelector('.kv'); if (t) t.textContent = k.fmt(v);
  });
}
function knobSet(id, v) {
  const k = KN[id];
  if (!k) return;
  v = Math.max(k.min, Math.min(k.max, Math.round(v / k.step) * k.step));
  v = +v.toFixed(4);
  if (v === k.get()) return;
  k.set(v); knobPaint(id);
}
let knobDrag = null;
document.addEventListener('pointerdown', e => {
  const b = e.target.closest && e.target.closest('.kn');
  if (!b) return;
  const id = b.parentNode.dataset.knob, k = KN[id];
  if (!k) return;
  e.preventDefault();
  try { b.setPointerCapture(e.pointerId); } catch (err) {}
  b.focus({ preventScroll: true });
  knobDrag = { id, b, y: e.clientY, v: k.get(), pid: e.pointerId };
});
document.addEventListener('pointermove', e => {
  if (!knobDrag || e.pointerId !== knobDrag.pid) return;
  const k = KN[knobDrag.id];
  knobSet(knobDrag.id, knobDrag.v + (knobDrag.y - e.clientY) / (e.shiftKey ? 700 : 160) * (k.max - k.min));
});
const knobEnd = e => { if (knobDrag && e.pointerId === knobDrag.pid) { try { knobDrag.b.releasePointerCapture(e.pointerId); } catch (err) {} knobDrag = null; } };
document.addEventListener('pointerup', knobEnd);
document.addEventListener('pointercancel', knobEnd);
document.addEventListener('dblclick', e => { const b = e.target.closest && e.target.closest('.kn'); if (b && KN[b.parentNode.dataset.knob]) knobSet(b.parentNode.dataset.knob, KN[b.parentNode.dataset.knob].def); });
document.addEventListener('keydown', e => {
  const b = e.target.closest && e.target.closest('.kn');
  if (!b) return;
  const id = b.parentNode.dataset.knob, k = KN[id];
  if (!k) return;
  const big = Math.max(k.step, (k.max - k.min) / 10), m = { ArrowUp: k.step, ArrowRight: k.step, ArrowDown: -k.step, ArrowLeft: -k.step, PageUp: big, PageDown: -big };
  if (e.key in m) { e.preventDefault(); knobSet(id, k.get() + m[e.key] * (e.shiftKey && Math.abs(m[e.key]) === k.step ? 10 : 1)); }
  else if (e.key === 'Home') { e.preventDefault(); knobSet(id, k.min); }
  else if (e.key === 'End') { e.preventDefault(); knobSet(id, k.max); }
}, true);

/* ---------- live lane macros: a DJ filter (left = low-pass, right = high-pass), drive and a reverb-and-echo send ---------- */
const LFX_LABEL = { filter: 'Filter', drive: 'Drive', space: 'Space' };
const laneFx = {};
let driveTable = null;
function driveCurve(amount) {
  const k = 1 + 19 * amount, c = new Float32Array(2048), t = Math.tanh(k);
  for (let i = 0; i < 2048; i++) { const x = i / 1023.5 - 1; c[i] = Math.tanh(k * x) / t; }
  return c;
}
function fxChain(input, dest) {   // input -> low-pass -> high-pass -> (dry + driven) -> dest, plus a send to the room and the echo
  const lp = F('lowpass', 20000, 0.7), hp = F('highpass', 10, 0.7), dry = G(1), wet = G(0), out = G(1, dest), send = G(0), sh = ctx.createWaveShaper();
  if (!driveTable) driveTable = driveCurve(0.55);
  sh.curve = driveTable; sh.oversample = '2x';
  input.connect(lp); lp.connect(hp); hp.connect(dry); hp.connect(sh); sh.connect(wet); dry.connect(out); wet.connect(out);
  out.connect(send); send.connect(revIn); send.connect(G(0.7, dlyIn));
  return { lp, hp, dry, wet, send };
}
function laneFxState(kit) {
  const st = ks(kit || curKit());
  if (!st.fx) st.fx = { drums: { filter: 0, drive: 0, space: 0 }, bass: { filter: 0, drive: 0, space: 0 }, chords: { filter: 0, drive: 0, space: 0 }, lead: { filter: 0, drive: 0, space: 0 } };
  return st.fx;
}
function applyLaneFx(lane) {
  const c = laneFx[lane];
  if (!c || !ctx) return;
  const v = laneFxState()[lane], t = ctx.currentTime, f = v.filter;
  c.lp.frequency.setTargetAtTime(f < 0 ? 20000 * Math.pow(2, f * 9) : 20000, t, 0.02);
  c.hp.frequency.setTargetAtTime(f > 0 ? 20 * Math.pow(2, f * 9.5) : 10, t, 0.02);
  const q = Math.abs(f) > 0.04 ? 1.6 : 0.7;
  c.lp.Q.setTargetAtTime(q, t, 0.02); c.hp.Q.setTargetAtTime(q, t, 0.02);
  c.wet.gain.setTargetAtTime(v.drive * 0.9, t, 0.02); c.dry.gain.setTargetAtTime(1 - v.drive * 0.55, t, 0.02);
  c.send.gain.setTargetAtTime(v.space * 0.85, t, 0.03);
}
function applyAllLaneFx() { ['drums', 'bass', 'chords', 'lead'].forEach(applyLaneFx); }
function lfxKnob(lane, p) {
  const id = 'lfx:' + lane + ':' + p;
  if (!KN[id]) KN[id] = { min: p === 'filter' ? -1 : 0, max: 1, step: 0.01, def: 0, bipolar: p === 'filter',
    fmt: v => p === 'filter' ? (Math.abs(v) < 0.005 ? 'open' : v < 0 ? 'low-pass ' + Math.round(-v * 100) + '%' : 'high-pass ' + Math.round(v * 100) + '%') : Math.round(v * 100) + '%',
    get: () => laneFxState()[lane][p], set: v => { laneFxState()[lane][p] = v; ensureAudio(); applyLaneFx(lane); } };
  return knobHTML(id, LFX_LABEL[p], true);
}
function lfxHTML(lane) { return '<span class="lfx" aria-label="Sound shaping">' + ['filter', 'drive', 'space'].map(p => lfxKnob(lane, p)).join('') + '</span>'; }
function lfxRepaint() { Object.keys(KN).filter(id => id.startsWith('lfx:')).forEach(knobPaint); }

/* ---------- Sound lab: parameters ---------- */
const DTIMES = [[1 / 16, '1/16'], [1 / 8, '1/8'], [3 / 16, '3/16'], [1 / 4, '1/4'], [3 / 8, '3/8']];
const hzOf = v => 20 * Math.pow(1000, v);
const fmtHz = v => { const h = hzOf(v); return h >= 1000 ? (h / 1000).toFixed(h >= 10000 ? 0 : 1) + ' kHz' : Math.round(h) + ' Hz'; };
const fmtMs = v => v >= 1000 ? (v / 1000).toFixed(2) + ' s' : Math.round(v) + ' ms';
const fmtPct = v => Math.round(v * 100) + '%';
const LAB_P = {   // id: [label, min, max, step, default, format, bipolar]
  pitch: ['Pitch', -24, 24, 1, 0, v => (v > 0 ? '+' : '') + v + ' st', true],
  fine: ['Fine', -100, 100, 1, 0, v => (v > 0 ? '+' : '') + v + ' ct', true],
  attack: ['Attack', 0, 1000, 1, 0, fmtMs],
  length: ['Length', 5, 100, 1, 100, v => Math.round(v) + '%'],
  release: ['Release', 5, 2000, 1, 40, fmtMs],
  cutoff: ['Cutoff', 0, 1, 0.002, 1, fmtHz],
  reso: ['Resonance', 0, 1, 0.01, 0.15, fmtPct],
  fenv: ['Sweep', -1, 1, 0.01, 0, v => (v > 0 ? '+' : '') + Math.round(v * 100) + '%', true],
  fdecay: ['Sweep time', 10, 2000, 1, 300, fmtMs],
  drive: ['Drive', 0, 1, 0.01, 0, fmtPct],
  bits: ['Bits', 4, 16, 1, 16, v => v + '-bit'],
  down: ['Sample rate', 1, 16, 1, 1, v => (44.1 / v).toFixed(1) + ' kHz'],
  gate: ['Gate depth', 0, 1, 0.01, 0, fmtPct],
  dtime: ['Echo time', 0, 4, 1, 2, v => DTIMES[v][1]],
  dfb: ['Feedback', 0, 0.85, 0.01, 0.35, fmtPct],
  dmix: ['Echo', 0, 1, 0.01, 0, fmtPct],
  rsize: ['Room size', 0.2, 5, 0.05, 1.6, v => v.toFixed(1) + ' s'],
  rmix: ['Reverb', 0, 1, 0.01, 0, fmtPct],
  width: ['Width', 0, 2, 0.01, 1, fmtPct],
  gain: ['Output', -18, 12, 0.5, 0, v => (v > 0 ? '+' : '') + v + ' dB', true]
};
const LAB_GROUPS = [['Pitch and trim', ['pitch', 'fine']], ['Envelope', ['attack', 'length', 'release']], ['Filter', ['cutoff', 'reso', 'fenv', 'fdecay']],
  ['Colour', ['drive', 'bits', 'down']], ['Gate', ['gate']], ['Echo and room', ['dtime', 'dfb', 'dmix', 'rsize', 'rmix', 'width']], ['Output', ['gain']]];
const labDefaults = () => Object.assign(Object.fromEntries(Object.entries(LAB_P).map(([k, d]) => [k, d[4]])),
  { ftype: 'lp', reverse: false, start: 0, end: 1, norm: true, pattern: [1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1].map(Boolean) });
function cleanParams(p) {   // any stored or suggested settings, clamped to what the lab can do
  const d = labDefaults(), out = {};
  Object.keys(LAB_P).forEach(k => { const [, lo, hi, st] = LAB_P[k], v = Number(p && p[k]); out[k] = isFinite(v) ? +(Math.max(lo, Math.min(hi, Math.round(v / st) * st))).toFixed(4) : d[k]; });
  out.ftype = ['lp', 'hp', 'bp'].includes(p && p.ftype) ? p.ftype : d.ftype;
  out.reverse = !!(p && p.reverse); out.norm = p && typeof p.norm === 'boolean' ? p.norm : d.norm;
  let s0 = Number(p && p.start), s1 = Number(p && p.end);
  if (!isFinite(s0)) s0 = 0; if (!isFinite(s1)) s1 = 1;
  s0 = Math.max(0, Math.min(0.98, s0)); s1 = Math.max(s0 + 0.01, Math.min(1, s1));
  out.start = +s0.toFixed(4); out.end = +s1.toFixed(4);
  out.pattern = Array.isArray(p && p.pattern) && p.pattern.length === 16 ? p.pattern.map(Boolean) : d.pattern;
  return out;
}
const LAB_PRESETS = [
  ['Clean', {}],
  ['Garage chop', { length: 45, release: 60, drive: 0.15, gate: 0.9, pattern: [1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0], dtime: 2, dmix: 0.18, dfb: 0.3, rmix: 0.12 }],
  ['Dub throw', { ftype: 'lp', cutoff: 0.73, reso: 0.25, dtime: 2, dfb: 0.62, dmix: 0.5, rsize: 2.4, rmix: 0.3, width: 1.3 }],
  ['Pitched up', { pitch: 5, ftype: 'hp', cutoff: 0.24, rmix: 0.15, rsize: 1.2 }],
  ['Lo-fi sampler', { bits: 12, down: 3, ftype: 'lp', cutoff: 0.86, drive: 0.22, width: 0.8 }],
  ['Reverse swell', { reverse: true, attack: 320, release: 120, rsize: 2.8, rmix: 0.45, width: 1.4 }],
  ['Siren stab', { pitch: -12, drive: 0.6, ftype: 'bp', cutoff: 0.6, reso: 0.55, fenv: 0.5, fdecay: 260, length: 40, release: 80, dtime: 1, dmix: 0.25 }],
  ['Minimal glitch', { bits: 8, down: 5, ftype: 'hp', cutoff: 0.36, gate: 1, pattern: [1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0], dtime: 0, dfb: 0.25, dmix: 0.2 }],
  ['House air', { ftype: 'hp', cutoff: 0.22, rsize: 1.7, rmix: 0.26, width: 1.45, dmix: 0.08 }],
  ['Sub drop', { pitch: -24, ftype: 'lp', cutoff: 0.5, fenv: 0.7, fdecay: 900, drive: 0.35, release: 600 }]
];
S.lab = store.get('lab', null);
if (!S.lab || typeof S.lab !== 'object') S.lab = { src: 's06', p: labDefaults(), root: 74 };
S.lab.p = cleanParams(S.lab.p);
S.lab.root = Math.max(24, Math.min(96, Math.round(+S.lab.root) || 60));
Object.keys(LAB_P).forEach(k => {
  const [label, min, max, step, def, fmt, bipolar] = LAB_P[k];
  KN['lab:' + k] = { min, max, step, def, fmt, bipolar: !!bipolar, get: () => S.lab.p[k], set: v => { S.lab.p[k] = v; labChanged(); } };
});
const LAB = { buf: null, out: null, srcName: '', file: false, rendering: false, dirty: false, play: null, loop: false, lane: 'off', built: false, oct: 0, renderT: 0, raf: 0 };

/* ---------- where a sound sits: its root note, from the pack's own labels ---------- */
const NOTE_PC = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
const NOTE_NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'];
const noteName = m => NOTE_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);
function parseNote(s, oct) {
  const m = /([A-G])([#b]?)(-?\d)?/.exec(String(s || ''));
  if (!m) return null;
  const pc = (NOTE_PC[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0) + 12) % 12;
  return pc + 12 * ((m[3] != null ? +m[3] : oct) + 1);
}
function rootOf(it) {
  if (it.noteName) return parseNote(it.noteName) || 60;
  const sung = /\bon ([A-G][#b]?\d)/.exec(it.note || '');
  if (sung) return parseNote(sung[1]);
  if (it.key) return parseNote(it.key, it.cat === 'bass' ? 2 : it.cat === 'hits' ? 3 : 4) || 60;
  return 60;
}

/* ---------- rendering: trim, reverse and crush in the page, then pitch, filter, drive, envelope, gate, echo and room offline ---------- */
const irCache = new Map();
function labIR(sr, size) {
  const key = sr + ':' + size.toFixed(2);
  if (!irCache.has(key)) {
    const n = Math.max(256, Math.floor(sr * size)), L = new Float32Array(n), R = new Float32Array(n);
    let s = 20261026, l1 = 0, l2 = 0;
    const rnd = () => { s = (s * 1664525 + 1013904223) >>> 0; return s / 2147483648 - 1; };   // seeded, so every render of a setting is identical
    for (let i = 0; i < n; i++) { const d = Math.pow(1 - i / n, 2.4); l1 = l1 * 0.55 + rnd() * 0.45; l2 = l2 * 0.55 + rnd() * 0.45; L[i] = l1 * d; R[i] = l2 * d; }
    irCache.set(key, [L, R]);
    if (irCache.size > 12) irCache.delete(irCache.keys().next().value);
  }
  return irCache.get(key);
}
async function renderDesign(src, p, bpm) {
  const SR = 44100, ch = Math.min(2, src.numberOfChannels), n0 = src.length;
  const a = Math.floor(p.start * n0), b = Math.max(a + 256, Math.floor(p.end * n0)), n = b - a, srate = src.sampleRate;
  const tb = new AudioBuffer({ length: n, numberOfChannels: ch, sampleRate: srate });
  const fadeN = Math.min(Math.floor(srate * 0.003), n >> 2);
  const q = Math.pow(2, p.bits - 1);
  for (let c = 0; c < ch; c++) {
    const x = src.getChannelData(c).slice(a, b);
    if (p.reverse) x.reverse();
    for (let i = 0; i < fadeN; i++) { const g = i / fadeN; x[i] *= g; x[n - 1 - i] *= g; }   // no clicks at the cut points
    if (p.down > 1 || p.bits < 16) {   // a vintage sampler: hold every Nth sample, fewer bits
      let held = 0;
      for (let i = 0; i < n; i++) { if (i % p.down === 0) held = p.bits < 16 ? Math.round(x[i] * q) / q : x[i]; x[i] = held; }
    }
    tb.copyToChannel(x, c);
  }
  const rate = Math.pow(2, (p.pitch + p.fine / 100) / 12), body = n / srate / rate;
  const att = p.attack / 1000, rel = p.release / 1000, hold = Math.max(att + 0.005, body * p.length / 100), soundEnd = Math.min(body, hold + rel);
  const dt = DTIMES[p.dtime][0] * 240 / bpm;
  const tail = Math.max(p.dmix > 0.001 ? Math.min(8, dt * Math.log(0.004) / Math.log(Math.max(0.05, p.dfb))) : 0, p.rmix > 0.001 ? p.rsize * 1.05 : 0);
  const total = Math.min(40, soundEnd + tail + 0.05);
  const oc = new OfflineAudioContext(2, Math.ceil(total * SR), SR);
  const s = oc.createBufferSource(); s.buffer = tb; s.playbackRate.value = rate;
  const f = oc.createBiquadFilter(); f.type = { lp: 'lowpass', hp: 'highpass', bp: 'bandpass' }[p.ftype];
  f.Q.value = p.ftype === 'bp' ? 0.6 + p.reso * 9 : 0.5 + p.reso * p.reso * 16;
  const fc = hzOf(p.cutoff), ny = SR * 0.45, clampF = h => Math.max(20, Math.min(ny, h));
  if (Math.abs(p.fenv) > 0.005) { f.frequency.setValueAtTime(clampF(fc * Math.pow(2, p.fenv * 5)), 0); f.frequency.exponentialRampToValueAtTime(clampF(fc), Math.max(0.01, p.fdecay / 1000)); }
  else f.frequency.value = clampF(fc);
  const sh = oc.createWaveShaper(); sh.curve = driveCurve(p.drive); sh.oversample = '4x';
  const env = oc.createGain();
  env.gain.setValueAtTime(att > 0 ? 0 : 1, 0);
  if (att > 0) env.gain.linearRampToValueAtTime(1, att);
  env.gain.setValueAtTime(1, hold);
  env.gain.linearRampToValueAtTime(0, Math.min(body, hold + rel));
  const gate = oc.createGain();
  if (p.gate > 0.001) {
    const st = 15 / bpm, lo = 1 - p.gate;
    gate.gain.setValueAtTime(p.pattern[0] ? 1 : lo, 0);
    for (let i = 1, tt = st; tt < total; i++, tt += st) {
      const v = p.pattern[i % 16] ? 1 : lo, prev = p.pattern[(i - 1) % 16] ? 1 : lo;
      if (v !== prev) { gate.gain.setValueAtTime(prev, tt - 0.003); gate.gain.linearRampToValueAtTime(v, tt + 0.003); }
    }
  }
  const mix = oc.createGain();
  s.connect(f); f.connect(sh); sh.connect(env); env.connect(gate); gate.connect(mix);
  if (p.dmix > 0.001) {   // dotted or straight echo, darker and thinner on every repeat
    const d = oc.createDelay(2), fb = oc.createGain(), lpf = oc.createBiquadFilter(), hpf = oc.createBiquadFilter(), wet = oc.createGain();
    d.delayTime.value = dt; fb.gain.value = p.dfb; lpf.type = 'lowpass'; lpf.frequency.value = 4800; hpf.type = 'highpass'; hpf.frequency.value = 160; wet.gain.value = p.dmix;
    gate.connect(d); d.connect(lpf); lpf.connect(hpf); hpf.connect(fb); fb.connect(d); hpf.connect(wet); wet.connect(mix);
  }
  if (p.rmix > 0.001) {
    const [L, R] = labIR(SR, p.rsize), ib = oc.createBuffer(2, L.length, SR);
    ib.copyToChannel(L, 0); ib.copyToChannel(R, 1);
    const cv = oc.createConvolver(); cv.buffer = ib;
    const wet = oc.createGain(); wet.gain.value = p.rmix * 0.9;
    gate.connect(cv); cv.connect(wet); wet.connect(mix);
  }
  mix.connect(oc.destination);
  s.start(0);
  const r = await oc.startRendering();
  const Lc = r.getChannelData(0), Rc = r.getChannelData(1), N = r.length;
  let peak = 0, last = 0;
  for (let i = 0; i < N; i++) {   // width (mid/side), then find the peak and the true end
    const m = (Lc[i] + Rc[i]) * 0.5, sd = (Lc[i] - Rc[i]) * 0.5 * p.width;
    Lc[i] = m + sd; Rc[i] = m - sd;
    const a2 = Math.max(Math.abs(Lc[i]), Math.abs(Rc[i]));
    if (a2 > peak) peak = a2;
    if (a2 > 1e-4) last = i;
  }
  const g = p.norm ? (peak > 1e-6 ? 0.891 / peak : 1) * Math.pow(10, Math.min(0, p.gain) / 20) : Math.pow(10, p.gain / 20);
  const end = Math.min(N, last + Math.floor(SR * 0.02) + 1), fo = Math.min(Math.floor(SR * 0.004), end >> 2);
  const out = new AudioBuffer({ length: Math.max(1, end), numberOfChannels: 2, sampleRate: SR });
  [Lc, Rc].forEach((x, c) => {
    const y = new Float32Array(end);
    for (let i = 0; i < end; i++) { let v = x[i] * g; if (i >= end - fo) v *= (end - i) / fo; y[i] = v > 0.999 ? 0.999 : v < -0.999 ? -0.999 : v; }
    out.copyToChannel(y, c);
  });
  return out;
}
function labChanged() {
  clearTimeout(LAB.saveT); LAB.saveT = setTimeout(() => store.set('lab', S.lab), 300);
  if (LAB.renderT) return;
  LAB.renderT = requestAnimationFrame(() => { LAB.renderT = 0; labRender(); });
}
async function labRender() {
  if (!LAB.buf) return;
  if (LAB.rendering) { LAB.dirty = true; return; }
  LAB.rendering = true;
  try {
    do {
      LAB.dirty = false;
      const p = cleanParams(S.lab.p), out = await renderDesign(LAB.buf, p, ks().bpm);
      if (!LAB.dirty) { LAB.out = out; drawLabWaves(); if (LAB.play) labPlay(); }
    } while (LAB.dirty);
  } catch (e) { console.error(e); toast("Couldn't render that setting."); }
  finally { LAB.rendering = false; }
}

/* ---------- loading a source: any sound in the pack, or the viewer's own file ---------- */
async function labLoadItem(id) {
  const it = BY_ID[id];
  if (!it) return;
  ensureAudio();
  const now = $('#labNow'); if (now) now.textContent = 'Loading ' + it.title + '…';
  try { LAB.buf = await loadBuffer(it.file); } catch (e) { toast("Couldn't load " + it.title + '.'); return; }
  S.lab.src = id; S.lab.root = rootOf(it); LAB.srcName = it.title; LAB.file = false;
  store.set('lab', S.lab);
  renderLabHead(); await labRender();
}
async function labLoadFile(file) {
  if (!file) return;
  if (file.size > 60e6) { toast('That file is over 60 MB. Try a shorter sound.'); return; }
  ensureAudio();
  try {
    const ab = await file.arrayBuffer();
    let buf = await new Promise((res, rej) => ctx.decodeAudioData(ab, res, rej));
    if (buf.duration > 30) {   // keep the first 30 seconds
      const n = Math.floor(buf.sampleRate * 30), cut = new AudioBuffer({ length: n, numberOfChannels: Math.min(2, buf.numberOfChannels), sampleRate: buf.sampleRate });
      for (let c = 0; c < cut.numberOfChannels; c++) cut.copyToChannel(buf.getChannelData(c).slice(0, n), c);
      buf = cut; toast('Using the first 30 seconds of ' + file.name + '.');
    }
    LAB.buf = buf; LAB.file = true; LAB.srcName = file.name.replace(/\.[a-z0-9]+$/i, ''); S.lab.src = null; S.lab.root = 60;
    S.lab.p.start = 0; S.lab.p.end = 1;
    renderLabHead(); await labRender();
  } catch (e) { toast("Couldn't read " + file.name + ' as audio. Try a WAV, MP3, AIFF or FLAC file.'); }
}

/* ---------- playing the result: once or looped, on the kit's next beat when the kit plays, and across a keyboard ---------- */
function labPlay(note) {
  if (!LAB.out) return;
  ensureAudio(); try { ctx.resume(); } catch (e) {}
  const src = ctx.createBufferSource(); src.buffer = LAB.out;
  const rate = note == null ? 1 : Math.pow(2, (note - S.lab.root) / 12);
  src.playbackRate.value = rate;
  const g = G(note == null ? 0.95 : 0.85, sampleBus); src.connect(g);
  let when = ctx.currentTime + 0.01;
  if (note == null) {
    labStop(true);
    if (S.playing) { const step = 60 / ks().bpm, pos = ctx.currentTime - seq.anchor; when = Math.max(when, seq.anchor + Math.ceil(pos / step + 0.02) * step); }
    src.loop = LAB.loop;
    LAB.play = { src, g, when, dur: LAB.out.duration };
    src.onended = () => { if (LAB.play && LAB.play.src === src) { LAB.play = null; labPlayUI(); } };
    labPlayUI(); labFrame();
  }
  src.start(when);
}
function labStop(quiet) {
  if (LAB.play) { try { LAB.play.g.gain.setTargetAtTime(0, ctx.currentTime, 0.01); LAB.play.src.stop(ctx.currentTime + 0.05); } catch (e) {} LAB.play = null; }
  if (!quiet) labPlayUI();
}
function labPlayUI() {
  const b = $('#labPlay');
  if (b) { b.innerHTML = (LAB.play ? ICON.stop + 'Stop' : ICON.play + 'Play'); b.classList.toggle('on', !!LAB.play); }
  const ph = $('#labHead'); if (ph && !LAB.play) ph.style.opacity = '0';
}
function labFrame() {
  if (LAB.raf) return;
  const step = () => {
    LAB.raf = 0;
    const ph = $('#labHead'), cv = $('#labOutCv');
    if (!LAB.play || !ph || !cv) return;
    const el = (ctx.currentTime - LAB.play.when) / LAB.play.dur, u = LAB.loop ? (el > 0 ? el % 1 : 0) : Math.max(0, Math.min(1, el));
    ph.style.opacity = el >= 0 ? '1' : '0'; ph.style.transform = 'translateX(' + (u * cv.clientWidth).toFixed(1) + 'px)';
    LAB.raf = requestAnimationFrame(step);
  };
  LAB.raf = requestAnimationFrame(step);
}
/* the designed sound on a kit lane: every note of that lane plays it, pitched from its root note */
function labVoice(ev, t, out) {
  const src = ctx.createBufferSource(); src.buffer = LAB.out;
  src.playbackRate.setValueAtTime(Math.pow(2, (ev.note - S.lab.root) / 12), t);
  const g = G(0, out); src.connect(g);
  const v = vg(ev.vel) * 0.9, end = t + Math.max(0.05, ev.dur);
  g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(v, t + 0.004);
  g.gain.setValueAtTime(v, end); g.gain.setTargetAtTime(0, end, 0.04);
  src.start(t); src.stop(end + 0.3);
}
const labOnLane = lane => LAB.lane !== 'off' && !!LAB.out && (MUTE_OF[lane] || lane) === LAB.lane;

/* ---------- drawing the source (with its trim handles) and the result ---------- */
function drawWave(cv, buf, opts) {
  if (!cv) return;
  const w = cv.clientWidth || cv.parentNode.clientWidth, h = cv.clientHeight || 90, dpr = Math.min(2, window.devicePixelRatio || 1);
  cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
  const g = cv.getContext('2d'); g.setTransform(dpr, 0, 0, dpr, 0, 0); g.clearRect(0, 0, w, h);
  const css = getComputedStyle(document.documentElement), C = n => css.getPropertyValue(n).trim();
  g.fillStyle = C('--line'); g.fillRect(0, h / 2 - 0.5, w, 1);
  if (!buf) return;
  const ch = buf.numberOfChannels, n = buf.length, L = buf.getChannelData(0), R = ch > 1 ? buf.getChannelData(1) : L, per = n / w;
  for (let x = 0; x < w; x++) {
    let lo = 0, hi = 0;
    const a = Math.floor(x * per), b = Math.min(n, Math.floor((x + 1) * per) + 1), stp = Math.max(1, Math.floor((b - a) / 64));
    for (let i = a; i < b; i += stp) { const v = (L[i] + R[i]) * 0.5; if (v < lo) lo = v; if (v > hi) hi = v; }
    const inSel = !opts || (x / w >= opts.start && x / w <= opts.end);
    g.fillStyle = inSel ? C(opts ? '--accent-text' : '--ink') : C('--line');
    g.globalAlpha = inSel ? 0.85 : 0.6;
    g.fillRect(x, h / 2 - hi * h * 0.46, 1, Math.max(1, (hi - lo) * h * 0.46));
  }
  g.globalAlpha = 1;
  if (opts) {   // trim handles
    [opts.start, opts.end].forEach((u, i) => {
      const x = Math.round(u * (w - 2)) + 1;
      g.fillStyle = C('--accent-text'); g.fillRect(x - 1, 0, 2, h);
      g.beginPath(); g.moveTo(x, 0); g.lineTo(x + (i ? -9 : 9), 0); g.lineTo(x, 12); g.closePath(); g.fill();
    });
  }
}
function drawLabWaves() {
  drawWave($('#labSrcCv'), LAB.buf, { start: S.lab.p.start, end: S.lab.p.end });
  drawWave($('#labOutCv'), LAB.out, null);
  const m = $('#labOutMeta');
  if (m) m.textContent = LAB.out ? LAB.out.duration.toFixed(2) + ' s, 44.1 kHz stereo' : '';
}
let trimDrag = null;
document.addEventListener('pointerdown', e => {
  const cv = e.target.closest && e.target.closest('#labSrcCv');
  if (!cv || !LAB.buf) return;
  const r = cv.getBoundingClientRect(), u = (e.clientX - r.left) / r.width, p = S.lab.p;
  trimDrag = { which: Math.abs(u - p.start) <= Math.abs(u - p.end) ? 'start' : 'end', r, pid: e.pointerId };
  try { cv.setPointerCapture(e.pointerId); } catch (err) {}
  trimMove(e);
});
function trimMove(e) {
  const p = S.lab.p, u = Math.max(0, Math.min(1, (e.clientX - trimDrag.r.left) / trimDrag.r.width));
  if (trimDrag.which === 'start') p.start = +Math.min(u, p.end - 0.01).toFixed(4); else p.end = +Math.max(u, p.start + 0.01).toFixed(4);
  drawWave($('#labSrcCv'), LAB.buf, { start: p.start, end: p.end }); labChanged();
}
document.addEventListener('pointermove', e => { if (trimDrag && e.pointerId === trimDrag.pid) trimMove(e); });
document.addEventListener('pointerup', e => { if (trimDrag && e.pointerId === trimDrag.pid) trimDrag = null; });

/* ---------- the lab view ---------- */
const KEYS = ['a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k', 'o', 'l', 'p', ';'];
function renderLabHead() {
  const now = $('#labNow');
  if (!now) return;
  const it = S.lab.src && BY_ID[S.lab.src];
  now.innerHTML = '<span class="labt">' + esc(LAB.file ? LAB.srcName : it ? it.title : 'No sound loaded') + '</span><span class="labm">' +
    esc(LAB.file ? 'Your own sound' : it ? (({ vox: 'Vocal', bass: 'Bass', keys: 'Keys', loops: 'Loop', fills: 'Fill', fx: 'FX', hits: 'Drum hit' })[it.cat] || '') + (it.key ? ', ' + it.key : '') + (it.bpm ? ', ' + it.bpm + ' BPM' : '') : '') + '</span>';
  const rs = $('#labRoot');
  if (rs) rs.value = String(S.lab.root);
  renderLabKeys();
  drawLabWaves();
}
function renderLabKeys() {   // a piano: ten white keys in a row, the black keys sitting between them
  const box = $('#labKeys');
  if (!box) return;
  const base = 60 + 12 * LAB.oct, whites = [], blacks = [];
  for (let i = 0; i < KEYS.length; i++) {
    const m = base + i, blk = [1, 3, 6, 8, 10].includes(m % 12);
    const b = '<button type="button" class="pk' + (blk ? ' blk' : '') + (m === S.lab.root ? ' root' : '') + '" data-labnote="' + m + '" aria-label="Play ' + noteName(m) + '"' +
      (blk ? ' style="left:calc(' + (whites.length * 10) + '% - 3.2%)"' : '') + '><span class="pn">' + noteName(m) + '</span><span class="pkk">' + KEYS[i].toUpperCase() + '</span></button>';
    (blk ? blacks : whites).push(b);
  }
  box.innerHTML = '<div class="whites">' + whites.join('') + '</div>' + blacks.join('');
  const o = $('#labOct'); if (o) o.textContent = 'Octave ' + (LAB.oct > 0 ? '+' : '') + LAB.oct;
}
function renderLab() {
  const box = $('#labBody');
  if (!box) return;
  const p = S.lab.p;
  const rootOpts = []; for (let m = 24; m <= 96; m++) rootOpts.push('<option value="' + m + '"' + (m === S.lab.root ? ' selected' : '') + '>' + noteName(m) + '</option>');
  box.innerHTML =
    '<div class="labtop">' +
      '<div class="labpick"><label class="search">' + ICON.search + '<input type="search" id="labQ" placeholder="Find a sound to design from: a stab, a vocal, a bass, an FX" aria-label="Find a sound to design from" autocomplete="off"></label><div class="labres" id="labRes" hidden></div></div>' +
      '<button class="btn ghost" id="labFileBtn" type="button">Use your own sound</button><input type="file" id="labFile" accept="audio/*,.wav,.mp3,.aif,.aiff,.flac,.ogg,.m4a" hidden>' +
    '</div>' +
    '<div class="labwaves" id="labDrop">' +
      '<div class="labsrc"><div class="labnow" id="labNow"></div><canvas id="labSrcCv" aria-label="Source waveform. Drag to set where the sound starts and ends." role="img"></canvas></div>' +
      '<div class="labout"><div class="labouthead"><span class="label">Result</span><span class="labm tnum" id="labOutMeta"></span></div><div class="labcvbox"><canvas id="labOutCv" role="img" aria-label="Result waveform"></canvas><div class="labhead" id="labHead"></div></div></div>' +
    '</div>' +
    '<div class="labbar">' +
      '<button class="btn solid" id="labPlay" type="button">' + ICON.play + 'Play</button>' +
      '<label class="toggle"><input type="checkbox" id="labLoop"' + (LAB.loop ? ' checked' : '') + '> Loop</label>' +
      '<label class="toggle"><input type="checkbox" id="labRev"' + (p.reverse ? ' checked' : '') + '> Reverse</label>' +
      '<label class="toggle"><input type="checkbox" id="labNorm"' + (p.norm ? ' checked' : '') + '> Normalize</label>' +
      '<span class="sp"></span><button class="btn ghost" id="labRand" type="button">Surprise me</button><button class="btn ghost" id="labReset" type="button">Reset</button>' +
    '</div>' +
    '<div class="chips labpresets" role="group" aria-label="Starting points">' + LAB_PRESETS.map(([n], i) => '<button class="chip" type="button" data-labpreset="' + i + '">' + esc(n) + '</button>').join('') + '</div>' +
    '<div class="labgrid">' + LAB_GROUPS.map(([name, ids]) => '<div class="labgrp"><div class="labgh">' + esc(name) +
      (name === 'Filter' ? '<span class="seg" role="group" aria-label="Filter type">' + [['lp', 'Low'], ['hp', 'High'], ['bp', 'Band']].map(([v, l]) => '<button type="button" data-ftype="' + v + '" aria-pressed="' + (p.ftype === v) + '">' + l + '</button>').join('') + '</span>' : '') + '</div>' +
      '<div class="labknobs">' + ids.map(id => knobHTML('lab:' + id, LAB_P[id][0])).join('') + '</div>' +
      (name === 'Gate' ? '<div class="gsteps" role="group" aria-label="Gate pattern, 16th notes">' + p.pattern.map((on, i) => '<button type="button" data-gstep="' + i + '" aria-pressed="' + on + '" aria-label="Step ' + (i + 1) + '"></button>').join('') + '</div>' : '') +
      '</div>').join('') + '</div>' +
    '<div class="labplay">' +
      '<div class="labkeyhead"><span class="label">Play it across the keyboard</span><span class="labroot"><label for="labRoot">Root note</label><select id="labRoot" class="keysel">' + rootOpts.join('') + '</select></span>' +
      '<span class="seg" role="group" aria-label="Keyboard octave"><button type="button" data-laboct="-1" aria-label="Octave down">−</button><button type="button" id="labOct" disabled>Octave 0</button><button type="button" data-laboct="1" aria-label="Octave up">+</button></span></div>' +
      '<div class="labkeys" id="labKeys"></div>' +
      '<div class="labuse"><span class="label">Play it on the kit</span><span class="seg" role="group" aria-label="Use the designed sound on a kit lane">' +
        [['off', 'Off'], ['bass', 'Bass'], ['chords', 'Chords'], ['lead', 'Melody']].map(([v, l]) => '<button type="button" data-lablane="' + v + '" aria-pressed="' + (LAB.lane === v) + '">' + l + '</button>').join('') + '</span>' +
        '<span class="hint" style="margin:0">Every note of that lane plays your sound, pitched from the root note.</span></div>' +
    '</div>' +
    '<form class="labask" id="labAsk" hidden><label class="label" for="labAskQ">Describe how it should sound</label><div class="askrow"><input id="labAskQ" type="text" maxlength="240" autocomplete="off" placeholder="Darker and dubby, with a long echo tail"><button class="btn solid" id="labAskGo" type="submit">Shape it</button></div><p class="hint" id="labAskMsg" aria-live="polite"></p></form>' +
    '<div class="labmine"><div class="labgh">My sounds <span class="hint" id="labMineNote" style="margin:0"></span></div><div id="labMine"></div></div>';
  LAB.built = true;
  renderLabHead(); renderMine(); labAskUI();
}
function labSetParams(p, keepTrim) {
  const cur = S.lab.p, np = cleanParams(Object.assign({}, labDefaults(), p));
  if (keepTrim) { np.start = cur.start; np.end = cur.end; }
  S.lab.p = np;
  Object.keys(LAB_P).forEach(k => knobPaint('lab:' + k));
  $$('[data-ftype]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.ftype === np.ftype)));
  $$('[data-gstep]').forEach(b => b.setAttribute('aria-pressed', String(!!np.pattern[+b.dataset.gstep])));
  const rv = $('#labRev'); if (rv) rv.checked = np.reverse;
  const nm = $('#labNorm'); if (nm) nm.checked = np.norm;
  labChanged();
}
function labSurprise() {
  const pick = a => a[Math.floor(Math.random() * a.length)], r = (lo, hi) => lo + Math.random() * (hi - lo);
  const hp = Math.random() < 0.3, p = {
    pitch: pick([-12, -7, -5, 0, 0, 0, 3, 5, 7, 12]), reverse: Math.random() < 0.15,
    ftype: hp ? 'hp' : Math.random() < 0.15 ? 'bp' : 'lp', cutoff: hp ? r(0.1, 0.4) : r(0.5, 1), reso: r(0, 0.6), fenv: Math.random() < 0.4 ? r(-0.6, 0.8) : 0, fdecay: r(80, 900),
    drive: Math.random() < 0.5 ? r(0, 0.5) : 0, bits: Math.random() < 0.25 ? Math.round(r(6, 12)) : 16, down: Math.random() < 0.25 ? Math.round(r(2, 6)) : 1,
    length: Math.random() < 0.4 ? Math.round(r(25, 80)) : 100, release: Math.round(r(20, 400)), attack: Math.random() < 0.2 ? Math.round(r(50, 400)) : 0,
    gate: Math.random() < 0.35 ? r(0.6, 1) : 0, pattern: Array.from({ length: 16 }, (_, i) => i % 4 === 0 || Math.random() < 0.45),
    dtime: Math.floor(r(0, 5)), dfb: r(0.2, 0.7), dmix: Math.random() < 0.6 ? r(0.05, 0.4) : 0, rsize: r(0.6, 3.2), rmix: Math.random() < 0.6 ? r(0.05, 0.4) : 0, width: r(0.8, 1.6)
  };
  labSetParams(p, true);
  if (!LAB.play) labPlay();
}

/* ---------- picking a source ---------- */
const LAB_CATS = ['vox', 'bass', 'keys', 'loops', 'fills', 'fx', 'hits'];
function labSearch(q) {
  const res = $('#labRes');
  q = q.trim().toLowerCase();
  if (!q) { res.hidden = true; res.innerHTML = ''; return; }
  const hits = ITEMS.filter(i => LAB_CATS.includes(i.cat) && !i.hidden && q.split(/\s+/).every(w => hay(i).includes(w))).slice(0, 40);
  res.innerHTML = hits.length ? hits.map(i => '<button type="button" data-labsrc="' + i.id + '"><span>' + esc(i.cat === 'vox' && i.group !== 'chops' && i.group !== 'pads' ? '“' + i.title + '”' : i.title) + '</span><span class="labm">' + esc(kindOf(i) + (i.key ? ', ' + i.key : '') + (i.bpm ? ', ' + i.bpm + ' BPM' : '')) + '</span></button>').join('')
    : '<p class="hint" style="margin:6px 10px">Nothing matches “' + esc(q) + '”.</p>';
  res.hidden = false;
}

/* ---------- saving: a WAV (in a zip, as every save here) and the settings ---------- */
function wavBytes(buf) {
  const ch = buf.numberOfChannels, n = buf.length, len = n * ch * 2, out = new Uint8Array(44 + len), v = new DataView(out.buffer);
  const w4 = (o, s) => { for (let i = 0; i < 4; i++) out[o + i] = s.charCodeAt(i); };
  w4(0, 'RIFF'); v.setUint32(4, 36 + len, true); w4(8, 'WAVE'); w4(12, 'fmt '); v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, ch, true);
  v.setUint32(24, buf.sampleRate, true); v.setUint32(28, buf.sampleRate * 2 * ch, true); v.setUint16(32, 2 * ch, true); v.setUint16(34, 16, true); w4(36, 'data'); v.setUint32(40, len, true);
  const chans = []; for (let c = 0; c < ch; c++) chans.push(buf.getChannelData(c));
  for (let i = 0, o = 44; i < n; i++) for (let c = 0; c < ch; c++, o += 2) { const x = Math.max(-1, Math.min(1, chans[c][i])); v.setInt16(o, x < 0 ? x * 32768 : x * 32767, true); }
  return out;
}
function settingsText(name) {
  const p = cleanParams(S.lab.p), it = S.lab.src && BY_ID[S.lab.src];
  return [name, '', 'Made in the Sound lab of the UKG & Minimal Pack.', 'Source: ' + (LAB.file ? LAB.srcName + ' (your own sound)' : it ? it.title + ' (' + (it.pack || it.id) + ')' : ''), 'Root note: ' + noteName(S.lab.root) + ' (MIDI ' + S.lab.root + ')', 'Tempo for the gate and echo: ' + ks().bpm + ' BPM', '',
    'Trim: ' + Math.round(p.start * 100) + '% to ' + Math.round(p.end * 100) + '%' + (p.reverse ? ', reversed' : ''),
    'Filter: ' + ({ lp: 'low-pass', hp: 'high-pass', bp: 'band-pass' })[p.ftype] + ' ' + fmtHz(p.cutoff),
    ...Object.keys(LAB_P).map(k => LAB_P[k][0] + ': ' + LAB_P[k][5](p[k])),
    'Gate pattern: ' + p.pattern.map(x => (x ? 'x' : '.')).join(''), 'Normalize: ' + (p.norm ? 'on' : 'off'), ''].join('\r\n');
}
const fileSafe = s => String(s).replace(/[\\/:*?"<>|]+/g, '').replace(/\s+/g, ' ').trim().slice(0, 60) || 'Sound';
async function labSave() {
  if (!LAB.out) { toast('Load a sound first.'); return; }
  const base = fileSafe(LAB.srcName + ' (designed)');
  await guarded(async () => {
    const parts = P.makeZipParts([{ name: base + '.wav', data: wavBytes(LAB.out) }, { name: base + ' - settings.txt', data: new TextEncoder().encode(settingsText(base)) }]);
    await offer(base + '.zip', parts);
  });
}

/* ---------- My sounds: designs (source + settings) kept in this browser, and in the viewer's account where it can be saved ---------- */
S.designs = (store.get('designs', []) || []).filter(d => d && typeof d === 'object' && d.id && BY_ID[d.src]);
let soundsDoc = null, soundsWriting = false, soundsAgain = false;
function keepDesign() {
  if (LAB.file || !S.lab.src || !BY_ID[S.lab.src]) { toast('My sounds keeps designs made from the pack. Save your own sound as a WAV instead.'); return; }
  const it = BY_ID[S.lab.src], n = S.designs.filter(d => d.src === S.lab.src).length + 1;
  S.designs.unshift({ id: 'd' + Date.now().toString(36) + Math.random().toString(36).slice(2, 6), name: it.title + (n > 1 ? ' ' + n : ''), src: S.lab.src, root: S.lab.root, p: cleanParams(S.lab.p), at: new Date().toISOString() });
  S.designs = S.designs.slice(0, 60);
  saveDesigns(); renderMine(); toast('Kept in My sounds.');
}
function saveDesigns() { store.set('designs', S.designs); pushSounds(); }
function renderMine() {
  const box = $('#labMine');
  if (!box) return;
  const note = $('#labMineNote');
  if (note) note.textContent = soundsDoc ? 'Saved to your account.' : 'Kept in this browser.';
  box.innerHTML = S.designs.length ? '<div class="mine">' + S.designs.map(d => '<div class="minerow"><button type="button" class="btn ghost" data-design="' + esc(d.id) + '">' + ICON.play + esc(d.name) + '</button><span class="labm">' + esc(kindOf(BY_ID[d.src]) + ', root ' + noteName(d.root) + (d.at ? ', kept ' + new Date(d.at).toLocaleDateString(undefined, { day: 'numeric', month: 'short' }) : '')) + '</span><button type="button" class="mute" data-deldesign="' + esc(d.id) + '" aria-label="Remove ' + esc(d.name) + '">Remove</button></div>').join('') + '</div>'
    : '<p class="hint" style="margin:6px 0 0">Shape a sound, then keep it here to come back to it. The design is saved, not the audio, so it stays small.</p>';
}
async function loadDesign(id) {
  const d = S.designs.find(x => x.id === id);
  if (!d) return;
  await labLoadItem(d.src);
  S.lab.root = d.root; labSetParams(d.p); renderLabHead();
  await labRender(); labPlay();
}
async function soundsSync(db, uid) {   // one document holds the designs: data/users/<id>/sounds
  let ref; try { ref = db.doc('data/users/' + uid + '/sounds'); } catch (e) { return; }
  soundsDoc = ref;
  let first = true;
  ref.onSnapshot(snap => {
    const d = snap && snap.exists ? snap.data() : null;
    const remote = (d && Array.isArray(d.items) ? d.items : []).filter(x => x && typeof x === 'object' && typeof x.id === 'string' && BY_ID[x.src]).map(x => ({ id: x.id, name: String(x.name || '').slice(0, 80), src: x.src, root: Math.max(24, Math.min(96, Math.round(+x.root) || 60)), p: cleanParams(x.p), at: String(x.at || '') }));
    if (first) {
      first = false;
      const base = new Set(store.get('designsBase', [])), localIds = new Set(S.designs.map(x => x.id)), rIds = new Set(remote.map(x => x.id));
      const merged = remote.filter(x => !(base.has(x.id) && !localIds.has(x.id))).concat(S.designs.filter(x => !rIds.has(x.id) && !base.has(x.id)));
      merged.sort((x, y) => (y.at > x.at ? 1 : -1));
      S.designs = merged.slice(0, 60); store.set('designs', S.designs);
      if (merged.length !== remote.length || merged.some(x => !rIds.has(x.id))) pushSounds(); else store.set('designsBase', [...rIds]);
      renderMine(); return;
    }
    if (snap.metadata && snap.metadata.hasPendingWrites) return;
    S.designs = remote; store.set('designs', remote); store.set('designsBase', remote.map(x => x.id)); renderMine();
  }, () => { soundsDoc = null; renderMine(); });
}
async function pushSounds() {
  const ref = soundsDoc;
  if (!ref) return;
  if (soundsWriting) { soundsAgain = true; return; }
  soundsWriting = true;
  try { await ref.set({ items: S.designs, updated: new Date().toISOString() }); store.set('designsBase', S.designs.map(x => x.id)); }
  catch (e) { if (e && ['invalid_argument', 'revoked', 'not_granted', 'capability_disabled', 'capability_removed'].includes(e.code)) { soundsDoc = null; renderMine(); } }
  finally { soundsWriting = false; if (soundsAgain) { soundsAgain = false; pushSounds(); } }
}

/* ---------- Claude shapes the sound from a description ---------- */
let labAskCtl = null;
function labAskUI() { const f = $('#labAsk'); if (f) f.hidden = !CAP.sample; }
function shapePrompt(desc) {
  const it = S.lab.src && BY_ID[S.lab.src], p = cleanParams(S.lab.p);
  const what = LAB.file ? 'the producer\'s own sound "' + LAB.srcName + '"' : it ? '"' + it.title + '", ' + kindOf(it) + (it.key ? ', ' + it.key : '') + (it.note ? ' (' + it.note + ')' : '') : 'a sound';
  return 'You are setting the controls of a sampler and effects chain that shapes one sound from a UK garage and house sample pack.\n' +
    'The sound: ' + what + ', ' + (LAB.buf ? LAB.buf.duration.toFixed(1) : '?') + ' seconds long, root note ' + noteName(S.lab.root) + '. The kit plays at ' + ks().bpm + ' BPM.\n' +
    'The producer wants: "' + desc + '"\n\n' +
    'Current settings: ' + JSON.stringify(p) + '\n\n' +
    'Controls:\n' +
    '- pitch: semitones, -24 to 24. fine: cents, -100 to 100.\n- start, end: the part of the sound used, 0 to 1, start below end. reverse: true or false.\n' +
    '- attack: ms 0 to 1000. length: percent of the sound kept, 5 to 100. release: ms 5 to 2000.\n' +
    '- ftype: "lp", "hp" or "bp". cutoff: 0 to 1 on a log scale (0 = 20 Hz, 0.33 = 200 Hz, 0.5 = 630 Hz, 0.67 = 2 kHz, 0.83 = 6.3 kHz, 1 = 20 kHz). reso: 0 to 1. fenv: -1 to 1, a filter sweep that starts higher (positive) or lower (negative) and settles at the cutoff. fdecay: sweep time in ms, 10 to 2000.\n' +
    '- drive: 0 to 1. bits: 4 to 16 (16 is clean). down: 1 to 16, a sample-rate divider (1 is clean).\n' +
    '- gate: 0 to 1, depth of a rhythmic gate. pattern: 16 booleans, one per 16th note, true = open.\n' +
    '- dtime: 0 to 4 for an echo of 1/16, 1/8, 3/16, 1/4 or 3/8 note. dfb: feedback 0 to 0.85. dmix: echo level 0 to 1.\n' +
    '- rsize: reverb length in seconds, 0.2 to 5. rmix: reverb level 0 to 1. width: stereo width 0 to 2 (1 = as recorded).\n' +
    '- gain: output in dB, -18 to 12. norm: true to normalize the result to -1 dBFS.\n\n' +
    'Reply with only a JSON object holding the controls to change (the rest stay as they are) and "note": one short sentence, for the producer, on what you changed. Example: {"ftype":"lp","cutoff":0.62,"dmix":0.4,"dfb":0.6,"note":"Closed the filter and added a long 3/16 echo."}';
}
async function labShape(desc) {
  desc = String(desc || '').trim().slice(0, 240);
  const msg = $('#labAskMsg');
  if (!desc) { msg.textContent = 'Describe the sound first, for example warmer, darker, glitchy or far away.'; return; }
  if (!CAP.sample || labAskCtl || !LAB.buf) return;
  labAskCtl = new AbortController();
  $('#labAskGo').disabled = true; msg.textContent = 'Claude is shaping it…';
  try {
    const r = await CAP.sample.json(shapePrompt(desc), { signal: labAskCtl.signal, modelTier: 'quick' });
    if (!r || typeof r !== 'object' || Array.isArray(r)) throw { code: 'invalid_json' };
    const next = Object.assign({}, S.lab.p);
    Object.keys(r).forEach(k => { if (k in next) next[k] = r[k]; });
    labSetParams(next);
    msg.textContent = String(r.note || 'Done.').slice(0, 200);
    if (!LAB.play) labPlay();
  } catch (e) {
    const code = e && e.code;
    msg.textContent = code === 'cancelled' ? '' : ASK_ERR[code] || ASK_ERR.upstream_error;
    if (ASK_OFF.includes(code)) { CAP.sample = null; $('#askBtn').hidden = true; labAskUI(); }
  } finally { labAskCtl = null; const b = $('#labAskGo'); if (b) b.disabled = false; }
}

/* ---------- the lab's own clicks, keys and inputs ---------- */
async function openLab(id) {
  if (S.view !== 'lab') setView('lab', true);
  if (id && id !== S.lab.src) await labLoadItem(id);
  window.scrollTo({ top: $('#view-lab').getBoundingClientRect().top + scrollY - $('#deck').offsetHeight - 6, behavior: 'auto' });
}
function labShow() {
  if (!LAB.built) renderLab();
  else { renderLabHead(); labAskUI(); }
  if (!LAB.buf && S.lab.src && BY_ID[S.lab.src]) labLoadItem(S.lab.src);
  else requestAnimationFrame(drawLabWaves);
}
document.addEventListener('click', e => {
  const t = e.target.closest && e.target.closest('button');
  if (!t) return;
  if (t.dataset.design !== undefined && t.closest('#labMine')) { loadDesign(t.dataset.design); return; }
  if (t.dataset.deldesign) { S.designs = S.designs.filter(d => d.id !== t.dataset.deldesign); saveDesigns(); renderMine(); return; }
  if (t.dataset.labsrc) { $('#labRes').hidden = true; $('#labQ').value = ''; labLoadItem(t.dataset.labsrc).then(() => labPlay()); return; }
  if (t.dataset.designsrc) { openLab(t.dataset.designsrc); return; }
  if (t.id === 'labFileBtn') { $('#labFile').click(); return; }
  if (t.id === 'labPlay') { if (LAB.play) labStop(); else labPlay(); return; }
  if (t.id === 'labRand') { labSurprise(); return; }
  if (t.id === 'labReset') { labSetParams({}); return; }
  if (t.id === 'labSave') { labSave(); return; }
  if (t.id === 'labKeep') { keepDesign(); return; }
  if (t.dataset.labpreset) { labSetParams(LAB_PRESETS[+t.dataset.labpreset][1], true); if (!LAB.play) labPlay(); return; }
  if (t.dataset.ftype) {
    const was = S.lab.p.ftype, v = t.dataset.ftype;
    if (was === v) return;
    S.lab.p.ftype = v;
    if (v === 'hp' && S.lab.p.cutoff > 0.8) S.lab.p.cutoff = 0.2; else if (v === 'lp' && S.lab.p.cutoff < 0.2) S.lab.p.cutoff = 1; else if (v === 'bp') S.lab.p.cutoff = 0.6;
    $$('[data-ftype]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.ftype === v))); knobPaint('lab:cutoff'); labChanged(); return;
  }
  if (t.dataset.gstep) { const i = +t.dataset.gstep; S.lab.p.pattern[i] = !S.lab.p.pattern[i]; t.setAttribute('aria-pressed', String(S.lab.p.pattern[i])); if (S.lab.p.gate < 0.05) { S.lab.p.gate = 1; knobPaint('lab:gate'); } labChanged(); return; }
  if (t.dataset.lablane) {
    LAB.lane = t.dataset.lablane;
    $$('[data-lablane]').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.lablane === LAB.lane)));
    if (LAB.lane !== 'off') toast((S.playing ? '' : 'Press Play: ') + curKit().id + "'s " + ({ bass: 'bass', chords: 'chords', lead: 'melody' })[LAB.lane] + ' now plays your sound.', 2600);
    return;
  }
  if (t.dataset.laboct) { LAB.oct = Math.max(-2, Math.min(2, LAB.oct + +t.dataset.laboct)); renderLabKeys(); return; }
  if (t.dataset.labnote) { labPlay(+t.dataset.labnote); keyFlash(t); return; }
});
document.addEventListener('change', e => {
  const id = e.target && e.target.id;
  if (id === 'labFile') { labLoadFile(e.target.files && e.target.files[0]); e.target.value = ''; }
  else if (id === 'labLoop') { LAB.loop = e.target.checked; if (LAB.play) labPlay(); }
  else if (id === 'labRev') { S.lab.p.reverse = e.target.checked; labChanged(); }
  else if (id === 'labNorm') { S.lab.p.norm = e.target.checked; labChanged(); }
  else if (id === 'labRoot') { S.lab.root = +e.target.value; store.set('lab', S.lab); renderLabKeys(); }
});
document.addEventListener('input', e => { if (e.target && e.target.id === 'labQ') labSearch(e.target.value); });
document.addEventListener('pointerdown', e => { const r = $('#labRes'); if (r && !r.hidden && !(e.target.closest && e.target.closest('.labpick'))) r.hidden = true; });
document.addEventListener('keydown', e => { if (e.key === 'Escape') { const r = $('#labRes'); if (r && !r.hidden) { r.hidden = true; const q = $('#labQ'); if (q) q.focus(); } } });
document.addEventListener('submit', e => { if (e.target && e.target.id === 'labAsk') { e.preventDefault(); labShape($('#labAskQ').value); } });
document.addEventListener('keydown', e => {   // the computer keyboard plays the designed sound while the lab is open
  if (S.view !== 'lab' || e.metaKey || e.ctrlKey || e.altKey || e.repeat) return;
  if (/INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName) || (e.target.closest && e.target.closest('.kn'))) return;
  const i = KEYS.indexOf((e.key || '').toLowerCase());
  if (i < 0) return;
  e.preventDefault();
  const m = 60 + 12 * LAB.oct + i, b = $('[data-labnote="' + m + '"]');
  labPlay(m);
  if (b) keyFlash(b);
});
function keyFlash(b) { b.classList.remove('hit'); void b.offsetWidth; b.classList.add('hit'); clearTimeout(b._ht); b._ht = setTimeout(() => b.classList.remove('hit'), 160); }
document.addEventListener('dragover', e => { if (S.view === 'lab' && e.target.closest && e.target.closest('#labDrop')) { e.preventDefault(); $('#labDrop').classList.add('drop'); } });
document.addEventListener('dragleave', e => { if (e.target.closest && e.target.closest('#labDrop')) $('#labDrop').classList.remove('drop'); });
document.addEventListener('drop', e => {
  if (!(e.target.closest && e.target.closest('#labDrop'))) return;
  e.preventDefault(); $('#labDrop').classList.remove('drop');
  const f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
  if (f) labLoadFile(f);
});
