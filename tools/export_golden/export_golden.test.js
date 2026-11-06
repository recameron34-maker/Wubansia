'use strict';
// Tests for tools/export_golden.js (the legacy MIDI baseline exporter) and for cmake/WubansiaGolden.cmake:
// its checker (cmake -P) and the golden_check target and test it defines when a CMakeLists.txt includes it.
//
// Run from the repo root with the Node version in .nvmrc, like the exporter itself:
//   node --test tools/export_golden/export_golden.test.js
// The CMake tests need cmake and ctest ($WUBANSIA_CMAKE and $WUBANSIA_CTEST, or on PATH); the project
// tests use Ninja when it is on PATH. ctest runs this file as the test golden.export_golden when CMake finds the pinned Node.
//
// These tests check the tools and the claims in their headers, not the baseline: they never read
// content/golden/legacy-v21.sha256, and they write only to temporary folders. The golden_check target
// compares the real export against the committed manifest.

const { describe, it, before, after } = require('node:test');
const assert = require('node:assert/strict');
const { spawn, spawnSync } = require('node:child_process');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const ROOT = path.resolve(__dirname, '..', '..');
const EXPORTER = path.join(ROOT, 'tools', 'export_golden.js');
const MODULE = path.join(ROOT, 'cmake', 'WubansiaGolden.cmake');
const CORE_JS = path.join(ROOT, 'legacy', 'web-pack', 'core.js');
const NVMRC = path.join(ROOT, '.nvmrc');
const CMAKE = process.env.WUBANSIA_CMAKE || 'cmake';
const CTEST = process.env.WUBANSIA_CTEST ||
  (path.isAbsolute(CMAKE) ? path.join(path.dirname(CMAKE), 'ctest' + (process.platform === 'win32' ? '.exe' : '')) : 'ctest');
const MANIFEST = 'legacy-v21.sha256';

// The spec: the 12 files the web app's "Save all kits" writes for each kit (core.js kitMidiFiles), in
// its order, under a name made of the kit ID and the part or form.
const SLOTS = ['Drums-Main', 'Drums-Variation', 'Drums-Fill', 'Bass-A', 'Bass-B', 'Chords-A', 'Chords-B',
  'Melody-A', 'Melody-B', 'FullArrangement', 'FullSong', 'ExtendedMix'];
const MULTI_TRACK = new Set(['FullArrangement', 'FullSong', 'ExtendedMix']); // SMF type 1; the rest are type 0

const P = require(CORE_JS);
const sha256 = data => crypto.createHash('sha256').update(data).digest('hex');
const byteOrder = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
const relPath = (kitId, slot) => kitId + '/' + kitId + '_' + slot + '.mid';
const EXPECTED = P.KITS.flatMap(k => SLOTS.map(s => relPath(k.id, s))).sort(byteOrder);

const tmpDirs = [];
function tmpDir(tag) {
  const d = fs.mkdtempSync(path.join(os.tmpdir(), 'export-golden-' + tag + '-'));
  tmpDirs.push(d);
  return d;
}
after(() => tmpDirs.forEach(d => fs.rmSync(d, { recursive: true, force: true })));

function run(cmd, args, opts = {}) {
  const r = spawnSync(cmd, args, Object.assign({ encoding: 'utf8', maxBuffer: 64 << 20 }, opts));
  if (r.error) throw r.error;
  return { code: r.status, out: r.stdout + r.stderr };
}
function runAsync(cmd, args) {
  return new Promise((resolve, reject) => {
    const c = spawn(cmd, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    let out = '';
    c.stdout.on('data', d => { out += d; });
    c.stderr.on('data', d => { out += d; });
    c.on('error', reject);
    c.on('close', code => resolve({ code, out }));
  });
}
const exportTo = (out, exporter = EXPORTER) => run(process.execPath, [exporter, '--out', out]);

function listFiles(dir, pred) { // relative paths of the files under dir for which pred(name) holds, byte-sorted
  const found = [];
  (function walk(rel) {
    for (const e of fs.readdirSync(path.join(dir, rel), { withFileTypes: true })) {
      const r = rel ? rel + '/' + e.name : e.name;
      if (e.isDirectory()) walk(r);
      else if (pred(e.name)) found.push(r);
    }
  })('');
  return found.sort(byteOrder);
}
const listMid = dir => listFiles(dir, n => n.endsWith('.mid'));

function parseManifest(text) {
  const header = [], entries = [];
  for (const line of text.split('\n').slice(0, -1)) { // the file ends with a newline
    if (line.startsWith('#')) { header.push(line); continue; }
    const m = /^([0-9a-f]{64}) {2}(\S+)$/.exec(line);
    assert.ok(m, 'manifest line is "<sha256>  <path>": ' + JSON.stringify(line));
    entries.push({ hash: m[1], path: m[2] });
  }
  return { header, entries };
}

// A small SMF reader: format, tracks, division, and every event, so a malformed file throws.
function readSmf(buf) {
  let i = 0;
  const u32 = () => { const v = buf.readUInt32BE(i); i += 4; return v; };
  const u16 = () => { const v = buf.readUInt16BE(i); i += 2; return v; };
  const vlq = () => { let v = 0, b, n = 0; do { b = buf[i++]; v = v * 128 + (b & 0x7f); assert.ok(++n <= 4, 'VLQ too long'); } while (b & 0x80); return v; };
  assert.equal(buf.toString('latin1', 0, 4), 'MThd');
  i = 4;
  assert.equal(u32(), 6);
  const format = u16(), ntrks = u16(), division = u16();
  const tracks = [];
  for (let k = 0; k < ntrks; k++) {
    assert.equal(buf.toString('latin1', i, i + 4), 'MTrk', 'track chunk ' + k);
    i += 4;
    const end = u32() + i;
    const events = [];
    let status = 0, tick = 0, ended = false;
    while (i < end) {
      assert.ok(!ended, 'events after end of track');
      tick += vlq();
      let st = buf[i];
      if (st >= 0x80) i++; else st = status; // running status
      if (st === 0xff) {
        const type = buf[i++], len = vlq();
        events.push({ tick, meta: type, data: buf.subarray(i, i + len) });
        i += len;
        if (type === 0x2f) ended = true;
      } else if (st === 0xf0 || st === 0xf7) {
        i += vlq();
      } else {
        assert.ok(st >= 0x80, 'status byte');
        status = st;
        const n = (st & 0xf0) === 0xc0 || (st & 0xf0) === 0xd0 ? 1 : 2;
        events.push({ tick, status: st, data: buf.subarray(i, i + n) });
        i += n;
      }
    }
    assert.equal(i, end, 'track chunk length');
    assert.ok(ended, 'track ends with an end-of-track event');
    tracks.push(events);
  }
  assert.equal(i, buf.length, 'no bytes after the last track');
  return { format, division, tracks };
}

// The hashes of every file core.js makes right now, in this process, by our path.
function inProcessHashes() {
  const out = new Map();
  for (const kit of P.KITS) P.kitMidiFiles(kit, kit.root, kit.bpm).forEach((f, n) => out.set(relPath(kit.id, SLOTS[n]), sha256(f.data)));
  return out;
}

// A copy of the repo layout the tools expect: .nvmrc, legacy/web-pack/core.js, tools/export_golden.js,
// cmake/WubansiaGolden.cmake and, when given, content/golden/legacy-v21.sha256.
function fakeRepo(tag, nvmrc, manifest) {
  const repo = tmpDir(tag);
  for (const [from, to] of [[EXPORTER, 'tools/export_golden.js'], [CORE_JS, 'legacy/web-pack/core.js'], [MODULE, 'cmake/WubansiaGolden.cmake']]) {
    fs.mkdirSync(path.join(repo, path.dirname(to)), { recursive: true });
    fs.copyFileSync(from, path.join(repo, to));
  }
  fs.writeFileSync(path.join(repo, '.nvmrc'), nvmrc);
  if (manifest !== undefined) {
    fs.mkdirSync(path.join(repo, 'content', 'golden'), { recursive: true });
    fs.writeFileSync(path.join(repo, 'content', 'golden', MANIFEST), manifest);
  }
  return repo;
}

// One export that every group starts from. Tests that change files work on a copy.
let out1;
before(() => {
  out1 = tmpDir('run1');
  const r = exportTo(out1);
  assert.equal(r.code, 0, r.out);
});
const manifest1 = () => fs.readFileSync(path.join(out1, MANIFEST));

describe('export_golden.js', () => {
  it('writes 12 files per kit for all 43 kits, named by kit ID and part or form', () => {
    assert.equal(P.KITS.length, 43);
    assert.equal(EXPECTED.length, 516);
    assert.deepEqual(listMid(out1), EXPECTED);
  });

  it('writes the bytes of core.js kitMidiFiles at each kit\'s own key and BPM', () => {
    for (const kit of P.KITS) {
      const files = P.kitMidiFiles(kit, kit.root, kit.bpm);
      assert.equal(files.length, SLOTS.length, kit.id);
      files.forEach((f, n) => {
        const disk = fs.readFileSync(path.join(out1, relPath(kit.id, SLOTS[n])));
        assert.ok(disk.equals(Buffer.from(f.data)), kit.id + ' ' + SLOTS[n]);
      });
    }
  });

  it('keeps each file well formed, at 480 PPQ, with the kit\'s tempo and key signature', () => {
    for (const kit of P.KITS) {
      const key = P.keyInfo(kit.root, kit.mode);
      for (const slot of SLOTS) {
        const name = kit.id + ' ' + slot;
        const smf = readSmf(fs.readFileSync(path.join(out1, relPath(kit.id, slot))));
        assert.equal(smf.format, MULTI_TRACK.has(slot) ? 1 : 0, name);
        assert.equal(smf.division, 480, name);
        const metas = smf.tracks[0].filter(e => e.meta !== undefined && e.tick === 0);
        const tempo = metas.find(e => e.meta === 0x51), keySig = metas.find(e => e.meta === 0x59);
        assert.equal(tempo.data.readUIntBE(0, 3), Math.round(60000000 / kit.bpm), name + ' tempo');
        assert.equal(keySig.data.readInt8(0), key.sf, name + ' key signature');
        assert.equal(keySig.data[1], kit.mode === 'major' ? 0 : 1, name + ' mode');
      }
    }
  });

  it('is the set the ADR 0005 audit measured: 516 files, 903,129 note-ons', () => {
    let notes = 0;
    for (const p of EXPECTED) {
      for (const track of readSmf(fs.readFileSync(path.join(out1, p))).tracks) {
        notes += track.filter(e => (e.status & 0xf0) === 0x90 && e.data[1] > 0).length;
      }
    }
    assert.equal(notes, 903129);
  });

  it('writes a sorted manifest with the Node version, the core.js SHA-256 and every file\'s SHA-256', () => {
    const text = manifest1().toString('latin1');
    const { header, entries } = parseManifest(text);
    assert.ok(header.includes('# node v' + process.versions.node), header.join('\n'));
    assert.ok(header.includes('# core.js sha256 ' + sha256(fs.readFileSync(CORE_JS))), header.join('\n'));
    assert.ok(header.includes('# files 516'), header.join('\n'));
    assert.match(text, /^[\n\x20-\x7e]*$/, 'printable ASCII and LF only');
    assert.ok(!/[;[\]\\]/.test(text), 'none of ; [ ] \\, which the CMake checker cannot read');
    assert.deepEqual(entries.map(e => e.path), EXPECTED);
    for (const e of entries) assert.equal(e.hash, sha256(fs.readFileSync(path.join(out1, e.path))), e.path);
  });

  it('gives identical bytes on a second run, from another folder, time zone and locale', () => {
    const cwd = tmpDir('cwd');
    const r = run(process.execPath, [EXPORTER, '--out', 'run2'], {
      cwd, env: Object.assign({}, process.env, { TZ: 'Pacific/Kiritimati', LC_ALL: 'tr_TR.UTF-8', LANG: 'tr_TR.UTF-8' })
    });
    assert.equal(r.code, 0, r.out);
    const out2 = path.join(cwd, 'run2');
    assert.ok(fs.readFileSync(path.join(out2, MANIFEST)).equals(manifest1()));
    assert.deepEqual(listMid(out2), EXPECTED);
    for (const p of EXPECTED) assert.ok(fs.readFileSync(path.join(out2, p)).equals(fs.readFileSync(path.join(out1, p))), p);
  });

  it('gives identical bytes without the JIT (--jitless)', () => {
    const out = tmpDir('jitless');
    const r = run(process.execPath, ['--jitless', EXPORTER, '--out', out]);
    assert.equal(r.code, 0, r.out);
    assert.ok(fs.readFileSync(path.join(out, MANIFEST)).equals(manifest1()));
  });

  it('does not depend on the order the kits are exported in (core.js keeps no state between calls)', () => {
    const want = new Map(parseManifest(manifest1().toString('latin1')).entries.map(e => [e.path, e.hash]));
    for (const kit of P.KITS.slice().reverse()) {
      P.kitMidiFiles(kit, kit.root, kit.bpm).reverse().forEach((f, n) => {
        const p = relPath(kit.id, SLOTS[SLOTS.length - 1 - n]);
        assert.equal(sha256(f.data), want.get(p), p);
      });
    }
  });

  it('does not hang on libm rounding: every Math.sin, cos, pow and log2 result 16 ulps off changes no byte', () => {
    // The header's claim, behind the Node pin: the transcendental results core.js rounds into MIDI bytes
    // all sit well clear of a rounding boundary, so a libm a few ulps off gives the same files.
    const want = new Map(parseManifest(manifest1().toString('latin1')).entries.map(e => [e.path, e.hash]));
    const orig = { sin: Math.sin, cos: Math.cos, pow: Math.pow, log2: Math.log2 };
    const f64 = new Float64Array(1), i64 = new BigInt64Array(f64.buffer);
    try {
      for (const k of [16n, -16n]) {
        let calls = 0;
        const nudge = x => { calls++; if (!Number.isFinite(x) || x === 0) return x; f64[0] = x; i64[0] += k; return f64[0]; };
        Math.sin = x => nudge(orig.sin(x));
        Math.cos = x => nudge(orig.cos(x));
        Math.pow = (x, y) => nudge(orig.pow(x, y));
        Math.log2 = x => nudge(orig.log2(x));
        const got = inProcessHashes();
        assert.ok(calls > 20000, 'the nudged functions were called (' + calls + ' calls)');
        for (const [p, h] of want) assert.equal(got.get(p), h, p + ' with ' + k + ' ulps');
      }
    } finally {
      Object.assign(Math, orig);
    }
    assert.equal(Math.sin, orig.sin);
  });

  it('on a rerun, removes only files its previous manifest listed that it no longer writes', () => {
    const base = tmpDir('rerun');
    const out = path.join(base, 'golden');
    assert.equal(exportTo(out).code, 0);
    const zero = '0'.repeat(64);
    fs.writeFileSync(path.join(out, 'UKG01', 'UKG01_Old.mid'), 'old');
    fs.writeFileSync(path.join(base, 'outside.mid'), 'keep');
    fs.writeFileSync(path.join(out, 'notes.txt'), 'keep');
    fs.writeFileSync(path.join(out, 'UKG01', 'UKG01_Mine.mid'), 'keep');
    fs.appendFileSync(path.join(out, MANIFEST), zero + '  UKG01/UKG01_Old.mid\n' + zero + '  ../outside.mid\n');
    const r = exportTo(out);
    assert.equal(r.code, 0, r.out);
    assert.match(r.out, /removed 1 stale/);
    assert.ok(!fs.existsSync(path.join(out, 'UKG01', 'UKG01_Old.mid')), 'the stale file is removed');
    assert.ok(fs.existsSync(path.join(base, 'outside.mid')), 'nothing outside the folder is touched');
    assert.ok(fs.existsSync(path.join(out, 'notes.txt')), 'files it never wrote are kept');
    assert.ok(fs.existsSync(path.join(out, 'UKG01', 'UKG01_Mine.mid')), 'MIDI files no manifest listed are kept');
    assert.ok(fs.readFileSync(path.join(out, MANIFEST)).equals(manifest1()));
  });

  it('replaces every file by renaming a complete temporary file into place', { skip: process.platform === 'win32' }, () => {
    // A rename gives each path a new inode; writing in place would keep the old one, and a reader could
    // see a half-written file.
    const out = path.join(tmpDir('atomic'), 'golden');
    fs.cpSync(out1, out, { recursive: true });
    const paths = [...EXPECTED, MANIFEST];
    const before = new Map(paths.map(p => [p, fs.statSync(path.join(out, p)).ino]));
    const r = exportTo(out);
    assert.equal(r.code, 0, r.out);
    for (const p of paths) assert.notEqual(fs.statSync(path.join(out, p)).ino, before.get(p), p);
  });

  it('survives two runs into one folder at once, and leaves no temporary files', async () => {
    const out = path.join(tmpDir('concurrent'), 'golden');
    const results = await Promise.all([runAsync(process.execPath, [EXPORTER, '--out', out]), runAsync(process.execPath, [EXPORTER, '--out', out])]);
    for (const r of results) assert.equal(r.code, 0, r.out);
    assert.ok(fs.readFileSync(path.join(out, MANIFEST)).equals(manifest1()));
    assert.deepEqual(listMid(out), EXPECTED);
    for (const p of EXPECTED) assert.ok(fs.readFileSync(path.join(out, p)).equals(fs.readFileSync(path.join(out1, p))), p);
    assert.deepEqual(listFiles(out, n => !n.endsWith('.mid') && n !== MANIFEST), [], 'only the MIDI files and the manifest');
  });

  describe('the Node pin', () => {
    it('.nvmrc holds one exact version, and this test runs on it', () => {
      const pinned = fs.readFileSync(NVMRC, 'utf8');
      assert.match(pinned, /^\d+\.\d+\.\d+\n$/);
      assert.equal(process.versions.node, pinned.trim(), 'run the tests with the Node version in .nvmrc');
    });

    it('refuses to run on any other Node version, before writing anything', () => {
      const repo = fakeRepo('pin', '0.0.1\n');
      const out = path.join(repo, 'out');
      const r = exportTo(out, path.join(repo, 'tools', 'export_golden.js'));
      assert.equal(r.code, 2, r.out);
      assert.match(r.out, /refused/);
      assert.match(r.out, /0\.0\.1/);
      assert.ok(r.out.includes(process.versions.node), r.out);
      assert.ok(!fs.existsSync(out), 'no output folder');
    });

    for (const bad of ['22\n', 'lts/jod\n', '\n', '22.22.0.1\n']) {
      it('refuses an .nvmrc that is not one exact version: ' + JSON.stringify(bad), () => {
        const repo = fakeRepo('pin', bad);
        const r = exportTo(path.join(repo, 'out'), path.join(repo, 'tools', 'export_golden.js'));
        assert.equal(r.code, 2, r.out);
        assert.match(r.out, /exact/);
      });
    }

    it('accepts a leading v and CRLF, as nvm does', () => {
      const repo = fakeRepo('pin', 'v' + process.versions.node + '\r\n');
      const r = exportTo(path.join(repo, 'out'), path.join(repo, 'tools', 'export_golden.js'));
      assert.equal(r.code, 0, r.out);
    });
  });

  it('rejects unknown arguments', () => {
    const r = run(process.execPath, [EXPORTER, '--bogus']);
    assert.equal(r.code, 2, r.out);
    assert.match(r.out, /Usage/);
  });
});

describe('the golden_check checker (cmake -P cmake/WubansiaGolden.cmake)', () => {
  let dir, manifest, fresh;
  before(() => {
    const base = tmpDir('check');
    dir = path.join(base, 'golden');
    fs.cpSync(out1, dir, { recursive: true });
    manifest = path.join(base, 'committed.sha256'); // stands in for content/golden/legacy-v21.sha256
    fs.copyFileSync(path.join(dir, MANIFEST), manifest);
    fresh = path.join(dir, MANIFEST);
  });

  function check(opts = {}) {
    const defs = Object.assign({ GOLDEN_DIR: dir, MANIFEST: manifest, FRESH_MANIFEST: fresh, CORE_JS, NVMRC }, opts);
    return run(CMAKE, [...Object.entries(defs).filter(([, v]) => v !== null).map(([k, v]) => '-D' + k + '=' + v), '-P', MODULE]);
  }
  function withManifest(edit, fn) { // runs fn on a manifest whose lines went through edit(lines)
    const lines = fs.readFileSync(manifest, 'latin1').split('\n');
    const edited = path.join(path.dirname(manifest), 'edited.sha256');
    fs.writeFileSync(edited, Buffer.from(edit(lines).join('\n'), 'latin1'));
    return fn(edited);
  }
  const editedCheck = edit => withManifest(edit, m => check({ MANIFEST: m, FRESH_MANIFEST: null }));
  const entryIndex = lines => lines.findIndex(l => !l.startsWith('#'));
  function failsWith(r, re) {
    assert.notEqual(r.code, 0, r.out);
    assert.match(r.out, /golden_check: FAILED/);
    assert.match(r.out, re);
  }

  it('passes on a fresh export', () => {
    const r = check();
    assert.equal(r.code, 0, r.out);
    assert.match(r.out, /516 files match/);
  });

  it('fails when one byte of one file changes, and names the file', () => {
    const f = path.join(dir, 'UKG01', 'UKG01_FullSong.mid');
    const orig = fs.readFileSync(f);
    const bad = Buffer.from(orig);
    bad[bad.length - 5] ^= 1;
    fs.writeFileSync(f, bad);
    try {
      failsWith(check(), /UKG01\/UKG01_FullSong\.mid/);
    } finally { fs.writeFileSync(f, orig); }
  });

  it('fails when a file is missing', () => {
    const f = path.join(dir, 'TH04', 'TH04_Drums-Fill.mid'), away = f + '.away';
    fs.renameSync(f, away);
    try {
      failsWith(check(), /missing[^\n]*\n?[^\n]*TH04\/TH04_Drums-Fill\.mid/i);
    } finally { fs.renameSync(away, f); }
  });

  it('fails when the folder holds a MIDI file the manifest does not list', () => {
    const f = path.join(dir, 'CH01', 'CH01_Extra.mid');
    fs.writeFileSync(f, 'x');
    try {
      failsWith(check(), /not in the manifest[^\n]*\n[^\n]*CH01\/CH01_Extra\.mid/);
    } finally { fs.rmSync(f); }
  });

  it('reads relative paths from the folder it runs in, the unlisted-file check included', () => {
    const base = path.dirname(dir);
    const rel = { GOLDEN_DIR: path.basename(dir), MANIFEST: path.basename(manifest), FRESH_MANIFEST: path.join(path.basename(dir), MANIFEST) };
    const relCheck = () => run(CMAKE, [...Object.entries(Object.assign({ CORE_JS, NVMRC }, rel)).map(([k, v]) => '-D' + k + '=' + v), '-P', MODULE], { cwd: base });
    const ok = relCheck();
    assert.equal(ok.code, 0, ok.out);
    assert.match(ok.out, /516 files match/);
    const f = path.join(dir, 'CH02', 'CH02_Extra.mid');
    fs.writeFileSync(f, 'x');
    try {
      failsWith(relCheck(), /not in the manifest[^\n]*\n[^\n]*CH02\/CH02_Extra\.mid/);
    } finally { fs.rmSync(f); }
  });

  it('reports every problem in one run', () => {
    const changed = path.join(dir, 'SG03', 'SG03_Bass-B.mid'), orig = fs.readFileSync(changed);
    const gone = path.join(dir, 'GV02', 'GV02_Chords-A.mid'), away = gone + '.away';
    const extra = path.join(dir, 'MTH01', 'MTH01_Extra.mid');
    fs.writeFileSync(changed, Buffer.concat([orig, Buffer.from([0])]));
    fs.renameSync(gone, away);
    fs.writeFileSync(extra, 'x');
    try {
      const r = check();
      failsWith(r, /3 problem/);
      for (const p of ['SG03/SG03_Bass-B.mid', 'GV02/GV02_Chords-A.mid', 'MTH01/MTH01_Extra.mid']) assert.ok(r.out.includes(p), p + '\n' + r.out);
    } finally {
      fs.writeFileSync(changed, orig);
      fs.renameSync(away, gone);
      fs.rmSync(extra);
    }
  });

  it('lists at most 20 paths per problem, then a count', () => {
    const moved = EXPECTED.slice(0, 25);
    moved.forEach(p => fs.renameSync(path.join(dir, p), path.join(dir, p + '.away')));
    try {
      const r = check();
      failsWith(r, /25 of the 516 files are missing/);
      assert.match(r.out, /\.\.\. and 5 more/);
      assert.ok(r.out.includes(moved[19]) && !r.out.includes(moved[20]), r.out);
    } finally { moved.forEach(p => fs.renameSync(path.join(dir, p + '.away'), path.join(dir, p))); }
  });

  it('fails when core.js differs from the one the manifest names', () => {
    const copy = path.join(path.dirname(manifest), 'core.js');
    fs.writeFileSync(copy, Buffer.concat([fs.readFileSync(CORE_JS), Buffer.from('\n')]));
    failsWith(check({ CORE_JS: copy }), /core\.js[^\n]*frozen/);
  });

  it('fails when .nvmrc no longer names the manifest\'s Node version', () => {
    const other = path.join(path.dirname(manifest), 'nvmrc');
    fs.writeFileSync(other, '22.22.1\n');
    failsWith(check({ NVMRC: other }), /22\.22\.1/);
  });

  it('fails when the export\'s own manifest differs from the committed one', () => {
    failsWith(withManifest(lines => ['# another header line', ...lines], m => check({ FRESH_MANIFEST: m })), /differs/);
  });

  it('fails when the export\'s own manifest differs only in line endings', () => {
    failsWith(withManifest(lines => lines.map((l, i) => (i < lines.length - 1 ? l + '\r' : l)), m => check({ FRESH_MANIFEST: m })), /differs/);
  });

  it('fails when the export wrote no manifest', () => {
    failsWith(check({ FRESH_MANIFEST: path.join(path.dirname(manifest), 'none.sha256') }), /wrote no manifest/);
  });

  it('fails when the folder does not exist', () => {
    failsWith(check({ GOLDEN_DIR: path.join(path.dirname(manifest), 'nowhere') }), /nowhere/);
  });

  it('fails when the committed manifest does not exist', () => {
    failsWith(check({ MANIFEST: path.join(path.dirname(manifest), 'absent.sha256') }), /absent\.sha256 does not exist/);
  });

  for (const v of ['GOLDEN_DIR', 'MANIFEST', 'CORE_JS', 'NVMRC']) {
    it('needs -D' + v, () => {
      const r = check({ [v]: null });
      assert.notEqual(r.code, 0, r.out);
      assert.match(r.out, new RegExp('-D' + v + '=<path> is missing'));
    });
  }

  describe('the manifest\'s format', () => {
    // Line numbers in the messages, from the manifest itself: its first entry and its '# node' line.
    const lineOf = pred => fs.readFileSync(manifest, 'latin1').split('\n').findIndex(pred) + 1;
    const first = () => lineOf(l => !l.startsWith('#'));
    const nodeLine = () => lineOf(l => l.startsWith('# node '));
    const atLine = n => new RegExp('line ' + n + ' is not ');
    const cases = [
      ['entries out of order', () => new RegExp('out of order at line ' + (first() + 1) + ': CH01/CH01_Bass-A\\.mid comes after ' +
        'CH01/CH01_Bass-B\\.mid\\. Entries are sorted by path in byte order, each path once\\.'),
        lines => { const i = entryIndex(lines); [lines[i], lines[i + 1]] = [lines[i + 1], lines[i]]; return lines; }],
      ['a duplicated entry', () => /out of order/, lines => { const i = entryIndex(lines); lines.splice(i, 0, lines[i]); return lines; }],
      ['a 63-digit hash', () => atLine(first()), lines => { const i = entryIndex(lines); lines[i] = lines[i].slice(1); return lines; }],
      ['an upper-case hash', () => atLine(first()), lines => { const i = entryIndex(lines); lines[i] = lines[i].slice(0, 64).toUpperCase() + lines[i].slice(64); return lines; }],
      ['one space instead of two', () => atLine(first()), lines => { const i = entryIndex(lines); lines[i] = lines[i].replace('  ', ' '); return lines; }],
      ['a path out of the folder', () => atLine(first()), lines => { const i = entryIndex(lines); lines[i] = lines[i].slice(0, 66) + '../outside.mid'; return lines; }],
      ['an absolute path', () => atLine(first()), lines => { const i = entryIndex(lines); lines[i] = lines[i].slice(0, 66) + '/etc/x.mid'; return lines; }],
      ['a blank line', () => atLine(first() + 1), lines => { lines.splice(entryIndex(lines) + 1, 0, ''); return lines; }],
      ['no newline at the end', () => /does not end with a newline/, lines => lines.slice(0, -1)],
      ['CRLF line endings', () => /0d \(CR: CRLF line endings\?\)\. It must be LF-only text: it is compared byte for byte/,
        lines => lines.map((l, i) => (i < lines.length - 1 ? l + '\r' : l))],
      ['one CRLF', () => /CR/, lines => { lines[3] += '\r'; return lines; }],
      ['a CR inside a line', () => /CR/, lines => { lines[0] += ' a\rb'; return lines; }],
      ['a NUL byte', () => /NUL/, lines => { lines[0] += ' a\x00b'; return lines; }],
      ['a semicolon', () => /has a semicolon, which the checker can't read/, lines => { lines[0] += ' a;b'; return lines; }],
      ['a [', () => /has a \[, which the checker can't read/, lines => { lines[0] += ' [x'; return lines; }],
      ['a ]', () => /has a \], which the checker can't read/, lines => { lines[0] += ' x]'; return lines; }],
      ['a backslash', () => /has a backslash, which the checker can't read/, lines => { lines[0] += ' a\\b'; return lines; }],
      ['a byte outside printable ASCII', () => /printable ASCII and LF: e9\./, lines => { lines[0] += ' caf\xe9'; return lines; }],
      ['a tab', () => /printable ASCII and LF: 09 \(tab\)/, lines => { lines[0] += '\tx'; return lines; }],
      ['a header line after the entries', () => /after the entries/, lines => { lines.splice(lines.length - 1, 0, '# late'); return lines; }],
      ['a wrong file count', () => /says '# files 515' in its header but lists 516 files/, lines => lines.map(l => (l === '# files 516' ? '# files 515' : l))],
      ['no node line', () => /no '# node v<version>'/, lines => lines.filter(l => !l.startsWith('# node '))],
      ['no core.js line', () => /no '# core\.js sha256/, lines => lines.filter(l => !l.startsWith('# core.js '))],
      ['no files line', () => /no '# files <count>'/, lines => lines.filter(l => !l.startsWith('# files '))],
      ['two node lines', () => /second '# node'/, lines => { lines.splice(1, 0, '# node v' + process.versions.node); return lines; }],
      ['a node line without an exact version', () => new RegExp('line ' + nodeLine() + " is not '# node v"), lines => lines.map(l => (l.startsWith('# node ') ? '# node v22' : l))],
      ['no entries', () => /lists no files/, lines => [...lines.filter(l => l.startsWith('#')).map(l => (l.startsWith('# files ') ? '# files 0' : l)), '']]
    ];
    for (const [what, re, edit] of cases) {
      it('rejects ' + what, () => failsWith(editedCheck(edit), re()));
    }
  });
});

describe('golden_check and its test, from a CMakeLists.txt that includes the module', () => {
  // A fake repo (with a space in its path) whose committed manifest is a fresh export, and a small
  // CMake project that includes its copy of the module. Only golden_check is built: nothing compiles.
  const ninja = spawnSync('ninja', ['--version']).status === 0;
  let manifestText;
  before(() => { manifestText = manifest1().toString('latin1'); });

  // Configures with this Node as WUBANSIA_NODE_EXECUTABLE, unless env is given: then node comes from its PATH.
  function project({ steps = '', nvmrc = process.versions.node + '\n', manifest = manifestText, cacheArgs = [], env } = {}) {
    const repo = fakeRepo('cmake repo', nvmrc, manifest);
    const src = path.join(repo, 'proj'), bin = path.join(repo, 'build', 'proj');
    fs.mkdirSync(src);
    fs.writeFileSync(path.join(src, 'CMakeLists.txt'), [
      'cmake_minimum_required(VERSION 3.25)',
      'project(golden_check_test NONE)',
      'list(APPEND CMAKE_MODULE_PATH "' + path.join(repo, 'cmake').split(path.sep).join('/') + '")',
      'include(WubansiaGolden)', // before the option and enable_testing(), on purpose: the definitions are deferred
      'option(WUBANSIA_BUILD_TESTS "" ON)',
      'if(WUBANSIA_BUILD_TESTS)',
      '    enable_testing()',
      'endif()',
      steps, ''
    ].join('\n'));
    const node = env ? [] : ['-DWUBANSIA_NODE_EXECUTABLE=' + process.execPath];
    const conf = run(CMAKE, ['-S', src, '-B', bin, ...(ninja ? ['-G', 'Ninja'] : []), ...node, ...cacheArgs], env ? { env } : {});
    return { repo, bin, conf, build: () => run(CMAKE, ['--build', bin, '--target', 'golden_check']) };
  }
  function ctestJson(bin) {
    const r = run(CTEST, ['--show-only=json-v1'], { cwd: bin });
    assert.equal(r.code, 0, r.out);
    return JSON.parse(r.out.slice(r.out.indexOf('{')));
  }

  it('with the pinned Node: says so, and golden_check exports, checks and notes the missing metrics step', () => {
    const { repo, conf, build } = project();
    assert.equal(conf.code, 0, conf.out);
    assert.match(conf.out, new RegExp('golden_check: Node v' + process.versions.node.replace(/\./g, '\\.') + ' \\(.*\\), the version in \\.nvmrc'));
    const r = build();
    assert.equal(r.code, 0, r.out);
    assert.match(r.out, /export_golden: wrote 516 MIDI files/);
    assert.match(r.out, /golden_check: 516 files match/);
    assert.match(r.out, /the metrics step [^\n]* is not wired yet/);
    assert.deepEqual(listMid(path.join(repo, 'build', 'golden')), EXPECTED, 'the default folder is <repo>/build/golden');
  });

  it('runs added steps after the check, in order, after building their DEPENDS, and drops the note for a metrics step', () => {
    const steps = [
      'add_custom_target(fake_tool COMMAND "${CMAKE_COMMAND}" -E echo "fake_tool built")',
      'wubansia_golden_check_add_step(NAME first COMMAND "${CMAKE_COMMAND}" -E echo "step one ran")',
      'wubansia_golden_check_add_step(NAME metrics DEPENDS fake_tool COMMAND "${CMAKE_COMMAND}" -E echo "metrics ran on" "${WUBANSIA_GOLDEN_DIR}")'
    ].join('\n');
    const { conf, build } = project({ steps });
    assert.equal(conf.code, 0, conf.out);
    const r = build();
    assert.equal(r.code, 0, r.out);
    const at = s => { const i = r.out.indexOf(s); assert.ok(i >= 0, s + '\n' + r.out); return i; };
    assert.ok(at('fake_tool built') < at('export_golden: wrote'), 'DEPENDS are built first');
    assert.ok(at('516 files match') < at('step one ran'), 'steps run after the check');
    assert.ok(at('step one ran') < at('metrics ran on '), 'in the order they were added');
    assert.match(r.out, /metrics ran on \S[^\n]*\/build\/golden\n/, 'a step sees WUBANSIA_GOLDEN_DIR');
    assert.doesNotMatch(r.out, /not wired yet/);
  });

  it('fails on a manifest mismatch, and then runs no added step', () => {
    const lines = manifestText.split('\n');
    const i = lines.findIndex(l => !l.startsWith('#'));
    lines[i] = (lines[i][0] === '0' ? '1' : '0') + lines[i].slice(1);
    const marker = path.join(tmpDir('marker'), 'metrics-ran').split(path.sep).join('/');
    const steps = 'wubansia_golden_check_add_step(NAME metrics COMMAND "${CMAKE_COMMAND}" -E touch "' + marker + '")';
    const { conf, build } = project({ manifest: lines.join('\n'), steps });
    assert.equal(conf.code, 0, conf.out);
    const r = build();
    assert.notEqual(r.code, 0, r.out);
    assert.match(r.out, /golden_check: FAILED/);
    assert.match(r.out, /1 of the 516 files differ from the manifest/);
    assert.ok(!fs.existsSync(marker), 'the metrics step did not run');
  });

  it('registers golden.export_golden as node --test on this file', () => {
    const { repo, bin, conf } = project();
    assert.equal(conf.code, 0, conf.out);
    const tests = ctestJson(bin).tests.filter(t => t.name === 'golden.export_golden');
    assert.equal(tests.length, 1);
    const [node, ...args] = tests[0].command;
    assert.deepEqual(args.slice(0, 2), ['--test', '--test-reporter=spec']);
    assert.equal(args.length, 3);
    const suffix = '/tools/export_golden/export_golden.test.js';
    assert.ok(args[2].endsWith(suffix), args[2]);
    assert.equal(fs.realpathSync(args[2].slice(0, -suffix.length)), fs.realpathSync(repo), 'the test file in the repo the module is in');
    assert.equal(fs.realpathSync(node), fs.realpathSync(process.execPath));
  });

  it('finds node on PATH when WUBANSIA_NODE_EXECUTABLE is empty', () => {
    const env = Object.assign({}, process.env, { PATH: path.dirname(process.execPath) + path.delimiter + process.env.PATH });
    const { bin, conf } = project({ env });
    assert.equal(conf.code, 0, conf.out);
    assert.match(conf.out, /golden_check: Node v[0-9.]+ \(.*\), the version in \.nvmrc/);
    const t = ctestJson(bin).tests.find(x => x.name === 'golden.export_golden');
    assert.equal(fs.realpathSync(t.command[0]), fs.realpathSync(process.execPath));
  });

  it('registers no test when WUBANSIA_BUILD_TESTS is off', () => {
    const { bin, conf } = project({ cacheArgs: ['-DWUBANSIA_BUILD_TESTS=OFF'] });
    assert.equal(conf.code, 0, conf.out);
    assert.equal(run(CTEST, ['-N'], { cwd: bin }).out.includes('golden.export_golden'), false);
  });

  it('without a Node: configures anyway, golden_check fails with the reason, and the test is skipped', () => {
    const { repo, bin, conf, build } = project({ cacheArgs: ['-DWUBANSIA_NODE_EXECUTABLE=/nonexistent/node'] });
    assert.equal(conf.code, 0, conf.out);
    assert.match(conf.out, /golden_check: no Node found \(WUBANSIA_NODE_EXECUTABLE=\/nonexistent\/node\)/);
    const r = build();
    assert.notEqual(r.code, 0, r.out);
    assert.match(r.out, /no Node found/);
    assert.ok(!fs.existsSync(path.join(repo, 'build', 'golden')), 'nothing exported');
    const t = run(CTEST, ['--output-on-failure'], { cwd: bin });
    assert.equal(t.code, 0, t.out);
    assert.match(t.out, /golden\.export_golden[ .]*\*+Skipped/);
  });

  it('with another Node version: says so, the exporter refuses in golden_check, and the test is skipped', { skip: process.platform === 'win32' }, () => {
    const fakeNodeDir = tmpDir('fake-node');
    const fakeNode = path.join(fakeNodeDir, 'node');
    // Says it is v0.0.1, and otherwise runs the real Node, so the exporter's own refusal is what fails.
    fs.writeFileSync(fakeNode, '#!/bin/sh\nif [ "$1" = "--version" ]; then echo v0.0.1; exit 0; fi\nexec "' + process.execPath + '" "$@"\n', { mode: 0o755 });
    const { bin, conf, build } = project({ nvmrc: '0.0.2\n', cacheArgs: ['-DWUBANSIA_NODE_EXECUTABLE=' + fakeNode] });
    assert.equal(conf.code, 0, conf.out);
    assert.match(conf.out, /golden_check: Node v0\.0\.1 \(.*\), but \.nvmrc pins 0\.0\.2/);
    const r = build();
    assert.notEqual(r.code, 0, r.out);
    assert.match(r.out, /export_golden: refused: this is Node [0-9.]+ .*but \.nvmrc pins 0\.0\.2/);
    const t = run(CTEST, [], { cwd: bin });
    assert.equal(t.code, 0, t.out);
    assert.match(t.out, /Skipped/);
  });

  it('refuses steps added after golden_check is defined', () => {
    const steps = 'cmake_language(DEFER CALL wubansia_golden_check_add_step NAME late COMMAND echo late)';
    const { conf } = project({ steps });
    assert.notEqual(conf.code, 0, conf.out);
    assert.match(conf.out, /golden_check is already defined/);
  });
});
