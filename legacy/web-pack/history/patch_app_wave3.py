"""App-wide pass: sprite-packed samples, new Bass & Keys / Loops & FX groups, search, Fits-this-kit, Crate,
looping auditions synced to the kit, compact phone layout, shorter header, kit cross-links, loading state."""
import sys

P = '/home/claude/work/app.js'
H = '/home/claude/work/app.src.html'
s = open(P).read()
h = open(H).read()
if 'spriteWav' in s:
    sys.exit('already patched')


def rep(src, old, new, count=1):
    n = src.count(old)
    assert n == count, (n, old[:100])
    return src.replace(old, new)


# ------------------------------------------------------------------ state
s = rep(s, "  songV: store.get('songV', 'ext') === 'full' ? 'full' : 'ext'\n};",
        "  songV: store.get('songV', 'ext') === 'full' ? 'full' : 'ext',\n"
        "  q: '', fits: store.get('fits', false), crate: new Set(store.get('crate', []))\n};")
s = rep(s, "  save: '<svg viewBox=\"0 0 16 16\"><path d=\"M7 1h2v8.2l2.6-2.6 1.4 1.4L8 13 3 8l1.4-1.4L7 9.2z\"/><path d=\"M2 14h12v2H2z\"/></svg>'\n};",
        "  save: '<svg viewBox=\"0 0 16 16\"><path d=\"M7 1h2v8.2l2.6-2.6 1.4 1.4L8 13 3 8l1.4-1.4L7 9.2z\"/><path d=\"M2 14h12v2H2z\"/></svg>',\n"
        "  star: '<svg viewBox=\"0 0 16 16\"><path d=\"M8 1.2l2 4.4 4.8.5-3.6 3.2 1 4.7L8 11.6 3.8 14l1-4.7L1.2 6.1 6 5.6z\"/></svg>',\n"
        "  search: '<svg viewBox=\"0 0 16 16\"><path d=\"M6.8 1.5a5.3 5.3 0 014.2 8.6l3.6 3.6-1.3 1.3-3.6-3.6A5.3 5.3 0 116.8 1.5zm0 1.9a3.4 3.4 0 100 6.8 3.4 3.4 0 000-6.8z\"/></svg>'\n};")

# ------------------------------------------------------------------ sprite-packed samples: each item is a slice of a bigger WAV
s = rep(s, "async function fetchBytes(url) {\n",
        "/* wave-3 samples live in a few sprite WAVs (16-bit PCM, 44-byte header); an item's file is\n"
        "   'sprite:<path>?o=<first frame>&n=<frames>&c=<channels>', cut out here as a standalone WAV */\n"
        "const spriteFiles = new Map();\n"
        "function spriteOf(url) { const m = /^sprite:([^?]+)\\?o=(\\d+)&n=(\\d+)&c=(\\d)$/.exec(url || ''); return m ? { path: m[1], o: +m[2], n: +m[3], c: +m[4] } : null; }\n"
        "function spriteFile(path) {\n"
        "  if (!spriteFiles.has(path)) {\n"
        "    const p = fetch(path).then(r => { if (!r.ok) throw new Error('HTTP ' + r.status + ' for ' + path); return r.arrayBuffer(); });\n"
        "    p.catch(() => spriteFiles.delete(path));\n"
        "    spriteFiles.set(path, p);\n"
        "  }\n"
        "  return spriteFiles.get(path);\n"
        "}\n"
        "async function spriteWav(url) {\n"
        "  const sp = spriteOf(url), ab = await spriteFile(sp.path), len = sp.n * 2 * sp.c;\n"
        "  const out = new Uint8Array(44 + len), v = new DataView(out.buffer), w4 = (o, t) => { for (let i = 0; i < 4; i++) out[o + i] = t.charCodeAt(i); };\n"
        "  w4(0, 'RIFF'); v.setUint32(4, 36 + len, true); w4(8, 'WAVE'); w4(12, 'fmt '); v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, sp.c, true);\n"
        "  v.setUint32(24, 44100, true); v.setUint32(28, 44100 * 2 * sp.c, true); v.setUint16(32, 2 * sp.c, true); v.setUint16(34, 16, true); w4(36, 'data'); v.setUint32(40, len, true);\n"
        "  out.set(new Uint8Array(ab, 44 + sp.o * 2 * sp.c, len), 44);\n"
        "  return out;\n"
        "}\n"
        "async function fetchBytes(url) {\n"
        "  if (String(url).startsWith('sprite:')) return spriteWav(url);\n")
s = rep(s, "    const p = fetch(url).then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.arrayBuffer(); })\n      .then(ab => new Promise((res, rej) => ctx.decodeAudioData(ab, res, rej)));",
        "    const p = (String(url).startsWith('sprite:') ? spriteWav(url).then(b => b.buffer) : fetch(url).then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.arrayBuffer(); }))\n      .then(ab => new Promise((res, rej) => ctx.decodeAudioData(ab, res, rej)));")

# ------------------------------------------------------------------ key text for keyless FX
s = rep(s, "  if (!it.key) return 'No key (drums)';\n",
        "  if (!it.key) return it.cat === 'loops' || it.cat === 'fills' || it.cat === 'hits' ? 'No key (drums)' : 'No key';\n")

# ------------------------------------------------------------------ auditions: loops repeat until stopped; drum loops follow the kit's tempo
s = rep(s, "/* ---------- one-shot sample player ---------- */\nlet cur = null;\n",
        "/* ---------- sample player: one-shots play once, loops repeat until stopped ---------- */\nlet cur = null;\n"
        "const ONCE_KINDS = new Set(['riser', 'bomb', 'whoosh', 'airhorn']);\n"
        "const isLoop = it => !!it.bars && it.cat !== 'fills' && !ONCE_KINDS.has(it.kind) && !(it.cat === 'fx' && it.kind !== 'texture') && (it.cat !== 'vox' || it.group === 'hooks' || it.group === 'brat');\n"
        "const isDrumLoop = it => isLoop(it) && !it.key && (it.cat === 'loops' || it.kind === 'texture');\n")
s = rep(s, "  if (!opts.overlap) stopItem();\n  const src = ctx.createBufferSource(); src.buffer = buf;\n  const g = G(1, sampleBus); src.connect(g);\n  let when = ctx.currentTime + 0.01;\n  if (S.playing && !opts.overlap) {\n    const beat = 60 / ks().bpm, pos = ctx.currentTime - seq.anchor;\n    when = Math.max(when, seq.anchor + Math.ceil(pos / beat + 0.05) * beat);\n  }\n  src.start(when);\n  if (opts.overlap) return;\n  cur = { it, src, g, when, dur: buf.duration };",
        "  if (!opts.overlap) stopItem();\n  const src = ctx.createBufferSource(); src.buffer = buf;\n  const loop = !opts.overlap && isLoop(it), sync = loop && S.playing && isDrumLoop(it) && it.bpm;\n"
        "  if (loop) { src.loop = true; src.loopStart = 0; src.loopEnd = buf.duration; }\n"
        "  const rate = sync ? ks().bpm / it.bpm : 1; src.playbackRate.value = rate;\n"
        "  const g = G(1, sampleBus); src.connect(g);\n  let when = ctx.currentTime + 0.01;\n  if (S.playing && !opts.overlap) {   // one-shots start on the next beat, loops on the next bar\n"
        "    const beat = 60 / ks().bpm * (loop ? 4 : 1), pos = ctx.currentTime - seq.anchor;\n    when = Math.max(when, seq.anchor + Math.ceil(pos / beat + 0.02) * beat);\n  }\n  src.start(when);\n  if (opts.overlap) return;\n"
        "  cur = { it, src, g, when, dur: buf.duration / rate, loop, sync: !!sync };\n"
        "  if (loop) toast((sync ? 'Looping in time with ' + curKit().id : 'Looping') + ' · tap again to stop', 1800);")
s = rep(s, "      const p = Math.min(1, Math.max(0, (ctx.currentTime - cur.when) / cur.dur));",
        "      const el = (ctx.currentTime - cur.when) / cur.dur, p = cur.loop ? (el > 0 ? el % 1 : 0) : Math.min(1, Math.max(0, el));")
s = rep(s, "  try { buf = await loadBuffer(it.file); } catch (e) { toast(\"Couldn't load \" + it.title + '.'); return; }",
        "  const pb = document.querySelector('[data-play=\"' + it.id + '\"]');\n"
        "  if (pb && !opts.overlap) pb.classList.add('load');\n"
        "  try { buf = await loadBuffer(it.file); } catch (e) { if (pb) pb.classList.remove('load'); toast(\"Couldn't load \" + it.title + '.'); return; }\n"
        "  if (pb) pb.classList.remove('load');")
s = rep(s, "    st.bpm = nb; updateKitText(); lcd(); if (S.playing) rebuild(); return;",
        "    st.bpm = nb; updateKitText(); lcd(); if (S.playing) rebuild();\n    if (cur && cur.sync) { cur.src.playbackRate.value = nb / cur.it.bpm; }\n    refreshFits(); return;")

# ------------------------------------------------------------------ search, Fits-this-kit and the crate
s = rep(s, "const VOX_FILTERS = [",
        "/* ---------- search, 'fits this kit', crate ---------- */\n"
        "function kitKey() { const kit = curKit(), st = ks(kit), k = P.keyInfo(st.pc, kit.mode); return { cam: k.camelot, label: k.label, bpm: st.bpm, kit }; }\n"
        "function camOK(a, b) {   // Camelot neighbours: same code, one step round the wheel, or the relative major/minor\n"
        "  const m1 = /^(\\d+)([AB])$/.exec(a || ''), m2 = /^(\\d+)([AB])$/.exec(b || '');\n"
        "  if (!m1 || !m2) return true;\n"
        "  const d = ((+m1[1] - +m2[1]) % 12 + 12) % 12;\n"
        "  return m1[2] === m2[2] ? d === 0 || d === 1 || d === 11 : d === 0;\n"
        "}\n"
        "function fitsKit(it) {\n"
        "  const k = kitKey();\n"
        "  if (it.camelot && !camOK(it.camelot, k.cam)) return false;\n"
        "  if (it.bars && it.bpm) return Math.abs(it.bpm - k.bpm) <= (it.key ? 4 : 8);\n"
        "  return true;\n"
        "}\n"
        "const hay = it => (it.hay || (it.hay = [it.title, it.note, it.voice, it.kind, it.group, it.key, it.camelot, it.bpm ? it.bpm + ' bpm' : '', GSHORT[it.genre], GENRE[it.genre], it.pack].filter(Boolean).join(' ').toLowerCase()));\n"
        "function shown(it) {\n"
        "  if (S.fits && !fitsKit(it)) return false;\n"
        "  const q = S.q.trim().toLowerCase();\n"
        "  return !q || q.split(/\\s+/).every(w => hay(it).includes(w));\n"
        "}\n"
        "function toolsHTML() {\n"
        "  const k = kitKey();\n"
        "  return '<div class=\"tools\"><label class=\"search\">' + ICON.search + '<input type=\"search\" data-q placeholder=\"Search sounds, keys, BPM\" aria-label=\"Search sounds\" value=\"' + esc(S.q) + '\"></label>' +\n"
        "    '<button class=\"chip fits\" data-fits aria-pressed=\"' + S.fits + '\" title=\"Only show sounds in a key that mixes with the kit and a tempo near it\">Fits ' + k.kit.id + ' · ' + k.cam + ' · ' + k.bpm + ' BPM</button></div>';\n"
        "}\n"
        "function renderTools() { $$('.toolsbox').forEach(b => { const has = b.querySelector('[data-q]'); const focus = has && document.activeElement === has; if (!focus) b.innerHTML = toolsHTML(); else b.querySelector('[data-fits]').outerHTML = toolsHTML().match(/<button[^]*<\\/button>/)[0]; }); }\n"
        "function refreshFits() { renderTools(); if (S.view === 'vocals') renderVox(); if (S.view === 'bass') renderBK(); if (S.view === 'fx') renderFx(); if (S.view === 'kits') updateFitCounts(); }\n"
        "const emptyHTML = () => '<p class=\"hint empty\">Nothing matches' + (S.q ? ' “' + esc(S.q) + '”' : '') + (S.fits ? ' that fits ' + curKit().id : '') + '. ' + (S.fits ? '<button class=\"btn ghost\" data-fitsoff>Show every key and tempo</button>' : '') + '</p>';\n"
        "function starBtn(it) { const on = S.crate.has(it.id); return '<button class=\"star\" data-star=\"' + it.id + '\" aria-pressed=\"' + on + '\" aria-label=\"' + (on ? 'Remove from crate' : 'Add to crate') + '\">' + ICON.star + '</button>'; }\n"
        "function toggleStar(id) {\n"
        "  if (S.crate.has(id)) S.crate.delete(id); else S.crate.add(id);\n"
        "  store.set('crate', [...S.crate]);\n"
        "  $$('[data-star=\"' + id + '\"]').forEach(b => { const on = S.crate.has(id); b.setAttribute('aria-pressed', String(on)); b.setAttribute('aria-label', on ? 'Remove from crate' : 'Add to crate'); });\n"
        "  $('#tCrate').textContent = S.crate.size || '';\n"
        "  if (S.view === 'crate') renderCrate();\n"
        "}\n"
        "function renderCrate() {\n"
        "  const mem = [...S.crate].map(id => BY_ID[id]).filter(Boolean);\n"
        "  $('#tCrate').textContent = mem.length || '';\n"
        "  if (!mem.length) { $('#crateRows').innerHTML = '<p class=\"hint empty\">Your crate is empty. Tap the star on any vocal, bass, keys, loop or FX row to collect it here.</p>'; markRow(); return; }\n"
        "  const groups = [['vox', 'Vocals'], ['bk', 'Bass & keys'], ['fx', 'Loops, fills & FX'], ['hits', 'Drum hits']];\n"
        "  const gOf = i => i.cat === 'vox' ? 'vox' : i.cat === 'bass' || i.cat === 'keys' ? 'bk' : i.cat === 'hits' ? 'hits' : 'fx';\n"
        "  $('#crateRows').innerHTML = groups.filter(([g]) => mem.some(i => gOf(i) === g)).map(([g, l]) => '<div class=\"groupname\"><span class=\"label\">' + l + '</span></div><div class=\"rows\">' + mem.filter(i => gOf(i) === g).map(rowHTML).join('') + '</div>').join('');\n"
        "  markRow();\n"
        "}\n"
        "function fitCount(list) { const was = S.q; S.q = ''; const f = S.fits; S.fits = true; const n = list.filter(shown).length; S.q = was; S.fits = f; return n; }\n"
        "function updateFitCounts() {\n"
        "  const box = $('#fitsrow'); if (!box) return;\n"
        "  box.innerHTML = '<span class=\"label\">Sounds that fit ' + curKit().id + ' (' + kitKey().cam + ', ' + kitKey().bpm + ' BPM)</span>' +\n"
        "    [['vocals', 'Vocals', voxItems()], ['bass', 'Bass & keys', bkItems()], ['fx', 'Loops & FX', fxItems()]].map(([v, l, list]) => '<button class=\"btn ghost\" data-gofits=\"' + v + '\">' + l + ' <b class=\"tnum\">' + fitCount(list) + '</b></button>').join('');\n"
        "}\n"
        "const VOX_FILTERS = [")

# rows: a star, a NEW tag for the latest samples, the loop marker
s = rep(s, "  const len = it.bars ? it.bars + (it.bars > 1 ? ' bars' : ' bar') : it.dur.toFixed(1) + ' s';\n  const k = keyText(it);",
        "  const len = it.bars ? it.bars + (it.bars > 1 ? ' bars' : ' bar') : it.dur.toFixed(1) + ' s';\n  const k = keyText(it);\n  const nw = it.wave === 3 ? '<span class=\"newtag\">New</span>' : '';")
s = rep(s, "    '<div class=\"rt\"><div class=\"t' + (isVox ? ' q' : '') + '\">' + title + (it.explicit ? '<span class=\"e\" title=\"Explicit\">E</span>' : '') + '</div><div class=\"s\">' + sub + '</div></div>' +",
        "    '<div class=\"rt\"><div class=\"t' + (isVox ? ' q' : '') + '\">' + title + (it.explicit ? '<span class=\"e\" title=\"Explicit\">E</span>' : '') + nw + '</div><div class=\"s\">' + sub + '</div></div>' +")
s = rep(s, "    '<div class=\"rowact\"><button class=\"btn ghost save\" data-saveitem=\"' + it.id + '\">' + ICON.save + 'Save</button></div></div>';",
        "    '<div class=\"rowact\">' + starBtn(it) + '<button class=\"btn ghost save\" data-saveitem=\"' + it.id + '\">' + ICON.save + '<span class=\"st\">Save</span></button></div></div>';")

# ------------------------------------------------------------------ Loops & FX and Bass & Keys: new groups and filters
s = rep(s, "const FX_FILTERS = [['all', 'All'], ['ukg-fills', 'UKG fills'], ['mth-fills', 'Minimal fills'], ['ukg-fx', 'UKG FX'], ['mth-fx', 'Minimal FX'], ['sg-fx', 'Speed garage FX'], ['loops', 'Drum loops']];\n"
        "const BK_FILTERS = [['all', 'All'], ['reese', 'Reese one-shots'], ['reeseloops', 'Reese loops'], ['organ', 'Organ bass'], ['piano', 'Piano'], ['strings', 'Strings & loops']];\n",
        "const FX_FILTERS = [['all', 'All'], ['new', 'New'], ['drums', 'Drum loops'], ['tops', 'Tops & perc'], ['breaks', 'Breaks'], ['fills', 'Fills'], ['fx', 'FX & textures']];\n"
        "const FX_GROUPS = [['dl-ukg', 'Drum loops · UK garage', 'drums', 'ukg'], ['dl-mth', 'Drum loops · minimal tech house', 'drums', 'mth'], ['dl-sg', 'Drum loops · speed garage', 'drums', 'sg'], ['dl-ch', 'Drum loops · classic house', 'drums', 'ch'],\n"
        "  ['tops', 'Top & percussion loops', 'tops'], ['breaks', 'Breaks · slice-ready and roomy', 'breaks', 'ukg'], ['ukg-fills', 'Fills · UK garage', 'fills', 'ukg'], ['mth-fills', 'Fills · minimal tech house', 'fills', 'mth'],\n"
        "  ['ukg-fx', 'FX · UK garage', 'fx', 'ukg'], ['mth-fx', 'FX · minimal tech house', 'fx', 'mth'], ['sg-fx', 'FX · speed garage', 'fx', 'sg'], ['ch-fx', 'FX · house & textures', 'fx', 'ch']];\n"
        "const BK_FILTERS = [['all', 'All'], ['new', 'New'], ['reese', 'Reese'], ['synth', 'Subs & synth bass'], ['organ', 'Organ bass'], ['piano', 'Piano'], ['keys', 'Keys & stabs'], ['strings', 'Strings']];\n"
        "const BK_GROUPS = [['reese', 'Reese bass · one-shots', 'reese', 'sg'], ['reeseloops', 'Reese bass · loops (kit basslines)', 'reese', 'sg'], ['subs', 'Subs & synth bass · one-shots', 'synth'],\n"
        "  ['bassloops', 'Bass loops & acid · from the new songs', 'synth'], ['organ', 'Organ bass', 'organ', 'ch'], ['piano', 'House piano', 'piano', 'ch'], ['garagekeys', 'Garage keys · organ stabs, Rhodes, bells', 'keys', 'ukg'],\n"
        "  ['dubchords', 'Dub chords & bleeps', 'keys', 'mth'], ['housestabs', 'Brass, orchestra hits & choir', 'keys', 'ch'], ['strings', 'Strings · pads, runs, stabs, loops', 'strings']];\n"
        "const BK_SETS = { reese: ['reese_classic', 'Reese Classic · chromatic C1–B1', 'sg', '2-second sustained notes, labelled by note'], subs: ['sub_2step', '2-Step Sub · chromatic C1–B1', 'ukg', '808-style sine sub with a long tail, one per note'],\n"
        "  organ: ['organ_bass', 'Organ Bass · chromatic C1–B2', 'ch', 'M1-style organ bass, 24 notes'], piano: ['piano_ms', 'House Piano · multisample C2–C6', 'ch', 'Dry notes every minor third, ready for a sampler'] };\n")
s = rep(s, "const fxKey = i => i.cat === 'loops' ? 'loops' : i.id.startsWith('sg_') ? 'sg-fx' : (i.id.startsWith('ukg') ? 'ukg-' : 'mth-') + (i.cat === 'fills' ? 'fills' : 'fx');\n",
        "const fxKey = i => {\n"
        "  if (i.grp === 'tops' || i.grp === 'breaks' || (i.grp || '').startsWith('dl-')) return i.grp;\n"
        "  if (i.grp === 'fx3') return (i.genre === 'all' ? 'ch' : i.genre) + '-fx';\n"
        "  if (i.cat === 'loops') return i.id.startsWith('drums_ch') ? 'dl-ch' : 'dl-sg';\n"
        "  return i.id.startsWith('sg_') ? 'sg-fx' : (i.id.startsWith('ukg') ? 'ukg-' : 'mth-') + (i.cat === 'fills' ? 'fills' : 'fx');\n"
        "};\n")
s = rep(s, "const bkKey = i => i.set === 'reese_classic' || /^reese_(wobble|growl|dark|dive|stab)$/.test(i.id) ? 'reese' : /^reese_(loop|wob)_/.test(i.id) ? 'reeseloops' :\n"
        "  i.set === 'organ_bass' || i.id.startsWith('organ_loop') ? 'organ' : (i.set === 'piano_ms' || /^p(stab|loop)_/.test(i.id)) ? 'piano' : 'strings';\n",
        "const bkKey = i => i.grp === 'sub' || i.grp === 'synthbass' ? 'subs' : i.grp === 'bassloops' ? 'bassloops' : ['garagekeys', 'dubchords', 'housestabs'].includes(i.grp) ? i.grp : i.grp === 'strings3' ? 'strings' :\n"
        "  i.set === 'reese_classic' || /^reese_(wobble|growl|dark|dive|stab)$/.test(i.id) ? 'reese' : /^reese_(loop|wob)_/.test(i.id) ? 'reeseloops' :\n"
        "  i.set === 'organ_bass' || i.id.startsWith('organ_loop') ? 'organ' : (i.set === 'piano_ms' || /^p(stab|loop)_/.test(i.id)) ? 'piano' : 'strings';\n"
        "if (!FX_FILTERS.some(f => f[0] === S.fxFilter)) S.fxFilter = 'all';\n"
        "if (!BK_FILTERS.some(f => f[0] === S.bkFilter)) S.bkFilter = 'all';\n")

# vocals: search + fits
s = rep(s, "  const all = voxItems().filter(i => !(S.hideExplicit && i.explicit)).filter(i => S.voxGenre === 'all' || i.genre === S.voxGenre || i.genre === 'all');",
        "  const all = voxItems().filter(i => !(S.hideExplicit && i.explicit)).filter(i => S.voxGenre === 'all' || i.genre === S.voxGenre || i.genre === 'all').filter(shown);")
s = rep(s, "    '<div class=\"groupname\"><span class=\"label\">' + l + '</span></div><div class=\"rows\">' + list.filter(i => i.group === g).map(rowHTML).join('') + '</div>').join('') ||\n    '<p class=\"hint\">Nothing matches these filters.</p>';",
        "    '<div class=\"groupname\"><span class=\"label\">' + l + '</span></div><div class=\"rows\">' + list.filter(i => i.group === g).map(rowHTML).join('') + '</div>').join('') ||\n    emptyHTML();")

# loops & fx
s = rep(s, "function renderFx() {\n  const all = fxItems();\n  $('#fxFilters').innerHTML = FX_FILTERS.map(([f, l]) => '<button class=\"chip\" data-fxf=\"' + f + '\" aria-pressed=\"' + (S.fxFilter === f) + '\">' + l + '<span class=\"n\">' + (f === 'all' ? all.length : all.filter(i => fxKey(i) === f).length) + '</span></button>').join('');\n"
        "  const groups = [['ukg-fills', 'UK garage fills'], ['mth-fills', 'Minimal tech house fills'], ['ukg-fx', 'UK garage FX & transitions'], ['mth-fx', 'Minimal tech house FX & transitions'], ['sg-fx', 'Speed garage FX'], ['loops', 'Drum loops · house & speed garage']];\n"
        "  $('#fxRows').innerHTML = groups.filter(([g]) => S.fxFilter === 'all' || S.fxFilter === g).map(([g, l]) =>\n"
        "    '<div class=\"groupname\"><span class=\"label\">' + l + '</span></div><div class=\"rows\">' + all.filter(i => fxKey(i) === g).map(rowHTML).join('') + '</div>').join('');\n  markRow();\n}",
        "function renderFx() {\n  const all = fxItems().filter(shown);\n"
        "  const inF = (i, f) => f === 'all' || (f === 'new' ? i.wave === 3 : (FX_GROUPS.find(g => g[0] === fxKey(i)) || [])[2] === f);\n"
        "  $('#fxFilters').innerHTML = FX_FILTERS.map(([f, l]) => '<button class=\"chip\" data-fxf=\"' + f + '\" aria-pressed=\"' + (S.fxFilter === f) + '\">' + l + '<span class=\"n\">' + all.filter(i => inF(i, f)).length + '</span></button>').join('');\n"
        "  const list = all.filter(i => inF(i, S.fxFilter));\n"
        "  $('#fxRows').innerHTML = FX_GROUPS.filter(([g]) => list.some(i => fxKey(i) === g)).map(([g, l, , gg]) =>\n"
        "    '<div class=\"groupname\">' + (gg ? '<span class=\"dot g-' + gg + '\"></span>' : '') + '<span class=\"label\">' + l + '</span></div><div class=\"rows\">' + list.filter(i => fxKey(i) === g).map(rowHTML).join('') + '</div>').join('') || emptyHTML();\n  markRow();\n}")

# bass & keys
s = rep(s, "function renderBK() {\n  const all = bkItems();\n  $('#bkFilters').innerHTML = BK_FILTERS.map(([f, l]) => '<button class=\"chip\" data-bkf=\"' + f + '\" aria-pressed=\"' + (S.bkFilter === f) + '\">' + l + '</button>').join('');\n"
        "  const show = g => S.bkFilter === 'all' || S.bkFilter === g;\n  const rows = (pred) => '<div class=\"rows\">' + all.filter(i => !i.set && pred(i)).map(rowHTML).join('') + '</div>';\n  let h = '';\n",
        "function renderBK() {\n  const all = bkItems().filter(shown);\n"
        "  const inF = (i, f) => f === 'all' || (f === 'new' ? i.wave === 3 : (BK_GROUPS.find(g => g[0] === bkKey(i)) || [])[2] === f);\n"
        "  $('#bkFilters').innerHTML = BK_FILTERS.map(([f, l]) => '<button class=\"chip\" data-bkf=\"' + f + '\" aria-pressed=\"' + (S.bkFilter === f) + '\">' + l + '<span class=\"n\">' + all.filter(i => inF(i, f)).length + '</span></button>').join('');\n"
        "  const list = all.filter(i => inF(i, S.bkFilter));\n"
        "  let h = BK_GROUPS.filter(([g]) => list.some(i => bkKey(i) === g)).map(([g, l, , gg]) => {\n"
        "    const set = BK_SETS[g], mem = set ? list.filter(i => i.set === set[0]) : [];\n"
        "    return '<div class=\"groupname\">' + (gg ? '<span class=\"dot g-' + gg + '\"></span>' : '') + '<span class=\"label\">' + l + '</span></div>' +\n"
        "      (mem.length ? setHTML(set[0], set[1], '<span class=\"gtag t-' + set[2] + '\">' + GSHORT[set[2]] + '</span>' + set[3], mem) : '') +\n"
        "      '<div class=\"rows\">' + list.filter(i => !i.set && bkKey(i) === g).map(rowHTML).join('') + '</div>';\n"
        "  }).join('') || emptyHTML();\n"
        "  if (false) {\n"
        "  const show = g => S.bkFilter === 'all' || S.bkFilter === g;\n  const rows = (pred) => '<div class=\"rows\">' + all.filter(i => !i.set && pred(i)).map(rowHTML).join('') + '</div>';\n")
s = rep(s, "  if (show('strings')) h += '<div class=\"groupname\"><span class=\"dot g-ch\"></span><span class=\"label\">Strings & loops</span></div>' + rows(i => bkKey(i) === 'strings');\n  $('#bkRows').innerHTML = h;",
        "  if (show('strings')) h += '<div class=\"groupname\"><span class=\"dot g-ch\"></span><span class=\"label\">Strings & loops</span></div>' + rows(i => bkKey(i) === 'strings');\n  }\n  $('#bkRows').innerHTML = h;")
s = rep(s, "function setHTML(setId, title, sub) {\n  const mem = ITEMS.filter(i => i.set === setId);",
        "function setHTML(setId, title, sub, only) {\n  const mem = only || ITEMS.filter(i => i.set === setId);")

# views: the crate tab
s = rep(s, "  ['kits', 'vocals', 'bass', 'fx', 'hits'].forEach(n => { $('#view-' + n).hidden = n !== v; });\n  if (v === 'kits') { S.sel = { type: 'kit' }; lcd(); requestAnimationFrame(drawRolls); }",
        "  ['kits', 'vocals', 'bass', 'fx', 'hits', 'crate'].forEach(n => { $('#view-' + n).hidden = n !== v; });\n  if (v === 'kits') { S.sel = { type: 'kit' }; lcd(); requestAnimationFrame(drawRolls); updateFitCounts(); }\n"
        "  if (v === 'vocals' || v === 'bass' || v === 'fx') { renderTools(); ({ vocals: renderVox, bass: renderBK, fx: renderFx })[v](); }\n  if (v === 'crate') renderCrate();")
s = rep(s, "setView(['kits', 'vocals', 'bass', 'fx', 'hits'].includes(hv) ? hv : (store.get('view', 'kits') || 'kits'), false);",
        "setView(['kits', 'vocals', 'bass', 'fx', 'hits', 'crate'].includes(hv) ? hv : (store.get('view', 'kits') || 'kits'), false);")

# kit detail: cross-links into the sample views
s = rep(s, "    '<p class=\"hint\" id=\"kitHint\"></p>';\n  updateKitText();",
        "    '<p class=\"hint\" id=\"kitHint\"></p>' +\n    '<div class=\"fitsrow\" id=\"fitsrow\"></div>';\n  updateKitText(); updateFitCounts();")
s = rep(s, "  if (e.target.id === 'keySel') { ks().pc = +e.target.value; updateKitText(); drawRolls(); drawSong(); lcd(); if (S.playing) rebuild(); }",
        "  if (e.target.id === 'keySel') { ks().pc = +e.target.value; updateKitText(); drawRolls(); drawSong(); lcd(); if (S.playing) rebuild(); refreshFits(); }")
s = rep(s, "  if (S.playing) switchKitWhilePlaying(curKit());\n}",
        "  if (S.playing) switchKitWhilePlaying(curKit());\n  renderTools();\n}")

# events: star, fits, search, crate save/clear, cross-links
s = rep(s, "  if (t.id === 'playBtn') { S.playing ? stopLoop() : startLoop('loop'); return; }",
        "  if (t.id === 'playBtn') { S.playing ? stopLoop() : startLoop('loop'); return; }\n"
        "  if (t.dataset.star) { toggleStar(t.dataset.star); return; }\n"
        "  if (t.hasAttribute('data-fits') || t.hasAttribute('data-fitsoff')) { S.fits = t.hasAttribute('data-fits') ? !S.fits : false; store.set('fits', S.fits); refreshFits(); return; }\n"
        "  if (t.dataset.gofits) { S.fits = true; store.set('fits', true); S.q = ''; setView(t.dataset.gofits, true); window.scrollTo({ top: 0 }); return; }\n"
        "  if (t.id === 'saveCrate') { const mem = [...S.crate].map(id => BY_ID[id]).filter(Boolean); if (!mem.length) { toast('Star a few sounds first.'); return; } saveGroup(mem, 'My-Crate_' + mem.length + '-sounds.zip', 'Packing your crate'); return; }\n"
        "  if (t.id === 'clearCrate') { if (!S.crate.size) return; S.crate.clear(); store.set('crate', []); renderCrate(); $$('[data-star]').forEach(b => b.setAttribute('aria-pressed', 'false')); toast('Crate cleared.'); return; }")
s = rep(s, "document.addEventListener('change', e => {",
        "let qT;\n"
        "document.addEventListener('input', e => {\n"
        "  if (!e.target.hasAttribute || !e.target.hasAttribute('data-q')) return;\n"
        "  S.q = e.target.value;\n"
        "  $$('[data-q]').forEach(x => { if (x !== e.target) x.value = S.q; });\n"
        "  clearTimeout(qT); qT = setTimeout(() => { if (S.view === 'vocals') renderVox(); if (S.view === 'bass') renderBK(); if (S.view === 'fx') renderFx(); }, 120);\n"
        "});\n"
        "document.addEventListener('change', e => {")

# boot: counts incl. crate; render tools
s = rep(s, "  $('#tKits').textContent = c.kits; $('#tVox').textContent = c.vox; $('#tBK').textContent = c.bk; $('#tFx').textContent = c.fx; $('#tHits').textContent = c.hits;",
        "  $('#tKits').textContent = c.kits; $('#tVox').textContent = c.vox; $('#tBK').textContent = c.bk; $('#tFx').textContent = c.fx; $('#tHits').textContent = c.hits; $('#tCrate').textContent = S.crate.size || '';")
s = rep(s, "renderKitList(); renderKitDetail(); renderVox(); renderFx(); renderBK(); renderPads();",
        "renderTools(); renderKitList(); renderKitDetail(); renderVox(); renderFx(); renderBK(); renderPads(); renderCrate();")
open(P, 'w').write(s)

# ------------------------------------------------------------------ page shell: header, tabs, tools, crate, CSS
h = rep(h, "    <span class=\"model\">MIDI · Songs · Reese · Vox — 128–140 BPM</span>",
        "    <span class=\"model\">MIDI · Songs · Samples · Vocals — 128–140 BPM</span>")
h = rep(h, h[h.index("  <p class=\"lede\">"):h.index("</p>", h.index("  <p class=\"lede\">")) + 4],
        "  <p class=\"lede\"><b><span id=\"cKits\">32</span> construction kits</b> for UK garage, minimal tech house, speed garage and classic house, each with drum, bass, chord and melody MIDI, a <b>full song</b> and an <b>extended mix</b>, plus <b><span id=\"cBK\">0</span> bass &amp; keys</b>, <b><span id=\"cFx\">0</span> loops, fills &amp; FX</b>, <b><span id=\"cHits\">0</span> drum hits</b> and <b><span id=\"cVox\">0</span> vocals</b>. Everything is 128–140 BPM with its key and Camelot code.</p>\n"
        "  <p class=\"lede how\">Play a kit, then audition anything over it: one-shots start on the next beat, loops on the next bar and drum loops follow the kit's tempo. <b>Fits</b> narrows every list to keys and tempos that mix with the kit; star sounds to collect them in your <b>Crate</b>.</p>")
h = rep(h, "      <button class=\"mode\" role=\"tab\" data-view=\"fx\"><span class=\"led\"></span>Fills &amp; FX <span class=\"count\" id=\"tFx\"></span></button>\n      <button class=\"mode\" role=\"tab\" data-view=\"hits\"><span class=\"led\"></span>Drum hits <span class=\"count\" id=\"tHits\"></span></button>",
        "      <button class=\"mode\" role=\"tab\" data-view=\"fx\"><span class=\"led\"></span>Loops &amp; FX <span class=\"count\" id=\"tFx\"></span></button>\n      <button class=\"mode\" role=\"tab\" data-view=\"hits\"><span class=\"led\"></span>Drum hits <span class=\"count\" id=\"tHits\"></span></button>\n"
        "      <button class=\"mode\" role=\"tab\" data-view=\"crate\"><span class=\"led\"></span>Crate <span class=\"count\" id=\"tCrate\"></span></button>")
h = rep(h, "    <div class=\"chips\" id=\"voxGenres\" style=\"margin-bottom:8px\"></div>", "    <div class=\"toolsbox\"></div>\n    <div class=\"chips\" id=\"voxGenres\" style=\"margin-bottom:8px\"></div>")
h = rep(h, "      <div><h2>Bass &amp; Keys</h2><p>Reese basses for speed garage, M1-style organ bass, house piano and strings. Chromatic sets play from the note buttons; each set saves as one zip.</p></div>",
        "      <div><h2>Bass &amp; Keys</h2><p>Reese, 2-step subs, rubber bass, donks, hoovers and bass loops from the new songs; organ bass and piano; garage organ and Rhodes, dub chords, brass, orchestra hits, choir and strings. Chromatic sets play from the note buttons and save as one zip.</p></div>")
h = rep(h, "    <div class=\"chips\" id=\"bkFilters\"></div>", "    <div class=\"toolsbox\"></div>\n    <div class=\"chips\" id=\"bkFilters\"></div>")
h = rep(h, "      <div><h2>Fills &amp; FX</h2><p>Bar-length drum fills, transitions and drum loops, cut to the grid at the tempo shown. Pitched FX are labelled with their key.</p></div>\n      <button class=\"btn save\" id=\"saveFx\"><svg viewBox=\"0 0 16 16\"><path d=\"M7 1h2v8.2l2.6-2.6 1.4 1.4L8 13 3 8l1.4-1.4L7 9.2z\"/><path d=\"M2 14h12v2H2z\"/></svg>Save all fills &amp; FX</button>",
        "      <div><h2>Loops &amp; FX</h2><p>Drum, top and percussion loops for all four genres, slice-ready breaks, bar-length fills, transitions, airhorns, risers and textures, cut to the grid at the tempo shown. Pitched FX are labelled with their key.</p></div>\n      <button class=\"btn save\" id=\"saveFx\"><svg viewBox=\"0 0 16 16\"><path d=\"M7 1h2v8.2l2.6-2.6 1.4 1.4L8 13 3 8l1.4-1.4L7 9.2z\"/><path d=\"M2 14h12v2H2z\"/></svg>Save all loops &amp; FX</button>")
h = rep(h, "    <div class=\"chips\" id=\"fxFilters\"></div>", "    <div class=\"toolsbox\"></div>\n    <div class=\"chips\" id=\"fxFilters\"></div>")
h = rep(h, "    <div class=\"pads\" id=\"pads\"></div>\n  </section>",
        "    <div class=\"pads\" id=\"pads\"></div>\n  </section>\n\n"
        "  <section class=\"view\" id=\"view-crate\" hidden>\n"
        "    <div class=\"viewhead\">\n"
        "      <div><h2>Crate</h2><p>Your starred sounds from every section, saved together as one zip with the README. The crate is kept in this browser.</p></div>\n"
        "      <div class=\"viewact\"><button class=\"btn save solid\" id=\"saveCrate\"><svg viewBox=\"0 0 16 16\"><path d=\"M7 1h2v8.2l2.6-2.6 1.4 1.4L8 13 3 8l1.4-1.4L7 9.2z\"/><path d=\"M2 14h12v2H2z\"/></svg>Save crate</button><button class=\"btn ghost\" id=\"clearCrate\">Clear</button></div>\n"
        "    </div>\n"
        "    <div id=\"crateRows\"></div>\n"
        "  </section>")
h = rep(h, "      <p>44.1 kHz / 16-bit WAV. Loops, fills and FX are cut to whole bars.",
        "      <p>44.1 kHz / 16-bit WAV. Loops, fills and FX are cut to whole bars and fold their tails back onto the start, so they repeat cleanly. The slice-ready break is one straight bar: cut it into 16 equal slices and it lines up with the Break Slices track in UKG07.")
css = """
/* ---------- search, fits, crate, loading (wave 3) ---------- */
.tools { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin: 0 0 10px; }
.search { flex: 1 1 260px; display: flex; align-items: center; gap: 8px; border: 1px solid var(--line); border-radius: 999px; background: var(--raised); padding: 0 12px; height: 38px; min-width: 0; }
.search svg { width: 15px; height: 15px; fill: var(--muted); flex: none; }
.search input { border: 0; background: transparent; outline: none; flex: 1; min-width: 0; font-size: 15px; height: 100%; }
.search:focus-within { border-color: var(--ink); }
.chip.fits { height: 38px; padding: 0 14px; }
.chip.fits[aria-pressed="true"] { background: var(--accent); border-color: var(--accent); color: var(--accent-ink); }
.star { width: 34px; height: 34px; border: 1px solid var(--line); border-radius: 50%; background: transparent; display: grid; place-items: center; color: var(--muted); flex: none; }
.star svg { width: 15px; height: 15px; fill: none; stroke: currentColor; stroke-width: 1.3; }
.star[aria-pressed="true"] { color: var(--accent); border-color: var(--accent); }
.star[aria-pressed="true"] svg { fill: currentColor; }
.rowact { gap: 6px; align-items: center; }
.rt .t .newtag { margin-left: 8px; vertical-align: 1px; }
.pbtn.load { background: var(--pad-hi); }
.pbtn.load svg { animation: spin .9s linear infinite; }
@keyframes spin { to { transform: rotate(360deg); } }
.viewact { display: flex; gap: 8px; flex-wrap: wrap; }
.empty { padding: 18px 0; display: flex; flex-wrap: wrap; gap: 10px; align-items: center; }
.fitsrow { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin-top: 14px; padding-top: 14px; border-top: 1px solid var(--line); }
.fitsrow .label { flex: 1 0 100%; }
.fitsrow b { color: var(--accent); margin-left: 2px; }
.lede.how { margin-top: 6px; font-size: 14px; }
@media (max-width: 900px) {
  .row { grid-template-columns: 40px minmax(0, 1fr) auto; padding: 8px 0; gap: 4px 12px; }
  .row .wave { grid-column: 2 / 3; grid-row: 2; height: 22px; }
  .row .rm { grid-column: 3; grid-row: 2; flex-direction: column; align-items: flex-end; gap: 0; font-size: 12px; text-align: right; }
  .row .rowact { grid-column: 3; grid-row: 1; }
  .rt .t.q { font-size: 15px; }
}
@media (max-width: 560px) {
  header.top { padding-block: 16px 10px; }
  .lede { font-size: 13.5px; }
  .lede.how { display: none; }
  .modes { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 5px; padding-block: 8px; }
  .mode { justify-content: center; padding: 7px 4px 6px; letter-spacing: .07em; font-size: 10.5px; gap: 5px; white-space: nowrap; min-width: 0; overflow: hidden; }
  .mode .led { display: none; }
  .mode[aria-selected="true"] { background: var(--ink); color: var(--chassis); }
  .mode[aria-selected="true"] .count { color: var(--chassis); opacity: .75; }
  .btn.save .st { display: none; }
  .rowact .btn.save { padding: 6px 8px; }
  .chip.fits { flex: 1 0 100%; justify-content: center; }
}
"""
h = rep(h, "@media (prefers-reduced-motion: reduce) { .lcd .blink { animation: none; } .toast { transition: none; } }",
        css.strip() + "\n@media (prefers-reduced-motion: reduce) { .lcd .blink { animation: none; } .toast { transition: none; } .pbtn.load svg { animation: none; } }")
open(H, 'w').write(h)
print('patched app.js and app.src.html')
