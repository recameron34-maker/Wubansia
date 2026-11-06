#!/usr/bin/env node
/* Exports the legacy MIDI baseline: the 516 MIDI files that frozen legacy/web-pack/core.js writes for
   its 43 kits, each at the kit's own key and BPM (docs/plans/S1-futuristic-slice.md section 4.3,
   decision D2 option A; ADR 0013). golden_check (cmake/WubansiaGolden.cmake) checks them against
   content/golden/legacy-v21.sha256, and the same files are listening condition L.

   Usage: node tools/export_golden.js [--out <dir>]        (default: build/golden/ in the repo)

   Writes <dir>/<KIT>/<KIT>_<slot>.mid for every kit and slot, then <dir>/legacy-v21.sha256: one
   "<sha256>  <path>" line per file, byte-sorted by path, under a header that names the Node version
   and core.js's own SHA-256. Run twice, it writes the same bytes. Nothing in it depends on the clock,
   the locale, the time zone, the working directory or the order the kits are exported in.

   The 12 files per kit are the ones the web app's "Save all kits" button writes for each kit
   (app.js:1880 allKitFiles, which calls core.js kitMidiFiles(kit, kit.root, kit.bpm)), in that order:
     Drums-Main, Drums-Variation, Drums-Fill   the 4-bar drum patterns (regionMidi, SMF type 0)
     Bass-A, Bass-B, Chords-A, Chords-B,       the 4-bar melodic parts, A and B (regionMidi, type 0;
       Melody-A, Melody-B                        core.js calls the lead "Melody")
     FullArrangement                           8 bars: the A parts, then the B parts, with the drum fill
                                                 on bar 8 (arrangementMidi, type 1)
     FullSong                                  the full song, one form per kit family (songMidi, type 1)
     ExtendedMix                               the extended mix, with its seeded layers, section markers
                                                 and CC and pitch-bend lanes (extendedMidi, type 1)
   That is every kind of MIDI file the app exports, so it covers every code path in core.js that
   writes MIDI. It is also the set the ADR 0005 audit measured: 516 files, 903,129 note-ons. The app's
   other buttons write subsets of it, or the same files at another key or tempo: "Save kit" and "Save
   lane" at the key and BPM on screen, "Save song" the full song or the extended mix. Transposed or
   re-tempoed copies are not exported (ROADMAP M1: original keys only).

   Files are named by kit ID and slot, never by core.js's own names, which also carry the display name,
   key, BPM and bar count (UKG01_PirateSignal_FullSong-144bars_Fmin-4A_134bpm.mid). A slot's name never
   changes, so a manifest diff only ever shows changed hashes. The export still checks every legacy
   name (kit ID, part, the kit's own key and BPM), so a file can't land in the wrong slot.

   The Node version is pinned: the exporter refuses to run on any version but the one in .nvmrc.
   core.js writes Math.sin, Math.cos, Math.pow and Math.log2 results, which no standard requires to be
   correctly rounded, into CC, pitch-bend and velocity bytes (for example core.js:2415, 2759, 2784,
   2919, 2935, 2957, 2963, 3052, 3066, 3077, 3107), so another V8 could in principle change a byte.
   Measured on Linux x86_64 (2026-11-04): with Node 22.22.0, moving every one of those 21,300 results
   per export by 1 to 256 ulps either way, or by a relative 1e-7, changes no file, and --jitless,
   --max-opt=0 and --always-turbofan give the same bytes. With the pin lifted, Node 20.20.0 (V8 11.3),
   Node 21.7.3 (V8 11.8) and Bun 1.4.4 (JavaScriptCore) write the same 516 files too. So the pin is a
   safety margin, not a hair trigger: a libm a few ulps off (another platform, another V8) would still
   give the same baseline. Only the manifest's "# node" line names the version.

   Concurrent runs into one folder are safe: every file and the manifest are written to a temporary
   name and renamed into place, so a reader sees either the previous complete file or the new one.

   Re-baselining: the export never touches content/golden/. The committed manifest changes only when you
   are asked to change it, in its own commit with an ADR note, never to make a check pass (CLAUDE.md,
   "Determinism and golden files"). Then: cp build/golden/legacy-v21.sha256 content/golden/

   Exit status: 0 done, 1 error, 2 refused (usage, or not the pinned Node). */
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const NVMRC = path.join(ROOT, '.nvmrc');
const CORE_JS = path.join(ROOT, 'legacy', 'web-pack', 'core.js');
const DEFAULT_OUT = path.join(ROOT, 'build', 'golden');
const MANIFEST = 'legacy-v21.sha256';
const KITS = 43;

// kitMidiFiles' 12 files, in its order (core.js:3526-3534): our slot name, and the part of core.js's
// own file name between "<id>_<display name>_" and "_<key>-<camelot>_<bpm>bpm.mid".
const SLOTS = [
  ['Drums-Main', 'Drums-Main'], ['Drums-Variation', 'Drums-Variation'], ['Drums-Fill', 'Drums-Fill'],
  ['Bass-A', 'Bass-A'], ['Bass-B', 'Bass-B'], ['Chords-A', 'Chords-A'], ['Chords-B', 'Chords-B'],
  ['Melody-A', 'Melody-A'], ['Melody-B', 'Melody-B'], ['FullArrangement', 'FullArrangement-8bars'],
  ['FullSong', 'FullSong-[0-9]+bars'], ['ExtendedMix', 'ExtendedMix-[0-9]+bars']
].map(([name, legacy]) => ({ name, legacy: new RegExp('^' + legacy + '$') }));

// The only paths it ever writes, and so the only ones a rerun may delete. golden_check's checker
// accepts the same shape (cmake/WubansiaGolden.cmake).
const OUR_PATH = /^([A-Z]+[0-9]+)\/\1_[A-Za-z]+(-[A-Za-z]+)?\.mid$/;

class Refusal extends Error {}

function usage() {
  return 'Usage: node tools/export_golden.js [--out <dir>]   (default: ' + path.relative(process.cwd(), DEFAULT_OUT) + ')';
}

function parseArgs(argv) {
  let out = DEFAULT_OUT;
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') { console.log(usage()); process.exit(0); }
    if (a === '--out' && i + 1 < argv.length) out = argv[++i];
    else if (a.startsWith('--out=') && a.length > 6) out = a.slice(6);
    else throw new Refusal('unknown argument ' + JSON.stringify(a) + '\n' + usage());
  }
  return { out: path.resolve(out) };
}

function checkNode() {
  let text;
  try { text = fs.readFileSync(NVMRC, 'utf8'); } catch (e) { throw new Refusal('cannot read ' + NVMRC + ': ' + e.message); }
  const want = text.trim().replace(/^v/, '');
  if (!/^[0-9]+\.[0-9]+\.[0-9]+$/.test(want)) {
    throw new Refusal('.nvmrc must hold one exact Node version, like 22.22.0, not ' + JSON.stringify(text.trim()));
  }
  if (process.versions.node !== want) {
    throw new Refusal('this is Node ' + process.versions.node + ' (' + process.execPath + '), but .nvmrc pins ' + want + '.\n' +
      'core.js writes Math.sin, Math.cos and Math.pow results into MIDI bytes, so the baseline is defined by that version.\n' +
      'Install it (nvm install, from the repo root) and run again. For the golden_check target, configure again\n' +
      'afterwards (cmake --preset release) so CMake finds it, or pass -DWUBANSIA_NODE_EXECUTABLE=<path to node ' + want + '>.');
  }
  return want;
}

const sha256 = data => crypto.createHash('sha256').update(data).digest('hex');
const byteOrder = (a, b) => (a < b ? -1 : a > b ? 1 : 0);
const escapeRe = s => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

// All 516 files, as { path, data }, in kit order. Each legacy name is checked against its slot.
function exportFiles(P) {
  if (P.KITS.length !== KITS) throw new Error('core.js has ' + P.KITS.length + ' kits, expected ' + KITS);
  const files = [];
  for (const kit of P.KITS) {
    const key = P.keyInfo(kit.root, kit.mode);
    const prefix = kit.id + '_' + P.slug(kit.name) + '_';
    const suffix = '_' + key.short.replace('#', 's') + '-' + key.camelot + '_' + kit.bpm + 'bpm.mid';
    const legacyName = new RegExp('^' + escapeRe(prefix) + '(.+)' + escapeRe(suffix) + '$');
    const made = P.kitMidiFiles(kit, kit.root, kit.bpm);
    if (made.length !== SLOTS.length) throw new Error(kit.id + ': kitMidiFiles gave ' + made.length + ' files, expected ' + SLOTS.length);
    made.forEach((f, i) => {
      const m = legacyName.exec(f.name);
      if (!m || !SLOTS[i].legacy.test(m[1])) {
        throw new Error(kit.id + ': file ' + (i + 1) + ' is ' + f.name + ', expected slot ' + SLOTS[i].name + ' at ' + key.label + ', ' + kit.bpm + ' BPM');
      }
      const rel = kit.id + '/' + kit.id + '_' + SLOTS[i].name + '.mid';
      if (!OUR_PATH.test(rel)) throw new Error('unexpected path ' + rel);
      files.push({ path: rel, data: f.data });
    });
  }
  return files;
}

// Printable ASCII only, and none of ; [ ] \ (the CMake checker splits the text into a list). Only
// stable facts: any change to this text changes the committed manifest, so the plan and ADR references
// live in this file's header instead.
function manifestText(nodeVersion, coreSha, entries) {
  return [
    '# Legacy MIDI baseline v21: the SHA-256 of every MIDI file tools/export_golden.js writes from',
    '# frozen legacy/web-pack/core.js (43 kits x 12 files, each at the kit\'s own key and BPM), by path',
    '# under build/golden/. golden_check (cmake/WubansiaGolden.cmake) fails on any difference. Never',
    '# edit or regenerate this file to make a check pass (CLAUDE.md, "Determinism and golden files").',
    '# node v' + nodeVersion,
    '# core.js sha256 ' + coreSha,
    '# files ' + entries.length,
    ...entries.map(e => e.hash + '  ' + e.path)
  ].join('\n') + '\n';
}

// Paths the previous export listed in <out>/legacy-v21.sha256. Lines that aren't ours are ignored.
function previousPaths(out) {
  let text;
  try { text = fs.readFileSync(path.join(out, MANIFEST), 'utf8'); } catch (e) { if (e.code === 'ENOENT') return []; throw e; }
  return text.split('\n').map(l => /^[0-9a-f]{64} {2}(\S+)$/.exec(l)).filter(m => m && OUR_PATH.test(m[1])).map(m => m[1]);
}

// Writes through a temporary name in the same folder, then renames it into place.
function writeAtomic(file, data) {
  const tmp = path.join(path.dirname(file), '.' + path.basename(file) + '.' + process.pid + '.tmp');
  try {
    fs.writeFileSync(tmp, data);
    fs.renameSync(tmp, file);
  } catch (e) {
    fs.rmSync(tmp, { force: true });
    throw e;
  }
}

function main() {
  const { out } = parseArgs(process.argv.slice(2));
  const nodeVersion = checkNode(); // before anything is read or written
  const coreSha = sha256(fs.readFileSync(CORE_JS));
  const P = require(CORE_JS);
  const files = exportFiles(P);
  const keep = new Set(files.map(f => f.path));

  // A rerun deletes the files the previous manifest listed that this run doesn't write (after a slot
  // rename, say). It never deletes anything else.
  fs.mkdirSync(out, { recursive: true });
  const stale = previousPaths(out).filter(p => !keep.has(p));
  stale.forEach(p => fs.rmSync(path.join(out, ...p.split('/')), { force: true }));

  let bytes = 0;
  for (const f of files) {
    const file = path.join(out, ...f.path.split('/'));
    fs.mkdirSync(path.dirname(file), { recursive: true });
    writeAtomic(file, f.data);
    bytes += f.data.length;
  }
  const entries = files.map(f => ({ path: f.path, hash: sha256(f.data) })).sort((a, b) => byteOrder(a.path, b.path));
  writeAtomic(path.join(out, MANIFEST), manifestText(nodeVersion, coreSha, entries));

  console.log('export_golden: wrote ' + files.length + ' MIDI files (' + P.KITS.length + ' kits x ' + SLOTS.length + ', ' + bytes +
    ' bytes) into ' + out + (stale.length ? ', removed ' + stale.length + ' stale' : '') + '\n' +
    'export_golden: Node v' + nodeVersion + ', core.js sha256 ' + coreSha + ', manifest ' + path.join(out, MANIFEST));
}

if (require.main === module) {
  try {
    main();
  } catch (e) {
    console.error('export_golden: ' + (e instanceof Refusal ? 'refused: ' + e.message : (e && e.stack) || e));
    process.exit(e instanceof Refusal ? 2 : 1);
  }
}
