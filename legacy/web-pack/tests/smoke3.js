const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// Smoke test for the wave-3 app pass: sprite-packed samples, new groups, search, Fits, crate, loop audition.
const { JSDOM } = require('jsdom');
const fs = require('fs');
const path = require('path');
const SITE = PACK_ROOT + '/site';
const STAGE = PACK_ROOT + '/out/wave3', STAGE4 = PACK_ROOT + '/out/wave4';
const stageOf = it => it.wave === 4 ? STAGE4 : STAGE;
const html = fs.readFileSync(process.env.HTML || path.join(SITE, 'index.html'), 'utf8');
const errors = [], saves = [], fetched = [], missing = [], decoded = [], sources = [];
let failures = 0;
const ok = (cond, msg) => { console.log((cond ? '  ok   ' : '  FAIL ') + msg); if (!cond) failures++; };

function fakeParam() { const p = { value: 0, setValueAtTime(v) { p.value = v; }, linearRampToValueAtTime() {}, exponentialRampToValueAtTime() {}, setTargetAtTime() {}, cancelScheduledValues() {}, cancelAndHoldAtTime() {} }; return p; }
function fakeNode() {
  return new Proxy({}, { get(t, k) {
    if (k in t) return t[k];
    if (['frequency', 'detune', 'gain', 'Q', 'pan', 'delayTime', 'threshold', 'knee', 'ratio', 'attack', 'release', 'playbackRate'].includes(k)) return (t[k] = fakeParam());
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
  decodeAudioData(ab, okf, bad) {
    const b = Buffer.from(ab);
    let res;
    if (b.toString('latin1', 0, 4) !== 'RIFF' || b.toString('latin1', 8, 12) !== 'WAVE') { const e = new Error('not a wav'); if (bad) bad(e); return Promise.reject(e); }
    const ch = b.readUInt16LE(22), len = b.readUInt32LE(40), n = len / (2 * ch);
    decoded.push({ bytes: b.length, ch, n, riff: b.readUInt32LE(4) });
    res = { duration: n / 44100, length: n, numberOfChannels: ch, getChannelData: () => new Float32Array(n) };
    const p = new Promise(r => setTimeout(() => r(res), 0)); if (okf) p.then(okf); return p;
  }
}
['createGain', 'createOscillator', 'createBiquadFilter', 'createDynamicsCompressor', 'createConvolver', 'createDelay', 'createStereoPanner', 'createWaveShaper']
  .forEach(m => { FakeAC.prototype[m] = function () { return fakeNode(); }; });
FakeAC.prototype.createBufferSource = function () { const n = fakeNode(); n.start = (w) => { n._when = w; }; n.stop = () => { n._stopped = true; }; sources.push(n); return n; };

const dom = new JSDOM(html.replace(/<link[^>]+fonts[^>]+>/g, ''), {
  runScripts: 'dangerously', pretendToBeVisual: true, url: 'https://example.test/',
  beforeParse(w) {
    // stale filters from the previous version must fall back to "All"
    w.localStorage.setItem('ukgmin.fxFilter', JSON.stringify('loops'));
    w.localStorage.setItem('ukgmin.bkFilter', JSON.stringify('reeseloops'));
    w.AudioContext = FakeAC; w.TextEncoder = require('util').TextEncoder; w.Blob = require('buffer').Blob;
    w.ResizeObserver = class { observe() {} };
    w.matchMedia = () => ({ matches: false, addEventListener() {} });
    w.scrollTo = () => {};
    w.HTMLCanvasElement.prototype.getContext = () => new Proxy({}, { get: (t, k) => (k in t ? t[k] : () => {}), set: (t, k, v) => { t[k] = v; return true; } });
    w.fetch = async (url) => {
      const p = path.join(SITE, String(url));
      if (!fs.existsSync(p)) { missing.push(String(url)); return { ok: false, status: 404 }; }
      fetched.push(String(url));
      const b = fs.readFileSync(p);
      return { ok: true, status: 200, arrayBuffer: async () => b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength) };
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
async function saveAndWait(sel, ms = 60000) {
  const n = saves.length; click($(sel), sel);
  const t0 = Date.now();
  while (saves.length === n && Date.now() - t0 < ms) await sleep(50);
  await sleep(30);
  if (saves.length === n) throw new Error('no save from ' + sel + ' — toast: ' + ($('.toast') && $('.toast').textContent));
  return saves[saves.length - 1];
}
// stored-only zip reader: name -> bytes
function unzip(buf) {
  const eocd = buf.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  const n = buf.readUInt16LE(eocd + 10), cdOff = buf.readUInt32LE(eocd + 16);
  const out = {}; let p = cdOff;
  for (let i = 0; i < n; i++) {
    const method = buf.readUInt16LE(p + 10), csize = buf.readUInt32LE(p + 20), fl = buf.readUInt16LE(p + 28), el = buf.readUInt16LE(p + 30), cl = buf.readUInt16LE(p + 32), lo = buf.readUInt32LE(p + 42);
    const name = buf.slice(p + 46, p + 46 + fl).toString();
    const lfl = buf.readUInt16LE(lo + 26), lel = buf.readUInt16LE(lo + 28);
    out[name] = { method, data: buf.slice(lo + 30 + lfl + lel, lo + 30 + lfl + lel + csize) };
    p += 46 + fl + el + cl;
  }
  return out;
}
async function playWait(el, what) { const n = sources.length; click(el, what); const t0 = Date.now(); while (sources.length === n && Date.now() - t0 < 4000) await sleep(5); await sleep(20); }
const typeQ = async (v) => { const i = $('.view:not([hidden]) [data-q]'); i.value = v; i.dispatchEvent(new w.Event('input', { bubbles: true })); await sleep(200); };
const MAN = JSON.parse(html.match(/"items":\[[^]*?\],"bytes":\d+\}/) ? '{' + html.match(/"items":\[[^]*?\],"bytes":\d+\}/)[0] : '{}');

(async () => {
  await sleep(500);
  const items = MAN.items || [];
  const w3 = items.filter(i => i.wave >= 3 && i.wave < 6);   // sprite waves (wave 6 vocals are FLAC: smoke6)
  console.log('manifest items ' + items.length + ', wave 3+4: ' + w3.length);
  ok(errors.length === 0, 'boots without errors');
  ok($$('.mode').length === 8 && $('.mode[data-view="lab"]') && $$('.mode').map(b => b.textContent).join('|').includes('Loops & FX') && $('.mode[data-view="crate"]') && $('.mode[data-view="halloween"]'), 'eight tabs incl. Loops & FX, Sound lab, Crate and Halloween: ' + $$('.mode').map(b => b.textContent.trim()).join(' / '));
  console.log('  header counts: ' + ['cKits', 'cBK', 'cFx', 'cHits', 'cVox'].map(id => id + '=' + $('#' + id).textContent).join(' ') + ' | ' + $('#packHint').textContent);

  // kit detail cross-links
  click($('.mode[data-view="kits"]')); await sleep(30);
  click($('[data-kit="UKG07"]'), 'UKG07'); await sleep(30);
  console.log('  fitsrow: ' + $('#fitsrow').textContent);
  ok($$('#fitsrow [data-gofits]').length === 3, 'kit detail shows three "fits this kit" links');

  // Loops & FX
  click($('.mode[data-view="fx"]')); await sleep(40);
  console.log('  fx chips: ' + $$('[data-fxf]').map(b => b.textContent).join(' · '));
  ok($('[data-fxf="all"]').getAttribute('aria-pressed') === 'true', 'stale fx filter fell back to All');
  const fxRows = $$('#fxRows .row').length, fxN = items.filter(i => ['fills', 'fx', 'loops'].includes(i.cat)).length;
  ok(fxRows === fxN, 'Loops & FX lists every fills/fx/loops item (' + fxRows + '/' + fxN + ')');
  console.log('  fx groups: ' + $$('#fxRows .groupname').map(x => x.textContent + '(' + x.nextElementSibling.querySelectorAll('.row').length + ')').join(' / '));
  for (const f of ['new', 'drums', 'tops', 'breaks', 'fills', 'fx']) { click($('[data-fxf="' + f + '"]')); await sleep(10); console.log('    ' + f + ': ' + $$('#fxRows .row').length + ' rows, groups ' + $$('#fxRows .groupname').map(x => x.textContent).join(' / ')); }
  click($('[data-fxf="all"]')); await sleep(10);
  ok($$('#fxRows .newtag').length === items.filter(i => i.wave >= 3 && ['fills', 'fx', 'loops'].includes(i.cat)).length, 'NEW tags on wave-3+ loops & FX (' + $$('#fxRows .newtag').length + ')');

  // play a drum loop alone (loops), then over the kit (tempo-synced)
  sources.length = 0; decoded.length = 0;
  await playWait($('[data-play="dl3_ukg07"]'), 'dl3_ukg07');
  let src = sources[sources.length - 1];
  ok(src && src.loop === true && Math.abs(src.playbackRate.value - 1) < 1e-9, 'drum loop audition repeats (loop=' + (src && src.loop) + ', rate=' + (src && src.playbackRate.value) + ')');
  ok(decoded.length === 1 && decoded[0].bytes === 44 + decoded[0].n * 2 * decoded[0].ch && decoded[0].riff === decoded[0].bytes - 8, 'sprite slice decodes as a valid standalone WAV (' + JSON.stringify(decoded[0]) + ')');
  ok(fetched.some(f => f === 'audio/s3/w3_ukg_loops.ogg') && !fetched.some(f => /\.wav$/.test(f) && /\/s[34]\//.test(f)), 'fetched the UKG loops FLAC sprite');
  console.log('  LCD: ' + $('#lcd1').textContent + ' / ' + $('#lcd2').textContent);
  click($('[data-play="dl3_ukg07"]')); await sleep(30);
  ok(!$('[data-play="dl3_ukg07"]').classList.contains('on'), 'second tap stops the loop');

  // kit at 130 BPM, then a 134 loop -> rate 130/134, starts on a bar line; then BPM up -> rate follows
  click($('.mode[data-view="kits"]')); await sleep(20);
  click($('[data-kit="UKG01"]'), 'UKG01'); await sleep(20);
  click($('#playBtn')); await sleep(300);
  FakeAC.inst.currentTime += 1.234; await sleep(50);
  const kitBpm = +($('#lcd1').textContent.match(/(\d{3})/) || [0, 0])[1] || null;
  click($('.mode[data-view="fx"]')); await sleep(20);
  sources.length = 0;
  await playWait($('[data-play="dl3_mth07"]'), 'dl3_mth07');
  src = sources[sources.length - 1];
  const it = items.find(i => i.id === 'dl3_mth07');
  console.log('  kit LCD: ' + $('#lcd1').textContent + ' | loop ' + it.bpm + ' BPM, rate ' + (src && src.playbackRate.value.toFixed(4)) + ', starts at ' + (src && src._when.toFixed(3)));
  ok(src && src.loop && Math.abs(src.playbackRate.value - 134 / it.bpm) < 1e-6, 'unpitched loop follows the kit tempo (UKG01 134 BPM)');
  click($('.mode[data-view="kits"]')); await sleep(20);
  click($('[data-bpm="-1"]') || $('[data-bpm]'), 'bpm-'); await sleep(30);
  ok(Math.abs(src.playbackRate.value - 133 / it.bpm) < 1e-6, 'tempo change re-times the synced loop (rate ' + src.playbackRate.value.toFixed(4) + ')');
  click($('[data-bpm="1"]') || $('[data-bpm]'), 'bpm+'); await sleep(30);
  // pitched loop keeps its own rate
  click($('.mode[data-view="bass"]')); await sleep(30);
  sources.length = 0;
  await playWait($('[data-play="bl3_ukg07"]'), 'bl3_ukg07');
  src = sources[sources.length - 1];
  ok(src && src.loop && src.playbackRate.value === 1, 'keyed bass loop loops at its own pitch');
  click($('[data-play="bl3_ukg07"]')); await sleep(20);
  click($('#playBtn')); await sleep(30);

  // Bass & Keys groups, sets
  console.log('  bk chips: ' + $$('[data-bkf]').map(b => b.textContent).join(' · '));
  ok($('[data-bkf="all"]').getAttribute('aria-pressed') === 'true', 'stale bass filter fell back to All');
  console.log('  bk groups: ' + $$('#bkRows .groupname').map(x => x.textContent).join(' / '));
  console.log('  sets: ' + $$('.setrow .rt .t').map(x => x.textContent + ' (' + x.closest('.setrow').querySelectorAll('.nbtn').length + ')').join(' / '));
  const bkN = items.filter(i => (i.cat === 'bass' || i.cat === 'keys') && !i.hidden);
  ok($$('#bkRows .row').length + $$('#bkRows .nbtn').length === bkN.length, 'Bass & Keys shows every item (' + ($$('#bkRows .row').length + $$('#bkRows .nbtn').length) + '/' + bkN.length + ')');
  for (const f of ['new', 'reese', 'synth', 'organ', 'piano', 'keys', 'strings']) { click($('[data-bkf="' + f + '"]')); await sleep(10); console.log('    ' + f + ': ' + $$('#bkRows .row').length + ' rows, ' + $$('#bkRows .nbtn').length + ' notes; ' + $$('#bkRows .groupname').map(x => x.textContent).join(' / ')); }
  click($('[data-bkf="all"]')); await sleep(10);
  // set save from a sprite
  let s = await saveAndWait('[data-saveset="sub_2step"]');
  let z = unzip(s.bytes);
  ok(Object.keys(z).length === 12, 'sub set zip has 12 notes: ' + s.filename);

  // byte-identical extraction vs the staged files
  let same = 0, diff = [];
  for (const [name, f] of Object.entries(z)) {
    const it = items.find(i => i.set === 'sub_2step' && i.pack.endsWith('/' + name.split('/').pop()));
    const ref = fs.readFileSync(path.join(STAGE, it.pack));
    if (Buffer.compare(ref, f.data) === 0) same++; else diff.push(name);
  }
  ok(same === 12, 'sub set WAVs are byte-identical to the rendered files (' + same + '/12)' + (diff.length ? ' diff: ' + diff : ''));

  // search
  click($('.mode[data-view="bass"]')); await sleep(10);
  await typeQ('acid');
  console.log('  search "acid" in Bass & Keys: ' + $$('#bkRows .row .t').map(x => x.textContent).join('; '));
  ok($$('#bkRows .row').length >= 3, 'search finds the acid lines');
  await typeQ('rhodes cm9');
  ok($$('#bkRows .row').length === 1, 'multi-word search narrows to one (' + $$('#bkRows .row .t').map(x => x.textContent).join('; ') + ')');
  await typeQ('zzzz');
  ok(!!$('#bkRows .empty'), 'no matches shows the empty hint: ' + ($('#bkRows .empty') && $('#bkRows .empty').textContent));
  click($('.mode[data-view="fx"]')); await sleep(30);
  ok($('.view:not([hidden]) [data-q]').value === 'zzzz', 'query carries across views');
  await typeQ('');

  // fits
  click($('.mode[data-view="kits"]')); await sleep(20);
  click($('[data-kit="MTH07"]'), 'MTH07'); await sleep(20);
  const fr = $('#fitsrow').textContent;
  click($('#fitsrow [data-gofits="fx"]')); await sleep(40);
  const fitsBtn = $('.view:not([hidden]) [data-fits]');
  console.log('  ' + fr + ' -> view ' + $$('.view').find(v => !v.hidden).id + ', chip "' + fitsBtn.textContent + '" pressed=' + fitsBtn.getAttribute('aria-pressed'));
  const fitRows = $$('#fxRows .row').map(r => items.find(i => i.id === r.dataset.row));
  ok(fitsBtn.getAttribute('aria-pressed') === 'true' && fitRows.length > 0 && fitRows.length < fxN, 'cross-link opens Loops & FX with Fits on (' + fitRows.length + ' rows)');
  const bad = fitRows.filter(i => i.bars && i.bpm && Math.abs(i.bpm - 128) > (i.key ? 4 : 8));
  ok(bad.length === 0, 'every shown loop is within tempo range of MTH07' + (bad.length ? ': ' + bad.map(i => i.id + '@' + i.bpm) : ''));
  console.log('    keys shown: ' + [...new Set(fitRows.filter(i => i.camelot).map(i => i.camelot))].join(' '));
  click(fitsBtn); await sleep(30);
  ok($$('#fxRows .row').length === fxN, 'Fits off restores the full list');
  click($('.view:not([hidden]) [data-fits]')); await sleep(10);
  click($('.mode[data-view="vocals"]')); await sleep(30);
  console.log('  vocals with Fits on: ' + $$('#voxRows .row').length + ' rows; keys ' + [...new Set($$('#voxRows .row').map(r => items.find(i => i.id === r.dataset.row).camelot).filter(Boolean))].join(' '));
  click($('.view:not([hidden]) [data-fits]')); await sleep(10);

  // crate
  // the rows for org3_dm9 are in another view; star it from Bass & Keys
  click($('.mode[data-view="bass"]')); await sleep(30);
  click($('[data-star="org3_dm9"]'), 'org3_dm9 star'); await sleep(10);
  click($('.mode[data-view="fx"]')); await sleep(30);
  click($('[data-star="dl3_ukg07"]'), 'dl3_ukg07 star'); click($('[data-star="brk3_slice"]'), 'brk3 star'); await sleep(10);
  click($('.mode[data-view="vocals"]')); await sleep(30);
  const v1 = $$('#voxRows .row')[0].dataset.row; click($('[data-star="' + v1 + '"]'), 'vox star'); await sleep(10);
  ok($('#tCrate').textContent === '4', 'crate count shows 4 (' + $('#tCrate').textContent + ')');
  ok(JSON.parse(w.localStorage.getItem('ukgmin.crate')).length === 4, 'crate is remembered');
  click($('.mode[data-view="crate"]')); await sleep(30);
  console.log('  crate: ' + $$('#crateRows .groupname').map(x => x.textContent).join(' / ') + ' | ' + $$('#crateRows .row .t').map(x => x.textContent).join('; '));
  ok($$('#crateRows .row').length === 4, 'crate view lists the starred rows');
  s = await saveAndWait('#saveCrate');
  z = unzip(s.bytes);
  console.log('  ' + s.filename + ': ' + Object.keys(z).join(', '));
  const readme = z['README.txt'] ? z['README.txt'].data.toString() : '';
  ok(Object.keys(z).filter(n => n.endsWith('.wav')).length >= 4 && readme.includes('LOOPS AND ONE-SHOTS'), 'crate zip has the WAVs and the updated README');
  const brkName = Object.keys(z).find(n => n.includes('Break-Slice') || n.includes('BreakSlice') || n.toLowerCase().includes('slice'));
  if (brkName) { const it = items.find(i => i.id === 'brk3_slice'); ok(Buffer.compare(fs.readFileSync(path.join(STAGE, it.pack)), z[brkName].data) === 0, 'break slice WAV byte-identical to the rendered file'); }
  click($('[data-star="' + v1 + '"]'), 'unstar in crate'); await sleep(20);
  ok($$('#crateRows .row').length === 3 && $('#tCrate').textContent === '3', 'unstarring in the crate removes the row');
  click($('#clearCrate')); await sleep(20);
  ok($$('#crateRows .row').length === 0 && $('#tCrate').textContent === '', 'Clear empties the crate');

  // all wave-3 rows reachable + saved as single files byte-identically (sample a few from each sprite)
  const bySprite = {};
  w3.filter(i => i.cat !== 'hits').forEach(i => { const sp = i.file.split('?')[0]; (bySprite[sp] = bySprite[sp] || []).push(i); });
  let checked = 0, good = 0;
  for (const [sp, list] of Object.entries(bySprite)) {
    for (const it of [list[0], list[list.length - 1]]) {
      const view = it.cat === 'vox' ? 'vocals' : (it.cat === 'bass' || it.cat === 'keys') ? 'bass' : 'fx';
      click($('.mode[data-view="' + view + '"]')); await sleep(20);
      let sel = '[data-saveitem="' + it.id + '"]';
      if (!$(sel)) { sel = '[data-saveset="' + it.set + '"]'; }
      s = await saveAndWait(sel);
      z = unzip(s.bytes);
      const f = Object.entries(z).find(([n]) => n.endsWith(it.pack.split('/').pop()));
      checked++;
      if (f && Buffer.compare(fs.readFileSync(path.join(stageOf(it), it.pack)), f[1].data) === 0) good++; else console.log('    mismatch ' + it.id + ' in ' + s.filename);
    }
  }
  ok(good === checked, 'first and last item of every sprite save byte-identical (' + good + '/' + checked + ')');

  // wave 4: garage house (Disclosure-style)
  click($('.mode[data-view="hits"]')); await sleep(30);
  const ghPads = $$('.pad').filter(p => /_gh$/.test(p.dataset.pad));
  ok(ghPads.length === 9 && ghPads.every(p => p.querySelector('.pg').textContent === 'GH kit'), 'nine GH kit pads (' + ghPads.length + ')');
  click($('.mode[data-view="fx"]')); await sleep(30);
  const ghGroup = $$('#fxRows .groupname').find(g => g.textContent.includes('garage house'));
  ok(ghGroup && ghGroup.nextElementSibling.querySelectorAll('.row').length === 4, 'Loops & FX shows the four garage-house grooves');
  click($('.mode[data-view="bass"]')); await sleep(30);
  const ghSets = $$('.setrow').filter(r => /gh_/.test(r.dataset.setrow));
  ok(ghSets.length === 2 && ghSets.map(r => r.querySelectorAll('.nbtn').length).join(',') === '13,17', 'glide bass and chord pluck sets with note buttons (' + ghSets.map(r => r.dataset.setrow + ':' + r.querySelectorAll('.nbtn').length).join(' ') + ')');
  sources.length = 0;
  await playWait($$('[data-setrow="gh_pluck"] .nbtn')[5], 'pluck note');
  ok(sources.length > 0, 'a chord pluck note plays');
  s = await saveAndWait('[data-saveset="gh_pluck"]');
  z = unzip(s.bytes);
  let ghSame = 0;
  for (const [name, f] of Object.entries(z)) { const it = items.find(i => i.set === 'gh_pluck' && i.pack.endsWith('/' + name.split('/').pop())); if (it && Buffer.compare(fs.readFileSync(path.join(STAGE4, it.pack)), f.data) === 0) ghSame++; }
  ok(ghSame === 17, 'chord pluck set zip byte-identical (' + ghSame + '/17)');

  // header genre pack includes the new samples
  s = await saveAndWait('[data-gpack="sg"]', 200000);
  z = unzip(s.bytes);
  const sgNew = w3.filter(i => i.genre === 'sg' || i.genre === 'all');
  const names = Object.keys(z);
  ok(sgNew.every(i => names.some(n => n.endsWith(i.pack))), 'speed garage pack contains all ' + sgNew.length + ' new SG + shared samples (' + (s.bytes.length / 1e6).toFixed(1) + ' MB, ' + names.length + ' files)');

  console.log('\nMISSING FETCHES', missing.length ? missing : 'none');
  console.log('ERRORS', errors.length ? errors : 'none');
  console.log(failures ? failures + ' FAILURES' : 'ALL PASSED');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.log('TEST FAILED', e.stack); console.log('toast:', $('.toast') && $('.toast').textContent); console.log(errors, missing); process.exit(1); });
