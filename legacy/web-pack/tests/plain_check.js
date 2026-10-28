const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// every plain WAV now served from a FLAC time sprite: decode the sprite, cut the slice, rebuild the WAV the page saves
// (44-byte header + PCM) and compare it byte for byte with the original file
const fs = require('fs');
const { flacDecode } = require(PACK_ROOT + '/flac.js');
const map = JSON.parse(fs.readFileSync(PACK_ROOT + '/out/plain_flac.json', 'utf8'));
const cache = {};
let bad = 0, n = 0;
for (const [rel, url] of Object.entries(map)) {
  const f = /^flac:([^?]+)\?ch=([\d,]+)&n=(\d+)&o=(\d+)$/.exec(url);
  const d = cache[f[1]] || (cache[f[1]] = flacDecode(new Uint8Array(fs.readFileSync(PACK_ROOT + '/site/' + f[1]))));
  const chs = f[2].split(',').map(Number), len = +f[3], o = +f[4], c = chs.length;
  const out = Buffer.alloc(44 + len * 2 * c);
  out.write('RIFF', 0); out.writeUInt32LE(36 + len * 2 * c, 4); out.write('WAVE', 8); out.write('fmt ', 12); out.writeUInt32LE(16, 16); out.writeUInt16LE(1, 20); out.writeUInt16LE(c, 22);
  out.writeUInt32LE(44100, 24); out.writeUInt32LE(44100 * 2 * c, 28); out.writeUInt16LE(2 * c, 32); out.writeUInt16LE(16, 34); out.write('data', 36); out.writeUInt32LE(len * 2 * c, 40);
  for (let i = 0, p = 44; i < len; i++) for (const k of chs) { out.writeInt16LE(d.data[k][o + i], p); p += 2; }
  const ref = fs.readFileSync(PACK_ROOT + '/out/plain_wav/' + rel.replace(/^audio\//, ''));
  n++; if (Buffer.compare(out, ref) !== 0) { bad++; console.log('MISMATCH', rel); }
}
console.log(n, 'plain files rebuilt from sprites,', bad, 'mismatches');
process.exit(bad ? 1 : 0);
