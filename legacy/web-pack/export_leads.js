const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '.');   // the legacy web-pack folder
const P = require('./core.js');
const out = {};
for (const k of P.KITS) {
  const c = P.compileMelodic(k, 'lead', 'A', 0);
  out[k.id] = { name: k.name, bpm: k.bpm, root: k.root, mode: k.mode, key: P.keyInfo(k.root, k.mode),
    lead: c.events.filter(e => e.t < 2 * 1920).map(e => ({ t: e.t, d: e.d, n: e.n })) };
}
require('fs').writeFileSync(PACK_ROOT + '/out/leads.json', JSON.stringify(out, null, 1));
console.log(Object.keys(out).length, 'kits exported');
