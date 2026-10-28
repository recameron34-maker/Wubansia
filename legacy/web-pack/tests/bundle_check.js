const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// every bundled sound decodes bit-exact from its shared file: compare against the source WAVs
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
for (const [id, e] of Object.entries(map)) {
  if (!e.bundle) continue;
  const f = /^flac:([^?]+)\?ch=([\d,]+)&n=(\d+)$/.exec(e.file);
  const path = PACK_ROOT + '/site/' + f[1];
  const d = cache[path] || (cache[path] = flacDecode(new Uint8Array(fs.readFileSync(path))));
  const want = wav(PACK_ROOT + '/out/vox_src/vox/' + id + '.wav'), chs = f[2].split(',').map(Number), len = +f[3];
  let ok = len === want[0].length && chs.every((k, j) => { for (let i = 0; i < len; i++) if (d.data[k][i] !== want[j][i]) return false; return true; });
  if (e.dry) {
    const g = /^flac:([^?]+)\?ch=(\d+)&n=(\d+)$/.exec(e.dry), dw = wav(PACK_ROOT + '/out/vox_src/vox_dry/' + id + '.wav')[0];
    for (let i = 0; i < +g[3] && ok; i++) if (d.data[+g[2]][i] !== dw[i]) ok = false;
    ok = ok && +g[3] === dw.length;
  }
  n++; if (!ok) { bad++; console.log('MISMATCH', id); }
  else console.log('ok', id, e.file.replace(/^flac:audio\/voxf\//, ''), e.dry ? '+dry' : '', d.ch + 'ch');
}
console.log(n, 'bundled sounds,', bad, 'mismatches');
