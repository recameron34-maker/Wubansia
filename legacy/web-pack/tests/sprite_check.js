const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// v20: every stab decodes bit-exact out of its time sprite (wet pair and dry, at its offset), and every FLAC sample sprite
// decodes to exactly the PCM of the WAV sprite it replaced.
const fs = require('fs');
const { flacDecode } = require(PACK_ROOT + '/flac.js');
const map = JSON.parse(fs.readFileSync(PACK_ROOT + '/out/vox_flac.json', 'utf8'));
function wav(p) {
  const b = fs.readFileSync(p); let o = 12, ch = 0, data = null;
  while (o + 8 <= b.length) { const id = b.toString('ascii', o, o + 4), sz = b.readUInt32LE(o + 4); if (id === 'fmt ') ch = b.readUInt16LE(o + 10); if (id === 'data') data = b.subarray(o + 8, o + 8 + sz); o += 8 + sz + (sz & 1); }
  const n = data.length / 2 / ch, out = [...Array(ch)].map(() => new Int16Array(n));
  for (let i = 0; i < n; i++) for (let c = 0; c < ch; c++) out[c][i] = data.readInt16LE((i * ch + c) * 2);
  return out;
}
const cache = {}; let bad = 0, n = 0;
const dec = p => cache[p] || (cache[p] = flacDecode(new Uint8Array(fs.readFileSync(PACK_ROOT + '/site/' + p))));
const same = (a, a0, b, len) => { for (let i = 0; i < len; i++) if (a[a0 + i] !== b[i]) return false; return true; };
for (const [id, e] of Object.entries(map)) {
  if (!e.sprite) continue;
  const f = /^flac:([^?]+)\?ch=(\d),(\d)&n=(\d+)&o=(\d+)$/.exec(e.file), g = /^flac:([^?]+)\?ch=(\d)&n=(\d+)&o=(\d+)$/.exec(e.dry);
  const d = dec(f[1]), w = wav(PACK_ROOT + '/out/vox_src/vox/' + id + '.wav'), dw = wav(PACK_ROOT + '/out/vox_src/vox_dry/' + id + '.wav')[0];
  const ok = f[1] === g[1] && f[5] === g[4] && +f[4] === w[0].length && +g[3] === dw.length && same(d.data[+f[2]], +f[5], w[0], w[0].length) && same(d.data[+f[3]], +f[5], w[1], w[1].length) && same(d.data[+g[2]], +g[4], dw, dw.length);
  n++; if (!ok) { bad++; console.log('MISMATCH', id); }
}
console.log(n, 'stabs from time sprites,', bad, 'mismatches');
let sb = 0, sn = 0;
for (const dir of ['s3', 's4']) for (const fn of fs.readdirSync(PACK_ROOT + '/site/audio/' + dir).filter(x => x.endsWith('.ogg'))) {
  const d = dec('audio/' + dir + '/' + fn), w = wav(PACK_ROOT + '/out/sprites_wav/' + fn.replace(/\.ogg$/, '.wav'));
  const ok = d.ch === w.length && d.n === w[0].length && w.every((c, k) => same(d.data[k], 0, c, c.length));
  sn++; if (!ok) sb++; console.log((ok ? 'ok ' : 'MISMATCH ') + fn + ': ' + d.ch + ' ch, ' + d.n + ' frames');
}
console.log(sn, 'sample sprites,', sb, 'mismatches');
process.exit(bad + sb ? 1 : 0);
