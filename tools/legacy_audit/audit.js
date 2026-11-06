#!/usr/bin/env node
/* The legacy audit: the generator metrics of docs/plans/S1-futuristic-slice.md section 5.1, measured
   on the 516-file legacy baseline (tools/export_golden.js), with the definitions in README.md. Its
   output is core/tests/legacy_expected.json, which the C++ metrics (core/metrics) must reproduce: the
   test of the test.

   Usage:
     node tools/legacy_audit/audit.js                 print a summary (reads build/golden/)
     node tools/legacy_audit/audit.js --check         exit 1 unless the result equals the committed
                                                      core/tests/legacy_expected.json byte for byte
     node tools/legacy_audit/audit.js --json          print the full result
     node tools/legacy_audit/audit.js --write <file>  write the full result to <file>
     --golden <dir>                                   read the baseline from <dir> instead

   Run tools/export_golden.js first. Every file the committed manifest lists is checked against its
   SHA-256 before anything is measured, so the result always describes the committed baseline.

   legacy_expected.json is treated like the golden manifest: never regenerated to make a test pass
   (CLAUDE.md, "Determinism and golden files"). If the C++ metrics disagree with it, fix the metric code.
   It changes only when a definition in README.md changes, in its own commit, with the reason.

   Node is pinned (.nvmrc): the only floating-point library call is Math.log in the DFA exponent (G7),
   and the reader, the integer metrics and the rounding are version-independent, but one pinned version
   keeps the file byte-stable. Exit status: 0 done, 1 error or --check difference, 2 refused. */
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const M = require('./model.js');
const X = require('./metrics.js');

const ROOT = path.resolve(__dirname, '..', '..');
const NVMRC = path.join(ROOT, '.nvmrc');
const MANIFEST = path.join(ROOT, 'content', 'golden', 'legacy-v21.sha256');
const EXPECTED = path.join(ROOT, 'core', 'tests', 'legacy_expected.json');
const DEFAULT_GOLDEN = path.join(ROOT, 'build', 'golden');

class Refusal extends Error {}

function pinnedNode() {
  const want = fs.readFileSync(NVMRC, 'utf8').trim().replace(/^v/, '');
  if (process.versions.node !== want) {
    throw new Refusal('this is Node ' + process.versions.node + ', but .nvmrc pins ' + want + '. Install it (nvm install) and run again.');
  }
  return want;
}

function parseArgs(argv) {
  const o = { golden: DEFAULT_GOLDEN, mode: 'summary', write: null };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--golden' && i + 1 < argv.length) o.golden = path.resolve(argv[++i]);
    else if (a === '--check') o.mode = 'check';
    else if (a === '--json') o.mode = 'json';
    else if (a === '--write' && i + 1 < argv.length) { o.mode = 'write'; o.write = path.resolve(argv[++i]); }
    else if (a === '-h' || a === '--help') { console.log('Usage: node tools/legacy_audit/audit.js [--golden <dir>] [--check | --json | --write <file>]'); process.exit(0); }
    else throw new Refusal('unknown argument ' + JSON.stringify(a));
  }
  return o;
}

const sha256 = data => crypto.createHash('sha256').update(data).digest('hex');

// The committed manifest's entries, checked against the files in the golden folder.
function loadBaseline(goldenDir) {
  const text = fs.readFileSync(MANIFEST, 'utf8');
  const header = {};
  const entries = [];
  for (const line of text.split('\n')) {
    let m;
    if ((m = /^# node (v\S+)$/.exec(line))) header.node = m[1];
    else if ((m = /^# core\.js sha256 ([0-9a-f]{64})$/.exec(line))) header.core = m[1];
    else if ((m = /^([0-9a-f]{64}) {2}(\S+)$/.exec(line))) entries.push({ hash: m[1], path: m[2] });
  }
  if (entries.length !== 516) throw new Error('the manifest lists ' + entries.length + ' files, expected 516');
  const files = [];
  for (const e of entries) {
    const file = path.join(goldenDir, ...e.path.split('/'));
    let data;
    try { data = fs.readFileSync(file); } catch (err) { throw new Refusal('missing ' + file + ': run node tools/export_golden.js first'); }
    if (sha256(data) !== e.hash) throw new Refusal(file + ' differs from the committed manifest: export the baseline again with the pinned Node');
    files.push({ path: e.path, data });
  }
  return { manifestSha: sha256(text), header, files };
}

// Every number in the result must be an integer: legacy_expected.json is read by core/'s
// integer-only JSON reader. No booleans or nulls either.
function assertIntegersOnly(v, where) {
  if (typeof v === 'number') { if (!Number.isSafeInteger(v)) throw new Error('not an integer at ' + where + ': ' + v); return; }
  if (typeof v === 'string') return;
  if (Array.isArray(v)) { v.forEach((x, i) => assertIntegersOnly(x, where + '[' + i + ']')); return; }
  if (v && typeof v === 'object') { for (const k of Object.keys(v)) assertIntegersOnly(v[k], where + '.' + k); return; }
  throw new Error('not an integer or string at ' + where + ': ' + v);
}

const BANDS = {
  G1: 'progression shapes of the first break and the first drop: distinct >= 50% of seeds, top shape <= 10%, each',
  G2: 'harmonic rhythm of the first 4 drop bars: "4 x 1-bar slots" (2,2,2,2 half-bars) <= 30% of seeds, >= 4 patterns',
  G3: 'one-bar repetition in 4-bar windows: drum lanes <= 40% (four-on-the-floor kick windows exempt), bass <= 30%',
  G4: 'macro changes on phrase-start or phrase-end bars (section bar mod 8 in {0, 7}): >= 50% for bass, chords and lead',
  G5: 'median per 8-bar window: hats, perc, ghosts >= 10 velocity values with SD 6-22; bass and lead >= 6 values with SD >= 8',
  G6: 'chord stacks with >= 2 velocities >= 70%; stack onset spread 0-4 ticks',
  G7: 'feel lanes (hats, perc), median over seeds: residual SD 1.5-6 ticks, lag-1 autocorrelation >= 0.2, DFA alpha 0.65-1.1',
  G8: '>= 3 distinct swings (2-tick tolerance) among the non-anchor lanes of the first drop, in every seed',
  G9: '100% of section boundaries carry a gesture; no fill position > 60%; median distinct fills per song >= 70%',
  G10: '>= 8 distinct form shapes, top form <= 25%, intervals between salient changes take >= 2 values in every seed'
};
// The file set whose result matches the plan's "Legacy (audit)" column, and the other sets measured.
const GATES = {
  G1: { primary: 'FullSong', sets: ['FullSong', 'ExtendedMix'], run: X.g1 },
  G2: { primary: 'FullSong', sets: ['FullSong', 'Chords-A', 'ExtendedMix'], run: X.g2 },
  G3: { primary: 'parts', sets: ['parts', 'FullSong'], run: X.g3 },
  G4: { primary: 'ExtendedMix', sets: ['ExtendedMix', 'FullSong'], run: X.g4 },
  G5: { primary: 'FullSong', sets: ['FullSong', 'ExtendedMix'], run: X.g5 },
  G6: { primary: 'chord_parts', sets: ['chord_parts', 'FullSong', 'ExtendedMix'], run: X.g6 },
  G7: { primary: 'ExtendedMix', sets: ['ExtendedMix', 'FullSong'], run: X.g7 },
  G8: { primary: 'FullSong', sets: ['FullSong', 'ExtendedMix'], run: X.g8 },
  G9: { primary: 'FullSong', sets: ['FullSong', 'ExtendedMix'], run: X.g9 },
  G10: { primary: 'FullSong', sets: ['FullSong', 'ExtendedMix'], run: X.g10 }
};
const SET_SLOTS = {
  parts: /^(Drums-Main|Drums-Variation|Bass-A|Bass-B)$/,
  chord_parts: /^Chords-[AB]$/,
  drum_parts: /^Drums-(Main|Variation)$/,
  lead_parts: /^Melody-[AB]$/,
  bass_parts: /^Bass-[AB]$/,
  all_parts: /^(Drums|Bass|Chords|Melody)-/,
  'Chords-A': /^Chords-A$/,
  FullArrangement: /^FullArrangement$/,
  FullSong: /^FullSong$/,
  ExtendedMix: /^ExtendedMix$/
};

function audit(baseline, nodeVersion) {
  // By path, in byte order, so nothing depends on the order the files were read in.
  const songs = baseline.files.map(f => M.loadSong(f.data, f.path)).sort((a, b) => (a.path < b.path ? -1 : a.path > b.path ? 1 : 0));
  const set = name => songs.filter(s => SET_SLOTS[name].test(s.slot));
  const notes = songs.reduce((a, s) => a + s.tracks.reduce((b, t) => b + t.notes.length, 0), 0);

  const gating = {};
  for (const [g, def] of Object.entries(GATES)) {
    const out = { band: BANDS[g], primary: def.primary };
    for (const name of def.sets) out[name] = def.run(set(name));
    out.legacy_fails = out[def.primary].pass ? 0 : 1;
    gating[g] = out;
  }
  gating.G5.part_levels = X.partLevels(set('all_parts'));

  const FS = set('FullSong'), EM = set('ExtendedMix');
  const reported = {
    R1_repetition_lag4: { band: 'kick 0.70-0.95, chords 0.50-0.85, bass 0.40-0.80, lead 0.30-0.70', FullSong: X.rRepetition(FS), ExtendedMix: X.rRepetition(EM) },
    R2_loop_tiling: { FullSong: X.rLoopTiling(FS), ExtendedMix: X.rLoopTiling(EM) },
    R3_hat_accent_period: { band: 'period <= 4 steps in <= 40% of bars', drum_parts: X.rHatAccent(set('drum_parts')), FullSong: X.rHatAccent(FS), ExtendedMix: X.rHatAccent(EM) },
    R4_gate_rule: { band: '<= 50% of melodic notes', FullArrangement: X.rGateRule(set('FullArrangement')), FullSong: X.rGateRule(FS), ExtendedMix: X.rGateRule(EM) },
    R5_chord_tones: { FullArrangement: X.rChordTones(set('FullArrangement')), FullSong: X.rChordTones(FS), ExtendedMix: X.rChordTones(EM) },
    R6_breath_complementarity: { lead_parts: X.rBreath(set('lead_parts')), ExtendedMix: X.rBreath(EM) },
    R7_lead_shape: { band: 'RIFF: step ratio 0.2-0.6, bar 1 = bar 3 rhythm 0.6-0.9; SENTENCE: step ratio 0.5-0.8, bar 1 = bar 3 <= 0.6', lead_parts: X.rLeadShape(set('lead_parts')), ExtendedMix: X.rLeadShape(EM) },
    R8_voicing_texture: { band: '>= 3 types per song, 4+-note voicings <= 60% of stacks, >= 25% of recurring chords re-voiced', FullSong: X.rVoicing(FS), ExtendedMix: X.rVoicing(EM) },
    R9_odd_cycles: { FullSong: X.rOddCycles(FS), ExtendedMix: X.rOddCycles(EM) },
    R10_cross_seed_duplicate_motifs: { band: '<= 3% of 2-bar motifs', lead_parts: X.rDuplicates(set('lead_parts'), 'lead'), bass_parts: X.rDuplicates(set('bass_parts'), 'bass'), ExtendedMix_lead: X.rDuplicates(EM, 'lead') },
    R11_syncopation_lhl: { FullSong: X.rSyncopation(FS), ExtendedMix: X.rSyncopation(EM) }
  };
  const invariants = {
    I1_in_key: { FullSong: X.iInKey(FS), ExtendedMix: X.iInKey(EM) },
    I2_risers_and_rolls_on_swung_grid: { FullSong: X.iSwungGrid(FS), ExtendedMix: X.iSwungGrid(EM) },
    I3_kick_bass_within_2_ticks: { FullSong: X.iKickBass(FS), ExtendedMix: X.iKickBass(EM) },
    I4_sections_on_8_bar_lines: { FullSong: X.iSections(FS), ExtendedMix: X.iSections(EM) },
    I5_kick_anchors_within_2_ticks: { FullSong: X.iAnchors(FS), ExtendedMix: X.iAnchors(EM) }
  };

  const result = {
    about: 'Legacy audit of the 516-file MIDI baseline (tools/legacy_audit/audit.js; definitions in tools/legacy_audit/README.md). Integers and strings only. Never regenerate it to make a test pass.',
    format: 1,
    node: 'v' + nodeVersion,
    manifest: 'content/golden/legacy-v21.sha256',
    manifest_sha256: baseline.manifestSha,
    manifest_node: baseline.header.node || '',
    core_js_sha256: baseline.header.core || '',
    files: songs.length,
    notes,
    sets: Object.fromEntries(Object.keys(SET_SLOTS).map(n => [n, set(n).length])),
    section_kinds: require('./legacy_sections.json').kinds,
    gating,
    reported,
    invariants
  };
  assertIntegersOnly(result, 'result');
  return result;
}

const toText = result => JSON.stringify(result, null, 1) + '\n';

function summary(r) {
  const lines = ['legacy audit: ' + r.files + ' files, ' + r.notes + ' notes, manifest ' + r.manifest_sha256.slice(0, 12) + ', Node ' + r.node, ''];
  const g = r.gating;
  const p = (pm, unit) => (pm === null || pm === undefined || pm < 0 ? '-' : (pm / 10).toFixed(1) + (unit || '%'));
  const row = (id, text) => lines.push(id.padEnd(4) + (g[id].legacy_fails ? 'legacy fails  ' : 'legacy PASSES ') + text);
  row('G1', 'break: top ' + g.G1.FullSong.break.top + '/' + g.G1.FullSong.break.measured + ' (' + p(g.G1.FullSong.break.top_pm) + '), distinct ' + g.G1.FullSong.break.distinct + '; drop: top ' + g.G1.FullSong.drop.top + ', distinct ' + g.G1.FullSong.drop.distinct);
  row('G2', '4 x 1-bar ' + g.G2.FullSong.four_by_one_bar + '/' + g.G2.FullSong.measured + ' (' + p(g.G2.FullSong.four_by_one_bar_pm) + '), ' + g.G2.FullSong.patterns + ' patterns');
  row('G3', 'drums ' + p(g.G3.parts.drum_repeated_pm) + ' (' + g.G3.parts.drum_repeated + '/' + g.G3.parts.drum_windows + '), bass ' + p(g.G3.parts.bass_repeated_pm) + ' (' + g.G3.parts.bass_repeated + '/' + g.G3.parts.bass_windows + ')');
  row('G4', 'on phrase: bass ' + p(g.G4.ExtendedMix.bass.on_phrase_pm) + ', chords ' + p(g.G4.ExtendedMix.chords.on_phrase_pm) + ', lead ' + p(g.G4.ExtendedMix.lead.on_phrase_pm) + ' (FullSong ' + p(g.G4.FullSong.bass.on_phrase_pm) + ' / ' + p(g.G4.FullSong.chords.on_phrase_pm) + ' / ' + p(g.G4.FullSong.lead.on_phrase_pm) + ')');
  const g5 = g.G5.FullSong;
  row('G5', 'median distinct: hats ' + g5.hats.median_distinct + ', perc ' + g5.perc.median_distinct + ', ghosts ' + g5.ghosts.median_distinct + ', bass ' + g5.bass.median_distinct + ', lead ' + g5.lead.median_distinct);
  row('G6', p(g.G6.chord_parts.two_velocities_pm) + ' of ' + g.G6.chord_parts.stacks + ' stacks with 2+ velocities');
  const g7 = g.G7.ExtendedMix;
  row('G7', 'hats: SD ' + (g7.hats.median_sd_milli / 1000).toFixed(3) + ', lag-1 ' + (g7.hats.median_lag1_pm / 1000).toFixed(3) + ', alpha ' + (g7.hats.median_alpha_milli / 1000).toFixed(3) + '; perc: SD ' + (g7.perc.median_sd_milli / 1000).toFixed(3) + ', lag-1 ' + (g7.perc.median_lag1_pm / 1000).toFixed(3) + ', alpha ' + (g7.perc.median_alpha_milli / 1000).toFixed(3));
  row('G8', g.G8.FullSong.with_3_or_more + '/' + g.G8.FullSong.measured + ' seeds with 3+ swings');
  row('G9', 'gestures ' + g.G9.FullSong.with_gesture + '/' + g.G9.FullSong.boundaries + ', top position ' + g.G9.FullSong.top_position + ' (' + p(g.G9.FullSong.top_position_pm) + '), median distinct ' + p(g.G9.FullSong.median_distinct_pm));
  row('G10', g.G10.FullSong.distinct_forms + ' forms, top ' + g.G10.FullSong.top_form_count + '/' + g.G10.FullSong.measured + ' (ExtendedMix: ' + g.G10.ExtendedMix.distinct_forms + ' forms)');
  return lines.join('\n') + '\n';
}

function main() {
  const opt = parseArgs(process.argv.slice(2));
  const nodeVersion = pinnedNode();
  const result = audit(loadBaseline(opt.golden), nodeVersion);
  const text = toText(result);
  if (opt.mode === 'json') process.stdout.write(text);
  else if (opt.mode === 'write') { fs.writeFileSync(opt.write, text); console.log('legacy audit: wrote ' + opt.write + ' (' + text.length + ' bytes)'); }
  else if (opt.mode === 'check') {
    const committed = fs.readFileSync(EXPECTED, 'utf8');
    if (committed !== text) {
      const a = JSON.parse(committed), keys = new Set([...Object.keys(a), ...Object.keys(result)]);
      const diff = [...keys].filter(k => JSON.stringify(a[k]) !== JSON.stringify(result[k]));
      console.error('legacy audit: the result differs from ' + path.relative(ROOT, EXPECTED) + ' in: ' + diff.join(', ') + '\nFix the metric code, never the expected file (README.md).');
      process.exit(1);
    }
    console.log('legacy audit: matches ' + path.relative(ROOT, EXPECTED));
  } else process.stdout.write(summary(result));
}

if (require.main === module) {
  try { main(); } catch (e) {
    console.error('legacy audit: ' + (e instanceof Refusal ? 'refused: ' + e.message : (e && e.stack) || e));
    process.exit(e instanceof Refusal ? 2 : 1);
  }
}

module.exports = { audit, loadBaseline, toText, assertIntegersOnly, summary, GATES, BANDS };
