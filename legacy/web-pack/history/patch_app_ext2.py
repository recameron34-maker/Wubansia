"""Page support for the eight ext-spec songs: new voices, pitch bends, CC74/CC11 automation, break slices,
the pack's vocals and shouts in song playback, kit-zip vocals, hints and README."""
import sys

P = '/home/claude/work/app.js'
s = open(P).read()
if 'BREAK_HITS' in s:
    sys.exit('already patched')


def rep(old, new):
    global s
    n = s.count(old)
    assert n == 1, (n, old[:90])
    s = s.replace(old, new)


# ---- audio graph: a bus for vocals and shouts
rep("let ctx = null, master, comp, seqOut, drumBus, musicBus, duck, revIn, dlyIn, dlyNode, sampleBus, noiseBuf, organWave;",
    "let ctx = null, master, comp, seqOut, drumBus, musicBus, duck, revIn, dlyIn, dlyNode, sampleBus, noiseBuf, organWave, vocalBus;")
rep("  duck = ctx.createGain(); duck.connect(seqOut);\n",
    "  duck = ctx.createGain(); duck.connect(seqOut);\n  vocalBus = ctx.createGain(); vocalBus.gain.value = 0.8; vocalBus.connect(seqOut);\n")

# ---- oscillators register themselves so a note can carry pitch bends
rep("function O(type, f, t, stop, det) {\n  const o = ctx.createOscillator();\n  if (type === 'organ') o.setPeriodicWave(organWave); else o.type = type;\n  o.frequency.setValueAtTime(f, t); if (det) o.detune.value = det;\n  o.start(t); o.stop(stop); return o;\n}",
    "let OSC_SINK = null;   // audio-rate oscillators of the note being built (pitch bends are applied to their detune)\n"
    "function O(type, f, t, stop, det) {\n  const o = ctx.createOscillator();\n  if (type === 'organ') o.setPeriodicWave(organWave); else o.type = type;\n  o.frequency.setValueAtTime(f, t); if (det) o.detune.value = det;\n  o.start(t); o.stop(stop);\n  if (OSC_SINK && f >= 20) OSC_SINK.push([o.detune, det || 0]);\n  return o;\n}\n"
    "function applyBends(list, b, t) {   // b: {c0: cents at the note start, pts: [[seconds after start, cents]]}\n"
    "  list.forEach(([p, base]) => { p.setValueAtTime(base + b.c0, t); b.pts.forEach(([dt, c]) => p.linearRampToValueAtTime(base + c, t + dt)); });\n}")

# ---- new synth voices + bends
rep("  const g = G(0); g.connect(out);\n  let stop = end + 0.6;\n  switch (kind) {\n",
    "  const g = G(0); g.connect(out);\n  let stop = end + 0.6;\n  OSC_SINK = ev.bends ? [] : null;\n  switch (kind) {\n"
    "    case 'donk': {   // a short sine that drops into pitch, with a little square bite\n"
    "      const o = O('sine', f * 2, t, stop); o.frequency.exponentialRampToValueAtTime(f, t + 0.03); o.connect(g);\n"
    "      O('square', f, t, stop).connect(F('lowpass', 1400, 1, G(0.12, g)));\n"
    "      env(g.gain, t, 0.75 * v, 0.002, 0.12, 0.0001, Math.min(end, t + 0.12), 0.04); sends(g, 0.12, 0.18); stop = t + 0.5; break;\n"
    "    }\n"
    "    case 'brass': {\n"
    "      const lp = F('lowpass', 700, 1.2, g);\n"
    "      lp.frequency.setValueAtTime(700, t); lp.frequency.linearRampToValueAtTime(acc ? 4200 : 3200, t + 0.04); lp.frequency.setTargetAtTime(1800, t + 0.05, 0.12);\n"
    "      [-7, 0, 7].forEach(d => O('sawtooth', f, t, stop, d).connect(lp));\n"
    "      env(g.gain, t, 0.34 * v, 0.018, 0.25, 0.75, end, 0.08); sends(g, 0.22, 0.08); break;\n"
    "    }\n"
    "    case 'orch': {   // orchestra hit: bright stacked saws and a noise burst, fast decay\n"
    "      const lp = F('lowpass', 7000, 0.8, g);\n"
    "      lp.frequency.setValueAtTime(7000, t); lp.frequency.setTargetAtTime(1600, t + 0.02, 0.12);\n"
    "      [-10, 0, 10].forEach(d => O('sawtooth', f, t, stop, d).connect(lp)); O('square', f / 2, t, stop).connect(G(0.3, lp));\n"
    "      const n = ctx.createBufferSource(); n.buffer = noiseBuf; n.start(t); n.stop(t + 0.08); n.connect(F('bandpass', 2500, 0.8, G(0.22 * v, g)));\n"
    "      env(g.gain, t, 0.3 * v, 0.002, 0.3, 0.0001, Math.min(end, t + 0.45), 0.12); sends(g, 0.4, 0.05); stop = t + 1.2; break;\n"
    "    }\n"
    "    case 'choir': {   // 'aah': detuned saws through three vowel formants, slow attack\n"
    "      const mix = G(1), l = O('sine', 5, t, stop + 0.8), lg = G(5); l.connect(lg);\n"
    "      [-9, 0, 9].forEach(d => { const o = O('sawtooth', f, t, stop + 0.8, d); lg.connect(o.detune); o.connect(mix); });\n"
    "      [[800, 5, 1], [1150, 7, 0.55], [2900, 9, 0.3]].forEach(([fr, q, a]) => mix.connect(F('bandpass', fr, q, G(a * 1.5, g))));\n"
    "      env(g.gain, t, 0.5 * v, dur < 0.25 ? 0.02 : 0.22, 0.4, 0.9, end, dur < 0.25 ? 0.12 : 0.4); sends(g, 0.5, 0.08); stop = end + 1.2; break;\n"
    "    }\n"
    "    case 'talkbox': {   // a saw through a moving vowel (wah), gliding between legato notes\n"
    "      const o = O('sawtooth', f, t, stop), prev = glide.talk;\n"
    "      if (prev && prev.legato && Math.abs(prev.end - t) < 0.08) { o.frequency.setValueAtTime(prev.f, t); o.frequency.exponentialRampToValueAtTime(f, t + 0.05); }\n"
    "      glide.talk = { f, legato: ev.legato, end };\n"
    "      const a = Math.min(0.12, dur / 2), b1 = F('bandpass', 450, 4, g), b2 = F('bandpass', 1000, 6, G(0.6, g));\n"
    "      b1.frequency.setValueAtTime(450, t); b1.frequency.linearRampToValueAtTime(1100, t + a); b1.frequency.linearRampToValueAtTime(700, Math.max(t + a + 0.01, end));\n"
    "      b2.frequency.setValueAtTime(1000, t); b2.frequency.linearRampToValueAtTime(1900, t + a); b2.frequency.linearRampToValueAtTime(1300, Math.max(t + a + 0.01, end));\n"
    "      o.connect(b1); o.connect(b2);\n"
    "      env(g.gain, t, 0.95 * v, 0.008, 0.2, 0.85, end, 0.05); sends(g, 0.2, 0.2); break;\n"
    "    }\n"
    "    case 'hoover': {   // a spread of detuned saws plus the octave below\n"
    "      const lp = F('lowpass', 3200, 1, g);\n"
    "      [-28, -12, 0, 12, 28].forEach(d => O('sawtooth', f, t, stop, d).connect(G(0.3, lp)));\n"
    "      O('sawtooth', f / 2, t, stop).connect(G(0.25, lp));\n"
    "      env(g.gain, t, 0.36 * v, 0.01, 0.3, 0.8, end, 0.12); sends(g, 0.3, 0.15); break;\n"
    "    }\n"
    "    case 'siren': {\n"
    "      O('sine', f, t, stop).connect(g); O('square', f, t, stop).connect(F('lowpass', 2400, 1, G(0.18, g)));\n"
    "      env(g.gain, t, 0.3 * v, 0.02, 0.2, 0.9, end, 0.1); sends(g, 0.45, 0.45); break;\n"
    "    }\n"
    "    case 'strings2': {   // the strings layer: short notes are pizzicato\n"
    "      if (dur < 0.2) {\n"
    "        const lp = F('lowpass', 2600, 0.8, g);\n"
    "        [-6, 6].forEach(d => O('triangle', f, t, stop, d).connect(lp)); O('sawtooth', f, t, stop).connect(G(0.25, lp));\n"
    "        env(g.gain, t, 0.5 * v, 0.003, 0.12, 0.0001, t + 0.14, 0.06); sends(g, 0.35, 0.05); stop = t + 0.8; break;\n"
    "      }\n"
    "      const lp = F('lowpass', 4200, 0.7, g);\n"
    "      [-11, -4, 3, 10].forEach(d => O('sawtooth', f, t, stop + 0.6, d).connect(lp));\n"
    "      env(g.gain, t, 0.22 * v, dur < 0.5 ? 0.02 : 0.07, 0.4, 0.85, end, 0.35); sends(g, 0.45, 0.12); stop = end + 1.2; break;\n"
    "    }\n")
rep("  setTimeout(() => { try { g.disconnect(); } catch (e) {} }, (stop - ctx.currentTime + 1.5) * 1000);\n}",
    "  if (OSC_SINK) { applyBends(OSC_SINK, ev.bends, t); OSC_SINK = null; }\n  setTimeout(() => { try { g.disconnect(); } catch (e) {} }, (stop - ctx.currentTime + 1.5) * 1000);\n}")
rep("  src.start(t); src.stop(end + rel * 3 + 0.05);\n  if (kind === 'piano') sends(g, 0.18, 0.06);",
    "  src.start(t); src.stop(end + rel * 3 + 0.05);\n  if (ev.bends && src.detune) applyBends([[src.detune, 0]], ev.bends, t);\n  if (kind === 'piano') sends(g, 0.18, 0.06);")

# ---- song-mode tables
rep("const SONG_KIND = { pad: 'pad', riser: 'saw', sub: 'sub', vox: 'vox' };\n"
    "const MUTE_OF = { pad: 'chords', arp: 'lead', riser: 'lead', sub: 'bass', stabs: 'chords', counter: 'lead', vox: 'lead', fx: 'drums' };\n",
    "const SONG_KIND = { pad: 'pad', riser: 'saw', sub: 'sub', vox: 'vox', donk: 'donk', acid: 'acidhi', piano: 'piano', organ: 'organ', strings: 'strings2', brass: 'brass', orch: 'orch', choir: 'choir', talkbox: 'talkbox', hoover: 'hoover', siren: 'siren', voxguide: 'vox' };\n"
    "const MUTE_OF = { pad: 'chords', arp: 'lead', riser: 'lead', sub: 'bass', stabs: 'chords', counter: 'lead', vox: 'lead', fx: 'drums',\n"
    "  donk: 'bass', acid: 'bass', piano: 'chords', organ: 'chords', strings: 'chords', brass: 'chords', orch: 'chords', choir: 'chords', talkbox: 'lead', hoover: 'lead', siren: 'lead', voxguide: 'lead', shouts: 'lead', vocal: 'lead' };\n"
    "// the break-slice track plays the kit's own hits: slice k of a one-bar break -> [lane, level]\n"
    "const BREAK_HITS = [[['K', 1], ['CH', 0.7]], [['CH', 0.5]], [['K', 0.85], ['CH', 0.6]], [['CH', 0.5]], [['SN', 1], ['CH', 0.6]], [['CH', 0.5], ['SN', 0.35]], [['CH', 0.6]], [['SN', 0.4], ['CH', 0.5]],\n"
    "  [['K', 0.95], ['CH', 0.7]], [['CH', 0.5]], [['K', 0.8], ['CH', 0.6]], [['CH', 0.5]], [['SN', 1], ['CH', 0.6]], [['CH', 0.5]], [['SN', 0.4]], [['CH', 0.55], ['SN', 0.35]]];\n"
    "const NO_CC = new Set(['drums', 'kick', 'snare', 'hats', 'perc', 'fills', 'poly', 'micro', 'breaks', 'fx', 'shouts', 'voxguide', 'sidechain']);\n")
rep("const EXT_DRUMS = new Set(['kick', 'snare', 'hats', 'perc', 'fills']);",
    "const EXT_DRUMS = new Set(['kick', 'snare', 'hats', 'perc', 'fills', 'poly', 'micro']);")
rep("subdrop: () => 'ukg_fx_subdrop' };", "subdrop: () => 'ukg_fx_subdrop', rewind: () => 'ukg_fx_rewind', tapestop: () => 'mth_fx_tapestop' };")
rep("  await Promise.all(Object.keys(FX_SAMPLE).map(async f => { const id = FX_SAMPLE[f](kit.genre); if (BY_ID[id] && !fxBufs[id]) fxBufs[id] = await loadBuffer(BY_ID[id].file); }));\n}",
    "  if (sg.T.breaks && sg.T.breaks.length) ['K', 'SN', 'CH'].forEach(l => lanes.add(l));\n"
    "  await Promise.all([...lanes].map(async l => { const n = hitName(kit, l); if (BY_ID[n] && !drumBufs[n]) drumBufs[n] = await loadBuffer(BY_ID[n].file); }));\n"
    "  await Promise.all(Object.keys(FX_SAMPLE).map(async f => { const id = FX_SAMPLE[f](kit.genre); if (BY_ID[id] && !fxBufs[id]) fxBufs[id] = await loadBuffer(BY_ID[id].file); }));\n"
    "  const ids = new Set((sg.vocalCues || []).map(c => c.id).concat(sg.shouts || []));   // the pack's own vocals and shouts\n"
    "  await Promise.all([...ids].map(async id => { if (BY_ID[id] && !fxBufs[id]) fxBufs[id] = await loadBuffer(BY_ID[id].file); }));\n"
    "  if (sg.T.piano && sg.T.piano.length) await loadSampler('piano');\n}\n"
    "const liveSrc = new Set();\n"
    "function playSample(ev, t) {   // a sung hook (tempo-matched) or a shout from the pack\n"
    "  const buf = fxBufs[ev.sample]; if (!buf) return;\n"
    "  const src = ctx.createBufferSource(); src.buffer = buf; src.playbackRate.value = ev.rate || 1;\n"
    "  src.connect(G((ev.lane === 'vocal' ? 0.85 : 0.7) * vg(ev.vel), vocalBus));\n"
    "  src.start(t); liveSrc.add(src); src.onended = () => liveSrc.delete(src);\n"
    "}\n"
    "function stopSamples() { liveSrc.forEach(x => { try { x.stop(); } catch (e) {} }); liveSrc.clear(); }\n"
    "const TO = {};   // per-track output in song mode: CC11 expression gain into a CC74 low-pass\n"
    "function outFor(lane) {\n"
    "  const grp = MUTE_OF[lane] || lane;\n"
    "  if (seq.mode !== 'song' || !laneOut[grp]) return laneOut[grp];\n"
    "  if (!TO[lane]) { const f = F('lowpass', 19000, 0.5, laneOut[grp]); TO[lane] = { f, g: G(1, f) }; }\n"
    "  return TO[lane].g;\n"
    "}\n"
    "function applyCC(trk, cc, val, t) {\n"
    "  if (!outFor(trk) || !TO[trk]) return;\n"
    "  if (cc === 74) TO[trk].f.frequency.setTargetAtTime(Math.min(19000, 120 * Math.pow(2, val / 127 * 7.3)), t, 0.03);\n"
    "  else TO[trk].g.gain.setTargetAtTime(Math.pow(val / 127, 1.5), t, 0.03);\n"
    "}\n"
    "function resetCC(t) { Object.values(TO).forEach(o => { o.f.frequency.cancelScheduledValues(t); o.f.frequency.setTargetAtTime(19000, t, 0.01); o.g.gain.cancelScheduledValues(t); o.g.gain.setTargetAtTime(1, t, 0.01); }); }\n"
    "function ccStateAt(time) {   // the filter and expression values in force at a song position (for starts and seeks)\n"
    "  const now = ctx.currentTime, last = {};\n"
    "  for (const e of seq.events) { if (e.time > time) break; if (e.lane === 'cc') last[e.trk + '/' + e.cc] = e; }\n"
    "  resetCC(now);\n"
    "  Object.values(last).forEach(e => applyCC(e.trk, e.cc, e.val, now + 0.012));\n"
    "}")
# the generic lane loader line above was replaced; drop the original duplicate
rep("  EXT_DRUMS.forEach(k => sg.T[k].forEach(e => lanes.add(e.lane)));\n  await Promise.all([...lanes].map(async l => { const n = hitName(kit, l); if (BY_ID[n] && !drumBufs[n]) drumBufs[n] = await loadBuffer(BY_ID[n].file); }));\n  if (sg.T.breaks",
    "  EXT_DRUMS.forEach(k => (sg.T[k] || []).forEach(e => lanes.add(e.lane)));\n  if (sg.T.breaks")

# ---- compile the song: break slices, shouts, vocals, bends, CCs
rep("  const ev = [];\n  Object.keys(sg.T).forEach(tk => sg.T[tk].forEach(e => {\n"
    "    if (tk === 'drums' || EXT_DRUMS.has(tk)) ev.push({ time: sec(e.t), lane: 'drums', drum: e.lane, vel: e.v });\n"
    "    else if (tk === 'fx') ev.push({ time: sec(e.t), dur: sec(e.d), lane: 'fx', fx: e.fx, vel: e.v });\n"
    "    else ev.push({ time: sec(e.t), dur: sec(e.d), lane: tk, note: e.n, vel: e.v, legato: e.legato });\n"
    "  }));\n",
    "  const ev = [];\n"
    "  // the pack's hooks play as audio when the key is the original and the tempo is close; otherwise the guide melody plays on a synth voice\n"
    "  const cues = sg.vocalCues || [], wav = cues.length > 0 && cues.every(c => c.shift === 0 && Math.abs(st.bpm / c.bpm - 1) <= 0.035 && BY_ID[c.id]);\n"
    "  if (wav) cues.forEach(c => ev.push({ time: sec(c.t), lane: 'vocal', sample: c.id, rate: st.bpm / c.bpm, vel: 110 }));\n"
    "  Object.keys(sg.T).forEach(tk => {\n"
    "    const B = sg.BEND && sg.BEND[tk] && sg.BEND[tk].length ? sg.BEND[tk] : null;\n"
    "    let j = -1;\n"
    "    sg.T[tk].forEach(e => {\n"
    "      if (tk === 'sidechain' || (tk === 'voxguide' && wav)) return;\n"
    "      if (tk === 'drums' || EXT_DRUMS.has(tk)) ev.push({ time: sec(e.t), lane: 'drums', drum: e.lane, vel: e.v });\n"
    "      else if (tk === 'breaks') (BREAK_HITS[e.n - 36] || []).forEach(([l, f]) => ev.push({ time: sec(e.t), lane: 'drums', drum: l, vel: e.v * f * 0.8 }));\n"
    "      else if (tk === 'fx') ev.push({ time: sec(e.t), dur: sec(e.d), lane: 'fx', fx: e.fx, vel: e.v });\n"
    "      else if (tk === 'shouts') ev.push({ time: sec(e.t), lane: 'shouts', sample: sg.shouts[e.n - 48], vel: e.v });\n"
    "      else {\n"
    "        const o = { time: sec(e.t), dur: sec(e.d), lane: tk, note: e.n, vel: tk === 'voxguide' ? e.v * 0.75 : e.v, legato: e.legato };\n"
    "        if (B) {   // the bend in force when the note starts, then each change while it sounds\n"
    "          while (j + 1 < B.length && B[j + 1].t <= e.t + 2) j++;\n"
    "          const c0 = j >= 0 ? B[j].v * 1200 / 8192 : 0, pts = [];\n"
    "          for (let k = j + 1; k < B.length && B[k].t < e.t + e.d + 120; k++) pts.push([sec(B[k].t - e.t), B[k].v * 1200 / 8192]);\n"
    "          if (c0 || pts.length) o.bends = { c0, pts };\n"
    "        }\n"
    "        ev.push(o);\n"
    "      }\n"
    "    });\n"
    "  });\n"
    "  Object.keys(sg.CC || {}).forEach(tk => { if (!NO_CC.has(tk)) sg.CC[tk].forEach(c => { if (c.cc === 74 || c.cc === 11) ev.push({ time: sec(c.t), lane: 'cc', trk: tk, cc: c.cc, val: c.val }); }); });\n")

# ---- the scheduler: CC events always apply; vocals, shouts and per-track outputs
rep("    if (seq.cursor >= seq.events.length) { seq.cursor = 0; seq.anchor += seq.loopDur; }\n    const ev = seq.events[seq.cursor], t = seq.anchor + ev.time;\n    if (t > ahead) break;\n",
    "    if (seq.cursor >= seq.events.length) { seq.cursor = 0; seq.anchor += seq.loopDur; if (seq.mode === 'song') resetCC(seq.anchor); }\n    const ev = seq.events[seq.cursor], t = seq.anchor + ev.time;\n    if (t > ahead) break;\n"
    "    if (ev.lane === 'cc') { applyCC(ev.trk, ev.cc, ev.val, Math.max(t, ctx.currentTime)); seq.cursor++; continue; }\n")
rep("      else if (ev.lane === 'fx') fxHit(kit, ev, t);\n      else {",
    "      else if (ev.lane === 'fx') fxHit(kit, ev, t);\n      else if (ev.lane === 'vocal' || ev.lane === 'shouts') playSample(ev, t);\n      else {")
rep("        if (SAMPLER[kind]) sampler(kind, ev, t, laneOut[MUTE_OF[ev.lane] || ev.lane]);\n        else synth(kind, ev, t, laneOut[MUTE_OF[ev.lane] || ev.lane]);",
    "        if (SAMPLER[kind]) sampler(kind, ev, t, outFor(ev.lane));\n        else synth(kind, ev, t, outFor(ev.lane));")
rep("  seq.timer = null; rebuild();\n  seq.anchor = ctx.currentTime + 0.08; seq.cursor = 0;\n",
    "  seq.timer = null; rebuild();\n  seq.anchor = ctx.currentTime + 0.08; seq.cursor = 0;\n  stopSamples(); if (seq.mode === 'song') ccStateAt(0);\n")
rep("  seq.cursor = seq.events.findIndex(e => e.time >= t);\n  if (seq.cursor < 0) seq.cursor = 0;\n}",
    "  seq.cursor = seq.events.findIndex(e => e.time >= t);\n  if (seq.cursor < 0) seq.cursor = 0;\n  stopSamples(); ccStateAt(t);\n}")
rep("  seq.timer = null; S.playing = false;\n", "  seq.timer = null; S.playing = false;\n  stopSamples();\n")
rep("    if (seq.cursor < 0) { seq.cursor = 0; seq.anchor += c.loopDur; }\n  } else { seq.events = c.events; seq.loopDur = c.loopDur; }",
    "    if (seq.cursor < 0) { seq.cursor = 0; seq.anchor += c.loopDur; }\n    if (seq.mode === 'song') ccStateAt(Math.max(0, rel));\n  } else { seq.events = c.events; seq.loopDur = c.loopDur; }")

# ---- the arrangement view hides the sidechain trigger
rep("  const rows = ext ? P.EXT_TRACKS.map(t => t.key).filter(k => sg.T[k].length) : ['drums', 'bass', 'chords', 'lead', 'pad', 'arp', 'riser'];",
    "  const rows = ext ? P.EXT_TRACKS.map(t => t.key).filter(k => sg.T[k].length && k !== 'sidechain') : ['drums', 'bass', 'chords', 'lead', 'pad', 'arp', 'riser'];")

# ---- kit list marks the new songs
rep("        '<span class=\"mt tnum\"><span>' + k.bpm + ' BPM</span><span>' + key.label + '</span><span class=\"cam\">' + key.camelot + '</span></span></button>';",
    "        '<span class=\"mt tnum\"><span>' + k.bpm + ' BPM</span><span>' + key.label + '</span><span class=\"cam\">' + key.camelot + '</span>' + (k.ext ? '<span class=\"newtag\">New layers</span>' : '') + '</span></button>';")

# ---- kit text: what each new song adds
rep("function updateKitText() {",
    "const EXT_HINT = {\n"
    "  UKG07: 'A one-bar break chopped into 16th slices and re-sequenced: straight in the intro, layered under the 2-step, half speed in the half-time switch and as jungle edits in the second drop. A sine sub doubles the Reese, the pack\\'s \"Hold me closer\" hook rides the verse and drops, and a tape stop winds down into drop 2.',\n"
    "  UKG08: 'Pizzicato, swelling and legato strings, choir oohs, and a sub that dips in pitch at the end of held notes (pitch bend). Two pack vocals: \"Sunday morning\" and, in the post-hook, the brat line \"Expensive taste, pretty face\".',\n"
    "  MTH07: 'Rominimal: bongo, clave and click cycles of 3, 5 and 7 steps drifting against the bar, crackle and glitch ratchets, a 16-bar micro-edit section (beat repeats, drop-outs, bass stutters, gated hats), a bass filter that zooms open and retracts, and the whispered \"Deeper, deeper\" hook.',\n"
    "  MTH08: 'A bouncing rubber bass (every note drops two semitones into pitch), a 303 acid line, a rave hoover with swoops and dives at the peak, cowbell and ghetto-house claps, ad-lib shouts on their own sampler track, the \"Rich bitch energy\" chant and a tape stop into the peak.',\n"
    "  SG07: 'A dub siren on pitch-bend LFOs, a rewind after eight bars of the first drop (\"Pull up!\", spin-back, \"Come again!\"), Reese dives at phrase ends, a hoover in the last two drops, MC shouts, and two pack vocals: \"Feel the pressure\" and \"Big bass, big bag\".',\n"
    "  SG08: 'An offbeat donk, the bassline wub (a cutoff LFO whose rate changes every two bars), orchestral string stabs, a 2-step switch, a tape stop into drop 2, MC shouts and \"All the girls say ooh\".',\n"
    "  CH07: 'Disco strings (pizzicato, legato, scale runs into the bar), brass stabs that fall off by pitch bend and stay out of the singer\\'s way, a talkbox answering the vocal, a filtered intro, a filter-sweep switch and a close-out, plus \"You got me dancing\" and \"Buy me diamonds, baby\".',\n"
    "  CH08: 'A four-part choir with gospel stabs, organ swells and comping, piano runs and broken-chord fills, orchestra hits and a semitone key change for the last chorus. \"Lift me higher\" carries the verse and both choruses.'\n"
    "};\n"
    "function updateKitText() {")
rep("  $('#kitHint').textContent = 'Saves 12 MIDI files in ' + key.label + ' (' + key.camelot + ') at ' + st.bpm + ' BPM: drums main, variation and fill; bass, chords and melody A/B; an 8-bar arrangement; the full song; and the 16-track extended mix.';\n"
    "  const sg = songOf(kit, st), ext = !!sg.extended;\n"
    "  const nTr = ext ? P.EXT_TRACKS.filter(t => sg.T[t.key].length).length : 7;\n",
    "  const sg = songOf(kit, st), ext = !!sg.extended;\n"
    "  const nTr = ext ? P.EXT_TRACKS.filter(t => sg.T[t.key].length).length : 7;\n"
    "  const xs = songOf(kit, st, 'ext'), nExt = P.EXT_TRACKS.filter(t => xs.T[t.key].length).length;\n"
    "  $('#kitHint').textContent = 'Saves 12 MIDI files in ' + key.label + ' (' + key.camelot + ') at ' + st.bpm + ' BPM: drums main, variation and fill; bass, chords and melody A/B; an 8-bar arrangement; the full song; and the ' + nExt + '-track extended mix' +\n"
    "    (kit.ext ? ' (with a vocal-guide track carrying the lyrics, pitch-bend moves and a sidechain trigger). The kit zip also carries the pack vocals and shouts the extended mix uses.' : '.');\n")
rep("  $('#songHint').textContent = sg.sections.map(x => x.name).join(' → ') + '. ' + (ext\n",
    "  $('#songHint').textContent = sg.sections.map(x => x.name).join(' → ') + '. ' + (ext && kit.ext\n"
    "    ? EXT_HINT[kit.id] + ' Vocals play from the pack in the original key; in another key the vocal guide plays on a synth voice. Tap a section to jump there while it plays.'\n"
    "    : ext\n")

# ---- kit zip: the vocals and shouts the extended mix refers to
rep("    const files = P.kitMidiFiles(kit, st.pc, st.bpm).map(f => ({ name: dir + f.name, data: f.data }));\n"
    "    await offer(kit.id + '_' + P.slug(kit.name) + '_' + key.short.replace('#', 's') + '-' + key.camelot + '_' + st.bpm + 'bpm_MIDI.zip', P.makeZipParts(files));",
    "    const files = P.kitMidiFiles(kit, st.pc, st.bpm).map(f => ({ name: dir + f.name, data: f.data }));\n"
    "    if (kit.ext) {\n"
    "      const sg = songOf(kit, st, 'ext');\n"
    "      [...new Set(sg.vocalCues.map(c => c.id).concat(sg.shouts))].forEach(id => { const it = BY_ID[id]; if (it) files.push({ name: dir + 'Vocals used in the extended mix/' + base(it.pack), url: it.file }); });\n"
    "    }\n"
    "    const parts = files.some(f => f.url) ? await packFiles(files, 'Packing') : P.makeZipParts(files);\n"
    "    await offer(kit.id + '_' + P.slug(kit.name) + '_' + key.short.replace('#', 's') + '-' + key.camelot + '_' + st.bpm + 'bpm_' + (kit.ext ? 'MIDI+Vocals' : 'MIDI') + '.zip', parts);")

# ---- README: the eight new songs
rep("  L.push('Regions are 4 bars at 480 PPQ with tempo, 4/4 and key signature. Swing is written into the timing.');",
    "  L.push('', 'NEW SONGS WITH EXTRA LAYERS (two per genre, built on current sounds in each scene)', '');\n"
    "  P.KITS.filter(k => k.ext).forEach(k => {\n"
    "    const key = P.keyInfo(k.root, k.mode), x = P.buildExtended(k, k.root);\n"
    "    L.push(k.id + '  ' + k.name + '  (' + k.bpm + ' BPM, ' + key.label + ', ' + key.camelot + ', ' + x.bars + ' bars, ' + P.EXT_TRACKS.filter(t => x.T[t.key].length).length + ' tracks)');\n"
    "    L.push('  ' + x.sections.map(s => s.name).join(' > '));\n"
    "    L.push('  ' + EXT_HINT[k.id]);\n"
    "    const voc = [...new Set(x.vocalCues.map(c => c.id))].map(id => id.toUpperCase() + ' \"' + P.VOCALS[id].title + '\"');\n"
    "    if (voc.length) L.push('  Vocals: ' + voc.join(', ') + ' (4-bar loops, each cue marked on the Vocal Guide track)');\n"
    "    if (x.shouts.length) L.push('  Shouts track: ' + x.shouts.map((id, i) => P.noteLabel(48 + i) + ' ' + (P.ADLIBS[id] || id) + ' (' + id.toUpperCase() + ')').join(', '));\n"
    "    L.push('');\n"
    "  });\n"
    "  L.push('How the new layers are written:',\n"
    "    '  Vocal Guide: the hook melody as sung, with each word as a MIDI lyric (FF 05) and a cue point (FF 07) naming the pack file at the',\n"
    "    '  start of every 4-bar block. Drop that WAV on the cue. The melody (Melody track) rests while the vocal sings and the vocal chops step around it.',\n"
    "    '  Break Slices: one bar of break cut into 16 equal slices mapped C1 (36) to D#2 (51), as a sampler in slice mode lays them out.',\n"
    "    '  Shouts: one ad-lib per key from C3 (48) upward; the track name lists them. Load the pack WAVs onto those keys.',\n"
    "    '  Pitch bends (Reese dives, hoover swoops, brass falls, siren LFOs, bounce and wub dips, tape stops) assume a +/-12 semitone bend range.',\n"
    "    '  The files set it with RPN 0 on each bent track; if your synth ignores that, set its bend range to 12.',\n"
    "    '  FX Hits adds note 43 rewind spin-back and 44 tape stop. Sidechain Trigger: note 35 at velocity 1 on every kick, to key a compressor.',\n"
    "    '  Layer tracks (acid, donk, piano, organ, strings, brass, orchestra hits, choir, talkbox, hoover, siren, shouts) take whichever MIDI channels',\n"
    "    '  are free; the vocal guide shares channel 12 with the vocal chops (same program).', '');\n"
    "  L.push('Regions are 4 bars at 480 PPQ with tempo, 4/4 and key signature. Swing is written into the timing.');")

open(P, 'w').write(s)
print('patched app.js')
