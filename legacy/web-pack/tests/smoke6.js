const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// Wave 6 smoke test: vocals stored as FLAC (decoded in the page), re-sung hooks, new hooks/pads/chops/stabs/spoken/ad-libs,
// the new hooks in the GH and new-song extended mixes, kit zip vocals, garage-house pack, README.
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

function wavData(buf) {   // the PCM bytes of a WAV
  let p = 12;
  while (p + 8 <= buf.length) {
    const id = buf.toString('ascii', p, p + 4), sz = buf.readUInt32LE(p + 4);
    if (id === 'data') return buf.subarray(p + 8, p + 8 + sz);
    p += 8 + sz + (sz & 1);
  }
  return null;
}
const SRC = PACK_ROOT + '/out/vox_src';

(async () => {
  await sleep(500);
  ok(errors.length === 0, 'boots without errors');
  const vox = ITEMS.filter(i => i.cat === 'vox');
  const flacItems = vox.filter(i => /^flac:/.test(i.file));
  ok(flacItems.length === vox.length, 'every vocal is served from FLAC (' + flacItems.length + ' of ' + vox.length + ')');
  ok(vox.filter(i => i.dry).every(i => { const w = /\?ch=(\d),(\d)&/.exec(i.file), d = /^flac:[^?]+\?ch=(\d)&n=\d+(?:&o=\d+)?$/.exec(i.dry); return w && d && i.dry.split('?')[0] === i.file.split('?')[0] && +d[1] === +w[2] + 1 && (/&o=(\d+)$/.exec(i.file) || [])[1] === (/&o=(\d+)$/.exec(i.dry) || [])[1]; }), 'wet and dry come from the same FLAC (wet pair, then its dry channel)');
  const nw = vox.filter(i => i.wave >= 6);
  console.log('  new vocals: ' + nw.length + ' (' + [...new Set(nw.map(i => i.group))].map(g => g + ' ' + nw.filter(i => i.group === g).length).join(', ') + ')');

  click($('.mode[data-view="vocals"]'), 'vocals view'); await sleep(40);
  const chips = $$('#voxFilters .chip').map(b => b.textContent);
  console.log('  filters: ' + chips.join(' · '));
  ok(chips.some(c => /^New\d+$/.test(c) && +c.replace(/\D/g, '') === nw.length), 'a New filter counting the ' + nw.length + ' new vocals');
  ok(chips.some(c => /^Vocal pads4$/.test(c)), 'a Vocal pads filter with 4 pads');
  click($('[data-voxf="new"]'), 'new filter'); await sleep(40);
  const rows = $$('#voxRows .row');
  ok(rows.length === nw.length && rows.every(r => r.querySelector('.newtag')), 'New shows ' + rows.length + ' rows, each tagged New');
  const ghRows = rows.filter(r => r.querySelector('.gtag.t-gh'));
  console.log('  garage-house tagged rows: ' + ghRows.map(r => r.dataset.row).join(' '));
  ok(ghRows.length >= 12 && ['h19', 'h20', 'h21', 'h22', 'p01'].every(id => ghRows.some(r => r.dataset.row === id)), 'GH vocals carry the garage-house tag');
  ok($$('#voxRows .groupname').some(g => /Vocal pads/.test(g.textContent)), 'pads have their own group');

  // play: a new hook, a re-sung hook, a pad, a stab
  for (const id of ['h19', 'h02', 'p01', 's13', 'v29']) {
    click($('[data-voxf="all"]')); await sleep(20);
    const b = $('[data-play="' + id + '"]');
    const s0 = JSON.parse(JSON.stringify(stats)), e0 = errors.length;
    click(b, 'play ' + id);
    let started = false;
    for (let k = 0; k < 200 && !started; k++) { await sleep(25); started = Object.keys(delta(s0.starts, stats.starts)).some(x => x.includes('/' + id + '.ogg')); }
    ok(started && errors.length === e0, 'plays ' + id + ' from its FLAC');
    click(b); await sleep(20);
  }

  // single-item save: wet + dry WAVs, bit-identical to the sources
  for (const id of ['h20', 'ba03', 'c01']) {
    const s = await saveAndWait($('[data-saveitem="' + id + '"]'));
    const z = unzip(s.bytes), names = Object.keys(z);
    const it = vox.find(i => i.id === id);
    const wet = names.find(n => n.endsWith(it.pack.split('/').pop())), dry = it.packDry ? names.find(n => n.endsWith(it.packDry.split('/').pop())) : null;
    const same = (zipBuf, file) => { const a = wavData(zipBuf), b = wavData(fs.readFileSync(file)); return a && b && a.length === b.length && a.equals(b); };
    ok(wet && same(z[wet], `${SRC}/vox/${id}.wav`) && (!it.dry || (dry && same(z[dry], `${SRC}/vox_dry/${id}.wav`))),
      s.filename + ': ' + names.join(', ') + ' — PCM identical to the masters');
  }

  // extended mixes play the hooks written for them
  const EXPECT = { GH01: ['h19', 'h06', 'h14'], GH02: ['h20', 'h02'], GH03: ['h21', 'h16', 'bh5'], GH04: ['h22', 'h03', 'bh3'], UKG08: ['h24', 'bh3'], CH07: ['h29', 'bh5'], CH08: ['h30', 'h14'], SG08: ['h28', 'h12'] };
  click($('.mode[data-view="kits"]')); await sleep(30);
  for (const [id, want] of Object.entries(EXPECT)) {
    click($('[data-kit="' + id + '"]'), id); await sleep(30);
    click($('[data-songv="ext"]')); await sleep(30);
    const meta = $('#songMeta').textContent, secs = +meta.match(/(\d+):(\d+)/).slice(1).reduce((m, x, i) => i ? m * 60 + +x : +x, 0);
    const s0 = JSON.parse(JSON.stringify(stats)), e0 = errors.length;
    click($('#songPlay'), 'songPlay');
    const ran = await playThrough(secs + 1);
    const dl = delta(s0.starts, stats.starts);
    const heard = Object.entries(dl).filter(([k]) => /\.ogg/.test(k)).map(([k, n]) => k.split('/').pop().split('.')[0] + '×' + n);
    console.log('  ' + id + ' ext (' + meta + '): ' + heard.join(' '));
    ok(ran && errors.length === e0 && want.every(h => heard.some(x => x.startsWith(h + '×'))), id + ' extended mix sings ' + want.join(' + '));
    click($('#songPlay'), 'stop'); await sleep(20);
  }

  // kit zip carries the new hook
  click($('[data-kit="GH01"]')); await sleep(30);
  let s = await saveAndWait($('#saveKit'));
  let z = unzip(s.bytes), names = Object.keys(z);
  const used = names.filter(n => /Vocals used in the extended mix\//.test(n)).map(n => n.split('/').pop());
  console.log('  ' + s.filename + ': ' + used.join(', '));
  ok(['H19', 'H06', 'H14'].every(h => used.some(n => n.includes('_' + h + '_'))), 'GH01 kit zip holds the vocals its extended mix uses');

  // garage-house pack
  s = await saveAndWait($('[data-gpack="gh"]'), 400000);
  z = unzip(s.bytes); names = Object.keys(z);
  const ghVox = vox.filter(i => /Garage House \(Disclosure-style\)/.test(i.pack));
  console.log('  ' + s.filename + ' ' + (s.bytes.length / 1e6).toFixed(1) + ' MB, ' + names.length + ' files; GH vocals ' + ghVox.length);
  ok(ghVox.every(i => names.some(n => n.endsWith(i.pack))), 'garage house pack holds all ' + ghVox.length + ' garage-house vocals');
  const readme = z[names.find(n => n.endsWith('README.txt'))].toString();
  ok(/VOCALS: HOW THEY ARE MADE/.test(readme) && /H19 /.test(readme) && /P01 /.test(readme) && /Orange light on your skin/.test(readme), 'README explains the vocals and lists the new ones');

  console.log('MISSING', missing.length ? [...new Set(missing)] : 'none');
  console.log('ERRORS', errors.length ? errors.slice(0, 5) : 'none');
  console.log(failures ? failures + ' FAILURES' : 'ALL PASSED');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('TEST FAILED', e.stack); console.log(errors.slice(0, 5), missing); process.exit(1); });
