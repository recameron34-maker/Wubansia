/* Tests of the legacy audit: node --test tools/legacy_audit/
   - the fixture files are what fixtures/make_fixtures.js writes, and the audit gives their hand-worked
     answers (fixtures/expected.json)
   - the reader follows core/midi/reader.cpp's rules, errors included
   - legacy_sections.json is what core.js says
   - on the 516-file baseline, exported fresh with the pinned Node, the audit gives exactly the
     committed core/tests/legacy_expected.json, twice in a row and whatever order it reads the files in */
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const DIR = path.resolve(__dirname, '..');
const ROOT = path.resolve(DIR, '..', '..');
const FIX = path.join(DIR, 'fixtures');
const smf = require('../smf.js');
const M = require('../model.js');
const X = require('../metrics.js');
const A = require('../audit.js');
const H = X.helpers;

const expected = JSON.parse(fs.readFileSync(path.join(FIX, 'expected.json'), 'utf8'));
const load = name => M.loadSong(fs.readFileSync(path.join(FIX, name)), 'fixtures/' + name);
const pick = (obj, keys) => Object.fromEntries(keys.map(k => [k, obj[k]]));
const pinned = fs.readFileSync(path.join(ROOT, '.nvmrc'), 'utf8').trim().replace(/^v/, '');
const isPinned = process.versions.node === pinned;

test('the committed fixtures are what make_fixtures.js writes', () => {
  const out = fs.mkdtempSync(path.join(os.tmpdir(), 'audit-fixtures-'));
  try {
    execFileSync(process.execPath, [path.join(FIX, 'make_fixtures.js'), '--out', out], { stdio: 'pipe' });
    const made = fs.readdirSync(out).sort();
    const committed = fs.readdirSync(FIX).filter(f => f !== 'make_fixtures.js').sort();
    assert.deepEqual(committed, made);
    for (const f of made) assert.ok(fs.readFileSync(path.join(out, f)).equals(fs.readFileSync(path.join(FIX, f))), f + ' differs: run make_fixtures.js');
  } finally { fs.rmSync(out, { recursive: true, force: true }); }
});

test('expected.json holds integers and strings only', () => {
  assert.doesNotThrow(() => A.assertIntegersOnly(expected, 'expected'));
});

/* ---------- reader ---------- */
test('reader: running status, FIFO pairing, unknown chunks, notes left sounding', () => {
  const e = expected.reader, bytes = fs.readFileSync(path.join(FIX, 'reader.mid'));
  const f = smf.readSmf(bytes);
  assert.equal(f.format, e.format);
  assert.equal(f.ppq, e.ppq);
  assert.equal(f.tracks.length, e.tracks);
  assert.deepEqual(f.tracks.map(t => t.endTick), e.track_end_ticks);
  assert.equal(smf.trackName(f.tracks[1]), e.track1_name);
  assert.deepEqual(smf.notesOf(f.tracks[1]), e.notes);
  assert.deepEqual(smf.markersOf(f), e.markers);
  assert.deepEqual(smf.keySignaturesOf(f).map(k => ({ tick: k.tick, sharps: k.sharps, minor: k.minor ? 1 : 0 })), e.key_signatures);
});

test('reader: refuses what reader.cpp refuses', () => {
  const head = (fmt, n, div) => [0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, fmt, 0, n, div >> 8, div & 255];
  const trk = data => [0x4d, 0x54, 0x72, 0x6b, 0, 0, 0, data.length, ...data];
  const eot = [0x00, 0xff, 0x2f, 0x00];
  const bad = {
    'not MThd': [0x52, 0x49, 0x46, 0x46, 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xe0],
    'format 2': [...head(2, 1, 480), ...trk(eot)],
    'SMPTE division': [...head(0, 1, 0xe250), ...trk(eot)],
    'zero division': [...head(0, 1, 0), ...trk(eot)],
    'format 0 with 2 tracks': [...head(0, 2, 480), ...trk(eot), ...trk(eot)],
    'missing track': [...head(1, 2, 480), ...trk(eot)],
    'no running status': [...head(0, 1, 480), ...trk([0x00, 0x3c, 0x40, ...eot])],
    'variable length over 4 bytes': [...head(0, 1, 480), ...trk([0x81, 0x81, 0x81, 0x81, 0x00, 0x90, 0x3c, 0x40, ...eot])],
    'no end of track': [...head(0, 1, 480), ...trk([0x00, 0x90, 0x3c, 0x40])],
    'data byte above 0x7F': [...head(0, 1, 480), ...trk([0x00, 0x90, 0xbc, 0x40, ...eot])],
    'truncated meta': [...head(0, 1, 480), ...trk([0x00, 0xff, 0x01, 0x05, 0x41])],
    'status 0xF4': [...head(0, 1, 480), ...trk([0x00, 0xf4, ...eot])]
  };
  for (const [what, bytes] of Object.entries(bad)) assert.throws(() => smf.readSmf(Uint8Array.from(bytes)), Error, what);
  // and accepts: bytes after the last track, and running status after a sysex event
  const ok = [...head(0, 1, 480), ...trk([0x00, 0x90, 0x3c, 0x40, 0x00, 0xf0, 0x01, 0xf7, 0x10, 0x3c, 0x00, ...eot]), 1, 2, 3];
  const f = smf.readSmf(Uint8Array.from(ok));
  assert.deepEqual(smf.notesOf(f.tracks[0]), [{ tick: 0, length: 16, channel: 0, key: 60, velocity: 64 }]);
});

test('the audit reads 480 ticks per quarter note only', () => {
  const b = Buffer.from(fs.readFileSync(path.join(FIX, 'lead.mid')));
  b[12] = 0x03; b[13] = 0xc0; // 960
  assert.throws(() => M.loadSong(b, 'x.mid'), /480/);
});

/* ---------- names ---------- */
test('section kinds, lane roles and drum groups', () => {
  for (const c of expected.names.section_kinds) assert.equal(M.sectionKind(c.text), c.kind, c.text);
  for (const c of expected.names.roles) assert.equal(M.roleOf(c.name, c.channel9 === 1, c.slot), c.role, c.name);
  for (const c of expected.names.drum_groups) assert.equal(M.drumGroup(c.key), c.group, String(c.key));
});

test('legacy_sections.json is what core.js says', () => {
  const { derive } = require('../section_table.js');
  assert.equal(fs.readFileSync(path.join(DIR, 'legacy_sections.json'), 'utf8'), derive());
});

/* ---------- integer helpers ---------- */
test('step attribution: the nearest 16th, a tie to the later step', () => {
  assert.equal(M.step16(59), 0);
  assert.equal(M.step16(60), 1);
  assert.equal(M.step16(179), 1);
  assert.equal(M.step16(180), 2);
  assert.equal(M.step16(1859), 15);
  assert.equal(M.step16(1860), 16); // the next bar's downbeat
  assert.equal(M.step16(-60), 0);
  assert.equal(M.step16(-61), -1);
});

test('residuals: nearest swung grid point, a tie keeps the earlier one, over 30 ticks left out', () => {
  // three onsets at 60 mod 240 make the delay -60, so the grid points are 0, 60 and 240 in each 8th
  const notes = [30, 60, 300, 540, 700].map(tick => ({ tick, length: 10, channel: 9, key: 42, velocity: 80 }));
  // 30: 30 from 0 and -30 from 60, a tie: +30. 700: 700 - 480 = 220, nearest 720: -20.
  assert.deepEqual(H.residuals({ sections: [], bars: 1 }, notes), [30, 0, 0, 0, -20]);
  const far = [{ tick: 200, length: 10, channel: 9, key: 42, velocity: 80 }]; // 80 from 120, -40 from 240
  assert.deepEqual(H.residuals({ sections: [], bars: 1 }, far), []);
});

test('rounding and medians', () => {
  assert.equal(H.permille(2, 3), 667);
  assert.equal(H.permille(1, 3), 333);
  assert.equal(H.permille(1, 8), 125); // 125.0 exactly
  assert.equal(H.permille(1, 16), 63); // 62.5 rounds up
  assert.equal(H.permille(0, 5), 0);
  assert.equal(H.permille(1, 0), null);
  assert.equal(H.permilleBig(-63n, 64n), -984); // -984.375
  assert.equal(H.permilleBig(-1n, 16n), -62); // -62.5 rounds up, toward +infinity
  assert.equal(H.permilleBig(1n, 16n), 63);
  assert.equal(H.lowerMedian([5, 1, 3, 2]), 2);
  assert.equal(H.lowerMedian([7]), 7);
  assert.equal(H.lowerMedian([]), null);
  assert.equal(H.sdMilli([70, 90]), 10000);
  assert.equal(H.sdMilli([1, 2, 3, 4]), 1118); // sqrt(1.25) = 1.1180
  assert.equal(H.distinctSwings([10, 12, 13]), 2);
  assert.equal(H.distinctSwings([10, 11, 19, 24, 31]), 4);
  assert.equal(H.distinctSwings([]), 0);
});

/* ---------- gating metrics on the fixtures ---------- */
test('G1 progression shapes', () => {
  const e = expected.g1, songs = e.files.map(load), r = X.g1(songs);
  for (const s of songs) assert.deepEqual(r.per_file[s.path], { break: e.shapes[s.path.slice(9)].break, drop: e.shapes[s.path.slice(9)].drop }, s.path);
  for (const k of ['break', 'drop']) assert.deepEqual(pick(r[k], Object.keys(e[k])), e[k], k);
  assert.equal(r.pass, e.pass);
});

test('G2 harmonic rhythm', () => {
  const e = expected.g2, songs = e.files.map(load), r = X.g2(songs);
  for (const s of songs) assert.equal(r.per_file[s.path], e.patterns[s.path.slice(9)], s.path);
  assert.deepEqual(pick(r, ['seeds', 'measured', 'four_by_one_bar', 'four_by_one_bar_pm', 'changes_on_bar_lines_only', 'pass']),
    pick(e, ['seeds', 'measured', 'four_by_one_bar', 'four_by_one_bar_pm', 'changes_on_bar_lines_only', 'pass']));
  assert.equal(r.patterns, e.patterns_count);
});

test('G3 one-bar repetition', () => {
  const e = expected.g3, r = X.g3([load(e.file)]);
  assert.deepEqual(pick(r, ['drum_windows', 'drum_repeated', 'kick_4x4_exempt', 'bass_windows', 'bass_repeated', 'drum_repeated_pm', 'bass_repeated_pm', 'pass']),
    pick(e, ['drum_windows', 'drum_repeated', 'kick_4x4_exempt', 'bass_windows', 'bass_repeated', 'drum_repeated_pm', 'bass_repeated_pm', 'pass']));
});

test('G4 macro changes', () => {
  const e = expected.g4, s = load(e.file), r = X.g4([s]);
  for (const role of ['bass', 'chords', 'lead']) assert.deepEqual(r[role], e[role], role);
  const bars = X.macroChangeBars(s);
  for (const role of ['bass', 'chords', 'lead']) assert.deepEqual(bars[role].map(c => c.bar), e.change_bars[role], role);
  assert.equal(r.pass, e.pass);
});

test('G4 does not count an 8-bar loop as changes', () => {
  const e = expected.g4_loop8, s = load(e.file), r = X.g4([s]);
  assert.deepEqual(pick(r.lead, ['changes', 'on_phrase']), e.lead);
  assert.deepEqual(X.macroChangeBars(s).lead.map(c => c.bar), e.lead_change_bars);
});

test('G5 velocity resolution', () => {
  const e = expected.g5, r = X.g5([load(e.file)]);
  for (const g of ['hats', 'perc', 'ghosts', 'bass', 'lead']) assert.deepEqual(r[g], e[g], g);
  assert.equal(r.pass, e.pass);
});

test('G6 chord stacks', () => {
  const e = expected.g6, r = X.g6([load(e.file)]);
  assert.deepEqual(pick(r, Object.keys(e).filter(k => k !== 'file')), pick(e, Object.keys(e).filter(k => k !== 'file')));
});

test('G7 microtiming: SD, lag-1 autocorrelation, DFA alpha', () => {
  const e = expected.g7, s = load(e.file), r = X.g7([s]);
  assert.deepEqual(r.per_file[s.path], { hats: e.hats, perc: e.perc });
  assert.equal(r.pass, e.pass);
});

test('G7 lag 1 with a residual mean that is not 0', () => {
  const e = expected.g7_mean, s = load(e.file), r = X.g7([s]);
  assert.deepEqual(r.per_file[s.path].hats, e.hats);
});

test('G7 DFA on known series: white noise near 0.5, a random walk near 1.5', () => {
  let seed = 12345;
  const rnd = () => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 16) % 9 - 4; }; // integers -4..4
  const white = Array.from({ length: 4096 }, rnd);
  const walk = []; let acc = 0; for (let i = 0; i < 4096; i++) { acc += rnd(); walk.push(acc); }
  const aw = H.dfaAlphaMilli(white), ar = H.dfaAlphaMilli(walk);
  assert.ok(aw > 400 && aw < 600, 'white noise alpha ' + aw);
  assert.ok(ar > 1350 && ar < 1650, 'random walk alpha ' + ar);
  assert.ok(Math.abs(H.lag1Permille(white)) < 60, 'white noise lag 1');
  assert.equal(H.dfaAlphaMilli(Array(100).fill(3)), null); // a constant series has no fluctuation
  assert.equal(H.dfaAlphaMilli(Array(63).fill(1)), null); // under 64 values
});

test('the gating bands of G3, G5, G6 and G7 hold at every edge', () => {
  const b = expected.bands, args = (c, keys) => keys.map(k => c[k]);
  for (const c of b.g3) assert.equal(X.bands.g3(...args(c, ['drum_windows', 'drum_repeated', 'bass_windows', 'bass_repeated'])) ? 1 : 0, c.pass, JSON.stringify(c));
  for (const c of b.g5_drums) assert.equal(X.bands.g5Drums(...args(c, ['windows', 'median_distinct', 'median_sd_milli'])) ? 1 : 0, c.pass, JSON.stringify(c));
  for (const c of b.g5_melodic) assert.equal(X.bands.g5Melodic(...args(c, ['windows', 'median_distinct', 'median_sd_milli'])) ? 1 : 0, c.pass, JSON.stringify(c));
  for (const c of b.g6) assert.equal(X.bands.g6(...args(c, ['stacks', 'two_velocities', 'max_spread_ticks'])) ? 1 : 0, c.pass, JSON.stringify(c));
  for (const c of b.g7) assert.equal(X.bands.g7(...args(c, ['files', 'median_sd_milli', 'median_lag1_pm', 'median_alpha_milli'])) ? 1 : 0, c.pass, JSON.stringify(c));
});

test('G8 per-lane swing', () => {
  const e = expected.g8, s = load(e.file), r = X.g8([s]);
  assert.deepEqual(r.per_file[s.path], { lanes: e.lanes, distinct: e.distinct });
  assert.equal(r.pass, e.pass);
});

test('G9 fills and boundary gestures', () => {
  const e = expected.g9, s = load(e.file), r = X.g9([s]);
  assert.deepEqual(pick(r, ['boundaries', 'with_gesture', 'with_gesture_pm', 'fills', 'position_histogram', 'top_position', 'top_position_pm', 'songs_with_2_fills', 'median_distinct_pm', 'pass']),
    pick(e, ['boundaries', 'with_gesture', 'with_gesture_pm', 'fills', 'position_histogram', 'top_position', 'top_position_pm', 'songs_with_2_fills', 'median_distinct_pm', 'pass']));
  assert.equal(r.per_file[s.path].missing_gesture_bars, e.missing_gesture_bars);
  assert.equal(r.per_file[s.path].fill_positions, e.fill_positions);
});

test('G10 form diversity and change intervals', () => {
  const e = expected.g10, songs = e.files.map(load), r = X.g10(songs);
  for (const s of songs) assert.deepEqual(r.per_file[s.path], { form: e.forms[s.path.slice(9)], interval_values: e.interval_values[s.path.slice(9)] }, s.path);
  assert.deepEqual(pick(r, ['distinct_forms', 'top_form_count', 'top_form_pm', 'every_song_two_interval_values', 'pass']),
    pick(e, ['distinct_forms', 'top_form_count', 'top_form_pm', 'every_song_two_interval_values', 'pass']));
  const m = X.g10([load(expected.g10_intervals.file)]);
  assert.equal(m.per_file['fixtures/' + expected.g10_intervals.file].interval_values, expected.g10_intervals.interval_values);
});

/* ---------- reported metrics and invariants on the fixtures ---------- */
test('R3 hat accent period', () => {
  const e = expected.r3;
  assert.deepEqual(X.rHatAccent([load(e.file)]), pick(e, ['bars', 'period_4_or_less', 'period_4_or_less_pm', 'period_histogram']));
});
test('R4 gate rule', () => {
  const e = expected.r4;
  assert.deepEqual(X.rGateRule([load(e.file)]), pick(e, ['notes', 'gate_rule', 'gate_rule_pm']));
});
test('R5 chord tones', () => {
  const e = expected.r5, r = X.rChordTones([load(e.file)]);
  for (const role of ['lead', 'bass']) assert.deepEqual(pick(r[role], ['strong', 'strong_chord_tones', 'weak', 'weak_chord_tones']), e[role], role);
});
test('R5 before the first chord, the harmony is the last one', () => {
  const e = expected.r5_wrap, r = X.rChordTones([load(e.file)]);
  assert.deepEqual(pick(r.lead, ['strong', 'strong_chord_tones', 'weak', 'weak_chord_tones']), e.lead);
});
test('R7 lead shape', () => {
  const e = expected.r7;
  assert.deepEqual(X.rLeadShape([load(e.file)]), pick(e, ['intervals', 'steps', 'step_pm', 'repeats_pm', 'windows', 'bar1_eq_bar3', 'bar1_eq_bar3_pm']));
});
test('R8 voicing texture', () => {
  const e = expected.r8, r = X.rVoicing([load(e.file)]);
  assert.deepEqual({ stacks: r.stacks, types: r.median_types_per_song, four_plus_pm: r.four_plus_pm, recurring: r.recurring, revoiced: r.revoiced, revoiced_pm: r.revoiced_pm, type_histogram: r.type_histogram },
    pick(e, ['stacks', 'types', 'four_plus_pm', 'recurring', 'revoiced', 'revoiced_pm', 'type_histogram']));
});
test('R9 odd cycles', () => {
  const e = expected.r9;
  assert.deepEqual(X.rOddCycles([load(e.file)]), pick(e, ['lane_windows', 'odd_cycles', 'odd_cycles_pm']));
});
test('R10 cross-seed duplicate motifs', () => {
  const e = expected.r10;
  assert.deepEqual(X.rDuplicates(e.files.map(load), 'lead'), pick(e, ['windows', 'duplicated', 'duplicated_pm']));
});
test('R11 LHL syncopation', () => {
  for (const c of expected.r11.cases) assert.equal(X.lhl(c.onsets.split(',').map(Number)), c.value, c.onsets);
});
test('I1 in key', () => {
  const e = expected.i1, r = X.iInKey([load(e.file)]);
  for (const role of ['lead', 'bass', 'chords']) assert.deepEqual(pick(r[role], ['notes', 'in_key']), e[role], role);
});
test('I2 risers and rolls on the swung grid', () => {
  const e = expected.i2, r = X.iSwungGrid([load(e.file)]);
  for (const k of ['riser', 'build_snare']) assert.deepEqual(pick(r[k], ['notes', 'swung', 'straight_only']), e[k], k);
});
test('I3 kick and bass within 2 ticks, I5 kick anchors', () => {
  const s = load(expected.i3.file);
  assert.deepEqual(pick(X.iKickBass([s]), ['pairs', 'within_2_ticks']), pick(expected.i3, ['pairs', 'within_2_ticks']));
  assert.deepEqual(pick(X.iAnchors([s]), ['kicks_near_beats', 'within_2_ticks']), pick(expected.i5, ['kicks_near_beats', 'within_2_ticks']));
});
test('I4 sections on 8-bar lines', () => {
  const e = expected.i4;
  assert.deepEqual(pick(X.iSections([load(e.file)]), ['sections', 'on_8_bar_lines']), pick(e, ['sections', 'on_8_bar_lines']));
});

/* ---------- the legacy baseline ---------- */
test('on the 516-file baseline the audit gives the committed legacy_expected.json, every time', { skip: isPinned ? false : 'needs Node ' + pinned + ' (.nvmrc), this is ' + process.versions.node }, () => {
  const out = fs.mkdtempSync(path.join(os.tmpdir(), 'audit-golden-'));
  try {
    execFileSync(process.execPath, [path.join(ROOT, 'tools', 'export_golden.js'), '--out', out], { stdio: 'pipe' });
    const base = A.loadBaseline(out);
    const first = A.toText(A.audit(base, pinned));
    assert.equal(first, fs.readFileSync(path.join(ROOT, 'core', 'tests', 'legacy_expected.json'), 'utf8'), 'fix the metric code, never legacy_expected.json');
    const reversed = Object.assign({}, base, { files: base.files.slice().reverse() });
    assert.equal(A.toText(A.audit(reversed, pinned)), first, 'the result depends on the order the files are read in');
    // a changed file is refused before anything is measured
    const one = path.join(out, 'UKG01', 'UKG01_Bass-A.mid');
    const b = fs.readFileSync(one); b[b.length - 5] ^= 1; fs.writeFileSync(one, b);
    assert.throws(() => A.loadBaseline(out), /differs from the committed manifest/);
  } finally { fs.rmSync(out, { recursive: true, force: true }); }
});
