/* A small FLAC decoder (integer PCM up to 24 bits, any channel count, fixed or variable block size, native or in an Ogg
   container): enough for the pack's vocals, which are stored as one 3-channel Ogg FLAC per vocal (wet left, wet right, dry). Returns { sr, ch, bps, n, data } where
   data[c] is an Int32Array of n samples. Bit-exact: residuals are summed in plain doubles (exact below 2^53), so LPC sums of
   16-bit audio never overflow. Checks each frame's CRC-16 and throws on a mismatch. */
function flacFromOgg(u8) {
  /* FLAC in an Ogg container (Xiph mapping: a 0x7F 'FLAC' packet carrying STREAMINFO, header packets, then one FLAC frame
     per packet) -> the native stream: 'fLaC', STREAMINFO marked as the last metadata block, then the frames. */
  const packets = [];
  let p = 0, cur = [], curLen = 0;
  while (p + 27 <= u8.length) {
    if (u8[p] !== 0x4f || u8[p + 1] !== 0x67 || u8[p + 2] !== 0x67 || u8[p + 3] !== 0x53) throw new Error('Ogg: lost page sync at byte ' + p);
    const nseg = u8[p + 26];
    let q = p + 27 + nseg;
    for (let i = 0; i < nseg; i++) {
      const l = u8[p + 27 + i];
      cur.push(u8.subarray(q, q + l)); curLen += l; q += l;
      if (l < 255) {
        const pk = new Uint8Array(curLen);
        let o = 0;
        cur.forEach(c => { pk.set(c, o); o += c.length; });
        packets.push(pk); cur = []; curLen = 0;
      }
    }
    p = q;
  }
  const h = packets[0];
  if (!h || h[0] !== 0x7f || String.fromCharCode(h[1], h[2], h[3], h[4]) !== 'FLAC') throw new Error('Ogg: not an Ogg FLAC stream');
  const nhead = (h[7] << 8) | h[8];
  const si = h.subarray(9 + 4, 9 + 4 + 38);          // STREAMINFO block (4-byte header + 34 bytes)
  const frames = packets.slice(1 + nhead);
  const total = 4 + si.length + frames.reduce((a, f) => a + f.length, 0);
  const out = new Uint8Array(total);
  out.set([0x66, 0x4c, 0x61, 0x43], 0);
  out.set(si, 4);
  out[4] = (out[4] & 0x7f) | 0x80;                     // the only metadata block now
  let o = 4 + si.length;
  frames.forEach(f => { out.set(f, o); o += f.length; });
  return out;
}
function flacDecode(bytes) {
  let u8 = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (u8[0] === 0x4f && u8[1] === 0x67 && u8[2] === 0x67 && u8[3] === 0x53) u8 = flacFromOgg(u8);
  let pos = 0, bit = 0;                        // byte position and bit offset (0 = MSB) within it
  const need = k => { if (pos + Math.ceil((bit + k) / 8) > u8.length) throw new Error('FLAC: unexpected end of data'); };
  function bits(k) {                           // unsigned read, k <= 32
    if (k === 0) return 0;
    need(k);
    let v = 0;
    while (k > 0) {
      const avail = 8 - bit, take = Math.min(avail, k);
      const b = (u8[pos] >> (avail - take)) & ((1 << take) - 1);
      v = v * (1 << take) + b;
      bit += take; k -= take;
      if (bit === 8) { bit = 0; pos++; }
    }
    return v;
  }
  function sbits(k) {                          // two's complement signed read
    if (k === 0) return 0;
    const v = bits(k), half = Math.pow(2, k - 1);
    return v >= half ? v - 2 * half : v;
  }
  function unary() {                           // count of 0 bits before the next 1
    let n = 0;
    for (;;) {
      if (pos >= u8.length) throw new Error('FLAC: unexpected end of data');
      const rest = u8[pos] & (0xff >> bit);
      if (rest) {
        const lead = Math.clz32(rest) - 24 - bit;
        n += lead; bit += lead + 1;
        if (bit === 8) { bit = 0; pos++; }
        return n;
      }
      n += 8 - bit; bit = 0; pos++;
    }
  }
  const align = () => { if (bit) { bit = 0; pos++; } };

  // ---- stream header and metadata
  if (u8[0] !== 0x66 || u8[1] !== 0x4c || u8[2] !== 0x61 || u8[3] !== 0x43) throw new Error('FLAC: not a FLAC stream');
  pos = 4;
  let sr = 0, ch = 0, bps = 0, total = 0, maxBlock = 0, last = false;
  while (!last) {
    last = !!bits(1);
    const type = bits(7), len = bits(24);
    if (type === 0) {
      const p0 = pos;
      bits(16); maxBlock = bits(16); bits(24); bits(24);
      sr = bits(20); ch = bits(3) + 1; bps = bits(5) + 1; total = bits(36);
      pos = p0 + len;
    } else pos += len;
  }
  if (!sr) throw new Error('FLAC: no STREAMINFO');
  const data = [];
  for (let c = 0; c < ch; c++) data.push(new Int32Array(total));

  // CRC-16 (poly 0x8005) over each frame, checked at the frame end
  const T16 = flacDecode.T16 || (flacDecode.T16 = (() => {
    const t = new Uint16Array(256);
    for (let i = 0; i < 256; i++) { let c = i << 8; for (let k = 0; k < 8; k++) c = (c & 0x8000) ? ((c << 1) ^ 0x8005) & 0xffff : (c << 1) & 0xffff; t[i] = c; }
    return t;
  })());
  const crc16 = (a, b) => { let c = 0; for (let i = a; i < b; i++) c = ((c << 8) & 0xffff) ^ T16[(c >> 8) ^ u8[i]]; return c; };

  const BS = [0, 192, 576, 1152, 2304, 4608, -8, -16, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768];
  const SS = [0, 8, 12, 0, 16, 20, 24, 32];
  let out = 0;
  const res = new Float64Array(Math.max(maxBlock, 65536));
  while (out < total && pos < u8.length - 2) {
    const f0 = pos;
    const sync = bits(14);
    if (sync !== 0x3ffe) throw new Error('FLAC: lost frame sync at byte ' + f0);
    bits(1); bits(1);                          // reserved, blocking strategy
    const bsCode = bits(4), srCode = bits(4), chCode = bits(4), ssCode = bits(3);
    bits(1);
    // UTF-8 style coded frame/sample number
    let b0 = bits(8), extra = 0;
    if (b0 >= 0xc0) { extra = b0 >= 0xfe ? 6 : b0 >= 0xfc ? 5 : b0 >= 0xf8 ? 4 : b0 >= 0xf0 ? 3 : b0 >= 0xe0 ? 2 : 1; }
    for (let i = 0; i < extra; i++) bits(8);
    let bs = BS[bsCode];
    if (bs === -8) bs = bits(8) + 1; else if (bs === -16) bs = bits(16) + 1;
    if (srCode === 12) bits(8); else if (srCode === 13 || srCode === 14) bits(16);
    bits(8);                                   // header CRC-8 (the frame CRC-16 below covers it too)
    const fbps = ssCode ? SS[ssCode] : bps;
    const nch = chCode < 8 ? chCode + 1 : 2;
    const sub = [];
    for (let c = 0; c < nch; c++) {
      const sideBps = fbps + ((chCode === 8 && c === 1) || (chCode === 9 && c === 0) || (chCode === 10 && c === 1) ? 1 : 0);
      sub.push(subframe(bs, sideBps));
    }
    align();
    const crc = bits(16);
    if (crc16(f0, pos - 2) !== crc) throw new Error('FLAC: CRC mismatch in frame at byte ' + f0);
    const n = Math.min(bs, total - out);
    if (chCode < 8) {
      for (let c = 0; c < nch; c++) data[c].set(sub[c].subarray(0, n), out);
    } else {
      const a = sub[0], b = sub[1], L = data[0], R = data[1];
      for (let i = 0; i < n; i++) {
        if (chCode === 8) { L[out + i] = a[i]; R[out + i] = a[i] - b[i]; }
        else if (chCode === 9) { L[out + i] = a[i] + b[i]; R[out + i] = b[i]; }
        else { let m = a[i] * 2 + (b[i] & 1); L[out + i] = (m + b[i]) >> 1; R[out + i] = (m - b[i]) >> 1; }
      }
    }
    out += n;
  }
  if (out < total) throw new Error('FLAC: stream ended early');
  return { sr, ch, bps, n: total, data };

  function subframe(bs, sbps) {
    bits(1);
    const type = bits(6);
    let wasted = 0;
    if (bits(1)) wasted = unary() + 1;
    const b = sbps - wasted;
    const s = new Int32Array(bs);
    if (type === 0) { const v = sbits(b); s.fill(v); }
    else if (type === 1) { for (let i = 0; i < bs; i++) s[i] = sbits(b); }
    else if (type >= 8 && type <= 12) {
      const order = type - 8;
      for (let i = 0; i < order; i++) s[i] = sbits(b);
      residual(bs, order);
      for (let i = order; i < bs; i++) {
        const r = res[i];
        s[i] = order === 0 ? r : order === 1 ? r + s[i - 1] : order === 2 ? r + 2 * s[i - 1] - s[i - 2]
          : order === 3 ? r + 3 * s[i - 1] - 3 * s[i - 2] + s[i - 3] : r + 4 * s[i - 1] - 6 * s[i - 2] + 4 * s[i - 3] - s[i - 4];
      }
    } else if (type >= 32) {
      const order = type - 31;
      for (let i = 0; i < order; i++) s[i] = sbits(b);
      const prec = bits(4) + 1;
      if (prec === 16) throw new Error('FLAC: bad LPC precision');
      const shift = sbits(5);
      const coef = new Float64Array(order);
      for (let i = 0; i < order; i++) coef[i] = sbits(prec);
      residual(bs, order);
      const div = Math.pow(2, Math.max(0, shift));
      for (let i = order; i < bs; i++) {
        let sum = 0;
        for (let j = 0; j < order; j++) sum += coef[j] * s[i - 1 - j];
        s[i] = res[i] + Math.floor(sum / div);
      }
    } else throw new Error('FLAC: reserved subframe type ' + type);
    if (wasted) for (let i = 0; i < bs; i++) s[i] *= Math.pow(2, wasted);
    return s;
  }
  function residual(bs, order) {
    const method = bits(2);
    if (method > 1) throw new Error('FLAC: reserved residual coding');
    const pb = method ? 5 : 4, esc = method ? 31 : 15;
    const porder = bits(4), parts = 1 << porder;
    let i = order;
    for (let p = 0; p < parts; p++) {
      const cnt = porder === 0 ? bs - order : p === 0 ? (bs >> porder) - order : bs >> porder;
      const k = bits(pb);
      if (k === esc) {
        const nb = bits(5);
        for (let j = 0; j < cnt; j++) res[i++] = sbits(nb);
      } else {
        const m = Math.pow(2, k);
        for (let j = 0; j < cnt; j++) {
          const q = unary(), u = q * m + bits(k);
          res[i++] = (u % 2) ? -(u + 1) / 2 : u / 2;     // zigzag
        }
      }
    }
  }
}
if (typeof module !== 'undefined') module.exports = { flacDecode, flacFromOgg };
