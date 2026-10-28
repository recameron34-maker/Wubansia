const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// Wave 5 smoke test: the garage-house kits GH01-GH04 in the page — list, detail, loop playback with the GH kit, samplers and
// square lead, full songs and extended mixes driven through the page's own scheduler, kit zip, garage-house pack, README.
const { JSDOM } = require('jsdom');
const fs = require('fs'), path = require('path');
const SITE = PACK_ROOT + '/site';
const html = fs.readFileSync(process.env.HTML || path.join(SITE, 'index.html'), 'utf8');
const MAN = JSON.parse('{' + html.match(/"items":\[[^]*?\],"bytes":\d+\}/)[0]);
const ITEMS = MAN.items;
const errors = [], saves = [], missing = [];
let failures = 0;
const ok = (cond, msg) => { console.log((cond ? '  ok   ' : '  FAIL ') + msg); if (!cond) failures++; };
// identify sprite slices by frame count + channels; plain files by URL
const byShape = new Map();
ITEMS.forEach(it => { const m = /^sprite:[^?]+\?o=\d+&n=(\d+)&c=(\d)$/.exec(it.file || ''); if (m) { const k = m[1] + '/' + m[2]; if (!byShape.has(k)) byShape.set(k, []); byShape.get(k).push(it.id); } });
// the wave-3/4 sprites are FLAC now: the page tags each decoded slice with its URL, which names the sample
const byUrl = new Map();
ITEMS.forEach(it => { if (/^flac:audio\/s[34]\//.test(it.file || '')) { if (!byUrl.has(it.file)) byUrl.set(it.file, []); byUrl.get(it.file).push(it.id); } });
const stats = { osc: 0, starts: {}, loops: 0, detune: 0, filt: 0 };
function fakeParam(kind) { return { value: 0, setValueAtTime(v) { this.value = v; }, linearRampToValueAtTime() { if (kind === 'detune') stats.detune++; }, exponentialRampToValueAtTime() {}, setTargetAtTime() { if (kind === 'frequency') stats.filt++; }, cancelScheduledValues() {}, cancelAndHoldAtTime() {} }; }
function fakeNode() {
  return new Proxy({}, { get(t, k) {
    if (k in t) return t[k];
    if (['frequency', 'detune', 'gain', 'Q', 'pan', 'delayTime', 'threshold', 'knee', 'ratio', 'attack', 'release', 'playbackRate'].includes(k)) return (t[k] = fakeParam(k));
    if (k === 'connect') return (n) => n;
    if (['disconnect', 'start', 'stop', 'setPeriodicWave'].includes(k)) return () => {};
    return undefined;
  }, set(t, k, v) { t[k] = v; return true; } });
}
class FakeAC {
  constructor() { this.sampleRate = 44100; this.currentTime = 0; this.destination = fakeNode(); this.state = 'running'; FakeAC.inst = this; }
  resume() { return Promise.resolve(); }
  createBuffer(ch, n) { return { getChannelData: () => new Float32Array(n), duration: n / 44100, numberOfChannels: ch, length: n }; }
  createPeriodicWave() { return {}; }
  decodeAudioData(ab, okf) {
    const b = Buffer.from(ab), ch = b.readUInt16LE(22), n = b.readUInt32LE(40) / (2 * ch);
    const ids = ab._url ? (byUrl.get(ab._url) || [ab._url]) : (byShape.get(n + '/' + ch) || ['?' + n + '/' + ch]);
    const res = { duration: n / 44100, length: n, numberOfChannels: ch, getChannelData: () => new Float32Array(n), _ids: ids };
    const p = Promise.resolve(res); if (okf) p.then(okf); return p;
  }
}
['createGain', 'createBiquadFilter', 'createDynamicsCompressor', 'createConvolver', 'createDelay', 'createStereoPanner', 'createWaveShaper'].forEach(m => { FakeAC.prototype[m] = function () { return fakeNode(); }; });
FakeAC.prototype.createOscillator = function () { stats.osc++; return fakeNode(); };
FakeAC.prototype.createBufferSource = function () {
  const n = fakeNode();
  n.start = () => { const k = n.buffer && n.buffer._ids ? n.buffer._ids.join('|') : 'noise'; stats.starts[k] = (stats.starts[k] || 0) + 1; if (n.loop && n.loopEnd > 0) stats.loops++; };
  return n;
};
const dom = new JSDOM(html.replace(/<link[^>]+fonts[^>]+>/g, ''), {
  runScripts: 'dangerously', pretendToBeVisual: true, url: 'https://example.test/',
  beforeParse(w) {
    w.AudioContext = FakeAC; w.TextEncoder = require('util').TextEncoder; w.Blob = require('buffer').Blob;
    w.ResizeObserver = class { observe() {} };
    w.matchMedia = () => ({ matches: false, addEventListener() {} });
    w.scrollTo = () => {};
    w.HTMLCanvasElement.prototype.getContext = () => new Proxy({}, { get: (t, k) => (k in t ? t[k] : () => {}), set: (t, k, v) => { t[k] = v; return true; } });
    const realSI = w.setInterval.bind(w), realCI = w.clearInterval.bind(w);
    w.setInterval = (fn, ms) => { if (ms === 25) { w.__tick = fn; return 777; } return realSI(fn, ms); };
    w.clearInterval = id => { if (id === 777) { w.__tick = null; return; } realCI(id); };
    w.fetch = async (url) => {
      const p = path.join(SITE, String(url));
      if (!fs.existsSync(p)) { missing.push(String(url)); return { ok: false, status: 404 }; }
      const b = fs.readFileSync(p);
      return { ok: true, status: 200, arrayBuffer: async () => { const ab = b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength); if (!/\/s\d\//.test(String(url))) ab._url = String(url); return ab; } };
    };
    w.claude = { use: async (name) => name === 'downloads' ? { save: async ({ filename, data }) => { const ab = await data.arrayBuffer(); saves.push({ filename, bytes: Buffer.from(ab) }); return { status: 'saved' }; } } : null };
    w.addEventListener('error', e => errors.push('window error: ' + (e.error && e.error.stack || e.message)));
    w.console.error = (...a) => errors.push('console.error: ' + a.map(x => x && x.stack || x).join(' '));
  }
});
const w = dom.window, d = w.document;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const $ = s => d.querySelector(s), $$ = s => [...d.querySelectorAll(s)];
const click = (el, what) => { if (!el) throw new Error('missing element: ' + what); el.dispatchEvent(new w.MouseEvent('click', { bubbles: true })); };
function unzip(buf) {
  const eocd = buf.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06])), n = buf.readUInt16LE(eocd + 10); let p = buf.readUInt32LE(eocd + 16); const out = {};
  for (let i = 0; i < n; i++) {
    const csize = buf.readUInt32LE(p + 20), fl = buf.readUInt16LE(p + 28), el = buf.readUInt16LE(p + 30), cl = buf.readUInt16LE(p + 32), lo = buf.readUInt32LE(p + 42);
    const name = buf.slice(p + 46, p + 46 + fl).toString(), lfl = buf.readUInt16LE(lo + 26), lel = buf.readUInt16LE(lo + 28);
    out[name] = buf.slice(lo + 30 + lfl + lel, lo + 30 + lfl + lel + csize); p += 46 + fl + el + cl;
  }
  return out;
}
async function saveAndWait(el, ms = 120000) {
  const n = saves.length; click(el, 'save');
  const t0 = Date.now(); while (saves.length === n && Date.now() - t0 < ms) await sleep(40);
  if (saves.length === n) throw new Error('no save — toast: ' + ($('.toast') && $('.toast').textContent));
  return saves[saves.length - 1];
}
const delta = (a, b) => { const o = {}; Object.keys(b).forEach(k => { const x = b[k] - (a[k] || 0); if (x) o[k] = x; }); return o; };
async function playThrough(secs) {
  for (let k = 0; k < 300 && !w.__tick; k++) await sleep(10);
  if (!w.__tick) return false;
  for (let x = 0; x < secs; x += 0.1) { FakeAC.inst.currentTime += 0.1; w.__tick && w.__tick(); }
  return true;
}

const BYID = id => ITEMS.find(i => i.id === id);
const STAGE8 = PACK_ROOT + '/out/wave8';
(async () => {
  await sleep(500);
  ok(errors.length === 0, 'boots without errors');
  click($('.mode[data-view="kits"]')); await sleep(30);
  console.log('  genres: ' + $$('#genres [data-genre]').map(b => b.textContent).join(' · '));
  const gv = $('#genres [data-genre="gv"]'), th = $('#genres [data-genre="th"]');
  ok(gv && /Groove house/.test(gv.textContent) && /3$/.test(gv.textContent.trim()) && th && /Tech house/.test(th.textContent) && /4$/.test(th.textContent.trim()), 'family chips: groove house 3 kits, tech house 4 kits');
  const groups = $$('.kitgroup').map(g => g.textContent);
  ok(groups.join('|') === 'UK Garage|Garage House (Disclosure-style)|Minimal Tech House|Speed Garage|Classic House|Groove House (Chris Stussy-style)|Tech House (Chris Lake & Riordan-style)', 'kit groups: ' + groups.join(' / '));
  const cards = $$('.kitcard').filter(c => /^(GV|TH)0/.test(c.dataset.kit));
  ok(cards.length === 7 && cards.every(c => c.querySelector('.newtag') && c.querySelector('.tag.t-' + c.dataset.kit.slice(0, 2).toLowerCase())), 'seven new cards with family tags and a New badge');
  click(gv); await sleep(40);
  ok($$('.kitcard').length === 3 && d.documentElement.dataset.genre === 'gv' && /Chris Stussy/.test($('#gDesc') ? $('#gDesc').textContent : d.body.textContent), 'groove house filter: 3 kits in the gv skin');
  console.log('  stage: ' + ($('#gName') && $('#gName').textContent) + ' | ' + $$('#gFacts div').map(x => x.textContent).join(' | '));
  click($('#genres [data-genre="th"]')); await sleep(40);
  ok($$('.kitcard').length === 4 && d.documentElement.dataset.genre === 'th', 'tech house filter: 4 kits in the th skin');
  click($('#genres [data-genre="all"]')); await sleep(30);

  const EXPECT = {   // per kit: hits sprite, then the sample sprites the loop must use
    GV01: ['gv_hits', ['gv_fm', 'gv_glow', 'gv_fifth']], GV02: ['gv_hits', ['gv_fm']], GV03: ['gv_hits', ['gv_fm', 'gv_glow']],
    TH01: ['th_hits', ['th_bounce']], TH02: ['th_hits', ['th_bounce']], TH03: ['th_hits', ['th_reese', 'th_rave']], TH04: ['th_hits', ['th_fifth']] };
  for (const [id, [hitsFile, sets]] of Object.entries(EXPECT)) {
    click($('[data-kit="' + id + '"]'), id); await sleep(40);
    let s0 = JSON.parse(JSON.stringify(stats));
    click($('#playBtn')); const started = await playThrough(12);
    let dl = delta(s0.starts, stats.starts);
    const has = f => Object.keys(dl).some(k => k.includes('/s8/' + f + '.ogg'));
    const n = f => Object.entries(dl).filter(([k]) => k.includes('/s8/' + f + '.ogg')).reduce((a, [, x]) => a + x, 0);
    console.log('  ' + id + ' loop: ' + [hitsFile].concat(sets).map(f => f + '×' + n(f)).join(' ') + ', osc ' + (stats.osc - s0.osc) + ', looped ' + (stats.loops - s0.loops));
    ok(started && n(hitsFile) > 50 && sets.every(has), id + ' loop plays its own kit and instruments');
    click($('#playBtn')); await sleep(30);
    for (const v of (process.env.QUICK ? [] : ['full', 'ext'])) {
      click($('[data-songv="' + v + '"]')); await sleep(30);
      const meta = $('#songMeta').textContent, secs = +meta.match(/(\d+):(\d+)/).slice(1).reduce((m, x, i) => i ? m * 60 + +x : +x, 0);
      s0 = JSON.parse(JSON.stringify(stats)); const e0 = errors.length;
      click($('#songPlay'), 'songPlay');
      const ran = await playThrough(secs + 1);
      dl = delta(s0.starts, stats.starts);
      const shots = Object.entries(dl).filter(([k]) => /w8_shots\.ogg/.test(k)).reduce((a, [, x]) => a + x, 0);
      const fx8 = Object.entries(dl).filter(([k]) => /w8_fx\.ogg/.test(k)).reduce((a, [, x]) => a + x, 0);
      console.log('  ' + id + ' ' + v + ' (' + meta + '): hits ' + n(hitsFile) + ', ' + sets.map(f => f + ' ' + n(f)).join(', ') + ', osc ' + (stats.osc - s0.osc) + ', shots ' + shots + ', fx8 ' + fx8);
      ok(ran && n(hitsFile) > 400 && sets.every(has) && errors.length === e0, id + ' ' + v + ' plays to the end without errors');
      if (v === 'ext' && /TH0[12]/.test(id)) ok(shots >= 6, id + ' extended mix fires the vocal shots (' + shots + ')');
      if (v === 'ext' && id === 'TH01') ok(fx8 > 0, 'TH01 extended mix uses the tech impact');
      click($('#songPlay'), 'stop'); await sleep(20);
    }
  }

  // Bass & keys, Loops & FX, pads
  click($('.mode[data-view="bass"]')); await sleep(40);
  const bkg = $$('#bkRows .groupname').map(x => x.textContent);
  console.log('  bass & keys groups: ' + bkg.filter(x => /groove|tech/i.test(x)).join(' / '));
  const sets = ['gv_fm', 'gv_glow', 'gv_fifth', 'th_bounce', 'th_fifth', 'th_reese', 'th_rave'];
  ok(sets.every(sid => $('[data-setrow="' + sid + '"]') && $$('[data-setrow="' + sid + '"] .nbtn').length === ITEMS.filter(i => i.set === sid).length), 'every new multisample has a set row with all its notes');
  ok(['gvbassloops', 'gvkeyloops', 'thbassloops', 'thkeyloops'].every(g => ITEMS.filter(i => i.grp === g).every(i => $('#bkRows [data-row="' + i.id + '"]'))), 'bass and key loops listed');
  click($('.mode[data-view="fx"]')); await sleep(40);
  const fxg = $$('#fxRows .groupname').map(x => x.textContent);
  console.log('  fx groups: ' + fxg.filter(x => /groove|tech/i.test(x)).join(' / '));
  ok(['dl8_gv01', 'dl8_th03', 'top8_gv', 'brk8_gv', 'bld8_th', 'fx8_riser', 'fx8_backspin'].every(id => $('#fxRows [data-row="' + id + '"]')) && fxg.some(x => /groove house/i.test(x)) && fxg.some(x => /tech house/i.test(x)), 'Loops & FX lists the new loops, build and FX in their groups');
  click($('.mode[data-view="hits"]')); await sleep(40);
  const padsG = $$('.pad').filter(p => BYID(p.dataset.pad) && BYID(p.dataset.pad).fam === 'gv'), padsT = $$('.pad').filter(p => BYID(p.dataset.pad) && BYID(p.dataset.pad).fam === 'th');
  ok(padsG.length === 8 && padsT.length === 11 && padsG.every(p => p.querySelector('.pg').textContent === 'Groove kit') && padsT.every(p => p.querySelector('.pg').textContent === 'TH kit'), 'pads: 8 groove-kit and 11 TH-kit hits');

  // "Save these" saves what the family filter shows
  click($('#genres [data-genre="th"]')); await sleep(40);
  const shown = $$('.pad').length;
  let sv0 = await saveAndWait($('#saveHits'));
  let z0 = Object.keys(unzip(sv0.bytes)).filter(n => n.endsWith('.wav'));
  ok(sv0.filename === 'Drum-Hits_Tech-House.zip' && z0.length === shown && z0.every(n => /Tech House|TH_/.test(n) || !/Groove House|GV_/.test(n)), 'tech house "Save these hits": ' + sv0.filename + ', ' + z0.length + ' hits = ' + shown + ' pads');
  click($('.mode[data-view="bass"]')); await sleep(40);
  sv0 = await saveAndWait($('#saveBK'), 300000); z0 = Object.keys(unzip(sv0.bytes)).filter(n => n.endsWith('.wav'));
  ok(sv0.filename === 'Bass-and-Keys_Tech-House.zip' && !z0.some(n => /Groove House/.test(n)) && z0.some(n => /Bounce Bass/.test(n)), 'tech house "Save these sounds" (bass & keys): ' + sv0.filename + ', ' + z0.length + ' files');
  click($('#genres [data-genre="all"]')); await sleep(40);
  // single saves come back bit-exact from the sprites
  if (!fs.existsSync(STAGE8)) console.log('  skip ten bit-exact saves: the staging WAVs (out/wave8) are not in the repo');
  else {
  let good = 0;
  for (const id of ['kick_gv', 'clap_th', 'gvfm_C1', 'gvglow_A4', 'thrave_C2', 'threese_C4', 'dl8_gv01', 'bl8_th01', 'ck8_gv01', 'fx8_riser']) {
    const it = BYID(id);
    if (!it) { console.log('    no item ' + id); continue; }
    click($('.mode[data-view="' + (it.cat === 'hits' ? 'hits' : it.cat === 'bass' || it.cat === 'keys' ? 'bass' : 'fx') + '"]')); await sleep(30);
    const btn = it.cat === 'hits' ? $('#saveHits') : $('[data-saveitem="' + id + '"]') || ($('[data-saveset="' + it.set + '"]'));
    if (!btn) { console.log('    no save button for ' + id); continue; }
    const sv = await saveAndWait(btn);
    const zz = sv.filename.endsWith('.zip') ? unzip(sv.bytes) : { [it.pack]: sv.bytes };
    const got = Object.entries(zz).find(([nm]) => nm.endsWith(it.pack.split('/').pop()));
    const ref = fs.readFileSync(STAGE8 + '/' + it.pack);
    if (got && Buffer.compare(got[1], ref) === 0) good++; else console.log('    mismatch ' + id + ' in ' + sv.filename);
  }
  ok(good === 10, 'ten sample saves are byte-identical to the rendered WAVs (' + good + '/10)');
  }

  // TH01 kit zip carries its vocal shots
  click($('.mode[data-view="kits"]')); await sleep(30);
  click($('[data-kit="TH01"]')); await sleep(30);
  let s = await saveAndWait($('#saveKit'));
  let z = unzip(s.bytes), names = Object.keys(z);
  ok(names.filter(n => n.endsWith('.mid')).length === 12 && ['TS01', 'TS02', 'TS06'].every(t => names.some(n => n.includes(t))), 'TH01 kit zip: 12 MIDI files and its vocal shots (' + names.filter(n => /TS0/.test(n)).length + ')');

  // the two family packs
  for (const fam of ['gv', 'th']) {
    s = await saveAndWait($('[data-gpack="' + fam + '"]'), 300000);
    z = unzip(s.bytes); names = Object.keys(z);
    const kitDirs = [...new Set(names.filter(n => /MIDI Kits\//.test(n)).map(n => n.split('/')[3]))];
    const famItems = ITEMS.filter(i => i.fam === fam);
    console.log('  ' + s.filename + ' ' + (s.bytes.length / 1e6).toFixed(1) + ' MB, ' + names.length + ' files; kits: ' + kitDirs.join(' | '));
    ok(kitDirs.length === (fam === 'gv' ? 3 : 4) && kitDirs.every(k => k.startsWith(fam.toUpperCase())), fam + ' pack holds its kits only');
    ok(famItems.every(i => names.some(n => n.endsWith(i.pack))), fam + ' pack holds all ' + famItems.length + ' of its sounds');
    const readme = z[names.find(n => n.endsWith('README.txt'))].toString();
    ok(/GROOVE HOUSE \(CHRIS STUSSY-STYLE\)/.test(readme) && /TECH HOUSE \(CHRIS LAKE & RIORDAN-STYLE\)/.test(readme) && /Groove house \(120 bars\)/.test(readme) && /TH04  Fifth Floor/.test(readme), fam + ' README describes both families');
  }
  s = await saveAndWait($('[data-gpack="ch"]'), 300000);
  z = unzip(s.bytes); names = Object.keys(z);
  ok(!names.some(n => /Groove House|Tech House|GV0|TH0/.test(n)) && names.some(n => /CH01/.test(n)), 'the classic house pack stays classic house (' + names.length + ' files)');

  console.log('MISSING', missing.length ? [...new Set(missing)] : 'none');
  console.log('ERRORS', errors.length ? errors.slice(0, 5) : 'none');
  console.log(failures ? failures + ' FAILURES' : 'ALL PASSED');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('TEST FAILED', e.stack); console.log(errors.slice(0, 5), missing); process.exit(1); });
