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

(async () => {
  await sleep(500);
  ok(errors.length === 0, 'boots without errors');
  click($('.mode[data-view="kits"]')); await sleep(30);
  const chip = $('[data-genre="gh"]');
  ok(chip && /Garage house/.test(chip.textContent) && /4$/.test(chip.textContent.trim()), 'kit filter chip for garage house with 4 kits: ' + (chip && chip.textContent));
  console.log('  genres: ' + $$('#genres [data-genre]').map(b => b.textContent).join(' · '));
  const groups = $$('.kitgroup').map(g => g.textContent);
  ok(groups.join('|') === 'UK Garage|Garage House (Disclosure-style)|Minimal Tech House|Speed Garage|Classic House|Groove House (Chris Stussy-style)|Tech House (Chris Lake & Riordan-style)', 'kit groups in order: ' + groups.join(' / '));
  const ghCards = $$('.kitcard').filter(c => /^GH0/.test(c.dataset.kit));
  ok(ghCards.length === 4 && ghCards.every(c => c.querySelector('.newtag') && c.querySelector('.tag.t-gh')), 'four GH cards with the gold tag and a New badge');
  click(chip); await sleep(30);
  ok($$('.kitcard').length === 4, 'garage house filter shows just the four kits');
  ok(w.document.documentElement.dataset.genre === 'gh', 'garage house skin is on (' + w.document.documentElement.dataset.genre + ')');
  click($('#genres [data-genre="all"]')); await sleep(30);

  // GH01 detail + loop playback
  click($('[data-kit="GH01"]'), 'GH01'); await sleep(40);
  ok($('#kitdetail .tag').textContent === 'Garage House' && $('#kitdetail .kd-tags .style').textContent === 'Garage house', 'detail tag: ' + $('#kitdetail .kd-tags').textContent);
  console.log('  LCD: ' + $('#lcd1').textContent + ' / ' + $('#lcd2').textContent);
  console.log('  prog: ' + $('#prog').textContent);
  console.log('  drum lane: ' + $('.lane[data-lane="drums"] .rollnote').textContent);
  ok(/Snap/.test($('.lane[data-lane="drums"] .rollnote').textContent), 'drum lane lists the snap');
  let s0 = JSON.parse(JSON.stringify(stats));
  click($('#playBtn')); const started = await playThrough(16);
  let dl = delta(s0.starts, stats.starts);
  console.log('  GH01 loop 16 s: osc ' + (stats.osc - s0.osc) + ', looped sources ' + (stats.loops - s0.loops) + ', starts ' + JSON.stringify(dl).slice(0, 400));
  const has = re => Object.keys(dl).some(k => re.test(k));
  ok(started && has(/kick_gh/) && has(/clap_gh/) && has(/snap_gh/) && has(/chat_gh/), 'loop plays the GH kit (kick, clap, snap, hats)');
  ok(has(/gbass4_/) && stats.loops - s0.loops > 0, 'glide bass plays from its samples and loops (' + (stats.loops - s0.loops) + ' looped notes)');
  ok(has(/pluck4_/), 'chords play on the chord-pluck samples');
  ok(stats.osc - s0.osc > 0, 'square lead runs on oscillators');
  click($('#playBtn')); await sleep(30);

  // full song + extended mix for every GH kit
  for (const id of ['GH01', 'GH02', 'GH03', 'GH04']) {
    click($('[data-kit="' + id + '"]'), id); await sleep(30);
    for (const v of ['full', 'ext']) {
      click($('[data-songv="' + v + '"]')); await sleep(30);
      const meta = $('#songMeta').textContent, secs = +meta.match(/(\d+):(\d+)/).slice(1).reduce((m, x, i) => i ? m * 60 + +x : +x, 0);
      s0 = JSON.parse(JSON.stringify(stats)); const e0 = errors.length;
      click($('#songPlay'), 'songPlay');
      const ran = await playThrough(secs + 1);
      dl = delta(s0.starts, stats.starts);
      const vox = Object.entries(dl).filter(([k]) => /vox\//i.test(k)).map(([k, n]) => k.split('/').pop().split('_').slice(0, 2).join('_') + '×' + n).join(' ');
      const hits = Object.entries(dl).filter(([k]) => /_gh\b|_gh\|/.test(k)).reduce((a, [, n]) => a + n, 0);
      const bassN = Object.entries(dl).filter(([k]) => /gbass4_/.test(k)).reduce((a, [, n]) => a + n, 0);
      const pluckN = Object.entries(dl).filter(([k]) => /pluck4_/.test(k)).reduce((a, [, n]) => a + n, 0);
      const pianoN = Object.entries(dl).filter(([k]) => /piano/.test(k)).reduce((a, [, n]) => a + n, 0);
      console.log('  ' + id + ' ' + v + ' (' + meta + '): GH hits ' + hits + ', glide bass ' + bassN + ', pluck ' + pluckN + ', piano ' + pianoN + ', osc ' + (stats.osc - s0.osc) + ', bends ' + (stats.detune - s0.detune) + ', vocals ' + (vox || '-'));
      ok(ran && hits > 500 && errors.length === e0, id + ' ' + v + ' plays to the end on the GH kit without errors');
      if (v === 'ext') {
        const expect = { GH01: ['h06', 'h14'], GH02: ['h02'], GH03: ['h16', 'bh5'], GH04: ['h03', 'bh3'] }[id];
        ok(expect.every(h => Object.keys(dl).some(k => k.toLowerCase().includes('_' + h + '_') || k.toLowerCase().includes('/' + h + '.') || k.toLowerCase().includes(h + '_'))), id + ' extended mix plays its pack vocals ' + expect.join('+') + ': ' + vox);
        ok(pluckN > 0, id + ' extended mix stabs on the chord pluck');
      }
      click($('#songPlay'), 'stop'); await sleep(20);
    }
  }
  console.log('  hint: ' + $('#songHint').textContent.slice(0, 200));

  // kit zip with vocals
  click($('[data-kit="GH03"]')); await sleep(30);
  let s = await saveAndWait($('#saveKit'));
  let z = unzip(s.bytes), names = Object.keys(z);
  console.log('  ' + s.filename + ': ' + names.length + ' files; ' + names.filter(n => /Vocals/.test(n)).map(n => n.split('/').pop()).join(', '));
  ok(names.filter(n => n.endsWith('.mid')).length === 12 && names.some(n => /Vocals used in the extended mix\/.*H16/i.test(n)) && names.some(n => /BH5/i.test(n)), 'GH03 kit zip: 12 MIDI files plus both hook WAVs');
  const ext = z[names.find(n => /ExtendedMix/.test(n))];
  ok(ext && ext.includes(Buffer.from('Snap')) === false && ext.includes(Buffer.from([0x99, 40])) || (ext && ext.indexOf(Buffer.from('Vocal Guide')) > 0), 'extended mix MIDI written (' + (ext ? ext.length : 0) + ' bytes)');

  // garage house pack
  s = await saveAndWait($('[data-gpack="gh"]'), 300000);
  z = unzip(s.bytes); names = Object.keys(z);
  const kitDirs = [...new Set(names.filter(n => /MIDI Kits\//.test(n)).map(n => n.split('/')[3]))];
  const ghItems = ITEMS.filter(i => /Garage House \(Disclosure-style\)/.test(i.pack || ''));
  const voxIn = names.filter(n => /\/Vocals\//.test(n) || /VOX/.test(n)).length;
  console.log('  ' + s.filename + ' ' + (s.bytes.length / 1e6).toFixed(1) + ' MB, ' + names.length + ' files; kit folders: ' + kitDirs.join(' | '));
  ok(s.filename === 'Garage-House_Pack.zip' && kitDirs.length === 4 && kitDirs.every(k => /^GH0/.test(k)), 'garage house pack holds the four GH kits under MIDI Kits/Garage House');
  ok(ghItems.every(i => names.some(n => n.endsWith(i.pack))), 'garage house pack holds all ' + ghItems.length + ' GH sounds');
  ok(['h06', 'h14', 'h02', 'h16', 'bh5', 'h03', 'bh3'].every(h => names.some(n => n.toLowerCase().includes('_' + h + '_'))), 'garage house pack holds the seven hooks the songs use (' + voxIn + ' vocal files)');
  const readme = z[names.find(n => n.endsWith('README.txt'))].toString();
  ok(/-- Garage House \(Disclosure-style\)\r\nGH01/.test(readme) && /Garage house \(136 bars\)/.test(readme) && /GH04  Chapel Steps/.test(readme) && /Snap 40/.test(readme), 'README lists the GH kits, the garage-house song form and the snap note');

  console.log('MISSING', missing.length ? [...new Set(missing)] : 'none');
  console.log('ERRORS', errors.length ? errors.slice(0, 5) : 'none');
  console.log(failures ? failures + ' FAILURES' : 'ALL PASSED');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('TEST FAILED', e.stack); console.log(errors.slice(0, 5), missing); process.exit(1); });
