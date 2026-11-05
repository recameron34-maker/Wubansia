"""Export content/plugin/<kit>/<KIT>_mix.json: each round 5 loop's mix, for the engine (docs/plugin/manifest.md, 3).

    python3 tools/sound/plugin_mix.py --kit FUT01 [--check]     (or --all)

Each kit's builder reads the recipe's own constants (faders, pans, sends, EQ, ducks, the sends' settings, the club
master's chain) and the values mix_trace.run() measures or solves on the recipe's own render of its club loop, in both
variants (clip ceilings from the lanes' true peaks, solved faders, the level ducks' tops, the club master's glue
threshold and drive), and writes the manifest in the contract's canonical layout, integers only. mix_trace.py
classifies every DSP call the recipe makes against it; engine_ref.py renders the mix from it; test_plugin_mix.py and
test_engine_ref.py hold both to the recipe and to the committed loops.

--check rebuilds the manifest and compares it with the committed file (exit 1 when they differ). ride_points(kit)
gives the CC74/CC71 rides P1 writes into the MIDI for each cc_filter channel, on the same maps.
"""
import argparse
import json
import math
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads

import numpy as np  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SR = D.SR
KITS = ('FUT01', 'FUT02', 'FUT03', 'FUT04')
CONTENT_VERSION = 1
LANES = ('kick', 'snare', 'hats', 'perc', 'bass', 'lead', 'chords', 'vox', 'accents', 'fx')
TICKS = 15360                     # the 4-bar loop, 960 PPQ
CC_STEP = 60                      # ticks between a ride's points (P1's midi.CC_STEP)
SILENT_MDB = -120000              # a channel the reference loop doesn't play
MAX_CHANNELS = 24
MAX_PATH_LATENCY = 64             # samples of insert latency from a channel through its group or return to the bus EQ
INSERT_LATENCY = {'clip': 27, 'drive': 27, 'comp': 23}   # rt_ref.LATENCY: lane clips and drives, the glue
DC_HZ = 5                         # the live master's causal DC blocker (rt_ref.dc_block) in place of the 10 Hz clean


# ------------------------------------------------------------------ units and the canonical layout
def iround(x, scale=1.0):
    """round half up: floor(x * scale + 0.5), as an int (the contract's plugin_common.iround)."""
    return int(math.floor(float(x) * scale + 0.5))


def mdb(db):
    return iround(db, 1000)


def pm(x):
    return iround(x, 1000)


def us(s):
    return iround(s, 1e6)


def dumps(obj):
    """The contract's canonical layout: 2-space indent, a member per line, arrays of scalars on one line, empty
    containers {} and [], strings with only ", \\ and control characters escaped, a final newline."""
    def emit(v, ind):
        if isinstance(v, bool):
            return 'true' if v else 'false'
        if isinstance(v, int):
            return str(v)
        if isinstance(v, str):
            return json.dumps(v, ensure_ascii=False)
        if isinstance(v, dict):
            if not v:
                return '{}'
            pad = ' ' * (ind + 2)
            rows = [f'{pad}{json.dumps(k, ensure_ascii=False)}: {emit(x, ind + 2)}' for k, x in v.items()]
            return '{\n' + ',\n'.join(rows) + '\n' + ' ' * ind + '}'
        if isinstance(v, list):
            if not v:
                return '[]'
            if all(isinstance(x, (int, str, bool)) for x in v):
                return '[' + ', '.join(emit(x, 0) for x in v) + ']'
            pad = ' ' * (ind + 2)
            return '[\n' + ',\n'.join(pad + emit(x, ind + 2) for x in v) + '\n' + ' ' * ind + ']'
        raise TypeError(f'not a manifest value: {v!r} ({type(v).__name__})')
    return emit(obj, 0) + '\n'


# ------------------------------------------------------------------ inserts (docs/plugin/manifest.md 3.2)
def when(variant='any', ab='none', side='a'):
    return {'variant': variant, 'ab': ab, 'is': side}


def _ins(kind, fields, cond=None):
    out = {'type': kind, **fields}
    if cond is not None:
        out['when'] = cond
    return out


def gain(db, cond=None):
    return _ins('gain', {'gain_mdb': mdb(db)}, cond)


def pan(p, cond=None):
    return _ins('pan', {'pan_pm': pm(p)}, cond)


def width(w, freq_hz=0, order=1, cond=None):
    """The side above freq_hz (a Butterworth high-pass of that order on the side) times w; freq_hz 0: the whole side."""
    return _ins('width', {'width_pm': pm(w), 'freq_hz': iround(freq_hz), 'order': int(order)}, cond)


def peak(f, g, q, cond=None):
    return _ins('peak', {'freq_hz': iround(f), 'gain_mdb': mdb(g), 'q_pm': pm(q)}, cond)


def low_shelf(f, g, q, cond=None):
    return _ins('low_shelf', {'freq_hz': iround(f), 'gain_mdb': mdb(g), 'q_pm': pm(q)}, cond)


def high_shelf(f, g, q, cond=None):
    return _ins('high_shelf', {'freq_hz': iround(f), 'gain_mdb': mdb(g), 'q_pm': pm(q)}, cond)


def band(lo=0, hi=0):
    return {'lo_hz': iround(lo), 'hi_hz': iround(hi)}


def highpass(f, order, q=0.0, stages=1, bnd=(0, 0), cond=None):
    return _ins('highpass', {'freq_hz': iround(f), 'order': int(order), 'q_pm': pm(q), 'stages': int(stages),
                             'band': band(*bnd)}, cond)


def lowpass(f, order, q=0.0, stages=1, bnd=(0, 0), cond=None):
    return _ins('lowpass', {'freq_hz': iround(f), 'order': int(order), 'q_pm': pm(q), 'stages': int(stages),
                            'band': band(*bnd)}, cond)


def cc_filter(response, lo_hz, hi_hz, q_lo, q_hi, cc74_default=0, cc71_default=0, bnd=(0, 0), cond=None):
    return _ins('cc_filter', {'response': response, 'cc74_lo_hz': iround(lo_hz), 'cc74_hi_hz': iround(hi_hz),
                              'cc74_default': int(cc74_default), 'cc71_lo_pm': pm(q_lo), 'cc71_hi_pm': pm(q_hi),
                              'cc71_default': int(cc71_default), 'band': band(*bnd)}, cond)


def mono_below(f, order=8, cond=None):
    return _ins('mono_below', {'freq_hz': iround(f), 'order': int(order)}, cond)


def clip(ceiling_db, knee_db=1.5, cond=None):
    return _ins('clip', {'ceiling_mdb': mdb(ceiling_db), 'knee_mdb': mdb(knee_db)}, cond)


def drive(d, bias=0.0, output_db=0.0, cond=None):
    return _ins('drive', {'drive_pm': pm(d), 'bias_pm': pm(bias), 'output_mdb': mdb(output_db)}, cond)


def duck_onset(key_lanes, depth_db, ramp_s, hold_s, release_s, min_velocity=1, pre_s=0.0, bnd=(0, 0), pump=True,
               cond=None):
    return _ins('duck', {'mode': 'onset', 'key_lanes': list(key_lanes), 'min_velocity': int(min_velocity),
                         'depth_mdb': mdb(depth_db), 'ramp_us': us(ramp_s), 'hold_us': us(hold_s),
                         'release_us': us(release_s), 'pre_us': us(pre_s), 'band': band(*bnd), 'pump': bool(pump)},
                cond)


def duck_level(key_channel, depth_db, range_db, top_db, attack_s, release_s, bnd=(0, 0), pump=False, cond=None):
    return _ins('duck', {'mode': 'level', 'key_channel': key_channel, 'depth_mdb': mdb(depth_db),
                         'range_mdb': mdb(range_db), 'top_mdb': mdb(top_db), 'attack_us': us(attack_s),
                         'release_us': us(release_s), 'band': band(*bnd), 'pump': bool(pump)}, cond)


def comp(threshold_db, ratio, knee_db, attack_s, release_fast_s, release_slow_s, makeup_db=0.0, cond=None):
    return _ins('comp', {'threshold_mdb': mdb(threshold_db), 'ratio_pm': pm(ratio), 'knee_mdb': mdb(knee_db),
                         'attack_us': us(attack_s), 'release_fast_us': us(release_fast_s),
                         'release_slow_us': us(release_slow_s), 'makeup_mdb': mdb(makeup_db)}, cond)


def parallel(level_db, inserts, cond=None):
    return _ins('parallel', {'level_mdb': mdb(level_db), 'inserts': list(inserts)}, cond)


def pump_duck(kw, lanes=('kick',), min_velocity=1, bnd=(0, 0), pump=True):
    """An onset duck from a phrase.pump_db keyword dict (depth_db, ramp, hold, release)."""
    return duck_onset(lanes, kw['depth_db'], kw['ramp'], kw['hold'], kw['release'], min_velocity, 0.0, bnd, pump)


def peaks(bands, cond=None):
    return [peak(f, g, q, cond) for f, g, q in bands]


def clip_per(ceilings, knee_db, ab):
    """One clip whose ceiling depends on the A/B side ({'a': dB, 'b': dB}) or the variant ({'vox': dB, 'novox': dB}),
    as gains around a single clip at the first key's ceiling: the clip scales with its ceiling (master.soft_clip's
    curve and ADAA are homogeneous), so x -> g^-1 clip(g x) at C is the clip at C - 20 log10 g, exactly, and the path
    keeps one clip's latency. ab: the switch ('kick', 'bass', 'stab') or 'variant'."""
    keys = list(ceilings)
    ref = ceilings[keys[0]]
    out_pre, out_post = [], []
    for k in keys[1:]:
        d = ceilings[k] - ref
        if mdb(d) == 0:
            continue
        cond = when(variant=k) if ab == 'variant' else when(ab=ab, side=k)
        out_pre.append(gain(-d, cond))
        out_post.append(gain(d, cond))
    return out_pre + [clip(ref, knee_db)] + out_post


def channel(cid, lane, group, inserts, sends=(0.0, 0.0, 0.0)):
    if lane not in LANES:
        raise ValueError(f'channel {cid}: lane {lane!r} is not one of {LANES}')
    return {'id': cid, 'lane': lane, 'group': group, 'inserts': list(inserts), 'sends_pm': [pm(s) for s in sends]}


# ------------------------------------------------------------------ the returns and the master
def space_return(K, seed, settings, inserts, return_db=3.0):
    """sends.space's settings, with the delay jitter and the input signs drawn from the kit's seed as sends.space
    draws them (numpy's PCG64: the engine can't redraw them)."""
    from tools.sound import sends as S
    s = dict(settings)
    r = K.space_rng(seed)
    d = np.round(np.array(S.FDN_DELAYS) * s.get('size', 1.0) * (1 + 0.03 * r.uniform(-1, 1, 8))).astype(int)
    signs = r.choice([-1, 1], 8)
    return {'delays_frames': [int(v) for v in d], 'signs': [int(v) for v in signs], 't60_ms': iround(s['t60'], 1000),
            't60_low_ms': iround(s['t60_low'] or 0.0, 1000), 'low_hz': iround(s['low_hz']),
            'damp_hz': iround(s['damp_hz']), 'predelay_us': us(s['predelay']), 'send_hp_hz': iround(s['send_hp']),
            'send_order': int(s['send_order']), 'shimmer_pm': pm(s['shimmer']),
            'shimmer_lo_hz': iround(s['shimmer_hp']), 'shimmer_hi_hz': iround(s['shimmer_lp']),
            'window_frames': int(s['window']), 'mono_hz': iround(s['mono_hz']), 'return_mdb': mdb(return_db),
            'inserts': list(inserts)}


def echo_return(K, seed, settings, inserts, steps=3, return_db=3.0):
    s = dict(settings)
    p = K.space_rng(seed).uniform(0, 2 * np.pi, 2)
    return {'steps': int(steps), 'feedback_pm': pm(s['feedback']), 'lp_hz': iround(s['lp_hz']),
            'hp_hz': iround(s['hp_hz']), 'drive_pm': pm(s['drive']), 'wow_ppm': iround(s['wow'], 1e6),
            'wow_mhz': iround(s['wow_hz'], 1000), 'flutter_ppm': iround(s['flutter'], 1e6),
            'flutter_mhz': iround(s['flutter_hz'], 1000), 'phases_mrad': [iround(v, 1000) for v in p],
            'return_mdb': mdb(return_db), 'inserts': list(inserts)}


def resonator_return(freqs, inserts, tau_s=0.4, send_hp=600.0, return_db=-21.0):
    return {'tones_mhz': [iround(f, 1000) for f in sorted(freqs)], 'tau_ms': iround(tau_s, 1000),
            'send_hp_hz': iround(send_hp), 'return_mdb': mdb(return_db), 'inserts': list(inserts)}


def master_section(s, caps):
    """The club master (master.py's chain, ADR 0021 draft): the settings, and per variant the glue threshold and the
    drive the recipe's master solved for its club loop."""
    per = {v: {'glue_threshold_mdb': mdb(caps[v].values['glue_threshold_db']),
               'drive_mdb': mdb(caps[v].values['drive_db'])} for v in ('vox', 'novox')}
    return {'lowcut_hz': iround(s.lowcut_hz), 'lowcut_order': int(s.lowcut_order), 'mono_below_hz': iround(s.mono_hz),
            'glue': {'ratio_pm': pm(s.glue_ratio), 'knee_mdb': mdb(s.glue_knee_db), 'attack_us': us(s.glue_attack_s),
                     'release_fast_us': us(s.glue_release_s[0]), 'release_slow_us': us(s.glue_release_s[1])},
            'vox': per['vox'], 'novox': per['novox'],
            'clip': {'knee_mdb': mdb(s.clip_knee_db), 'ceiling_mdb': mdb(s.ceiling_dbtp + s.clip_db)},
            'limiter': {'ceiling_mdb': mdb(s.ceiling_dbtp), 'lookahead_us': us(s.limit_lookahead_s),
                        'hold_us': us(s.limit_hold_s), 'release_us': us(s.limit_release_s)},
            'dc_hz': DC_HZ}


# ------------------------------------------------------------------ rides (the cc_filters' CC74/CC71)
def cc_value(f, lo, hi):
    """The CC74 value for a cutoff on a map lo..hi Hz (log): round half up, clamped to 0-127."""
    return min(127, max(0, iround(127.0 * math.log(f / lo) / math.log(hi / lo))))


def dedupe(points):
    """A ride's (tick, value) points with the repeats left out (P1's midi.dedupe)."""
    out, last = [], None
    for t, v in points:
        if v != last:
            out.append((t, v))
            last = v
    return out


def ride_points(kit):
    """{channel: {74: [(tick, value)], 71: [...]}}: the cc_filter rides over the 4-bar loop (TICKS), a point every
    CC_STEP ticks where the value moves, from the recipe's own curve on this manifest's maps (the kit module's
    ride_points()). Ticks map onto the recipe's loop of L samples (tick * L / TICKS), unswung: P1 writes these."""
    return kit_module(kit).ride_points()


def curve_points(curve, L, value):
    """A curve over the loop's L samples as a ride: value(curve at tick t) every CC_STEP ticks, deduped."""
    pts = [(t, value(curve[min(L - 1, iround(t * L / TICKS))])) for t in range(0, TICKS, CC_STEP)]
    return dedupe(pts)


# ------------------------------------------------------------------ the score engine_ref plays the ducks from
def keys(kit, variant='vox'):
    """The duck keys of the loop in a variant, (lane, sample in the loop, velocity 1-127): the notes of the duck-key
    instruments as the MIDI has them, velocity round(v x 127), the layer's own (the kit module's keys())."""
    import inspect
    fn = kit_module(kit).keys
    return tuple(fn(variant) if inspect.signature(fn).parameters else fn())


def loop_info(kit):
    """(loop length in samples, BPM) of the kit's context loop."""
    return kit_module(kit).loop_info()


# ------------------------------------------------------------------ the kits
def kit_module(kit):
    """The kit's own mix module (<RECIPES>.plugin_mix): shared code reaches a kit only through its RECIPES (ADR
    0008)."""
    from tools.sound import mix_trace as T
    return T.kit_module(kit.upper())


def captures(kit):
    from tools.sound import mix_trace as T
    return {v: T.run(kit, v)[1] for v in ('vox', 'novox')}


def value(caps, key, variant='vox'):
    """A value the trace measured or solved in a variant's loop (KeyError naming it when it didn't)."""
    v = caps[variant].values
    if key not in v:
        raise KeyError(f'the trace measured no {key!r} in the {variant} loop')
    return v[key]


def build(kit, with_reference=True):
    """The kit's mix manifest (a dict in the contract's key order): the kit module's build() on the trace's captures,
    the club master's section, the reference."""
    kit = kit.upper()
    caps = captures(kit)
    chans, groups, returns, bus_eq, settings = kit_module(kit).build(caps)
    m = {'kit': kit, 'content_version': CONTENT_VERSION, 'channels': chans, 'groups': groups, 'returns': returns,
         'bus_eq': bus_eq, 'master': master_section(settings, caps),
         'reference': {var: {'pre_master_lufs_mlu': 0, 'club_lufs_mlu': 0,
                             'channel_true_peak_mdb': [SILENT_MDB] * len(chans)} for var in ('vox', 'novox')}}
    if with_reference:
        m['reference'] = reference(kit, m, caps)
    validate(m)
    return m


def reference(kit, m, caps):
    """What the reference loops measured, per variant: the club master's input and output loudness (the recipe's
    master report) and each channel's true peak, read on engine_ref's render of the loop (pass 2: the gates hold it to
    the committed loops), silence as SILENT_MDB."""
    from tools.sound import engine_ref as E
    out = {}
    for var in ('vox', 'novox'):
        _, meta = E.render(kit, var, mix=m, master='off', taps=('channels',))
        tps = []
        for c in m['channels']:
            x = meta['channels'].get(c['id'])
            tp = D.true_peak_db(x) if x is not None and np.any(x) else None
            tps.append(SILENT_MDB if tp is None or tp < -120.0 else mdb(tp))
        vals = caps[var].values
        out[var] = {'pre_master_lufs_mlu': mdb(vals['pre_master_lufs']), 'club_lufs_mlu': mdb(vals['club_lufs']),
                    'channel_true_peak_mdb': tps}
    return out


# ------------------------------------------------------------------ checks
def _walk(v, path='$'):
    yield path, v
    if isinstance(v, dict):
        for k, x in v.items():
            yield from _walk(x, f'{path}.{k}')
    elif isinstance(v, list):
        for i, x in enumerate(v):
            yield from _walk(x, f'{path}[{i}]')


def insert_latency(ins):
    """An insert's latency in samples at 48 kHz (rt_ref.LATENCY): clip and drive 27, comp 23, a parallel insert its
    chain's, everything else 0. Inactive (when-switched) inserts keep their latency."""
    if ins['type'] == 'parallel':
        return sum(insert_latency(i) for i in ins['inserts'])
    return INSERT_LATENCY.get(ins['type'], 0)


def path_latencies(m):
    """{path: samples}: each channel through its group (or straight) to the bus EQ, and each return's, in every
    variant's bus EQ."""
    out = {}
    eq = max(sum(insert_latency(i) for i in m['bus_eq'][v]) for v in ('vox', 'novox'))
    for c in m['channels']:
        lat = sum(insert_latency(i) for i in c['inserts'])
        grp = sum(insert_latency(i) for i in m['groups'][c['group']]['inserts']) if c['group'] != 'none' else 0
        out[c['id']] = lat + grp + eq
        for r, ret in m['returns'].items():
            if any(c['sends_pm']):
                out[f"{c['id']}>{r}"] = lat + sum(insert_latency(i) for i in ret['inserts']) + eq
    return out


def level_duck_cycles(m):
    """Level-duck cycles among the channels (A keyed on B, B on A, or longer), as lists of channel ids."""
    edges = {c['id']: {i['key_channel'] for i in c['inserts'] if i['type'] == 'duck' and i['mode'] == 'level'}
             for c in m['channels']}
    cycles, state = [], {}

    def visit(n, stack):
        state[n] = 1
        for k in sorted(edges.get(n, ())):
            if state.get(k) == 1:
                cycles.append(stack[stack.index(k):] + [k])
            elif not state.get(k):
                visit(k, stack + [k])
        state[n] = 2

    for n in sorted(edges):
        if not state.get(n):
            visit(n, [n])
    return cycles


def validate(m):
    """The contract's rules a writer must keep (ValueError on the first broken one): integers only, ids, the channel
    count, every level duck's key channel, onset ducks on one key lane sharing a pre_us, one reference value per
    channel, the latency budget per path (MAX_PATH_LATENCY) and no level-duck cycles; every string passes the
    denylists."""
    from tools.sound import denylist as DL
    import re
    bad = []
    for path, v in _walk(m):
        if isinstance(v, float):
            bad.append(f'{path}: a float ({v!r}): manifests are integers only')
    ids = [c['id'] for c in m['channels']]
    if len(ids) > MAX_CHANNELS:
        bad.append(f'{len(ids)} channels, over {MAX_CHANNELS}')
    if len(set(ids)) != len(ids):
        bad.append('channel ids repeat')
    for i in ids:
        if not re.fullmatch(r'[a-z0-9_]{1,32}', i):
            bad.append(f'channel id {i!r} is not [a-z0-9_]{{1,32}}')
    all_inserts = []
    for c in m['channels']:
        all_inserts += [(f"channels[{c['id']}]", c['id'], i) for i in c['inserts']]
    for g, grp in m['groups'].items():
        all_inserts += [(f'groups.{g}', None, i) for i in grp['inserts']]
    for r, ret in m['returns'].items():
        all_inserts += [(f'returns.{r}', None, i) for i in ret['inserts']]
    for var, lst in m['bus_eq'].items():
        all_inserts += [(f'bus_eq.{var}', None, i) for i in lst]
    pre = {}
    for where, own, ins in all_inserts:
        if ins['type'] != 'duck':
            continue
        if ins['mode'] == 'level':
            if ins['key_channel'] not in ids:
                bad.append(f"{where}: level duck keyed on {ins['key_channel']!r}, no such channel")
            if ins['key_channel'] == own:
                bad.append(f'{where}: a level duck keyed on its own channel')
        else:
            for lane in ins['key_lanes']:
                if pre.setdefault(lane, ins['pre_us']) != ins['pre_us']:
                    bad.append(f"{where}: onset ducks on the {lane} lane need one pre_us")
    for var, ref in m['reference'].items():
        if len(ref['channel_true_peak_mdb']) != len(ids):
            bad.append(f'reference.{var}: {len(ref["channel_true_peak_mdb"])} peaks for {len(ids)} channels')
    for p, lat in path_latencies(m).items():
        if lat > MAX_PATH_LATENCY:
            bad.append(f'{p}: {lat} samples of insert latency, over {MAX_PATH_LATENCY}')
    for cyc in level_duck_cycles(m):
        bad.append('a level-duck cycle: ' + ' -> '.join(cyc))
    lists = {n: DL.load(n) for n in ('artists', 'slurs')}
    for path, v in _walk(m):
        if isinstance(v, str):
            for n, digests in lists.items():
                if DL.hits(v, digests):
                    bad.append(f'{path}: {v!r} hits the {n} denylist')
    if bad:
        raise ValueError(f"{m.get('kit')} mix.json: " + '; '.join(bad))
    return m


# ------------------------------------------------------------------ files
def path(kit):
    return ROOT / 'content' / 'plugin' / kit.lower() / f'{kit.upper()}_mix.json'


def export(kit, check=False):
    """Write (or with check, compare) the kit's manifest; returns True when the file is (or was) up to date."""
    text = dumps(build(kit))
    p = path(kit)
    if check:
        return p.exists() and p.read_text(encoding='utf-8') == text
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding='utf-8')
    return True


def load(kit):
    return json.loads(path(kit).read_text(encoding='utf-8'))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--kit', choices=KITS)
    g.add_argument('--all', action='store_true')
    ap.add_argument('--check', action='store_true', help='compare with the committed manifest instead of writing it')
    a = ap.parse_args(argv)
    ok = True
    for kit in (KITS if a.all else (a.kit,)):
        same = export(kit, check=a.check)
        if a.check:
            print(f'{kit}: {"up to date" if same else "differs from a fresh export"} ({path(kit)})')
        else:
            print(f'{kit}: wrote {path(kit)}')
        ok = ok and same
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
