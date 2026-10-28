// Export the new kits' parts as note lists (seconds) for the wave-8 loop renders.
const P = require('./core.js');
const fs = require('fs');
const out = {};
for (const kit of P.KITS.filter(k => /^(GV|TH)\d\d$/.test(k.id))) {
  const spb = 60 / kit.bpm, tick = spb / 480;
  const key = P.keyInfo(kit.root, kit.mode);
  const o = { id: kit.id, name: kit.name, bpm: kit.bpm, swing: kit.swing, root: kit.root, mode: kit.mode, key: key.label, camelot: key.camelot, short: key.short, sound: kit.sound, parts: {}, drums: {} };
  for (const part of ['bass', 'chords', 'lead']) for (const v of ['A', 'B']) {
    const c = P.compilePart(kit, part, v, 0);
    o.parts[part + v] = c.events.map(e => ({ t: e.t * tick, d: e.d * tick, n: e.n, v: e.v, legato: !!e.legato }));
  }
  for (const v of ['main', 'var', 'fill']) {
    const d = P.compileDrums(kit, v);
    o.drums[v] = d.events.map(e => ({ t: e.t * tick, lane: e.lane, v: e.v }));
  }
  o.barSec = 4 * spb;
  out[kit.id] = o;
}
fs.writeFileSync('out/w8_parts.json', JSON.stringify(out));
console.log(Object.keys(out).join(' '));
