    /* ==== ext-spec kits: custom forms, the pack's vocals as guide tracks, extra instruments and pitch-bend moves ==== */
    const SEMI = 8192 / 12;   // bends are written for a ±12-semitone range (the file sets it with RPN 0)
    const bend = (trk, t, v) => BEND[trk].push({ t: Math.max(0, Math.round(t)), v: Math.max(-8192, Math.min(8191, Math.round(v))) });
    const DRUMK = ['kick', 'snare', 'hats', 'perc', 'fills', 'breaks', 'poly', 'micro'];
    const NOTMEL = new Set(DRUMK.concat(['fx', 'shouts', 'voxguide', 'sidechain']));
    const clearRange = (keys, t0, t1) => keys.forEach(k => { T[k] = T[k].filter(e => !(e.t >= t0 && e.t < t1)); });
    const evIn = (trk, t0, t1) => T[trk].filter(e => e.t >= t0 && e.t < t1).sort((a, b) => a.t - b.t);

    function vocalBlock(id, b) {   // one of the pack's 4-bar hooks at bar b: guide notes, lyrics and a cue naming the audio file
      const V = VOCALS[id];
      if (!V) throw new Error(kit.id + ' unknown vocal ' + id);
      const src = KITS.find(k => k.id === V.kit), sw = src ? src.swing : kit.swing;
      const W = p => Math.round(warp(p, sw) * STEP);
      let d = ((kit.root - V.root) % 12 + 12) % 12; if (d > 6) d -= 12;
      const sh = d + tr + V.oct, t0 = b * BAR;
      META.voxguide.push({ t: t0, kind: 'cue', text: id + '.wav - ' + V.title });
      vocalBlocks.push([t0 - 130, t0 + 4 * BAR - 130]);
      cuts.push({ t0: t0 - 130, t1: t0 - 129, only: ['lead'] });
      vocalCues.push({ t: t0, id, bars: 4, bpm: src ? src.bpm : kit.bpm, shift: d + tr });
      V.mel.forEach((m, i) => {
        const talk = !!V.talk, step = m[0], nx = i + 1 < V.mel.length ? V.mel[i + 1][0] : 64;
        const len = talk ? Math.min(6, nx - step) : m[1], text = talk ? m[2] : m[3];
        const t = t0 + W(step), e = t0 + W(step + len);
        pushN('voxguide', t, e - t - 12, noteNum(talk ? m[1] : m[2]) + sh, 100);
        if (text !== '~') META.voxguide.push({ t, kind: 'lyric', text });
        vocalSpans.push([t - 120, e + 30]);
      });
    }
    function holdChords(trk, b0, n, H, lo, hi, max, vel, prev0) {   // one voicing per harmonic change, held
      let prev = prev0 || null, cur = null;
      const flush = () => { if (cur) cur.v.forEach(x => pushN(trk, cur.t, cur.end - cur.t - 30, x, vel)); cur = null; };
      for (let i = 0; i < n; i++) for (const st of [0, 8]) {
        const b = b0 + i, c = H(b, st), t = b * BAR + st * 120;
        if (cur && cur.c === c) { cur.end = t + 960; continue; }
        flush();
        const v = voice(c.pcs, prev, lo, hi, max); prev = v;
        cur = { c, t, end: t + 960, v };
      }
      flush();
      return prev;
    }

    /* -- drums and percussion -- */
    function breaksBar(b, pat, vf) {
      for (let s = 0; s < 16; s++) {
        const t = b * BAR + P16(s), e = b * BAR + P16(s + 1);
        pushN('breaks', t, e - t - 8, 36 + pat[s], (s % 4 === 0 ? 108 : s % 2 ? 80 : 94) * vf * (0.94 + R() * 0.12));
      }
    }
    function breaksLayer(b0, n, mode) {   // a one-bar break sliced into 16ths (slice k on note 36 + k), re-sequenced
      for (let i = 0; i < n; i++) {
        const b = b0 + i, ph = i % 8;
        if (mode === 'halftime') {   // half speed: the first eight slices, one per 8th
          for (let k = 0; k < 8; k++) { const t = b * BAR + P16(2 * k), e = b * BAR + P16(2 * k + 2); pushN('breaks', t, e - t - 10, 36 + k, (k % 4 === 0 ? 112 : k % 2 ? 78 : 96) * (0.94 + R() * 0.12)); }
          continue;
        }
        if (mode === 'roll') {
          if (i < n - 2) breaksBar(b, i % 2 ? SLICE.J2 : SLICE.J1, 0.55 + 0.35 * i / n);
          else if (i === n - 2) breaksBar(b, SLICE.J5, 0.9);
          else for (let k = 0; k < 32; k++) pushN('breaks', b * BAR + k * 60, 50, 40, 46 + 74 * k / 31);   // the break's snare in 32nds
          continue;
        }
        const vf = mode === 'layer' ? 0.6 : mode === 'jungle' ? 0.9 : 0.85;
        let pat = mode === 'straight' ? (ph === 3 ? SLICE.J3 : SLICE.J1) : mode === 'layer' ? [SLICE.J1, SLICE.J1, SLICE.J2, SLICE.J3][i % 4] : [SLICE.J2, SLICE.J3, SLICE.J4, SLICE.J2][i % 4];
        if (ph === 7 && mode !== 'layer') pat = SLICE.J5;
        breaksBar(b, pat, vf);
      }
      if (mode === 'straight') ccRamp('breaks', b0, n, 74, 45, 127, f => f * f);
      else ccSet('breaks', b0 * BAR, 74, 127);
    }
    function polyLayer(b0, n, dense, type) {   // polymetric percussion: 3-, 5- and 7-step cycles drifting against the bar
      const V = [['BH', 3, [0], 66], ['CLV', 5, [0, 2], 56], ['WB', 7, [0, 3], 50]];
      if (dense) V.push(['BL', 6, [0, 4], 58], ['COH', 9, [0, 5], 52]);
      V.forEach(([lane, cyc, hits, v]) => {
        for (let s = 0; s < n * 16; s++) {
          const k = s % cyc; if (!hits.includes(k)) continue;
          const i = Math.floor(s / 16), f = i / n;
          if (type === 'intro' && R() > 0.25 + f) continue;
          if (type === 'outro' && R() > 1.6 - 1.6 * f) continue;
          pushD(lane, (b0 + i) * BAR + P16(s % 16) + jit(4), v * (k === 0 ? 1 : 0.72) * (0.8 + R() * 0.4) * (type === 'breakdown' ? 0.85 : 1), 'poly', 40);
        }
      });
    }
    function microLayer(b0, n, glitch, type) {   // crackle: tiny clicks on 32nds; glitch adds ratchets
      for (let i = 0; i < n; i++) {
        if (type === 'intro' && i < n / 4) continue;
        const b = b0 + i, cnt = 2 + Math.floor(R() * 4);
        for (let k = 0; k < cnt; k++) pushD(pick(['WB', 'RIM', 'CLV']), b * BAR + Math.floor(R() * 32) * 60 + jit(6), 20 + R() * 34, 'micro', 24);
        if (glitch && i % 2 === 1) {
          const t0 = b * BAR + P16(pick([6, 10, 14])), rate = pick([60, 30]);
          for (let x = 0; x < 240; x += rate) pushD('RIM', t0 + x, 28 + 76 * x / 240, 'micro', Math.max(15, rate - 10));
        }
      }
    }
    function cowbellLayer(b0, n, type) {
      const CBP = ['..5..5....5..5..', '5..5..5...5..5..', '..5...5.5...5...', '......5..5..5.5.'];
      for (let i = 0; i < n; i++) {
        const b = b0 + i, s = CBP[(i + (type === 'switch' ? 1 : 0)) % 4];
        const vf = type === 'intro' ? 0.5 + 0.5 * i / n : type === 'outro' ? 1 - 0.5 * i / n : 1;
        for (let p = 0; p < 16; p++) if (s[p] !== '.') pushD('CB', b * BAR + P16(p) + jit(3), (13 + (+s[p]) * 12.7) * vf * (0.9 + R() * 0.2), 'perc');
        if (type === 'switch') [0, 8].forEach(p => pushD('CL', b * BAR + P16(p), 92, 'snare'));   // ghetto-house claps on every beat
      }
    }

    /* -- harmony instruments (H: the section's chord on any 16th) -- */
    function stringsLayer(b0, n, mode, H) {
      if (mode === 'pizz') {
        const SEQ = [0, 1, 2, 1, 3, 2, 1, 2];
        for (let i = 0; i < n; i++) {
          const b = b0 + i, up = (i >> 1) % 2;
          for (let k = 0; k < 8; k++) {
            if ((k === 3 || k === 7) && chance(0.3)) continue;
            const s = k * 2, tones = tonesIn(H(b, s), 62, 81);
            if (tones.length) pushN('strings', b * BAR + P16(s), 95, tones[Math.min(tones.length - 1, SEQ[k] + up)], (k % 2 ? 64 : 80) + R() * 8);
          }
        }
        ccSet('strings', b0 * BAR, 11, 100);
      } else if (mode === 'swell') {
        holdChords('strings', b0, n, H, 55, 79, 4, 84); ccRamp('strings', b0, n, 11, 24, 127, f => f * f);
      } else if (mode === 'pad') {
        holdChords('strings', b0, n, H, 55, 76, 4, 62); ccSet('strings', b0 * BAR, 11, 92);
      } else if (mode === 'legato') {
        holdChords('strings', b0, n, H, 55, 81, 4, 72);
        for (let k = 0; k < n; k += 4) ccRamp('strings', b0 + k, Math.min(4, n - k), 11, 64, 118, f => Math.pow(Math.sin(f * Math.PI), 0.7));
      } else if (mode === 'stabs') {   // orchestral string stabs
        const SR_ = [[0, 6, 10], [0, 3, 6, 14]];
        let prev = null;
        for (let i = 0; i < n; i++) {
          const b = b0 + i;
          SR_[i % 2].forEach((s, k) => { const v = voice(H(b, s).pcs, prev, 60, 84, 4); prev = v; v.forEach(x => pushN('strings', b * BAR + P16(s), 150, x, k ? 92 : 110)); });
        }
        ccSet('strings', b0 * BAR, 11, 110);
      } else if (mode === 'runs') {   // disco: held chords, and a fast scale run up into every other bar
        holdChords('strings', b0, n, H, 55, 76, 3, 58);
        ccSet('strings', b0 * BAR, 11, 108);
        for (let i = 1; i < n; i += 2) {
          const b = b0 + i, nb = b + 1;
          if (nb >= b0 + n) continue;
          const tones = tonesIn(H(nb, 0), 74, 86); if (!tones.length) continue;
          const tgt = tones[(i >> 1) % tones.length], t0 = b * BAR + 3 * 480;
          for (let k = 0; k < 7; k++) pushN('strings', t0 + 60 + k * 60, 52, shiftDeg(tgt, k - 7), 60 + k * 7);
          pushN('strings', nb * BAR, BAR - 80, tgt, 104);
        }
      }
    }
    function choirLayer(b0, n, mode, H) {
      if (mode === 'oohs') {
        holdChords('choir', b0, n, H, 57, 76, 3, 64);
        for (let k = 0; k < n; k += 4) ccRamp('choir', b0 + k, Math.min(4, n - k), 11, 58, 108, f => Math.pow(Math.sin(f * Math.PI), 0.8));
        return;
      }
      let prev = null;   // full: four parts held through the phrase, then gospel stabs on the and of 2, the 3 and the and of 4
      for (let ph = 0; ph < n; ph += 4) {
        const len = Math.min(4, n - ph);
        if (len < 4) { prev = holdChords('choir', b0 + ph, len, H, 52, 79, 4, 82, prev); continue; }
        prev = holdChords('choir', b0 + ph, 3, H, 52, 79, 4, 82, prev);
        const b = b0 + ph + 3, v0 = voice(H(b, 0).pcs, prev, 52, 79, 4);
        v0.forEach(x => pushN('choir', b * BAR, P16(6) - 30, x, 82)); prev = v0;
        [6, 10, 14].forEach((s, k) => {
          const c = s === 14 && b + 1 < b0 + n ? H(b + 1, 0) : H(b, s), v = voice(c.pcs, prev, 52, 79, 4); prev = v;
          v.forEach(x => pushN('choir', b * BAR + P16(s), 140, x, 96 + k * 6));
        });
      }
      ccSet('choir', b0 * BAR, 11, 112);
    }
    function organLayer(b0, n, mode, H) {
      if (mode === 'swell') {
        holdChords('organ', b0, n, H, 52, 76, 4, 74);
        for (let k = 0; k < n; k += 2) { ccRamp('organ', b0 + k, Math.min(2, n - k), 11, 38, 118, f => f * f); ccSet('organ', (b0 + k) * BAR, 1, 0); ccSet('organ', (b0 + k) * BAR + 2880, 1, 100); }
        return;
      }
      let prev = null;   // comping: a chord on the one, short stabs after it
      for (let i = 0; i < n; i++) {
        const b = b0 + i;
        [[0, 4, 80], [6, 1, 70], [10, 1, 74], [14, 1, 78]].forEach(([s, len, v]) => {
          const vv = voice(H(b, s).pcs, prev, 55, 77, 4); prev = vv;
          const t = b * BAR + P16(s), d = len > 1 ? P16(s + len) - P16(s) - 30 : 130;
          vv.forEach(x => pushN('organ', t, d, x, v));
        });
      }
      ccSet('organ', b0 * BAR, 11, 100); ccSet('organ', b0 * BAR, 1, 64);
    }
    function pianoLayer(b0, n, mode, H) {
      const IDX = [0, 2, 1, 3, 2, 4, 3, 5, 4, 3, 2, 1];
      for (let i = 0; i < n; i++) {
        const b = b0 + i;
        if (mode === 'solo' && i % 2 === 1) for (let s = 0; s < 12; s++) {   // broken-chord 16ths in the right hand
          const tones = tonesIn(H(b, s), 64, 86);
          if (tones.length) pushN('piano', b * BAR + P16(s), 100, tones[Math.min(tones.length - 1, IDX[s])], (s % 4 ? 62 : 82) + R() * 10);
        }
        if (i % 2 === 0) continue;
        if (i % 4 === 1) tonesIn(H(b, 12), 67, 88).reverse().slice(0, 4).forEach((x, k) => pushN('piano', b * BAR + P16(12 + k), 90, x, 94 - k * 6));   // a crush down the chord
        else {   // a gospel run down the scale into the next bar
          const top = tonesIn(H(b, 12), 79, 92)[0] || 84;
          pushN('piano', b * BAR + P16(12), 110, top, 96);
          for (let k = 1; k < 8; k++) pushN('piano', b * BAR + P16(12) + k * 60, 52, shiftDeg(top, -k), 90 - k * 3);
        }
      }
    }
    function orchLayer(b0, n, type, H) {   // orchestra hits: low root, its octave, a close voicing on top
      const hit = (b, st, v, len) => {
        const c = H(b, st), t = b * BAR + P16(st);
        let r = 36 + c.root; if (r < 40) r += 12;
        [r, r + 12].concat(voice(c.pcs, null, 60, 81, 3)).forEach(x => pushN('orch', t, len || 360, x, v));
      };
      if (type === 'build') {
        for (let i = Math.max(0, n - 4); i < n - 1; i++) hit(b0 + i, 0, 92 + 6 * (i - n + 4));
        [0, 4, 8, 12].forEach((s, k) => hit(b0 + n - 1, s, 100 + 6 * k, 220));
      } else {
        for (let i = 0; i < n; i += 8) hit(b0 + i, 0, 116);
        for (let i = 15; i < n; i += 16) { hit(b0 + i, 8, 100, 240); hit(b0 + i, 12, 108, 240); }
      }
    }
    const BRASS = [[[0, 1, 0], [3, 1, 0], [6, 4, 1], [26, 1, 0], [28, 3, 1]], [[2, 1, 0], [4, 1, 0], [6, 2, 0], [14, 1, 0], [16, 6, 1]], [[0, 2, 0], [6, 1, 0], [10, 4, 1], [22, 1, 0], [24, 1, 0], [26, 4, 1]]];
    function brassLayer(b0, n, mode, H) {   // two-bar brass figures; the long notes fall off by pitch bend
      let prev = null;
      for (let u = 0; u < n; u += 2) {
        const rows = mode === 'sparse' ? [[16, 4, 1]] : BRASS[(u >> 1) % BRASS.length];
        rows.forEach(([s32, len, fall]) => {
          const b = b0 + u + (s32 >> 4), s = s32 % 16;
          if (b >= b0 + n) return;
          const v = voice(H(b, s).pcs, prev, 58, 79, 3); prev = v;
          const t = b * BAR + P16(s), d = len === 1 ? 90 : P16(s + len) - P16(s) - 20;
          v.forEach(x => pushN('brass', t, d, x, fall ? 112 : 100));
          if (fall) { const fs = t + d - 150; [0, 0.25, 0.5, 0.75, 1].forEach(f => bend('brass', fs + f * 150, -5 * SEMI * f * f)); bend('brass', t + d + 8, 0); }
        });
      }
      ccSet('brass', b0 * BAR, 11, 104);
    }
    function talkboxLayer(b0, n, H) {   // answering phrases in the gaps the vocal and melody leave: slides, a scoop into each phrase, a wah per note
      const busyEv = T.lead.concat(T.voxguide).filter(e => e.t >= b0 * BAR - 1920 && e.t < (b0 + n) * BAR);
      let prev = 74;
      for (let hb = 0; hb < n * 2; hb++) {
        const bar = b0 + (hb >> 1), half = hb & 1, ts = bar * BAR + half * 960;
        if (busyEv.some(e => (e.t >= ts - 60 && e.t < ts + 960) || (e.t < ts && e.t + e.d > ts + 120))) continue;
        if (!half && chance(0.6)) continue;
        const rhy = pick(CTR_RHY), pos = [];
        for (let s = 0; s < 8; s++) if (rhy[s] === 'x') pos.push(half * 8 + s);
        pos.forEach((p, i) => {
          const t = bar * BAR + P16(p), tones = tonesIn(H(bar, p), 67, 84);
          if (!tones.length) return;
          const end = i + 1 < pos.length ? bar * BAR + P16(pos[i + 1]) : Math.min(bar * BAR + P16(half * 8 + 8), t + 420);
          let nn = nearest(tones, prev, i ? prev : null);
          if (i && t % 480 >= 40 && end - t < 200 && chance(0.35)) nn = shiftDeg(prev, nn > prev ? 1 : -1);   // a passing step on a weak 16th
          const leg = i + 1 < pos.length && end - t < 200;
          pushN('talkbox', t, end - t + (leg ? 30 : -30), nn, 84 + R() * 14, leg ? { legato: true } : null);
          if (i === 0) { bend('talkbox', t - 4, -SEMI); bend('talkbox', t + 45, -0.45 * SEMI); bend('talkbox', t + 90, 0); }
          ccSet('talkbox', t, 74, 35); ccSet('talkbox', t + Math.min(160, (end - t) / 2), 74, 105); ccSet('talkbox', Math.max(t + 1, end - 40), 74, 60);
          prev = nn;
        });
      }
    }
    function acidLayer(b0, n, H, type) {   // a 303 line: chord tones on the beats, passing notes, accents and slides, cutoff sweeps
      const mk = () => {
        const p = [];
        for (let s = 0; s < 32; s++) {
          const on = s % 4 === 0 || chance(s % 2 ? 0.45 : 0.65);
          p.push(on ? { deg: s % 4 === 0 ? pick([0, 0, 0, 1, 2]) : pick([0, 1, 2, 3]), oct: chance(0.22), acc: chance(0.3) || s % 8 === 6, slide: chance(0.28), pass: s % 2 === 1 && chance(0.35) ? pick([-1, 1]) : 0 } : null);
        }
        return p;
      };
      const pats = [mk(), mk()], out = [];
      let prev = null;
      for (let i = 0; i < n; i++) {
        const b = b0 + i, pat = pats[(i >> 3) % 2], half = i % 2;
        for (let s = 0; s < 16; s++) {
          const st = pat[half * 16 + s]; if (!st) continue;
          if (type === 'build' && i < n / 2 && s % 2) continue;
          const c = H(b, s); let r = 36 + c.root; if (r > 45) r -= 12;
          const tones = tonesIn(c, r, r + 12), pass = !!(st.pass && prev !== null && s % 4 !== 0);
          let nn = pass ? shiftDeg(prev, st.pass) : tones[Math.min(st.deg, tones.length - 1)];
          if (st.oct && nn + 12 <= 70) nn += 12;
          out.push({ t: b * BAR + P16(s), n: nn, v: st.acc ? 120 : 82, slide: st.slide, pass });
          prev = nn;
        }
      }
      out.forEach((e, k) => {
        const nx = out[k + 1], leg = e.slide && nx && nx.t - e.t <= 260 && (!e.pass || nx.t - e.t + 30 < 240);
        pushN('acid', e.t, leg ? nx.t - e.t + 30 : 100, e.n, e.v * (type === 'build' ? 0.6 + 0.4 * (e.t / BAR - b0) / n : 1), leg ? { legato: true } : null);
      });
      if (type === 'build') ccRamp('acid', b0, n, 74, 25, 120, f => f * f);
      else for (let k = 0; k < n; k += 8) ccRamp('acid', b0 + k, Math.min(8, n - k), 74, 40, 112, f => 0.5 - 0.5 * Math.cos(f * 2 * Math.PI));
      ccSet('acid', b0 * BAR, 71, 104);
    }
    function hooverLayer(b0, n, H) {   // rave hoover: a riff on roots and fifths, each note swoops up into pitch, a dive closes each phrase
      const RH = [[[0, 3], [3, 3], [6, 4], [10, 3], [13, 3]], [[0, 6], [6, 4], [10, 6]]];
      for (let i = 0; i < n; i++) {
        const b = b0 + i;
        RH[i % 2].forEach(([s, len], k) => {
          const c = H(b, s); let r = 48 + c.root; if (r > 57) r -= 12;
          const nn = k === 2 && c.pcs.includes((c.root + 7) % 12) ? r + 7 : r;
          const t = b * BAR + P16(s), e = b * BAR + P16(s + len);
          pushN('hoover', t, e - t - 12, nn, k === 0 ? 112 : 98);
          bend('hoover', t - 3, -2 * SEMI); bend('hoover', t + 35, -0.8 * SEMI); bend('hoover', t + 75, 0);
        });
        if (i % 4 === 3) {
          const t0 = b * BAR + P16(10) + 140, t1 = (b + 1) * BAR - 14;
          for (let s = 0; s <= 6; s++) bend('hoover', t0 + (t1 - t0) * s / 6, -12 * SEMI * Math.pow(s / 6, 1.6));
          bend('hoover', t1 + 2, 0);
        }
      }
      ccSet('hoover', b0 * BAR, 1, 90);
    }
    function sirenNote(t, d, shape, period, depth, v) {   // one blast: a held note swept by a pitch-bend LFO
      let base = 60 + home; while (base < 76) base += 12;
      pushN('siren', t, d, base, v);
      for (let x = 0; x < d; x += 30) {
        const ph = (x % period) / period, y = shape === 'saw' ? ph : shape === 'square' ? (ph < 0.5 ? 0 : 1) : 1 - Math.abs(2 * ph - 1);
        bend('siren', t + x, depth * SEMI * y);
      }
      bend('siren', t + d + 2, 0);
    }
    function sirenLayer(b0, n, mode) {
      const B = b => (b0 + b) * BAR;
      if (mode === 'intro') { sirenNote(B(4), 2 * BAR - 60, 'saw', 480, 5, 78); if (n > 13) sirenNote(B(12), 2 * BAR - 60, 'tri', 240, 7, 86); }
      if (mode === 'build') {   // one long rise up an octave, wobbling
        let base = 60 + home; while (base < 76) base += 12;
        const d = n * BAR - 300;
        pushN('siren', B(0), d, base, 84);
        for (let x = 0; x < d; x += 30) bend('siren', B(0) + x, 12 * SEMI * x / d + SEMI * Math.sin(2 * Math.PI * x / 240));
        bend('siren', B(0) + d + 2, 0);
        ccRamp('siren', b0, n, 11, 50, 120);
      }
      if (mode === 'drop') { sirenNote(B(0), BAR - 60, 'saw', 240, 7, 96); if (n > 16) sirenNote(B(16), BAR - 60, 'square', 240, 5, 92); }
      if (mode === 'call') for (let i = 1; i < n; i += 4) sirenNote(B(i), BAR - 60, i % 8 === 1 ? 'tri' : 'saw', i % 8 === 1 ? 240 : 160, 7, 90);
      if (mode === 'dub') for (let i = 0; i < n; i += 4) for (let r = 0; r < 4; r++) sirenNote(B(i) + r * 360, 300, 'tri', 120, 5, 100 * Math.pow(0.68, r));
      if (mode === 'outro') { sirenNote(B(0), 4 * BAR - 60, 'tri', 480, 5, 84); ccRamp('siren', b0, 4, 11, 110, 20); }
      ccSet('siren', B(0), 91, mode === 'dub' ? 120 : 80);
    }
    function donkLayer(b0, n, mode, H) {   // the bassline donk: offbeat plucks on the root, the octave on the last one
      const from = mode === 'intro' ? Math.floor(n / 2) : 0, to = mode === 'outro' ? Math.floor(n / 2) : n;
      for (let i = from; i < to; i++) {
        const b = b0 + i;
        [2, 6, 10, 14].forEach(s => {
          let r = 36 + H(b, s).root; if (r < 40) r += 12;
          pushN('donk', b * BAR + P16(s), 100, s === 14 ? r + 12 : r, s === 6 || s === 14 ? 106 : 96);
        });
      }
      if (mode === 'intro') ccRamp('donk', b0 + from, to - from, 74, 25, 110, f => f * f);
      else if (mode === 'outro') ccRamp('donk', b0, to, 74, 110, 25);
      else ccSet('donk', b0 * BAR, 74, 110);
    }

    /* -- shouts, sub layer, pitch-bend moves, filters, edits -- */
    function shout(t, id) {
      if (!SH.length) return;
      let k = id ? SH.indexOf(id) : -1;
      if (k < 0) k = shoutIdx++ % SH.length;
      if (t - lastShoutT < 1500) return;
      if (inSpans(vocalSpans, t) || inSpans(vocalSpans, t + 300) || inSpans(vocalSpans, t + 600)) return;
      pushN('shouts', t, 720, 48 + k, 108);
      lastShoutT = t;
    }
    function shoutsLayer(b0, n, mode) {
      const at = (bar, st) => shout((b0 + bar) * BAR + P16(st));
      if (pendingShout) { shout(b0 * BAR, pendingShout); pendingShout = null; }
      if (mode === 'intro') { at(0, 0); at(8, 0); at(n - 2, 8); }
      if (mode === 'sparse') for (let i = 7; i < n; i += 8) at(i, 8);
      if (mode === 'answer') for (let i = 3; i < n; i += 4) at(i, 8);
      if (mode === 'drop') { at(0, 0); for (let i = 7; i < n; i += 8) at(i, 8); }
      if (mode === 'call') for (let i = 1; i < n; i += 2) at(i, 8);
    }
    function subDouble(b0, n) {   // a sine sub under the Reese: the same line, folded into the D1-C#2 octave
      evIn('bass', b0 * BAR, (b0 + n) * BAR).forEach(e => {
        let s = e.n; while (s > 37) s -= 12; while (s < 26) s += 12;
        T.sub.push({ t: e.t, d: e.d, n: s, v: clampV(e.v * 0.9) });
      });
    }
    function wubLayer(b0, n) {
      const t0 = b0 * BAR, t1 = (b0 + n) * BAR, ev = evIn('bass', t0, t1);
      if (kit.sound.bass === 'sub') {   // the sub dips in pitch at the tail of each held note
        ev.forEach((e, i) => {
          if (e.d < 300) return;
          const s = e.t + e.d - 200, nx = ev[i + 1], endT = nx ? Math.min(nx.t - 4, e.t + e.d + 5) : e.t + e.d + 5;
          [0, 0.33, 0.66, 1].forEach(f => bend('bass', s + f * 180, -4 * SEMI * f * f));
          bend('bass', endT, 0);
        });
      } else {   // the bassline wub: a cutoff LFO whose rate changes every two bars
        CC.bass = CC.bass.filter(c => !(c.cc === 74 && c.t >= t0 && c.t < t1));
        for (let k = 0; k < n; k += 2) {
          const rate = pick([240, 480, 320, 160]);
          for (let x = 0; x < Math.min(2, n - k) * BAR; x += rate / 4) ccSet('bass', (b0 + k) * BAR + x, 74, 64 + 50 * Math.cos(2 * Math.PI * (x % rate) / rate));
        }
        ccSet('bass', t1 - 1, 74, 100);
      }
    }
    function bounceLayer(b0, n) {   // every bass note drops two semitones into pitch
      evIn('bass', b0 * BAR, (b0 + n) * BAR).forEach(e => { [[-3, 2], [18, 1.1], [40, 0.45], [70, 0]].forEach(([dt, s]) => bend('bass', e.t + dt, s * SEMI)); });
    }
    function divesLayer(b0, n) {   // the Reese dives an octave on the last beat of a phrase
      for (let i = 0; i < n; i++) {
        const dive = i % 8 === 7 || (i % 8 === 3 && chance(0.5));
        if (!dive) continue;
        const t0 = (b0 + i) * BAR + 3 * 480, t1 = (b0 + i + 1) * BAR;
        if (!T.bass.some(e => e.t < t1 && e.t + e.d > t0)) continue;
        for (let s = 0; s <= 6; s++) bend('bass', t0 + s * 75, -12 * SEMI * Math.pow(s / 6, 1.7));
        bend('bass', t1 - 6, 0);
      }
    }
    function zoomLayer(b0, n) {   // the rominimal bass zooms open and retracts (cutoff), with a resonance and dub-send move per phrase
      CC.bass = CC.bass.filter(c => !(c.cc === 74 && c.t >= b0 * BAR && c.t < (b0 + n) * BAR));
      for (let ph = 0; ph < n / 4; ph++) {
        const b = b0 + ph * 4, style = pick(['in', 'out', 'blink', 'breathe']);
        if (style === 'in') { ccRamp('bass', b, 2, 74, 30, 108, f => f * f); ccRamp('bass', b + 2, 1, 74, 108, 100); ccRamp('bass', b + 3, 1, 74, 100, 50); }
        if (style === 'out') ccRamp('bass', b, 4, 74, 104, 34, f => Math.sqrt(f));
        if (style === 'blink') { for (let k = 0; k < 4; k++) ccSet('bass', b * BAR + k * 480, 74, k % 2 ? 40 : 96); ccRamp('bass', b + 1, 3, 74, 70, 90); }
        if (style === 'breathe') ccRamp('bass', b, 4, 74, 40, 110, f => 0.5 - 0.5 * Math.cos(f * 2 * Math.PI));
        ccSet('bass', b * BAR, 71, pick([40, 64, 88]));
        ccSet('bass', b * BAR, 91, ph % 2 ? 70 : 30);
      }
    }
    function filterLayer(b0, n, mode) {   // a filter-house cutoff move on every pitched track playing in the section
      const t0 = b0 * BAR, t1 = (b0 + n) * BAR;
      Object.keys(T).forEach(k => {
        if (NOTMEL.has(k) || !T[k].some(e => e.t >= t0 && e.t < t1)) return;
        CC[k] = CC[k].filter(c => !(c.cc === 74 && c.t >= t0 && c.t < t1));
        if (mode === 'open') ccRamp(k, b0, n, 74, 12, 127, f => f * f);
        if (mode === 'close') ccRamp(k, b0, n, 74, 127, 18, f => Math.sqrt(f));
        if (mode === 'sweep') { ccRamp(k, b0, n / 2, 74, 127, 26, f => Math.sqrt(f)); ccRamp(k, b0 + n / 2, n / 2, 74, 26, 127, f => f * f); }
      });
    }
    function tapeStop(tEnd) {   // the last beat winds down like a stopped tape: drums cut, everything still sounding bends down an octave
      const t0 = tEnd - 480;
      fx('tapestop', t0, 480, 110);
      clearRange(DRUMK, t0, tEnd);
      Object.keys(T).forEach(k => {
        if (NOTMEL.has(k)) return;
        T[k] = T[k].filter(e => !(e.t >= t0 && e.t < tEnd));
        let any = false;
        T[k].forEach(e => { if (e.t < t0 && e.t + e.d > t0 - 20) { e.d = Math.max(e.d, tEnd - e.t - 12); any = true; } });
        if (!any) return;
        for (let s = 0; s <= 8; s++) bend(k, t0 + s * 56, -12 * SEMI * Math.pow(s / 8, 1.5));
        bend(k, tEnd - 4, 0);
      });
      const gi = gaps.indexOf(tEnd); if (gi >= 0) gaps.splice(gi, 1);
    }
    function rewindAt(bar) {   // "pull up": the MC calls it, the record spins back, the next section drops again
      const t0 = bar * BAR;
      T.shouts = T.shouts.filter(e => !(e.t >= t0 - 1500 && e.t < t0 + BAR));
      const k = SH.indexOf('a12');
      if (k >= 0) { pushN('shouts', t0 - 960, 720, 48 + k, 112); lastShoutT = t0 - 960; }
      fx('rewind', t0, BAR, 116);
      cuts.push({ t0, t1: t0 + BAR, keep: ['fx', 'shouts'] });
      if (SH.includes('a02')) pendingShout = 'a02';
    }

    /* -- new section types -- */
    function breakdown2(b0, n) {   // breakdownSec, but the bars under a vocal take the progression the hook was written on
      const bd = kit.song.bd.map(([nm, v]) => chordFromName(nm, v.split(' '), tr));
      const isVoc = b => inSpans(vocalBlocks, b * BAR + 240);
      const bdAt = b => bd[Math.floor((b - b0) / 2) % bd.length];
      const HB2 = (b, st) => (isVoc(b) ? harm(b, st, null) : bdAt(b));
      for (let i = 0; i < n; i++) if (!isVoc(b0 + i)) for (let st = 0; st < 16; st++) HL.set((b0 + i) * 16 + st, bdAt(b0 + i));
      fx('downlifter', b0 * BAR, 2 * BAR, 100);
      drumPhrase(b0, n, i => ({ base: 'main', kick: 'none', snare: false, hats: i >= n / 2 ? 'eighths' : 'none', percBase: false, lib: [[PC_[0], 1]], vel: 0.45 + 0.3 * i / n }));
      const segs = [];
      for (let i = 0; i < n; i++) for (const st of [0, 8]) {
        const b = b0 + i, c = HB2(b, st), t = b * BAR + st * 120, voc = isVoc(b), last = segs[segs.length - 1];
        if (last && last.c === c && last.voc === voc) last.end = t + 960; else segs.push({ c, t, end: t + 960, voc });
      }
      let prev = null;
      segs.forEach((sg, k) => {
        const c = sg.c, len = sg.end - sg.t, v = !sg.voc && c.voicing ? c.voicing : voice(c.pcs, prev, 55, 79, 4);
        prev = v.filter(x => x >= 52);
        v.forEach(x => pushN('pad', sg.t, len - 40, x, sg.voc ? 62 : 74));
        let r = !sg.voc && c.voicing ? c.voicing[0] : 24 + c.root; while (r > 40) r -= 12; while (r < 28) r += 12;
        pushN(g === 'sg' || g === 'ch' ? 'sub' : 'bass', sg.t, len - 60, r, sg.voc ? 78 : 84);
        if (!sg.voc && (k % 2 === 1 || sg.t >= (b0 + n / 2) * BAR)) { const top = v[v.length - 1], tv = top + 12 <= 86 ? top + 12 : top; pushN('vox', sg.t + P16(2), Math.min(len, BAR) - P16(2) - 60, tv, 80); }
      });
      if (g === 'ch') {   // house piano comping
        let pv = null;
        for (let i = 0; i < n; i++) {
          const b = b0 + i, rhy = PIANO_RHY[i % PIANO_RHY.length];
          for (let p = 0; p < 16; p++) if (rhy[p] !== '.') { const v = voice(HB2(b, p).pcs, pv, 55, 77, 4); pv = v; v.forEach(x => pushN('chords', b * BAR + P16(p), 170, x, (13 + (+rhy[p]) * 12.7) * (0.55 + 0.4 * i / n))); }
        }
        ccRamp('chords', b0, n, 74, 60, 110);
      }
      const pats = ['up', 'updown', 'broken', 'down'];
      for (let i = 0; i < n; i++) { const b = b0 + i; arpBar(b, st => HB2(b, st), i < n / 2 ? 8 : 16, pats[Math.floor(i / 2) % 4], (isVoc(b) ? 0.42 : 0.6) + 0.4 * i / n, 60); }
      let pn = 72;
      for (let i = 0; i < n; i++) {
        const b = b0 + i; if (isVoc(b) || i < n / 4) continue;
        const nn = nearest(tonesIn(HB2(b, 0), 64, 79), pn, i % 2 ? pn : null);
        pushN('counter', b * BAR, BAR - 40, nn, 64 + 20 * i / n); pn = nn;
      }
      ccRamp('pad', b0, n, 11, 60, 120); ccRamp('arp', b0, n, 74, 40, 115); ccRamp('counter', b0, n, 11, 50, 110);
      return HB2;
    }
    function halftimeSec(b0, n, o) {   // half-time: kick on the one, snare on the three, long Reese notes, dark chords
      fx('impact', b0 * BAR, BAR, 104);
      if (o.subdrop) fx('subdrop', b0 * BAR, 2 * BAR, 112);
      const KP = [[0, 10], [0, 7, 10], [0, 10, 13], [0, 3, 10]];
      for (let i = 0; i < n; i++) {
        const b = b0 + i, t0 = b * BAR, fillBar = i % 8 === 7;
        KP[i % 4].forEach(p => pushD('K', t0 + P16(p), p ? 100 : 120));
        pushD('SN', t0 + P16(8), 114); pushD('CL', t0 + P16(8) + 3, 98);
        [5, 11, 15].forEach(p => { if (!(fillBar && p >= 12) && chance(0.55)) pushD('SN', t0 + P16(p) + jit(3), 30 + R() * 16); });
        for (let s = 0; s < 16; s += 2) if (!(fillBar && s >= 12)) pushD(s === 14 ? 'OH' : 'CH', t0 + P16(s) + jit(4), (s === 14 ? 80 : s % 4 ? 64 : 90) * (0.92 + R() * 0.16));
        if (i % 2) libGen(PC_[1], i >> 1).forEach(h => pushD(PC_[1], t0 + P16(h.p) + jit(5), h.v * 0.8));
        if (fillBar) for (let k = 0; k < 8; k++) pushD('SN', t0 + P16(12) + k * 60, 50 + k * 9, 'fills', 50);
        if (i === 0 || i === n / 2) pushD('CR', t0, 104, 'fills', 240);
      }
      for (let i = 0; i < n; i++) {   // the Reese: a long root, a slide up the octave, the second chord, an approach on odd bars
        const b = b0 + i, t0 = b * BAR, c0 = harm(b, 0, null), c8 = harm(b, 8, null), cn = harm(b + 1, 0, null);
        const r0 = bassRange(24 + c0.root), r8 = bassRange(24 + c8.root), rn = bassRange(24 + cn.root);
        pushN('bass', t0, P16(6) - 20, r0, 112);
        pushN('bass', t0 + P16(6), P16(8) - P16(6) + 30, r0 + 12 <= 55 ? r0 + 12 : r0, 96, { legato: true });
        pushN('bass', t0 + P16(8), P16(13) - P16(8) - 20, r8, 108);
        if (i % 2) pushN('bass', t0 + P16(13), P16(16) - P16(13) - 20, bassRange(shiftDeg(rn, rn >= r8 ? -1 : 1)), 92);
      }
      reeseAuto(b0, n);
      place('chords', M.chordsA, b0, n, { vel: () => 0.72 }); ccSet('chords', b0 * BAR, 74, 58);
      counterCall(b0 + n / 2, n / 2, null, 0.8);
      voxPhrase(b0 + n / 2, n / 2, null, 0.75, 2);
    }
    function microSec(b0, n) {   // rominimal edits every other bar: beat repeat, drop-out, bass stutter, gated hats
      grooveDrums(b0, n, 3, { crash: i => i === 0, noFillLast: true, fill: () => null });
      for (let it = 0; it < n / 4; it++) place('bass', it % 2 ? M.bassB : M.bassA, b0 + it * 4, 4, { xf: () => (it % 2 ? [approach(null)] : []) });
      stabPhrase(b0, n, null, 0.72, 2);
      const types = ['repeat', 'drop', 'stutter', 'gate'];
      for (let i = 1; i < n; i += 2) {
        const b = b0 + i, t0 = b * BAR, a = t0 + 3 * 480, type = types[((i - 1) >> 1) % 4];
        if (type === 'repeat') {
          clearRange(['kick', 'snare', 'hats', 'perc'], a, t0 + BAR);
          [[0, 116], [240, 92], [360, 100], [420, 108]].forEach(([x, v]) => pushD('K', a + x, v));
          for (let k = 0; k < 8; k++) pushD('RIM', a + k * 60, 40 + k * 9, 'micro', 30);
        } else if (type === 'drop') clearRange(['kick', 'hats'], t0 + 960, t0 + BAR);
        else if (type === 'stutter') {
          const src = T.bass.filter(e => e.t < a + 60 && e.t + e.d > a - 240).sort((x, y) => x.t - y.t).pop();
          const nn = src ? src.n : bassRange(24 + harm(b, 12, null).root);
          T.bass = T.bass.filter(e => !(e.t >= a && e.t < t0 + BAR));
          T.bass.forEach(e => { if (e.t < a && e.t + e.d > a) e.d = Math.max(15, a - e.t - 5); });
          for (let k = 0; k < 8; k++) pushN('bass', a + k * 60, 45, nn, 70 + k * 6);
        } else {
          clearRange(['hats'], t0, t0 + BAR);
          for (let s = 0; s < 16; s++) pushD(s === 14 ? 'OH' : 'CH', t0 + P16(s), s === 14 ? 88 : s % 2 ? 28 : 92);
        }
      }
    }
    function twostepSec(b0, n) {   // the four-to-the-floor drops to 2-step: kick on the one and the and of three, shuffled hats
      const rb = withB(null);
      fx('impact', b0 * BAR, BAR, 104);
      drumPhrase(b0, n, i => ({ base: i % 8 < 4 ? 'main' : 'var', kick: 'none', snare: true, hats: ['shuffle', 'base'][Math.floor(i / 4) % 2], percBase: true, lib: [[PC_[1], i >> 2]], ghosts: 0.2, fill: i % 8 === 7 ? pick(FILLW[g]) : null, crash: i === 0 }));
      const KP = [[0, 10], [0, 7, 10], [0, 10, 13], [0, 3, 10]];
      for (let i = 0; i < n; i++) KP[i % 4].forEach(p => pushD('K', (b0 + i) * BAR + P16(p), p ? 104 : 118));
      for (let it = 0; it < n / 4; it++) place('bass', M.bassB, b0 + it * 4, 4, { xf: () => (it % 2 ? [approach(rb), bfill(rb)] : [approach(rb)]) });
      place('chords', M.chordsB, b0, n, { vel: () => 0.85 });
      place('lead', M.leadB, b0, n / 2, { xf: () => [snapTo(rb), orn] });
      counterCall(b0 + n / 2, n / 2, rb, 0.85);
      stabPhrase(b0 + n / 2, n / 2, rb, 0.8, 1);
      subRoots(b0, n, rb, 0.9);
    }

    function customSection(type, b0, n, o) {
      const end = (b0 + n) * BAR, rh = o.rh || null;
      if (o.lift) { lift = o.lift; keyChanges.push({ t: b0 * BAR, pc: (home + o.lift) % 12 }); }
      if (o.lead === false) leadOff.push([b0 * BAR - 130, end - 130]);
      if (o.vocal) (o.vocalBars || [0]).forEach(v => vocalBlock(o.vocal, b0 + v));
      if (o.vocal2) (o.vocal2Bars || []).forEach(v => vocalBlock(o.vocal2, b0 + v));
      let H = (b, s) => harm(b, s, null);
      if (type === 'intro') { intro(b0, n); H = (b, s) => harm(b, s, withB(null)); }
      else if (type === 'groove') groove(b0, n, o);
      else if (type === 'build') { build(b0, n, { rh }); H = (b, s) => harm(b, s, withB(rh)); }
      else if (type === 'drop') { drop(b0, n, o); H = (b, s) => harm(b, s, rh); }
      else if (type === 'switch') { switchSec(b0, n, o); H = (b, s) => harm(b, s, withB(null)); }
      else if (type === 'breakdown') H = breakdown2(b0, n);
      else if (type === 'dubBreak') dubBreak(b0, n, o);
      else if (type === 'bassSolo') bassSolo(b0, n);
      else if (type === 'outro') outro(b0, n);
      else if (type === 'halftime') halftimeSec(b0, n, o);
      else if (type === 'micro') microSec(b0, n);
      else if (type === 'twostep') { twostepSec(b0, n); H = (b, s) => harm(b, s, withB(null)); }
      else throw new Error(kit.id + ' unknown section type ' + type);
      // extra layers: pitched ones wait out the first quarter of an intro and stop halfway through an outro
      const m0 = type === 'intro' ? b0 + n / 4 : b0, mn = type === 'intro' ? n - n / 4 : type === 'outro' ? n / 2 : n;
      if (o.breaks) breaksLayer(b0, n, o.breaks);
      if (o.poly) polyLayer(b0, n, o.poly === 'dense', type);
      if (o.micro) microLayer(b0, n, o.micro === 'glitch', type);
      if (o.cowbell) cowbellLayer(b0, n, type);
      if (o.strings) stringsLayer(m0, mn, o.strings, H);
      if (o.choir) choirLayer(m0, mn, o.choir, H);
      if (o.organ) organLayer(m0, mn, o.organ === true ? 'comp' : o.organ, H);
      if (o.piano) pianoLayer(m0, mn, o.piano, H);
      if (o.orch) orchLayer(b0, n, type, H);
      if (o.brass) brassLayer(m0, mn, o.brass === true ? 'full' : o.brass, H);
      if (o.talkbox) talkboxLayer(m0, mn, H);
      if (o.acid) acidLayer(b0, n, H, type);
      if (o.hoover) hooverLayer(b0, n, H);
      if (o.donk) donkLayer(b0, n, o.donk === true ? 'full' : o.donk, H);
      if (o.siren) sirenLayer(b0, n, o.siren);
      if (o.shouts || pendingShout) shoutsLayer(b0, n, o.shouts);
      if (kit.ext.sub) subDouble(b0, n);
      if (o.wub) wubLayer(b0, n);
      if (o.bounce) bounceLayer(b0, n);
      if (o.dives) divesLayer(b0, n);
      if (o.zoom) zoomLayer(b0, n);
      if (o.filter) filterLayer(b0, n, o.filter);
      if (o.rewind) rewindAt(b0 + n - 1);
      if (o.tapestop) tapeStop(end);
      if (o.lift) { lift = 0; keyChanges.push({ t: end, pc: home }); }
    }
