#!/usr/bin/env node
/* Derives legacy_sections.json from frozen legacy/web-pack/core.js: every section name its full songs
   (buildSong) and extended mixes (buildExtended) write as a marker, with core.js's own section kind.
   core.js's "breakdown" is called "break" here, as in generator v2. It reads core.js and never changes it.

   Usage: node tools/legacy_audit/section_table.js           print the table
          node tools/legacy_audit/section_table.js --check   exit 1 unless legacy_sections.json matches

   The audit itself reads only the MIDI files and this table: a marker's text is all a file says about
   its section, and core.js keeps the kind out of the file. */
'use strict';

const fs = require('fs');
const path = require('path');

const CORE_JS = path.resolve(__dirname, '..', '..', 'legacy', 'web-pack', 'core.js');
const TABLE = path.join(__dirname, 'legacy_sections.json');

function derive() {
  const P = require(CORE_JS);
  const kinds = new Map();
  for (const kit of P.KITS) {
    for (const sg of [P.buildSong(kit, kit.root), P.buildExtended(kit, kit.root)]) {
      for (const s of sg.sections) {
        const kind = s.kind === 'breakdown' ? 'break' : s.kind;
        if (kinds.has(s.name) && kinds.get(s.name) !== kind) throw new Error('section "' + s.name + '" has two kinds: ' + kinds.get(s.name) + ', ' + kind);
        kinds.set(s.name, kind);
      }
    }
  }
  const sorted = [...kinds.keys()].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));
  const table = {
    about: 'Section kind of every marker name in the legacy full songs and extended mixes, from core.js (breakdown is called break). Made by section_table.js; never edit by hand.',
    kinds: Object.fromEntries(sorted.map(n => [n, kinds.get(n)]))
  };
  return JSON.stringify(table, null, 1) + '\n';
}

if (require.main === module) {
  const text = derive();
  if (process.argv.includes('--check')) {
    const now = fs.readFileSync(TABLE, 'utf8');
    if (now !== text) { console.error('section_table: legacy_sections.json differs from core.js'); process.exit(1); }
    console.log('section_table: legacy_sections.json matches core.js');
  } else process.stdout.write(text);
}

module.exports = { derive };
