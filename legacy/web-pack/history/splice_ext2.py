"""Splice the ext-spec engine (custom forms, vocal guide, new layers, pitch bends, lyrics) into core.js."""
import sys

P = '/home/claude/work/core.js'
s = open(P).read()
if 'const EXT_LAYERS' in s:
    sys.exit('already spliced')
W = '/home/claude/work/'
tables = open(W + 'ext2_tables.js').read()
state = open(W + 'ext2_state.js').read()
layers = open(W + 'ext2_layers.js').read()


def rep(old, new, count=1):
    global s
    n = s.count(old)
    assert n == count, (n, old[:80])
    s = s.replace(old, new)


# module-level tables right after FX_NOTE
rep("  const FX_NOTE = { impact: 36, revcrash: 38, downlifter: 40, subdrop: 41 };\n",
    "  const FX_NOTE = { impact: 36, revcrash: 38, downlifter: 40, subdrop: 41 };\n" + tables)

# per-build state
rep("    EXT_TRACKS.forEach(t => { T[t.key] = []; CC[t.key] = []; });\n",
    "    EXT_TRACKS.forEach(t => { T[t.key] = []; CC[t.key] = []; });\n" + state)

# pushN: vocal mutes, unpitched tracks ignore the key lift
rep("    const pushN = (trk, t, d, n, v, extra) => { const m = n + lift - (lift && trk === 'bass' && n + lift > 55 ? 12 : 0); T[trk].push(",
    "    const pushN = (trk, t, d, n, v, extra) => { if (muted(trk, t)) return; const L = NOLIFT.has(trk) ? 0 : lift; const m = n + L - (L && trk === 'bass' && n + L > 55 ? 12 : 0); T[trk].push(")

# counter-melody answers make room for the vocal guide too
rep("const lead = T.lead.filter(e => e.t >= b0 * BAR - 60",
    "const lead = T.lead.concat(T.voxguide).filter(e => e.t >= b0 * BAR - 60")

# new section types and layers before the forms
rep("    /* ---- the forms ---- */\n", layers + "\n    /* ---- the forms ---- */\n")
rep("    if (g === 'ukg') {\n",
    "    if (kit.ext && kit.ext.form) {\n"
    "      kit.ext.form.forEach(([name, type, bars, o]) => sec(name, SECTION_KIND[type] || type, bars, (b, n) => customSection(type, b, n, o || {})));\n"
    "    } else if (g === 'ukg') {\n")

# cuts (rewinds, vocal-block lead cuts) before the tidy pass; more monophonic tracks
rep("    const MONO = new Set(['bass', 'sub', 'lead', 'counter']);\n",
    "    cuts.forEach(c => Object.keys(T).forEach(k => {\n"
    "      if ((c.keep && c.keep.includes(k)) || (c.only && !c.only.includes(k))) return;\n"
    "      T[k] = T[k].filter(e => !(e.t >= c.t0 && e.t < c.t1));\n"
    "      T[k].forEach(e => { if (e.t < c.t0 && e.t + e.d > c.t0) e.d = Math.max(15, c.t0 - e.t); });\n"
    "    }));\n"
    "    const MONO = new Set(['bass', 'sub', 'lead', 'counter', 'breaks', 'acid', 'donk', 'hoover', 'siren', 'talkbox', 'voxguide']);\n")

# bends, lyrics, sidechain; key-lift amount from the key change itself
rep("    const harmLine = []; for (let i = 0; i < bars * 16; i++) { const c = HL.get(i); harmLine.push(c ? c.pcs.map(x => (x + (keyChanges.length && i * 120 >= keyChanges[0].t && i * 120 < keyChanges[1].t ? 2 : 0)) % 12) : null); }\n"
    "    return { T, CC, sections, bars, lengthTicks: L, keyChanges, harmLine, extended: true };\n",
    "    Object.keys(BEND).forEach(k => {\n"
    "      const out = [];\n"
    "      BEND[k].filter(x => x.t < L).sort((x, y) => x.t - y.t).forEach(x => { const p = out[out.length - 1]; if (p && p.t === x.t) p.v = x.v; else if (!p || p.v !== x.v) out.push(x); });\n"
    "      BEND[k] = T[k].length ? out : [];\n"
    "    });\n"
    "    Object.keys(META).forEach(k => { META[k] = META[k].filter(m => m.t < L).sort((a, b) => a.t - b.t); });\n"
    "    if (kit.ext && kit.ext.sidechain) {\n"
    "      const seen = new Set();\n"
    "      T.kick.concat(T.fills.filter(e => e.lane === 'K')).sort((a, b) => a.t - b.t).forEach(e => { if (!seen.has(e.t)) { seen.add(e.t); T.sidechain.push({ t: e.t, d: 60, n: 35, v: 1 }); } });\n"
    "    }\n"
    "    const liftAt = t => { for (let k = 0; k + 1 < keyChanges.length; k += 2) if (t >= keyChanges[k].t && t < keyChanges[k + 1].t) return ((keyChanges[k].pc - home) % 12 + 12) % 12; return 0; };\n"
    "    const harmLine = []; for (let i = 0; i < bars * 16; i++) { const c = HL.get(i), lf = liftAt(i * 120); harmLine.push(c ? c.pcs.map(x => (x + lf) % 12) : null); }\n"
    "    return { T, CC, BEND, META, sections, bars, lengthTicks: L, keyChanges, harmLine, vocalCues, shouts: SH, extended: true };\n")

# extended MIDI: dynamic channels for layer tracks, bends, lyrics and cues
rep("    const tracks = EXT_TRACKS.map(tk => ({ name: tk.name, events: sg.T[tk.key], ch: tk.ch, program: tk.program, ccs: sg.CC[tk.key] || [] })).filter(t => t.events.length);\n",
    "    const chans = extChannels(sg);\n"
    "    const tracks = EXT_TRACKS.map(tk => ({ name: extTrackName(tk, sg), events: sg.T[tk.key], ch: chans[tk.key], program: tk.program, ccs: sg.CC[tk.key] || [], bends: sg.BEND[tk.key], metas: sg.META[tk.key] })).filter(t => t.events.length);\n")
rep("  function extendedMidi(kit, targetPc, bpm) {\n",
    "  function extChannels(sg) {   // fixed channels for the standard tracks; layer tracks take whatever is free\n"
    "    const ch = {}, used = new Set();\n"
    "    EXT_TRACKS.forEach(tk => { if (tk.ch != null && sg.T[tk.key].length) { ch[tk.key] = tk.ch; used.add(tk.ch); } });\n"
    "    const free = [0, 13, 14, 15, 7, 5, 6, 4].filter(c => !used.has(c));\n"
    "    EXT_TRACKS.forEach(tk => { if (tk.ch == null && sg.T[tk.key].length) { if (!free.length) throw new Error('no free MIDI channel for ' + tk.key); ch[tk.key] = free.shift(); } });\n"
    "    return ch;\n"
    "  }\n"
    "  function extTrackName(tk, sg) {\n"
    "    if (tk.key !== 'shouts' || !sg.shouts.length) return tk.name;\n"
    "    return 'Shouts (' + sg.shouts.map((id, k) => noteLabel(48 + k) + ' ' + (ADLIBS[id] || id)).join(', ') + ')';\n"
    "  }\n"
    "  function extendedMidi(kit, targetPc, bpm) {\n")

# MIDI writer: pitch bends (with an RPN bend-range of 12), lyric and cue meta events
rep("  function noteEvents(events, ch, program, ccs) {\n    const out = [];\n    if (program != null && ch !== 9) out.push({ t: 0, o: 5, bytes: [0xc0 | ch, program] });\n",
    "  function noteEvents(events, ch, program, ccs, bends, metas) {\n    const out = [];\n    if (program != null && ch !== 9) out.push({ t: 0, o: 5, bytes: [0xc0 | ch, program] });\n"
    "    if (bends && bends.length && ch !== 9) [[101, 0], [100, 0], [6, 12], [38, 0], [101, 127], [100, 127]].forEach(([c, v], i) => out.push({ t: 0, o: 6 + i * 0.1, bytes: [0xb0 | ch, c, v] }));\n"
    "    (bends || []).forEach(b => { const v = b.v + 8192; out.push({ t: b.t, o: 16, bytes: [0xe0 | ch, v & 127, (v >> 7) & 127] }); });\n"
    "    (metas || []).forEach(m => { const b = strBytes(m.text); out.push({ t: m.t, o: m.kind === 'cue' ? 4 : 19, bytes: [0xff, m.kind === 'cue' ? 0x07 : 0x05, ...vlq(b.length), ...b] }); });\n")
rep("...noteEvents(t.events, t.ch, t.program, t.ccs)], opts.lengthTicks));",
    "...noteEvents(t.events, t.ch, t.program, t.ccs, t.bends, t.metas)], opts.lengthTicks));")

# export the tables the page needs
rep("buildExtended, extendedMidi, EXT_TRACKS, FX_NOTE,", "buildExtended, extendedMidi, EXT_TRACKS, FX_NOTE, VOCALS, ADLIBS, SLICE,")
open(P, 'w').write(s)
print('spliced; core.js now', s.count('\n'), 'lines')
