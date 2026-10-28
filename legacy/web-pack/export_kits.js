// Export every kit (patterns compiled to events) plus a few extended-mix layers, for the audio renderers.
const P = require('./core.js'), fs = require('fs');
const out = {}, layers = {};
for (const k of P.KITS) {
  const parts = {};
  ['bass', 'chords', 'lead'].forEach(p => ['A', 'B'].forEach(v => {
    parts[p + v] = P.compileMelodic(k, p, v, 0).events.map(e => [e.t, e.d, e.n, e.v, e.legato ? 1 : 0, e.chord || '']);
  }));
  out[k.id] = { name: k.name, bpm: k.bpm, root: k.root, mode: k.mode, genre: k.genre, key: P.keyInfo(k.root, k.mode), swing: k.swing, chords: k.chords, parts, drums: k.drums, bd: k.song.bd };
  if (k.ext) {   // 4 bars of each generated layer from the first drop, starting at the drop's first bar
    const s = P.buildExtended(k, k.root), drop = s.sections.find(x => x.kind === 'drop');
    const t0 = drop.bar * P.BAR, t1 = t0 + 4 * P.BAR;
    layers[k.id] = {};
    Object.keys(s.T).forEach(trk => {
      const ev = s.T[trk].filter(e => e.t >= t0 && e.t < t1).map(e => [e.t - t0, e.d, e.n, e.v, e.legato ? 1 : 0, e.lane || '']);
      if (ev.length) layers[k.id][trk] = ev;
    });
    layers[k.id]._bends = {};
    Object.keys(s.BEND).forEach(trk => { const b = s.BEND[trk].filter(x => x.t >= t0 && x.t < t1).map(x => [x.t - t0, x.v]); if (b.length) layers[k.id]._bends[trk] = b; });
    layers[k.id]._cc = {};
    Object.keys(s.CC).forEach(trk => { const c = s.CC[trk].filter(x => x.t >= t0 && x.t < t1).map(x => [x.t - t0, x.cc, x.val]); if (c.length) layers[k.id]._cc[trk] = c; });
    layers[k.id]._drop = drop.name;
  }
}
fs.writeFileSync('out/kits.json', JSON.stringify(out));
fs.writeFileSync('out/ext_layers.json', JSON.stringify(layers));
console.log(Object.keys(out).length, 'kits;', Object.keys(layers).map(k => k + ':' + Object.keys(layers[k]).filter(x => x[0] !== '_').join('/')).join(' | '));
