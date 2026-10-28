const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// Halloween smoke test: the Halloween view and skin, its pads (mouse and keyboard), rows, explicit toggle, filters in Vocals and
// Loops & FX, leaving by genre, the Halloween pack zip and README, FLAC decoding of the new files.
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
const BYID = id => ITEMS.find(i => i.id === id);
const played = (dlt, id) => Object.keys(dlt).some(k => k === BYID(id).file);
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
  const hw = ITEMS.filter(i => i.group === 'halloween' || i.grp === 'halloween');
  const hwVox = hw.filter(i => i.cat === 'vox'), hwFx = hw.filter(i => i.cat === 'fx');
  console.log('  halloween items: ' + hw.length + ' (' + hwVox.length + ' vocals, ' + hwFx.length + ' FX)');
  ok(hw.length >= 90 && hw.every(i => /^flac:audio\/voxf\/[\w]+\.ogg\?ch=\d,\d&n=\d+(?:&o=\d+)?$/.test(i.file)), 'every Halloween item is a FLAC (' + hw.length + ')');
  ok(hwFx.every(i => !i.dry) && hwVox.every(i => /\?ch=\d&n=\d+(?:&o=\d+)?$/.test(i.dry || '')), 'FX have no dry channel; vocals carry a dry take');
  ok(hw.every(i => /^Halloween\//.test(i.pack)), 'every Halloween file is packed under Halloween/');
  const tab = $('.mode[data-view="halloween"]');
  ok(tab && tab.querySelector('.count').textContent === String(hw.length), 'Halloween tab with count ' + (tab && tab.querySelector('.count').textContent));

  // enter by the hero button
  ok($('#hwBtn') && $('#hwBtnTxt').textContent === 'Halloween sounds', 'hero has the Halloween button');
  const skin0 = d.documentElement.dataset.genre;
  click($('#hwBtn'), 'hero halloween'); await sleep(60);
  ok(d.documentElement.dataset.genre === 'hw' && !$('#view-halloween').hidden && $('#gName').textContent === 'Halloween', 'hero button opens Halloween in its own skin (was ' + skin0 + ')');
  ok($('#hwBtnTxt').textContent === 'Back to the kits' && $('#shuffleBtn').hidden && !$('#hwSave').hidden && $('#gpackBtn').hidden, 'hero swaps to Back to the kits and the Halloween pack');
  console.log('  facts: ' + $$('#gFacts div').map(x => x.textContent).join(' | '));
  const pads = $$('#hwPads .pad');
  ok(pads.length === 16 && pads.every(p => p.dataset.pad && hw.some(i => i.id === p.dataset.pad)), '16 haunted pads, all Halloween sounds: ' + pads.map(p => p.querySelector('.pn').textContent).join(', '));
  const groups = $$('#hwRows .groupname').map(g => g.textContent);
  console.log('  groups: ' + groups.map((g, i) => g + ' (' + $$('#hwRows .rows')[i].querySelectorAll('.row').length + ')').join(' / '));
  ok(groups.length === 11 && $$('#hwRows .row').length === hw.length, 'eleven groups listing all ' + hw.length + ' items');
  const castle = hw.filter(i => i.hsub === 'castle' || (i.castle && !i.zf));
  const zf = hw.filter(i => i.hsub === 'zf' || i.zf);
  ok(/Murder at Cameron Castle/.test(groups[0]) && $$('#hwRows .rows')[0].querySelectorAll('.row').length === castle.length, 'the castle set leads the list (' + castle.length + ' items)');
  ok(castle.every(i => /^Halloween\/Murder at Cameron Castle\//.test(i.pack)), 'castle files pack into their own folder');
  const supper = hw.filter(i => i.hsub === 'supper');
  ok(/Midnight Supper/.test(groups[1]) && $$('#hwRows .rows')[1].querySelectorAll('.row').length === supper.length && supper.every(i => /^Halloween\/Murder at Cameron Castle\//.test(i.pack)), 'the Midnight Supper set follows (' + supper.length + ' items)');

  // pads: mouse and keyboard
  let before = Object.assign({}, stats.starts);
  click(pads[0], 'pad 1'); await sleep(150);
  let dlt = delta(before, stats.starts);
  ok(played(dlt, 'hw_fx_thunder'), 'pad 1 plays the thunder: ' + JSON.stringify(dlt));
  before = Object.assign({}, stats.starts);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: 'q', bubbles: true })); await sleep(150);
  dlt = delta(before, stats.starts);
  ok(played(dlt, 'hv14'), 'key Q plays "Boo!": ' + JSON.stringify(dlt));
  before = Object.assign({}, stats.starts);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: 'x', bubbles: true })); await sleep(150);
  dlt = delta(before, stats.starts);
  ok(played(dlt, 'hv21'), 'key X plays the evil laugh: ' + JSON.stringify(dlt));

  // the castle pad bank
  click($('[data-hwbank="castle"]'), 'castle bank'); await sleep(40);
  const cp = $$('#hwPads .pad');
  ok(cp.length === 16 && cp.every(p => castle.some(i => i.id === p.dataset.pad) || /^hw_fx_/.test(p.dataset.pad)) && $('[data-hwbank="castle"]').getAttribute('aria-pressed') === 'true', 'castle bank: ' + cp.map(p => p.querySelector('.pn').textContent).join(', '));
  ok(cp.every(p => !BYID(p.dataset.pad).explicit), 'no explicit drops on the pads');
  before = Object.assign({}, stats.starts);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: '1', bubbles: true })); await sleep(150);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: '3', bubbles: true })); await sleep(150);
  dlt = delta(before, stats.starts);
  ok(played(dlt, 'mc_fx_gong') && played(dlt, 'mc02'), 'keys 1 and 3 play the gong and "Murder at Cameron Castle": ' + JSON.stringify(dlt));
  click($('[data-hwbank="supper"]'), 'supper bank'); await sleep(40);
  const sp = $$('#hwPads .pad');
  ok(sp.length === 16 && sp.every(p => supper.some(i => i.id === p.dataset.pad) && !BYID(p.dataset.pad).explicit), 'supper bank: ' + sp.map(p => p.querySelector('.pn').textContent).join(', '));
  before = Object.assign({}, stats.starts);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: '1', bubbles: true })); await sleep(150);
  dlt = delta(before, stats.starts);
  ok(played(dlt, 'mc22'), 'key 1 announces the supper: ' + JSON.stringify(dlt));
  click($('[data-hwbank="zf"]'), 'zf bank'); await sleep(40);
  const zp = $$('#hwPads .pad');
  ok(zp.length === 16 && zf.every(i => zp.some(p => p.dataset.pad === i.id)) && $$('#hwRows .groupname')[2].textContent.includes('Zebedee & Flash') && $$('#hwRows .rows')[2].querySelectorAll('.row').length === zf.length,
    'Zebedee & Flash bank holds all ' + zf.length + ' of its sounds: ' + zp.map(p => p.querySelector('.pn').textContent).join(', '));
  before = Object.assign({}, stats.starts);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: '4', bubbles: true })); await sleep(150);
  d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key: 'a', bubbles: true })); await sleep(150);
  dlt = delta(before, stats.starts);
  ok(played(dlt, 'mc42') && played(dlt, 'mc_fx_flash'), 'keys 4 and A play the Zebedee chop and the camera flash: ' + JSON.stringify(dlt));
  // name and drama stabs: three banks, two groups, every stab keyed to its A-minor hit
  const nameStabs = hw.filter(i => i.hsub === 'names'), drama = hw.filter(i => i.hsub === 'drama');
  ok(nameStabs.length === 32 && drama.length === 19 && drama.filter(i => i.explicit).length === 2, 'stabs: ' + nameStabs.length + ' names, ' + drama.length + ' drama (' + drama.filter(i => i.explicit).length + ' explicit)');
  ok([...nameStabs, ...drama].every(i => i.style === 'stab' && i.key === 'A minor' && i.camelot === '8A' && /_Amin-8A_128bpm\.wav$/.test(i.pack) && /_128bpm_DRY\.wav$/.test(i.packDry) && !/Amin/.test(i.packDry)),
    'every stab carries the hit\'s key (A minor, 8A) in its name; the dry voice does not');
  ok(nameStabs.every(i => /^Halloween\/Murder at Cameron Castle\/Name Stabs\//.test(i.pack)) && drama.every(i => /^Halloween\/Murder at Cameron Castle\/Drama Stabs\//.test(i.pack)), 'stabs pack into Name Stabs and Drama Stabs');
  const gi = l => groups.findIndex(g => g.includes(l));
  ok(gi('Name stabs') === 3 && gi('Drama stabs') === 4 && $$('#hwRows .rows')[3].querySelectorAll('.row').length === 32 && $$('#hwRows .rows')[4].querySelectorAll('.row').length === 19, 'Name stabs and Drama stabs follow Zebedee & Flash');
  const seen = new Set();
  for (const [bank, key, want] of [['names1', '1', 'mc47'], ['names2', 'v', 'mc78'], ['drama', '1', 'mc97']]) {
    click($('[data-hwbank="' + bank + '"]'), bank); await sleep(40);
    const bp = $$('#hwPads .pad');
    bp.forEach(p => seen.add(p.dataset.pad));
    ok(bp.length === 16 && bp.every(p => BYID(p.dataset.pad).style === 'stab' && !BYID(p.dataset.pad).explicit && p.querySelector('.pg').textContent === 'Stab') && $('[data-hwbank="' + bank + '"]').getAttribute('aria-pressed') === 'true',
      bank + ' bank: ' + bp.map(p => p.querySelector('.pn').textContent).join(', '));
    before = Object.assign({}, stats.starts);
    d.body.dispatchEvent(new w.KeyboardEvent('keydown', { key, bubbles: true })); await sleep(250);
    dlt = delta(before, stats.starts);
    ok(played(dlt, want), 'key ' + key.toUpperCase() + ' on ' + bank + ' plays ' + BYID(want).title + ': ' + JSON.stringify(dlt));
  }
  ok(nameStabs.every(i => seen.has(i.id)), 'the two name banks hold all 32 names');
  ok($$('#hwPads .pad .pn').every(x => x.textContent.replace(/\u00AD/g, '').length <= 22), 'pad labels stay short: ' + $$('#hwPads .pad .pn').map(x => x.textContent.length).join(','));
  click($('[data-hwbank="haunted"]'), 'haunted bank'); await sleep(40);
  ok($('#hwPads .pad').dataset.pad === 'hw_fx_thunder', 'back to the haunted bank');

  // a loop row loops; a sung loop syncs to the kit tempo when the kit plays
  before = Object.assign({}, stats.starts); const l0 = stats.loops;
  click($('[data-play="hw_fx_theremin"]'), 'theremin'); await sleep(150);
  ok(stats.loops > l0, 'theremin loop plays looped');
  click($('[data-play="hw_fx_theremin"]'), 'stop theremin'); await sleep(40);

  // explicit toggle
  const nAll = $$('#hwRows .row').length;
  const he = $('#hideE2'); he.checked = true; he.dispatchEvent(new w.Event('change', { bubbles: true })); await sleep(40);
  const nClean = $$('#hwRows .row').length;
  ok(nClean === nAll - hw.filter(i => i.explicit).length && !$$('#hwRows .row').some(r => r.querySelector('.e')), 'Hide explicit removes the ' + (nAll - nClean) + ' explicit rows');
  he.checked = false; he.dispatchEvent(new w.Event('change', { bubbles: true })); await sleep(40);

  // the pack
  const s = await saveAndWait($('#saveHw'));
  const z = unzip(s.bytes), names = Object.keys(z);
  console.log('  ' + s.filename + ' ' + (s.bytes.length / 1e6).toFixed(1) + ' MB, ' + names.length + ' files');
  ok(hw.every(i => names.includes(i.pack)) && hwVox.every(i => names.some(n => n.endsWith(i.packDry.split('/').pop()))), 'Halloween pack holds every file and every dry take');
  for (const [id, which] of [['mc47', 'vox'], ['mc62', 'vox'], ['mc78', 'vox_dry'], ['mc96', 'vox'], ['mc97', 'vox_dry']]) {
    const it = BYID(id), got = z[which === 'vox' ? it.pack : it.packDry], ref = fs.readFileSync(SRC + '/' + which + '/' + id + '.wav');
    ok(got && Buffer.compare(wavData(got), wavData(ref)) === 0, id + (which === 'vox' ? ' wet' : ' dry') + ' saves bit-exact from its time sprite (' + (got ? got.length : 0) + ' bytes)');
  }
  const thunder = z[hwFx.find(i => i.id === 'hw_fx_thunder').pack];
  ok(thunder && thunder.toString('ascii', 0, 4) === 'RIFF' && thunder.readUInt16LE(22) === 2 && thunder.readUInt32LE(24) === 44100, 'thunder saves as a 44.1 kHz stereo WAV (' + (thunder ? thunder.length : 0) + ' bytes)');
  const readme = z[names.find(n => n.endsWith('README.txt'))].toString();
  ok(/HALLOWEEN \(a seasonal set/.test(readme) && /HV23 /.test(readme) && /Macbeth/.test(readme) && /HW_FX_Thunderclap\.wav/.test(readme), 'README has the Halloween section and lists the files');
  ok(/Name Stabs: every character announced/.test(readme) && /MC47 /.test(readme) && /MC97 /.test(readme), 'README describes the stabs and lists them');

  // genre button leaves Halloween
  click($('#genres [data-genre="mth"]'), 'genre mth'); await sleep(80);
  ok(d.documentElement.dataset.genre === 'mth' && $('#view-halloween').hidden && !$('#view-kits').hidden && $('#gName').textContent !== 'Halloween', 'a genre button leaves Halloween for the kits in that skin');
  ok($('#hwBtnTxt').textContent === 'Halloween sounds' && !$('#shuffleBtn').hidden && $('#hwSave').hidden, 'hero buttons restored');

  // tabs: in and out
  click(tab, 'tab'); await sleep(60);
  ok(d.documentElement.dataset.genre === 'hw', 'the tab opens Halloween too');
  click($('.mode[data-view="vocals"]'), 'vocals'); await sleep(60);
  ok(d.documentElement.dataset.genre === 'mth', 'another tab restores the genre skin');
  const chip = $('[data-voxf="halloween"]');
  ok(chip && /Halloween/.test(chip.textContent), 'Vocals has a Halloween filter: ' + (chip && chip.textContent));
  click(chip, 'chip'); await sleep(40);
  ok($$('#voxRows .row').length === hwVox.length && $$('#voxRows .groupname').length === 1, 'the filter lists the ' + hwVox.length + ' Halloween vocals');
  click($('[data-voxf="all"]'), 'all'); await sleep(20);
  click($('.mode[data-view="fx"]'), 'fx'); await sleep(60);
  const fchip = $('[data-fxf="halloween"]');
  click(fchip, 'fx chip'); await sleep(40);
  ok($$('#fxRows .row').length === hwFx.length, 'Loops & FX has a Halloween filter with the ' + hwFx.length + ' FX');
  click($('[data-fxf="all"]'), 'all'); await sleep(20);

  // FLAC decode of a few new files through the page decoder
  const dec = [];
  for (const id of ['hw_fx_bell', 'hw_fx_choir', 'hv23', 'hv27', 'mc_fx_gong', 'mc17', 'mc38', 'mc42', 'mc46', 'mc_fx_flash', 'mc47', 'mc78', 'mc95', 'mc97']) {
    const it = ITEMS.find(i => i.id === id), f = it.file.match(/^flac:([^?]+)\?ch=([\d,]+)&n=(\d+)(?:&o=(\d+))?$/), o = +(f[4] || 0);
    const b = fs.readFileSync(path.join(SITE, f[1]));
    try { const r = require(PACK_ROOT + '/flac.js').flacDecode(new Uint8Array(b)), want = f[2].split(',').map(Number); ok(r.n >= o + +f[3] && r.sr === 44100 && want.every(k => k < r.ch), id + ' decodes: channels ' + f[2] + ' of ' + r.ch + ', ' + r.n + ' frames (want ' + f[3] + ' from ' + o + ')'); }
    catch (e) { ok(false, id + ' decode failed: ' + e.message); }
  }

  // reload straight into Halloween: the stored view brings its skin back
  console.log('MISSING', missing.length ? [...new Set(missing)] : 'none');
  console.log('ERRORS', errors.length ? errors.slice(0, 5) : 'none');
  console.log(failures ? failures + ' FAILURES' : 'ALL PASSED');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('TEST FAILED', e.stack); console.log(errors.slice(0, 5), missing); process.exit(1); });
