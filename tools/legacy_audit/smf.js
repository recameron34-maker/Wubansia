/* Standard MIDI File reader for the legacy audit. It follows core/midi/reader.cpp rule for rule, so the
   audit and the C++ metrics read every file the same way (README.md, "Reading the files"):
   - formats 0 and 1 only, with a ticks-per-quarter-note division; format 0 must have exactly one track
   - MTrk chunks are read in order; any other chunk is skipped; bytes after the last track are ignored
   - delta times and lengths are variable-length numbers of at most 4 bytes
   - running status is kept after meta and sysex events too (a data byte there has no other reading)
   - a track ends at its end-of-track event (FF 2F); anything after it in the chunk is ignored, and a
     track without one is an error
   - notes pair first in, first out per channel and key: a note-off (8n, or 9n with velocity 0) ends the
     oldest sounding note on its channel and key; a note-off with nothing to end is ignored; a note still
     sounding at the end of the track ends on the track's end tick
   - notes sort by (tick, channel, key), stable, so notes that tie keep the order of their note-ons
   Anything malformed throws an Error that says what and at which byte. No float or double anywhere. */
'use strict';

function fail(where, message) {
  throw new Error(where + ': ' + message);
}

// A variable-length quantity of at most 4 bytes at p. Returns [value, nextP].
function varLen(d, p, where) {
  let v = 0;
  for (let i = 0; i < 4; i++) {
    if (p >= d.length) fail(where, 'truncated inside a variable-length number');
    const b = d[p++];
    v = v * 128 + (b & 0x7f);
    if ((b & 0x80) === 0) return [v, p];
  }
  fail(where, 'a variable-length number longer than 4 bytes');
}

const hex = b => '0x' + b.toString(16).toUpperCase().padStart(2, '0');

function readTrack(d, index, base) {
  const events = [];
  let p = 0, tick = 0, running = 0;
  const at = q => 'track ' + index + ', byte ' + (base + q);
  while (p < d.length) {
    const start = p;
    let delta;
    [delta, p] = varLen(d, p, at(start));
    tick += delta;
    if (p >= d.length) fail(at(p), 'truncated: the track ends after a delta time');
    let st = d[p];
    if (st < 0x80) {
      if (running === 0) fail(at(p), 'data byte ' + hex(st) + ' with no running status');
      st = running;
    } else p++;
    if (st < 0xf0) {
      const kind = st & 0xf0;
      const need = kind === 0xc0 || kind === 0xd0 ? 1 : 2;
      if (d.length - p < need) fail(at(p), 'truncated inside a channel message');
      for (let k = 0; k < need; k++) if (d[p + k] > 0x7f) fail(at(p + k), 'expected a data byte, found ' + hex(d[p + k]));
      events.push({ tick, status: st, data1: d[p], data2: need === 2 ? d[p + 1] : 0, payload: null });
      p += need;
      running = st;
      continue;
    }
    if (st === 0xff || st === 0xf0 || st === 0xf7) {
      let type = 0;
      if (st === 0xff) {
        if (p >= d.length) fail(at(p), 'truncated inside a meta event');
        type = d[p];
        if (type > 0x7f) fail(at(p), 'meta type ' + hex(type) + ' is above 0x7F');
        p++;
      }
      let length;
      [length, p] = varLen(d, p, at(p));
      if (d.length - p < length) fail(at(p), 'truncated: an event of ' + length + ' bytes with ' + (d.length - p) + ' left');
      if (st === 0xff && type === 0x2f) return { events, endTick: tick };
      events.push({ tick, status: st, data1: type, data2: 0, payload: d.subarray(p, p + length) });
      p += length;
      continue; // running status carries on, as in reader.cpp
    }
    fail(at(p - 1), 'status ' + hex(st) + " isn't allowed in a MIDI file");
  }
  fail('track ' + index, 'no end-of-track event');
}

const be32 = (b, i) => ((b[i] << 24) >>> 0) + (b[i + 1] << 16) + (b[i + 2] << 8) + b[i + 3];
const be16 = (b, i) => (b[i] << 8) | b[i + 1];
const isId = (b, i, id) => b[i] === id.charCodeAt(0) && b[i + 1] === id.charCodeAt(1) && b[i + 2] === id.charCodeAt(2) && b[i + 3] === id.charCodeAt(3);

// bytes: a Uint8Array (or Buffer). Returns { format, ppq, tracks: [{ events, endTick }] }.
function readSmf(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (b.length < 8 || !isId(b, 0, 'MThd')) fail('header', "not a MIDI file: it doesn't start with an MThd chunk");
  const headerLength = be32(b, 4);
  if (headerLength < 6) fail('header', 'the MThd header is ' + headerLength + ' bytes; it needs 6');
  if (b.length - 8 < headerLength) fail('header', 'truncated MThd header');
  const format = be16(b, 8), trackCount = be16(b, 10), division = be16(b, 12);
  if (format === 2) fail('header', "format 2 (independent sequences) isn't supported");
  if (format > 2) fail('header', 'format ' + format + " isn't a MIDI file format");
  if (division & 0x8000) fail('header', "SMPTE time division isn't supported");
  if (division === 0) fail('header', '0 ticks per quarter note');
  if (format === 0 && trackCount !== 1) fail('header', 'format 0 has one track; this file says ' + trackCount);
  const tracks = [];
  let pos = 8 + headerLength;
  while (tracks.length < trackCount) {
    if (b.length - pos < 8) fail('file', 'truncated after ' + tracks.length + ' of ' + trackCount + ' tracks');
    const length = be32(b, pos + 4);
    if (b.length - pos - 8 < length) fail('file', 'truncated chunk at byte ' + pos);
    if (isId(b, pos, 'MTrk')) tracks.push(readTrack(b.subarray(pos + 8, pos + 8 + length), tracks.length, pos + 8));
    pos += 8 + length;
  }
  return { format, ppq: division, tracks };
}

const utf8 = new TextDecoder('utf-8');
const textOf = e => utf8.decode(e.payload);

// The text of the track's first track-name event (FF 03), or ''.
function trackName(track) {
  for (const e of track.events) if (e.status === 0xff && e.data1 === 0x03) return textOf(e);
  return '';
}

const isNoteOn = e => e.status >= 0x80 && e.status < 0xf0 && (e.status & 0xf0) === 0x90 && e.data2 > 0;
const isNoteOff = e => e.status >= 0x80 && e.status < 0xf0 && ((e.status & 0xf0) === 0x80 || ((e.status & 0xf0) === 0x90 && e.data2 === 0));

// The track's notes: { tick, length, channel, key, velocity }, paired as described at the top.
function notesOf(track) {
  const notes = [];
  const sounding = new Map(); // channel * 128 + key -> indices into notes, oldest first
  for (const e of track.events) {
    if (isNoteOn(e)) {
      const k = (e.status & 0x0f) * 128 + e.data1;
      if (!sounding.has(k)) sounding.set(k, []);
      sounding.get(k).push(notes.length);
      notes.push({ tick: e.tick, length: 0, channel: e.status & 0x0f, key: e.data1, velocity: e.data2, order: notes.length });
    } else if (isNoteOff(e)) {
      const open = sounding.get((e.status & 0x0f) * 128 + e.data1);
      if (!open || open.length === 0) continue;
      const n = notes[open.shift()];
      n.length = e.tick - n.tick;
    }
  }
  for (const open of sounding.values()) for (const i of open) notes[i].length = Math.max(0, track.endTick - notes[i].tick);
  notes.sort((a, b) => a.tick - b.tick || a.channel - b.channel || a.key - b.key || a.order - b.order);
  return notes.map(({ tick, length, channel, key, velocity }) => ({ tick, length, channel, key, velocity }));
}

// Every meta event of one type across the file, by tick; on one tick, track order then file order.
function metasOf(file, type) {
  const found = [];
  file.tracks.forEach((t, ti) => t.events.forEach((e, ei) => { if (e.status === 0xff && e.data1 === type) found.push({ e, ti, ei }); }));
  found.sort((a, b) => a.e.tick - b.e.tick || a.ti - b.ti || a.ei - b.ei);
  return found.map(x => x.e);
}

const markersOf = file => metasOf(file, 0x06).map(e => ({ tick: e.tick, text: textOf(e) }));
const keySignaturesOf = file => metasOf(file, 0x59).filter(e => e.payload.length === 2)
  .map(e => ({ tick: e.tick, sharps: e.payload[0] > 127 ? e.payload[0] - 256 : e.payload[0], minor: e.payload[1] !== 0 }));

module.exports = { readSmf, trackName, notesOf, markersOf, keySignaturesOf };
