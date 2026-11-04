#!/usr/bin/env node
/* Writes the audit's fixture MIDI files and their known answers (fixtures/expected.json).

   Every answer below is worked out by hand from the notes written here, with the reasoning in the
   comments, never by running the audit. tools/legacy_audit/test/audit.test.js checks that the audit gives
   these answers, and that this script still writes exactly the committed files. The C++ metrics
   (core/metrics) can check the same answers: expected.json holds integers and strings only.

   Usage: node tools/legacy_audit/fixtures/make_fixtures.js [--out <dir>]   (default: this folder)

   The files are written by the tiny SMF writer below, not by the audit's reader or core/midi, so a
   reader bug can't cancel itself out. 480 ticks per quarter note, 4/4; a bar is 1920 ticks, a 16th 120. */
'use strict';

const fs = require('fs');
const path = require('path');

const BAR = 1920, S = 120; // S: one 16th

/* ---------- a minimal SMF writer ---------- */
function vlq(n) {
  const out = [n & 0x7f];
  while ((n = Math.floor(n / 128)) > 0) out.unshift((n & 0x7f) | 0x80);
  return out;
}
const ascii = s => Array.from(Buffer.from(s, 'utf8'));
const be32 = n => [(n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255];
const meta = (type, data) => [0xff, type, ...vlq(data.length), ...data];
// events: [tick, order, bytes]; same tick sorts by order (note-offs 1 before note-ons 2).
function chunk(events, endTick) {
  const ev = events.slice().sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const data = [];
  let last = 0;
  for (const [t, , bytes] of ev) { data.push(...vlq(t - last), ...bytes); last = t; }
  data.push(...vlq(Math.max(0, endTick - last)), 0xff, 0x2f, 0x00);
  return [...ascii('MTrk'), ...be32(data.length), ...data];
}
// notes: [tick, length, key, velocity]
function noteEvents(ch, notes) {
  const out = [];
  for (const [t, len, key, vel] of notes) {
    out.push([t, 2, [0x90 | ch, key, vel]]);
    out.push([t + len, 1, [0x80 | ch, key, 0x40]]);
  }
  return out;
}
// A type 1 song: a conductor (name, 134 BPM, 4/4, key, markers [bar, text]) and one track per lane.
function song({ bars, markers = [], key = [-3, 1], tracks }) {
  const end = bars * BAR;
  const cond = [[0, 0, meta(0x03, ascii('fixture'))], [0, 0, meta(0x51, [0x06, 0xd5, 0x11])], [0, 0, meta(0x58, [4, 2, 24, 8])],
    [0, 0, meta(0x59, [key[0] & 255, key[1]])], ...markers.map(([bar, text]) => [bar * BAR, 0, meta(0x06, ascii(text))])];
  const chunks = [chunk(cond, end)];
  for (const tr of tracks) chunks.push(chunk([[0, 0, meta(0x03, ascii(tr.name))], ...noteEvents(tr.ch, tr.notes)], end));
  return Buffer.from([...ascii('MThd'), ...be32(6), 0, 1, 0, chunks.length, 0x01, 0xe0, ...chunks.flat()]);
}
const range = (n, f) => Array.from({ length: n }, (_, i) => f(i));
const flat = a => a.flat();

const files = {};
const expected = {};

/* ---------- reader.mid: the reader's rules, in hand-written bytes ---------- */
// Track 0: name, tempo, key signature C minor (sf -3, minor), markers "drop" at 0 and "break" at 1920,
// end of track at 3840. Then an unknown chunk "XFIH", which the reader must skip. Track 1 ("Bass"):
//   t=0     90 24 64      note-on key 36 vel 100
//   t=0        26 50      running status: note-on key 38 vel 80
//   t=120   FF 01 03 abc  a text event; running status carries on after it
//   t=120      24 00      running status 90 with velocity 0: ends key 36 -> 36 is 120 long
//   t=240   90 24 70      key 36 vel 112
//   t=360      24 71      key 36 vel 113: two notes on one key now
//   t=480   80 24 40      note-off 36: ends the oldest (t=240), 240 long
//   t=600   80 24 40      ends the t=360 one, 240 long
//   t=700   80 30 00      note-off for key 48, which isn't sounding: ignored
//   t=720   91 30 60      channel 1 key 48 vel 96, never ended: lasts to the track's end, 1920 - 720 = 1200
//   t=960   80 26 00      ends key 38: 960 long
//   end of track at 1920
{
  const t0 = [
    0x00, ...meta(0x03, ascii('Conductor')), 0x00, ...meta(0x51, [0x07, 0xa1, 0x20]), 0x00, ...meta(0x59, [0xfd, 0x01]),
    0x00, ...meta(0x06, ascii('drop')), ...vlq(1920), ...meta(0x06, ascii('break')), ...vlq(1920), 0xff, 0x2f, 0x00
  ];
  const t1 = [
    0x00, ...meta(0x03, ascii('Bass')),
    0x00, 0x90, 36, 100, 0x00, 38, 80,
    ...vlq(120), 0xff, 0x01, 0x03, ...ascii('abc'), 0x00, 36, 0,
    ...vlq(120), 0x90, 36, 112, ...vlq(120), 36, 113,
    ...vlq(120), 0x80, 36, 0x40, ...vlq(120), 0x80, 36, 0x40,
    ...vlq(100), 0x80, 48, 0, ...vlq(20), 0x91, 48, 96,
    ...vlq(240), 0x80, 38, 0, ...vlq(960), 0xff, 0x2f, 0x00
  ];
  const mk = d => [...ascii('MTrk'), ...be32(d.length), ...d];
  files['reader.mid'] = Buffer.from([...ascii('MThd'), ...be32(6), 0, 1, 0, 2, 0x01, 0xe0, ...mk(t0), ...ascii('XFIH'), ...be32(4), 1, 2, 3, 4, ...mk(t1)]);
  expected.reader = {
    format: 1, ppq: 480, tracks: 2, track_end_ticks: [3840, 1920], track1_name: 'Bass',
    notes: [
      { tick: 0, length: 120, channel: 0, key: 36, velocity: 100 },
      { tick: 0, length: 960, channel: 0, key: 38, velocity: 80 },
      { tick: 240, length: 240, channel: 0, key: 36, velocity: 112 },
      { tick: 360, length: 240, channel: 0, key: 36, velocity: 113 },
      { tick: 720, length: 1200, channel: 1, key: 48, velocity: 96 }
    ],
    markers: [{ tick: 0, text: 'drop' }, { tick: 1920, text: 'break' }],
    key_signatures: [{ tick: 0, sharps: -3, minor: 1 }]
  };
}

/* ---------- harmony_a/b/c.mid: G1 progression shapes, G2 harmonic rhythm ---------- */
// Each: intro (bars 0-7), drop (8-15), break (16-23), outro (24-31). Chords lanes: "Stabs" and "Pad".
const HM = [[0, 'intro'], [8, 'drop'], [16, 'break'], [24, 'outro']];
const chordAt = (t, len, keys, vel) => keys.map(k => [t, len, k, vel]);
{
  // Song A, drop: stabs on steps 2 and 10 of each bar, and the bar's chord again on step 14, 240
  // ticks before the next bar. Bars 8-11: Cm (60 63 67), Ab (56 60 63), Fm (53 56 60), G (55 59 62),
  // then the same 4 bars again. A Bb stab (58 62 65) at tick 15120, 240 before the drop.
  // Slot 0 (tick 15360) looks at [15110, 16070): Bb at 15120 and Cm at 15600 are both 240 away; the tie
  // goes to the later, Cm. Slot 1 (16320): Cm at 16560. Slot 2 (17280): Cm at 17040 (step 14) and Ab at
  // 17520, both 240 away: Ab. So the slots are Cm Cm Ab Ab Fm Fm G G, twice.
  // Relative to slot 0's lowest key, 60 (pitch class 0): Cm {0,3,7} = 0x089, Ab {8,0,3} = 0x109,
  // Fm {5,8,0} = 0x121, G {7,11,2} = 0x884. G2 runs: 2,2,2,2.
  // Break: Pad, a chord every 2 bars on the bar line, 2 bars long less 40 ticks: Abmaj7 (56 60 63 67),
  // Bb (58 62 65), Gm7 (55 58 62 65), Cm9 (48 55 58 62 63). Reference: Ab, 56 (pitch class 8).
  // Relative: {0,4,7,11} = 0x891, {2,6,9} = 0x244, {11,2,6,9} = 0xa44, {4,11,2,6,7} = 0x8d4; 4 slots each.
  // A single-note line in the break on "Stabs" (steps 3 and 11) has one pitch class: never harmony.
  const prog = [[60, 63, 67], [56, 60, 63], [53, 56, 60], [55, 59, 62]];
  const stabs = [...chordAt(15120, 100, [58, 62, 65], 90)];
  for (let b = 8; b < 16; b++) { const c = prog[(b - 8) % 4]; for (const st of [2, 10, 14]) stabs.push(...chordAt(b * BAR + st * S, 100, c, 90)); }
  for (let b = 16; b < 24; b++) for (const st of [3, 11]) stabs.push([b * BAR + st * S, 60, 72, 70]);
  const bd = [[56, 60, 63, 67], [58, 62, 65], [55, 58, 62, 65], [48, 55, 58, 62, 63]];
  const pad = flat(bd.map((c, i) => chordAt((16 + 2 * i) * BAR, 2 * BAR - 40, c, 70)));
  files['harmony_a.mid'] = song({ bars: 32, markers: HM, tracks: [{ name: 'Stabs', ch: 2, notes: stabs }, { name: 'Pad', ch: 4, notes: pad }] });
}
{
  // Song B: the same break 5 semitones up, each chord pushed 120 ticks early (the first one starts at
  // 30600, 120 before the break). The nearest stack to each slot start is the pushed one, so the shape
  // relative to its lowest key (61) is song A's. Drop: Bbm (58 61 65) for bars 8-9, Gb (54 58 61) for
  // 10-11, twice, struck on steps 0 and 8. Relative to 58: {0,3,7} = 0x089 and {8,0,3} = 0x109, 4 slots
  // each. Runs 4,4. Single notes on steps 3, 5, 11 and 13 of the same "Stabs" track are ignored.
  const stabs = [];
  for (let b = 8; b < 16; b++) {
    const c = (b - 8) % 4 < 2 ? [58, 61, 65] : [54, 58, 61];
    for (const st of [0, 8]) stabs.push(...chordAt(b * BAR + st * S, 200, c, 90));
    for (const st of [3, 5, 11, 13]) stabs.push([b * BAR + st * S, 60, 77, 70]);
  }
  const bd = [[61, 65, 68, 72], [63, 67, 70], [60, 63, 67, 70], [53, 60, 63, 67, 68]];
  const pad = flat(bd.map((c, i) => chordAt((16 + 2 * i) * BAR - 120, 2 * BAR - 40, c, 70)));
  files['harmony_b.mid'] = song({ bars: 32, markers: HM, tracks: [{ name: 'Stabs', ch: 2, notes: stabs }, { name: 'Pad', ch: 4, notes: pad }] });
}
{
  // Song C, drop, sustained "Pad" chords struck once at each change: nothing in slot 0 (the first chord
  // starts at bar 8 + 960, slot 1), Eb (63 67 70) at bar 9 + 960 (slot 3), Ab (56 60 63) at bar 10 (slot 4),
  // Bb (58 62 65) at bar 11 (slot 6), then the same from bar 12 (a Cm at bar 12 starts the repeat).
  // Slot 0 is empty and takes slot 1's chord: Cm Cm Cm Eb Ab Ab Bb Bb, runs 3,1,2,2.
  // Relative to 60: Cm 0x089, Eb {3,7,10} = 0x488, Ab 0x109, Bb {10,2,5} = 0x424.
  // Slots 8-15: Cm at bar 12 + 0 (slot 8), Eb at 13 + 960 (11), Ab at 14 (12), Bb at 15 (14), and Bb
  // struck again at bar 15 + 960 (slot 15), so slot 15 has a stack of its own: the empty slot 0 must take
  // slot 1's Cm, not wrap around to the end.
  // Break: Cm (48 55 60 63) for 4 bars, Ab (44 51 56 60) for 4: relative to 48, 0x089 then 0x109.
  const pad = [
    ...chordAt(8 * BAR + 960, 1900, [60, 63, 67], 70), ...chordAt(9 * BAR + 960, 900, [63, 67, 70], 70),
    ...chordAt(10 * BAR, 1900, [56, 60, 63], 70), ...chordAt(11 * BAR, 1900, [58, 62, 65], 70),
    ...chordAt(12 * BAR, 2860, [60, 63, 67], 70), ...chordAt(13 * BAR + 960, 900, [63, 67, 70], 70),
    ...chordAt(14 * BAR, 1900, [56, 60, 63], 70), ...chordAt(15 * BAR, 900, [58, 62, 65], 70), ...chordAt(15 * BAR + 960, 900, [58, 62, 65], 70),
    ...chordAt(16 * BAR, 4 * BAR - 40, [48, 55, 60, 63], 70), ...chordAt(20 * BAR, 4 * BAR - 40, [44, 51, 56, 60], 70)
  ];
  files['harmony_c.mid'] = song({ bars: 32, markers: HM, tracks: [{ name: 'Pad', ch: 4, notes: pad }] });
}
{
  const A_DROP = '089.089.109.109.121.121.884.884.089.089.109.109.121.121.884.884';
  const AB_BREAK = '891.891.891.891.244.244.244.244.a44.a44.a44.a44.8d4.8d4.8d4.8d4';
  const B_DROP = '089.089.089.089.109.109.109.109.089.089.089.089.109.109.109.109';
  const C_DROP = '089.089.089.488.109.109.424.424.089.089.089.488.109.109.424.424';
  const C_BREAK = '089.089.089.089.089.089.089.089.109.109.109.109.109.109.109.109';
  expected.g1 = {
    files: ['harmony_a.mid', 'harmony_b.mid', 'harmony_c.mid'],
    shapes: { 'harmony_a.mid': { break: AB_BREAK, drop: A_DROP }, 'harmony_b.mid': { break: AB_BREAK, drop: B_DROP }, 'harmony_c.mid': { break: C_BREAK, drop: C_DROP } },
    // break: 2 distinct of 3, the A/B shape twice; drop: 3 distinct. With 3 seeds, "top <= 10%" can't hold.
    break: { seeds: 3, measured: 3, distinct: 2, top: 2, distinct_pm: 667, top_pm: 667, pass: 0 },
    drop: { seeds: 3, measured: 3, distinct: 3, top: 1, distinct_pm: 1000, top_pm: 333, pass: 0 },
    pass: 0
  };
  expected.g2 = {
    files: ['harmony_a.mid', 'harmony_b.mid', 'harmony_c.mid'],
    patterns: { 'harmony_a.mid': '2,2,2,2', 'harmony_b.mid': '4,4', 'harmony_c.mid': '3,1,2,2' },
    // one "4 x 1-bar" of 3 (333 permille > 300), 3 patterns (< 4): fails; A and B change on bar lines only.
    seeds: 3, measured: 3, four_by_one_bar: 1, four_by_one_bar_pm: 333, patterns_count: 3, changes_on_bar_lines_only: 2, pass: 0
  };
}

/* ---------- repetition.mid: G3 ---------- */
// One "groove" section of 8 bars: windows bars 0-3 and 4-7. Drums (channel 10):
//   36 kick four on the floor in every bar (bar 6 adds step 14): exempt in both windows
//   35 a second kick key, only in bar 0 on beat 1: active in window 1, not repeated, not exempt
//   39 clap on 480 and 1440, velocity 100, every bar: repeated in both windows
//   42 hat on every 8th, velocity 80; in bar 2 the step-4 hat has velocity 90: window 1 not repeated,
//      window 2 repeated
//   38 snare once, bar 7: window 1 inactive, window 2 active and not repeated
// Drum windows counted: window 1: 35, 39, 42; window 2: 39, 42, 38. 6 windows, 3 repeated, 2 exempt.
// Bass: the same rhythm (0, 720, 1200) every bar with other pitches in window 1 (rhythm only: repeated);
// bar 5 adds 1680, so window 2 isn't. 2 windows, 1 repeated. 30 > 4 * 6 and 10 > 3 * 2: fails.
{
  const drums = [];
  for (let b = 0; b < 8; b++) {
    for (const st of [0, 4, 8, 12]) drums.push([b * BAR + st * S, 60, 36, 110]);
    if (b === 6) drums.push([b * BAR + 14 * S, 60, 36, 100]);
    for (const st of [4, 12]) drums.push([b * BAR + st * S, 60, 39, 100]);
    for (let st = 0; st < 16; st += 2) drums.push([b * BAR + st * S, 30, 42, b === 2 && st === 4 ? 90 : 80]);
  }
  drums.push([0, 60, 35, 100], [7 * BAR + 4 * S, 60, 38, 90]);
  const bass = [];
  for (let b = 0; b < 8; b++) {
    const k = [36, 39, 41, 43][b % 4];
    for (const t of [0, 720, 1200]) bass.push([b * BAR + t, 200, k, 100]);
    if (b === 5) bass.push([b * BAR + 1680, 120, k, 100]);
  }
  files['repetition.mid'] = song({ bars: 8, markers: [[0, 'groove']], tracks: [{ name: 'Drums', ch: 9, notes: drums }, { name: 'Bass', ch: 1, notes: bass }] });
  expected.g3 = { file: 'repetition.mid', drum_windows: 6, drum_repeated: 3, kick_4x4_exempt: 2, bass_windows: 2, bass_repeated: 1, drum_repeated_pm: 500, bass_repeated_pm: 500, pass: 0 };
}

/* ---------- macro.mid: G4 (and G10's change intervals) ---------- */
// Sections: "drop" bars 0-23 (24 bars), "outro" 24-31 (8 bars: under 9, never compared).
// Bass, a one-bar figure (steps 0, 6, 10, key 36) except: bar 13 (steps 0, 6, 12) and bar 15 (key 38 on
// step 0, 6, 10), then from bar 16 a new figure (steps 0, 4, 8, 12, key 41).
//   bar 13 differs from bar 5 and bar 12 didn't: a change at 13 (13 mod 8 = 5, not a phrase bar)
//   bar 14 equals bar 6: no
//   bar 15 differs from 7, bar 14 didn't: a change at 15 (mod 8 = 7: phrase end)
//   bars 16-23 differ from 8-15: one run with 15, no new change
// Bass: 2 changes, 1 on a phrase bar.
// Stabs: 60 63 67 on step 0 of bars 0-15, 60 63 68 from bar 16: a change at 16 (mod 8 = 0). 1 of 1.
// Lead: key 72 on step 0 from bar 8 (bars 0-7 empty), and at bar 20 on step 4 instead:
//   bar 8 differs from the empty bar 0: a change at 8 (phrase start); bars 9-15 continue the run;
//   bar 16 equals bar 8: the run ends; bar 20 differs from bar 12 (step 0): a change at 20 (mod 8 = 4).
// Lead: 2 changes, 1 on a phrase bar. Every role has >= 50%: passes.
// G10 intervals: section starts {0, 24} and the changes {13, 15, 16, 8, 20}: 0 8 13 15 16 20 24, so the
// intervals 8 5 2 1 4 4 take 5 values.
{
  const bass = [], stabs = [], lead = [];
  for (let b = 0; b < 24; b++) {
    let steps = [0, 6, 10], key = 36;
    if (b === 13) steps = [0, 6, 12];
    if (b === 15) key = 38;
    if (b >= 16) { steps = [0, 4, 8, 12]; key = 41; }
    steps.forEach(st => bass.push([b * BAR + st * S, 100, key, 100]));
    stabs.push(...chordAt(b * BAR, 200, b < 16 ? [60, 63, 67] : [60, 63, 68], 90));
    if (b >= 8) lead.push([b * BAR + (b === 20 ? 4 : 0) * S, 300, 72, 90]);
  }
  files['macro.mid'] = song({ bars: 32, markers: [[0, 'drop'], [24, 'outro']], tracks: [{ name: 'Bass', ch: 1, notes: bass }, { name: 'Stabs', ch: 2, notes: stabs }, { name: 'Lead', ch: 3, notes: lead }] });
  expected.g4 = {
    file: 'macro.mid',
    bass: { changes: 2, on_phrase: 1, on_phrase_pm: 500 }, chords: { changes: 1, on_phrase: 1, on_phrase_pm: 1000 }, lead: { changes: 2, on_phrase: 1, on_phrase_pm: 500 },
    change_bars: { bass: [13, 15], chords: [16], lead: [8, 20] },
    pass: 1
  };
  expected.g10_intervals = { file: 'macro.mid', interval_values: 5 };
}

/* ---------- macro8.mid: G4 against an 8-bar loop ---------- */
// One "drop" of 24 bars. Lead: key 60 + (bar mod 8) on step 0, an 8-bar loop, except bar 21 (key 72).
// Every bar equals the bar 8 before except 21 (and nothing after it differs from 8 bars before it): one
// change, at 21 (21 mod 8 = 5, not a phrase bar). Compared with the bar 4 before, every bar from 4 on
// would differ: one long run starting at bar 8, a phrase start.
{
  const lead = range(24, b => [b * BAR, 300, b === 21 ? 72 : 60 + (b % 8), 90]);
  files['macro8.mid'] = song({ bars: 24, markers: [[0, 'drop']], tracks: [{ name: 'Lead', ch: 3, notes: lead }] });
  expected.g4_loop8 = { file: 'macro8.mid', lead: { changes: 1, on_phrase: 0 }, lead_change_bars: [21] };
}

/* ---------- velocity.mid: G5 ---------- */
// "drop" bars 0-7 (one window), "build" bars 8-15 (skipped). In the drop:
//   hats (42): 10 per bar (steps 0 2 4 6 8 10 12 14 3 11), the 10 velocities 61, 63, ..., 79 in every
//     bar: 80 notes, 10 values, SD sqrt(33) = 5.7446 (deviations +-1 +-3 +-5 +-7 +-9 from 70): 5745 milli
//   perc (56): steps 0 and 8, velocities 50 and 70: 16 notes, 2 values, SD 10: 10000 milli
//   snare (38) on the backbeats (480, 1440) at velocity 110: not ghosts. A clap (39) at 484: inside
//     the backbeat window [450, 510], not a ghost either
//   ghosts: snare on steps 3 and 11 (360, 1320) with velocities 40, 41, ..., 55 in order: 16 notes,
//     16 values, SD sqrt(21.25) = 4.6098: 4610 milli
//   bass: one note per bar, 80 90 100 110 80 90 100 110: 4 values, SD sqrt(125) = 11.1803: 11180 milli
//   lead: 7 notes, under the 8-note minimum: no window
// In the build, hats at velocity 1 must not count.
{
  const drums = [], bass = [], lead = [];
  for (let b = 0; b < 8; b++) {
    [0, 2, 4, 6, 8, 10, 12, 14, 3, 11].forEach((st, i) => drums.push([b * BAR + st * S, 30, 42, 61 + 2 * i]));
    drums.push([b * BAR, 60, 56, 50], [b * BAR + 8 * S, 60, 56, 70]);
    drums.push([b * BAR + 480, 60, 38, 110], [b * BAR + 1440, 60, 38, 110], [b * BAR + 484, 60, 39, 100]);
    drums.push([b * BAR + 360, 60, 38, 40 + 2 * b], [b * BAR + 1320, 60, 38, 41 + 2 * b]);
    bass.push([b * BAR, 400, 36, [80, 90, 100, 110][b % 4]]);
    if (b < 7) lead.push([b * BAR, 400, 72, 60 + 5 * b]);
  }
  for (let b = 8; b < 16; b++) for (let st = 0; st < 16; st++) drums.push([b * BAR + st * S, 30, 42, 1]);
  files['velocity.mid'] = song({ bars: 16, markers: [[0, 'drop'], [8, 'build']], tracks: [{ name: 'Drums', ch: 9, notes: drums }, { name: 'Bass', ch: 1, notes: bass }, { name: 'Lead', ch: 3, notes: lead }] });
  expected.g5 = {
    file: 'velocity.mid',
    hats: { windows: 1, median_distinct: 10, median_sd_milli: 5745 }, perc: { windows: 1, median_distinct: 2, median_sd_milli: 10000 },
    ghosts: { windows: 1, median_distinct: 16, median_sd_milli: 4610 }, bass: { windows: 1, median_distinct: 4, median_sd_milli: 11180 },
    lead: { windows: 0, median_distinct: -1, median_sd_milli: -1 }, pass: 0
  };
}

/* ---------- stacks.mid: G6 ---------- */
// Pad (channel 5): 0: 60 64 67 at 70 70 76 (2 velocities, spread 0); 1920: 60 63 67 all 80 (1 velocity);
// 3840/3842/3843: 60 63 67 at 70 72 74 (3 velocities, spread 3); 5760 and 5772: 12 ticks apart, so two
// single notes, no stack; 7680 and 7690: 10 apart, one stack, 90 and 91 (spread 10).
// Stabs (channel 3): 72 76 at 0, both 100: 1 velocity.
// 5 stacks, 3 with 2+ velocities (600 permille), largest spread 10: fails.
{
  const pad = [[0, 400, 60, 70], [0, 400, 64, 70], [0, 400, 67, 76], ...chordAt(1920, 400, [60, 63, 67], 80),
    [3840, 400, 60, 70], [3842, 400, 63, 72], [3843, 400, 67, 74], [5760, 400, 60, 80], [5772, 400, 63, 80], [7680, 400, 60, 90], [7690, 400, 67, 91]];
  files['stacks.mid'] = song({ bars: 5, tracks: [{ name: 'Pad', ch: 4, notes: pad }, { name: 'Stabs', ch: 2, notes: chordAt(0, 200, [72, 76], 100) }] });
  expected.g6 = { file: 'stacks.mid', stacks: 5, two_velocities: 3, two_velocities_pm: 600, max_spread_ticks: 10, spread_histogram: [{ value: 0, count: 3 }, { value: 3, count: 1 }, { value: 10, count: 1 }], pass: 0 };
}

/* ---------- microtiming.mid: G7 ---------- */
// No markers (the whole file is one section), 8 bars. Hats (42) on every 8th, alternately 1 tick late
// and 1 tick early: 240k + 1, 240k - 1 (k = 0..63; the first is at 1). No hat onset falls in [60, 180)
// mod 240, so the swing estimate is 0 and the residuals are +1, -1, +1, ... An open hat (46) at
// 3840 + 200 is 40 ticks from the nearest grid line (4080): excluded (over 30).
//   n 64, SD 1: 1000 milli
//   lag 1: every product is -1, mean 0: -(n - 1)/n = -63/64 = -0.984375: -984 permille
//   DFA: the profile is 1 0 1 0 ...; in a box of s values (s even, starting on a 1) the least-squares
//     line leaves F^2(s) = 1/4 - 3 / (4 (s^2 - 1)), so F^2(4) = 0.2, F^2(8) = 0.238095, F^2(16) = 0.247059
//     (boxes 4, 8, 16: s <= 64/4). alpha is the slope of ln F against ln s: worked out below.
// Perc (56) on every beat, exactly on the grid: 32 notes, all residuals 0: SD 0; under 64 notes, so no
// lag 1 or alpha (-9999).
{
  const drums = [];
  for (let k = 0; k < 64; k++) drums.push([240 * k + (k % 2 ? -1 : 1), 30, 42, 80]);
  drums.push([3840 + 200, 30, 46, 80]);
  for (let k = 0; k < 32; k++) drums.push([480 * k, 30, 56, 70]);
  files['microtiming.mid'] = song({ bars: 8, tracks: [{ name: 'Drums', ch: 9, notes: drums }] });
  const sizes = [4, 8, 16], lx = sizes.map(Math.log), ly = sizes.map(s => 0.5 * Math.log(0.25 - 3 / (4 * (s * s - 1))));
  const mx = (lx[0] + lx[1] + lx[2]) / 3, my = (ly[0] + ly[1] + ly[2]) / 3;
  const slope = ((lx[0] - mx) * (ly[0] - my) + (lx[1] - mx) * (ly[1] - my) + (lx[2] - mx) * (ly[2] - my)) / ((lx[0] - mx) ** 2 + (lx[1] - mx) ** 2 + (lx[2] - mx) ** 2);
  expected.g7 = {
    file: 'microtiming.mid',
    hats: { n: 64, sd_milli: 1000, lag1_pm: -984, alpha_milli: Math.floor(1000 * slope + 0.5) },
    perc: { n: 32, sd_milli: 0, lag1_pm: -9999, alpha_milli: -9999 },
    pass: 0
  };
}

/* ---------- microtiming2.mid: G7 with a residual mean that isn't 0 ---------- */
// Hats on every 8th, 3 ticks late then 1 tick late, alternately: residuals 3 1 3 1 ... (n 64, mean 2).
//   SD 1: 1000 milli. Lag 1 from the exact formula: P = 63 * 3 = 189, S1 = 128, S2 = 320, A = 125,
//   B = 127: [4096 * 189 - 64 * 128 * 252 + 63 * 16384] / [64 * (64 * 320 - 16384)] = -258048 / 262144
//   = -0.984375: -984 (the deviations from the mean are +-1, as in microtiming.mid). The profile of the
//   deviations is the same too, so alpha is the same.
{
  const drums = range(64, k => [240 * k + (k % 2 ? 1 : 3), 30, 42, 80]);
  files['microtiming2.mid'] = song({ bars: 8, tracks: [{ name: 'Drums', ch: 9, notes: drums }] });
  expected.g7_mean = { file: 'microtiming2.mid', hats: { n: 64, sd_milli: 1000, lag1_pm: -984, alpha_milli: expected.g7.hats.alpha_milli } };
}

/* ---------- swing.mid: G8 ---------- */
// "intro" bars 0-3 (straight hats, ignored), "drop" bars 4-11. In the drop, odd 16ths are delayed:
//   hats (42) every 16th, odd ones +24: 64 candidates, swing 24
//   snare (38) on 480 and 1440 (even), ghosts on steps 3 and 11 +19: 16 candidates, swing 19
//   perc (56) on steps 1 5 9 13 +31: 32 candidates, swing 31
//   kick (36) on steps 0 and 7 (+0): an anchor lane, not measured
//   Bass (track 2) on steps 3 and 7 +10: 16 candidates, 10
//   Stabs (track 3) dyads on steps 5 and 13 +11: 32 notes, 11
//   Lead (track 4) 10 odd notes: under 16, not measured
//   Vox (track 5) on steps 1 and 9, 25 ticks early (tick mod 240 = 95): 16 candidates, -25
// Swings -25 10 11 19 24 31; clusters {-25}, {10, 11}, {19}, {24}, {31}: 5 distinct, so the one seed passes.
{
  const drums = [], bass = [], stabs = [], lead = [];
  for (let b = 0; b < 4; b++) for (let st = 0; st < 16; st++) drums.push([b * BAR + st * S, 30, 42, 70]);
  for (let b = 4; b < 12; b++) {
    for (let st = 0; st < 16; st++) drums.push([b * BAR + st * S + (st % 2 ? 24 : 0), 30, 42, 70]);
    drums.push([b * BAR + 480, 60, 38, 110], [b * BAR + 1440, 60, 38, 110], [b * BAR + 3 * S + 19, 60, 38, 50], [b * BAR + 11 * S + 19, 60, 38, 50]);
    for (const st of [1, 5, 9, 13]) drums.push([b * BAR + st * S + 31, 30, 56, 60]);
    drums.push([b * BAR, 60, 36, 110], [b * BAR + 7 * S, 60, 36, 100]);
    for (const st of [3, 7]) bass.push([b * BAR + st * S + 10, 100, 36, 100]);
    for (const st of [5, 13]) stabs.push(...chordAt(b * BAR + st * S + 11, 100, [60, 67], 90));
  }
  for (let i = 0; i < 10; i++) lead.push([4 * BAR + i * 2 * S + S, 100, 72, 90]);
  const vox = [];
  for (let b = 4; b < 12; b++) for (const st of [1, 9]) vox.push([b * BAR + st * S - 25, 100, 79, 80]);
  files['swing.mid'] = song({ bars: 12, markers: [[0, 'intro'], [4, 'drop']], tracks: [{ name: 'Drums', ch: 9, notes: drums }, { name: 'Bass', ch: 1, notes: bass }, { name: 'Stabs', ch: 2, notes: stabs }, { name: 'Lead', ch: 3, notes: lead }, { name: 'Vox', ch: 11, notes: vox }] });
  expected.g8 = { file: 'swing.mid', lanes: 'snare=19 hats=24 perc=31 bass:2=10 chords:3=11 vox:5=-25', distinct: 5, pass: 1 };
}

/* ---------- fills.mid: G9 ---------- */
// Sections: intro 0-7, drop 8-23, build 24-31, drop2 32-39, groove 40-47, break 48-55, outro 56-63.
// Everywhere: kick (36) on the 4 beats, clap (39) on 480 and 1440, closed hat (42) on every 8th.
// Fills (section bar index, position = index mod 8):
//   intro 7: the kick drops beats 3 and 4. Kick steps 8 and 12 are in 7 of the intro's 8 bars
//     (7 * 4 >= 3 * 8): both missing, a fill (position 7)
//   drop 3 (bar 11): toms (45 47 50) on steps 12 13 14. tom:12 and tom:14 are in 2 of 16 bars (also
//     bar 23), tom:13 in 1: all rare (8 * 2 <= 16): a fill (position 3)
//   drop 7 (bar 15): snare (38) on steps 8-15: 8 rare steps, a fill (position 7)
//   drop 15 (bar 23): toms on steps 10 12 14: a fill (position 7)
// The build has a snare on every 8th in all 8 bars (not rare) and a "Riser" (FX) on every 16th of bars
// 28-31. Its last bar (31) drops the kick on beats 3 and 4: a fill, but builds aren't counted for
// positions or distinctness. The break has no kick, so no kick step is common there.
// drop2 has toms on steps 12 13 14 in its bars 3 and 7 (bars 35 and 39): 2 of 8 bars, more than an
// eighth, so not rare and not fills.
// Boundary gestures (bar: what):
//   8: the intro's bar 7 is a fill; 24: the drop's bar 15 is a fill; 32: riser notes start in the
//   2 bars before (and the build's last bar is a fill); 40: nothing (no fill, no FX, no cymbal, and the
//   hat on step 14 of bar 39 plays);
//   48: a gap, no hat on step 14 of bar 47, so no drum onset in the last 240 ticks; 56: a crash (49)
//   on the downbeat.
// 6 boundaries, 5 with a gesture. 4 fills: position 7 three times (750 permille > 600), position 3 once.
// The 4 fill bars have 4 different drum contents: 1000 permille distinct.
{
  const drums = [], riser = [];
  for (let b = 0; b < 64; b++) {
    const inBreak = b >= 48 && b < 56;
    for (const st of [0, 4, 8, 12]) if (!inBreak && !((b === 7 || b === 31) && st >= 8)) drums.push([b * BAR + st * S, 60, 36, 110]);
    for (const st of [4, 12]) drums.push([b * BAR + st * S, 60, 39, 100]);
    for (let st = 0; st < 16; st += 2) if (!(b === 47 && st === 14)) drums.push([b * BAR + st * S, 30, 42, 70]);
    if (b === 11) for (const [st, k] of [[12, 45], [13, 47], [14, 50]]) drums.push([b * BAR + st * S, 60, k, 100]);
    if (b === 15) for (let st = 8; st < 16; st++) drums.push([b * BAR + st * S, 60, 38, 60 + 5 * (st - 8)]);
    if (b === 23) for (const [st, k] of [[10, 45], [12, 47], [14, 50]]) drums.push([b * BAR + st * S, 60, k, 100]);
    if (b === 35 || b === 39) for (const [st, k] of [[12, 45], [13, 47], [14, 50]]) drums.push([b * BAR + st * S, 60, k, 100]);
    if (b >= 24 && b < 32) for (let st = 0; st < 16; st += 2) drums.push([b * BAR + st * S, 60, 38, 80]);
    if (b >= 28 && b < 32) for (let st = 0; st < 16; st++) riser.push([b * BAR + st * S, 100, 60 + (b - 28), 70]);
  }
  drums.push([56 * BAR, 200, 49, 110]);
  files['fills.mid'] = song({
    bars: 64, markers: [[0, 'intro'], [8, 'drop'], [24, 'build'], [32, 'drop2'], [40, 'groove'], [48, 'break'], [56, 'outro']],
    tracks: [{ name: 'Drums', ch: 9, notes: drums }, { name: 'Riser', ch: 6, notes: riser }]
  });
  expected.g9 = {
    file: 'fills.mid', boundaries: 6, with_gesture: 5, with_gesture_pm: 833, missing_gesture_bars: '40',
    fills: 4, fill_positions: '7,3,7,7', position_histogram: [{ value: 3, count: 1 }, { value: 7, count: 3 }], top_position: 7, top_position_pm: 750,
    songs_with_2_fills: 1, median_distinct_pm: 1000, pass: 0
  };
}

/* ---------- forms_a1/a2/b.mid: G10 ---------- */
// a1 and a2: intro 16, drop 16, break 8, drop 16, outro 16 (72 bars); b: intro 16, drop 16, outro 16.
// One kick per bar, nothing melodic, so the salient changes are the section starts only:
// a: 0 16 32 40 56, intervals 16 16 8 16 (2 values); b: 0 16 32, intervals 16 16 (1 value).
// 2 distinct forms, the top one twice (667 permille); under 8 forms, and b has 1 interval value: fails.
{
  const kicks = bars => range(bars, b => [b * BAR, 60, 36, 110]);
  const fa = [[0, 'intro'], [16, 'drop'], [32, 'break'], [40, 'drop'], [56, 'outro']];
  files['forms_a1.mid'] = song({ bars: 72, markers: fa, tracks: [{ name: 'Drums', ch: 9, notes: kicks(72) }] });
  files['forms_a2.mid'] = song({ bars: 72, markers: fa, tracks: [{ name: 'Drums', ch: 9, notes: kicks(72) }] });
  files['forms_b.mid'] = song({ bars: 48, markers: [[0, 'intro'], [16, 'drop'], [32, 'outro']], tracks: [{ name: 'Drums', ch: 9, notes: kicks(48) }] });
  expected.g10 = {
    files: ['forms_a1.mid', 'forms_a2.mid', 'forms_b.mid'],
    forms: { 'forms_a1.mid': 'intro16 drop16 break8 drop16 outro16', 'forms_a2.mid': 'intro16 drop16 break8 drop16 outro16', 'forms_b.mid': 'intro16 drop16 outro16' },
    interval_values: { 'forms_a1.mid': 2, 'forms_a2.mid': 2, 'forms_b.mid': 1 },
    distinct_forms: 2, top_form_count: 2, top_form_pm: 667, every_song_two_interval_values: 0, pass: 0
  };
}

/* ---------- accent.mid: R3 hat accent period ---------- */
// Closed hat (42): bar 0, 16ths all at 80: period 1. Bar 1, 16ths alternating 100 60: period 2.
// Bar 2, 16ths 100 60 60 60 repeating but the last at 80: no period below 16. Bar 3: 3 onsets only, not
// counted. Bar 4, 8ths 100 70 80 70 repeating: steps 0 2 4 6 = 100 70 80 70, step 4 != step 0, so period 8.
// Bar 5: open hat (46) only, not a closed hat. 4 bars counted, 2 with a period of 4 or less.
{
  const d = [];
  for (let st = 0; st < 16; st++) d.push([st * S, 30, 42, 80]);
  for (let st = 0; st < 16; st++) d.push([BAR + st * S, 30, 42, st % 2 ? 60 : 100]);
  for (let st = 0; st < 16; st++) d.push([2 * BAR + st * S, 30, 42, st === 15 ? 80 : st % 4 ? 60 : 100]);
  for (const st of [0, 4, 8]) d.push([3 * BAR + st * S, 30, 42, 80]);
  for (let st = 0; st < 16; st += 2) d.push([4 * BAR + st * S, 30, 42, [100, 70, 80, 70][(st / 2) % 4]]);
  for (let st = 0; st < 16; st += 2) d.push([5 * BAR + st * S, 30, 46, 80]);
  files['accent.mid'] = song({ bars: 6, tracks: [{ name: 'Drums', ch: 9, notes: d }] });
  expected.r3 = { file: 'accent.mid', bars: 4, period_4_or_less: 2, period_4_or_less_pm: 500, period_histogram: [{ value: 1, count: 1 }, { value: 2, count: 1 }, { value: 8, count: 1 }, { value: 16, count: 1 }] };
}

/* ---------- gate.mid: R4 gate rule ---------- */
// Hats with odd 16ths 20 ticks late set the file's swing to 20 (the bass has no odd-16th onsets, so the
// file's swing is used: the lower median of the hats' 16 offsets of +20). Bass notes (on, length -> end):
//   0, 226 -> 226: 226 + 14 = 240, an 8th line: the rule
//   480, 150 -> 630: 644 and 600 are on no line (600 mod 240 = 120, the odd line is at 140): not
//   960, 126 -> 1086: 1100 mod 240 = 140, the swung odd line: the rule
//   1440, 270 -> 1710: 1710 - 30 = 1680, an 8th line (legato): the rule
//   1920, 1920 -> 3840, the file's end: the rule
//   2400, 100 -> 2500: 2514 mod 240 = 114, 2470 mod 240 = 70: not
// 6 notes, 4 by the rule: 667 permille.
{
  const d = [];
  for (let b = 0; b < 2; b++) for (let st = 0; st < 16; st++) d.push([b * BAR + st * S + (st % 2 ? 20 : 0), 30, 42, 70]);
  const bass = [[0, 226, 36, 100], [480, 150, 38, 100], [960, 126, 39, 100], [1440, 270, 41, 100], [1920, 1920, 36, 100], [2400, 100, 43, 100]];
  files['gate.mid'] = song({ bars: 2, tracks: [{ name: 'Drums', ch: 9, notes: d }, { name: 'Bass', ch: 1, notes: bass }] });
  expected.r4 = { file: 'gate.mid', notes: 6, gate_rule: 4, gate_rule_pm: 667 };
}

/* ---------- melodic.mid: R5 chord tones, I1 in key ---------- */
// C minor (sf -3, minor). Pad: Cm (60 63 67) at 0, Ab (56 60 63) at 1920.
// Lead (strong = 16th step mod 4 == 0):
//   72 C @0 strong, in Cm: yes;  74 D @120 weak: no;   75 Eb @480 strong: yes;  79 G @840 weak: yes
//   68 Ab @1920 strong, in Ab: yes;  70 Bb @2160 weak (step 2): no;  72 C @2400 strong: yes
//   71 B @3000 weak (step 25): no, and out of key
// Lead: strong 4 of 4, weak 1 of 4. Bass: 36 C @0 strong yes; 43 G @600 weak yes; 44 Ab @1920 strong
// yes; 46 Bb @2640 weak no: strong 2 of 2, weak 1 of 2.
// In key (C natural minor C D Eb F G Ab Bb): lead 7 of 8 (B is out), bass 4 of 4, chords 6 of 6.
{
  const pad = [...chordAt(0, 1900, [60, 63, 67], 70), ...chordAt(1920, 1900, [56, 60, 63], 70)];
  const lead = [[0, 100, 72, 90], [120, 100, 74, 90], [480, 100, 75, 90], [840, 100, 79, 90], [1920, 100, 68, 90], [2160, 100, 70, 90], [2400, 100, 72, 90], [3000, 100, 71, 90]];
  const bass = [[0, 200, 36, 100], [600, 200, 43, 100], [1920, 200, 44, 100], [2640, 200, 46, 100]];
  files['melodic.mid'] = song({ bars: 2, tracks: [{ name: 'Pad', ch: 4, notes: pad }, { name: 'Lead', ch: 3, notes: lead }, { name: 'Bass', ch: 1, notes: bass }] });
  expected.r5 = { file: 'melodic.mid', lead: { strong: 4, strong_chord_tones: 4, weak: 4, weak_chord_tones: 1 }, bass: { strong: 2, strong_chord_tones: 2, weak: 2, weak_chord_tones: 1 } };
  expected.i1 = { file: 'melodic.mid', lead: { notes: 8, in_key: 7 }, bass: { notes: 4, in_key: 4 }, chords: { notes: 6, in_key: 6 } };
}

/* ---------- wrap.mid: R5 before the first chord ---------- */
// Pad: Cm (60 63 67) at 960, Ab (56 60 63) at 2880. Lead G (79) at 0, before any chord: the harmony there
// is the last one (a loop), Ab, which has no G: not a chord tone. Lead G at 1920: Cm, a chord tone.
// Both on strong steps: strong 2, 1 chord tone; weak 0.
{
  const pad = [...chordAt(960, 1800, [60, 63, 67], 70), ...chordAt(2880, 900, [56, 60, 63], 70)];
  files['wrap.mid'] = song({ bars: 2, tracks: [{ name: 'Pad', ch: 4, notes: pad }, { name: 'Lead', ch: 3, notes: [[0, 100, 79, 90], [1920, 100, 79, 90]] }] });
  expected.r5_wrap = { file: 'wrap.mid', lead: { strong: 2, strong_chord_tones: 1, weak: 0, weak_chord_tones: 0 } };
}

/* ---------- lead.mid: R7 lead shape ---------- */
// One 4-bar window (no markers). Bar 0 steps 0 4 8 (60 62 64), bar 1 steps 0 6 (67 65), bar 2 steps
// 0 4 8 (64 62 60), bar 3 step 2 (72). Intervals 2 2 3 2 1 2 2 12: 6 of 8 are steps (750 permille), no
// repeats. Bar 1 and bar 3 (indices 0 and 2) have the same rhythm: 1 of 1 window.
{
  const lead = [[0, 0, 60], [0, 4, 62], [0, 8, 64], [1, 0, 67], [1, 6, 65], [2, 0, 64], [2, 4, 62], [2, 8, 60], [3, 2, 72]].map(([b, st, k]) => [b * BAR + st * S, 100, k, 90]);
  files['lead.mid'] = song({ bars: 4, tracks: [{ name: 'Lead', ch: 3, notes: lead }] });
  expected.r7 = { file: 'lead.mid', intervals: 8, steps: 6, step_pm: 750, repeats_pm: 0, windows: 1, bar1_eq_bar3: 1, bar1_eq_bar3_pm: 1000 };
}

/* ---------- voicing.mid: R8 voicing texture ---------- */
// Stabs, one stack per bar: dyad 60 67; triad 60 63 67; quartal 60 65 70; four_plus 60 63 67 70;
// the triad again as 63 67 72 (same pitch classes, other keys: re-voiced); the dyad again as 60 67
// (the same voicing: recurring, not re-voiced); an octave 60 72 (one pitch class).
// 7 stacks, 5 types, four_plus 1 of 7 (143 permille), 2 recurring, 1 re-voiced (500 permille).
{
  const st = [[60, 67], [60, 63, 67], [60, 65, 70], [60, 63, 67, 70], [63, 67, 72], [60, 67], [60, 72]];
  files['voicing.mid'] = song({ bars: 7, tracks: [{ name: 'Stabs', ch: 2, notes: flat(st.map((c, b) => chordAt(b * BAR, 200, c, 90))) }] });
  expected.r8 = { file: 'voicing.mid', stacks: 7, types: 5, four_plus_pm: 143, recurring: 2, revoiced: 1, revoiced_pm: 500,
    type_histogram: [{ type: 'dyad', count: 2 }, { type: 'four_plus', count: 1 }, { type: 'octave', count: 1 }, { type: 'quartal', count: 1 }, { type: 'triad', count: 2 }] };
}

/* ---------- polymeter.mid: R9 odd cycles ---------- */
// 8 bars, no markers (one 8-bar window). Perc (56) every 3rd 16th: period 3, odd. Rim (37) every 24th:
// period 24, odd (it neither divides 16 nor is a multiple of it). Hats (42) every 2nd: period 2. Kick
// (36) on steps 0 and 10 of every bar: period 16. 4 lane windows, 2 odd: 500 permille.
{
  const d = [];
  for (let s = 0; s < 128; s++) {
    if (s % 3 === 0) d.push([s * S, 30, 56, 70]);
    if (s % 24 === 0) d.push([s * S, 30, 37, 70]);
    if (s % 2 === 0) d.push([s * S, 30, 42, 70]);
    if (s % 16 === 0 || s % 16 === 10) d.push([s * S, 30, 36, 110]);
  }
  files['polymeter.mid'] = song({ bars: 8, tracks: [{ name: 'Drums', ch: 9, notes: d }] });
  expected.r9 = { file: 'polymeter.mid', lane_windows: 4, odd_cycles: 2, odd_cycles_pm: 500 };
}

/* ---------- dup_a/dup_b.mid: R10 cross-seed duplicate motifs ---------- */
// 4 bars each, two 2-bar windows. The motif (step 0, +0), (step 6, +3), (step 16, +7) is A's window 0
// (60 63 67) and, 5 semitones up, B's window 1 (65 68 72 from bar 2). A's window 1 and B's window 0
// are different. 4 windows, 2 duplicated: 500 permille.
{
  const motif = (bar, base) => [[bar * BAR, 100, base, 90], [bar * BAR + 6 * S, 100, base + 3, 90], [(bar + 1) * BAR, 100, base + 7, 90]];
  files['dup_a.mid'] = song({ bars: 4, tracks: [{ name: 'Lead', ch: 3, notes: [...motif(0, 60), [2 * BAR, 100, 72, 90], [2 * BAR + 4 * S, 100, 74, 90]] }] });
  files['dup_b.mid'] = song({ bars: 4, tracks: [{ name: 'Lead', ch: 3, notes: [[0, 100, 70, 90], [8 * S, 100, 69, 90], ...motif(2, 65)] }] });
  expected.r10 = { files: ['dup_a.mid', 'dup_b.mid'], windows: 4, duplicated: 2, duplicated_pm: 500 };
}

/* ---------- R11 LHL syncopation: values of the bar function (no file) ---------- */
// Weights per 16th: 0 -4 -3 -4 -2 -4 -3 -4 -1 -4 -3 -4 -2 -4 -3 -4; the bar wraps to the next downbeat.
//   0 4 8 12: no rest is stronger than the note before it: 0
//   2: rests 3..17 include the next downbeat (0): 0 - (-3) = 3
//   0 6: after 0 the rests 1-5 are weaker; after 6 (-3) the strongest rest is 8 (-1): 2
//   3 7 11 15: 4 (-2) after 3 (-4): 2; 8 (-1) after 7: 3; 12 (-2) after 11: 2; the next downbeat
//     (0) after 15 (-4): 4. Total 11
expected.r11 = { cases: [{ onsets: '0,4,8,12', value: 0 }, { onsets: '2', value: 3 }, { onsets: '0,6', value: 2 }, { onsets: '3,7,11,15', value: 11 }] };

/* ---------- riser.mid: I2 risers and rolls on the swung grid ---------- */
// "build" bars 0-1. Hats with odd 16ths 20 late (16 odd onsets at +20). A "Riser" on every 16th of
// bar 0, straight, and a snare roll (38) on the 8 16ths of bar 1's second half, straight: their 8 + 4
// odd notes add 12 offsets of 0, so the file's swing is the lower median (index 13) of 12 zeros and 16
// twenties: 20. Riser: 8 even notes on the swung grid, 8 odd ones straight only. Snare: 4 even (swung
// grid), 4 odd (straight only).
{
  const d = [];
  for (let b = 0; b < 2; b++) for (let st = 0; st < 16; st++) d.push([b * BAR + st * S + (st % 2 ? 20 : 0), 30, 42, 70]);
  for (let st = 8; st < 16; st++) d.push([BAR + st * S, 60, 38, 80]);
  files['riser.mid'] = song({ bars: 2, markers: [[0, 'build']], tracks: [{ name: 'Drums', ch: 9, notes: d }, { name: 'Riser', ch: 6, notes: range(16, st => [st * S, 100, 60, 70]) }] });
  expected.i2 = { file: 'riser.mid', riser: { notes: 16, swung: 8, straight_only: 8 }, build_snare: { notes: 8, swung: 4, straight_only: 4 } };
}

/* ---------- lock.mid: I3 kick and bass, I5 kick anchors ---------- */
// Kicks at 0 480 960 1440 1923 2100. Bass at 0 (kick 0 away), 482 (2), 965 (5), 1200 (no kick within 30),
// 1470 (30 from the kick at 1440: a pair, not tight). Pairs 4, within 2 ticks 2.
// Kicks near a beat (within 30): 0 480 960 1440 1923 (3 from 1920); 2100 is 180 from a beat. 5 near,
// 4 within 2 ticks.
{
  files['lock.mid'] = song({ bars: 2, tracks: [
    { name: 'Drums', ch: 9, notes: [0, 480, 960, 1440, 1923, 2100].map(t => [t, 60, 36, 110]) },
    { name: 'Bass', ch: 1, notes: [0, 482, 965, 1200, 1470].map(t => [t, 100, 36, 100]) }
  ] });
  expected.i3 = { file: 'lock.mid', pairs: 4, within_2_ticks: 2 };
  expected.i5 = { file: 'lock.mid', kicks_near_beats: 5, within_2_ticks: 4 };
}

/* ---------- offline.mid: I4 sections on 8-bar lines ---------- */
// Markers at bars 0 and 12: one of 2 sections starts on an 8-bar line.
files['offline.mid'] = song({ bars: 16, markers: [[0, 'intro'], [12, 'drop']], tracks: [{ name: 'Drums', ch: 9, notes: [[0, 60, 36, 100]] }] });
expected.i4 = { file: 'offline.mid', sections: 2, on_8_bar_lines: 1 };

/* ---------- names: section kinds and lane roles (no file) ---------- */
expected.names = {
  section_kinds: [
    { text: 'drop', kind: 'drop' }, { text: 'drop2 (four to the floor)', kind: 'drop2' }, { text: 'break', kind: 'break' },
    { text: 'Intro 2', kind: 'groove' }, { text: 'Intro (DJ)', kind: 'intro' }, { text: 'Drop 2', kind: 'drop' }, { text: 'Breakdown', kind: 'break' },
    { text: 'Piano intro', kind: 'groove' }, { text: 'Drop', kind: 'drop' }, { text: 'mystery', kind: 'unknown' }
  ],
  roles: [
    { name: 'Bass', channel9: 0, slot: '', role: 'bass' }, { name: 'Sub', channel9: 0, slot: '', role: 'bass' }, { name: 'Stabs', channel9: 0, slot: '', role: 'chords' },
    { name: 'Pad (breakdown)', channel9: 0, slot: '', role: 'chords' }, { name: 'Lead', channel9: 0, slot: '', role: 'lead' }, { name: 'Melody', channel9: 0, slot: '', role: 'lead' },
    { name: 'Vox', channel9: 0, slot: '', role: 'vox' }, { name: 'FX', channel9: 0, slot: '', role: 'fx' }, { name: 'Riser (builds)', channel9: 0, slot: '', role: 'fx' },
    { name: 'Kick', channel9: 1, slot: '', role: 'drums' }, { name: 'Sidechain Trigger (ghost kick, note 35)', channel9: 1, slot: '', role: 'ignore' },
    { name: 'Break Slices (note 36 = slice 1, one 16th each)', channel9: 0, slot: '', role: 'other' },
    { name: 'Pirate Signal Bass A (F minor, 134 BPM)', channel9: 0, slot: 'Bass-A', role: 'bass' },
    { name: 'Pirate Signal Melody B (F minor, 134 BPM)', channel9: 0, slot: 'Melody-B', role: 'lead' },
    { name: 'Pirate Signal Drums Fill (F minor, 134 BPM)', channel9: 1, slot: 'Drums-Fill', role: 'drums' },
    { name: 'Bass', channel9: 0, slot: 'FullSong', role: 'bass' }
  ],
  drum_groups: [
    { key: 35, group: 'kick' }, { key: 36, group: 'kick' }, { key: 37, group: 'rim' }, { key: 38, group: 'snare' }, { key: 39, group: 'clap' },
    { key: 40, group: 'snare' }, { key: 42, group: 'hats' }, { key: 46, group: 'hats' }, { key: 49, group: 'cymbal' }, { key: 50, group: 'tom' },
    { key: 51, group: 'hats' }, { key: 54, group: 'hats' }, { key: 56, group: 'perc' }, { key: 63, group: 'perc' }, { key: 70, group: 'hats' }, { key: 76, group: 'perc' }
  ]
};

/* ---------- write ---------- */
function main() {
  const i = process.argv.indexOf('--out');
  const out = i > 0 && process.argv[i + 1] ? path.resolve(process.argv[i + 1]) : __dirname;
  fs.mkdirSync(out, { recursive: true });
  for (const name of Object.keys(files).sort()) fs.writeFileSync(path.join(out, name), files[name]);
  const doc = Object.assign({ about: 'Known answers for the fixture files, worked out by hand in make_fixtures.js. Integers and strings only.' }, expected);
  fs.writeFileSync(path.join(out, 'expected.json'), JSON.stringify(doc, null, 1) + '\n');
  console.log('make_fixtures: wrote ' + Object.keys(files).length + ' MIDI files and expected.json into ' + out);
}

if (require.main === module) main();
module.exports = { files, expected };
