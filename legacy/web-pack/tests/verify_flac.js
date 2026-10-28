const PACK_ROOT = process.env.PACK_ROOT || require('path').resolve(__dirname, '..');   // the legacy web-pack folder
// Every vocal FLAC on the site decodes (with the page's own decoder) to exactly the WAV sources it was made from:
// wet left/right and dry, sample for sample, at the frame counts the manifest URLs give.
const { flacDecode } = require(PACK_ROOT + '/flac.js');
const fs = require('fs');
const W = PACK_ROOT;
const map = JSON.parse(fs.readFileSync(W + '/out/vox_flac.json'));
function wav(path) {   // 16-bit PCM WAV -> { ch, n, data: Int16Array interleaved }
  const b = fs.readFileSync(path);
  let p = 12, fmt = null, data = null;
  while (p + 8 <= b.length) {
    const id = b.toString('ascii', p, p + 4), sz = b.readUInt32LE(p + 4);
    if (id === 'fmt ') fmt = { ch: b.readUInt16LE(p + 10), sr: b.readUInt32LE(p + 12), bits: b.readUInt16LE(p + 22) };
    if (id === 'data') { data = b.subarray(p + 8, p + 8 + sz); break; }
    p += 8 + sz + (sz & 1);
  }
  if (!fmt || fmt.bits !== 16) throw new Error('not 16-bit PCM: ' + path);
  const pcm = new Int16Array(data.buffer.slice(data.byteOffset, data.byteOffset + data.length));
  return { ch: fmt.ch, n: pcm.length / fmt.ch, data: pcm };
}
let bad = 0, n = 0, ms = 0, bytes = 0, wavBytes = 0;
for (const [id, e] of Object.entries(map)) {
  const m = /^flac:([^?]+)\?ch=([\d,]+)&n=(\d+)$/.exec(e.file);
  const buf = fs.readFileSync(W + '/site/' + m[1]);
  const t = Date.now();
  const d = flacDecode(buf);
  ms += Date.now() - t; bytes += buf.length; wavBytes += e.wav_bytes;
  const w = wav(`${W}/out/vox_src/vox/${id}.wav`);
  const errs = [];
  if (+m[3] !== w.n) errs.push('wet frames ' + m[3] + ' vs ' + w.n);
  for (let i = 0; i < w.n && errs.length < 3; i++) for (let c = 0; c < 2; c++) if (d.data[c][i] !== w.data[i * 2 + c]) { errs.push(`wet ch${c} @${i}: ${d.data[c][i]} vs ${w.data[i * 2 + c]}`); break; }
  if (e.dry) {
    const md = /^flac:[^?]+\?ch=2&n=(\d+)$/.exec(e.dry), dr = wav(`${W}/out/vox_src/vox_dry/${id}.wav`);
    if (+md[1] !== dr.n) errs.push('dry frames ' + md[1] + ' vs ' + dr.n);
    for (let i = 0; i < dr.n && errs.length < 3; i++) if (d.data[2][i] !== dr.data[i]) { errs.push(`dry @${i}: ${d.data[2][i]} vs ${dr.data[i]}`); break; }
    for (let i = dr.n; i < d.n && errs.length < 3; i++) if (d.data[2][i] !== 0) { errs.push('dry padding not silent'); break; }
  } else if (d.ch !== 2) errs.push('expected 2 channels, got ' + d.ch);
  n++;
  if (errs.length) { bad++; console.log('MISMATCH', id, errs.join('; ')); }
}
console.log(`${n} vocal FLACs checked, ${bad} mismatches; ${(bytes / 1e6).toFixed(1)} MB FLAC for ${(wavBytes / 1e6).toFixed(1)} MB of WAV; decode ${ms} ms total (${(ms / n).toFixed(0)} ms each)`);
process.exit(bad ? 1 : 0);
