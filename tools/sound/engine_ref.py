"""The streaming reference of the engine's mix: a kit's loop rendered causally in numpy from the recipe's own dry lanes
and the mix manifest alone (content/plugin/<kit>/<KIT>_mix.json), as the C++ mixer (P8) plays it.

    audio, meta = render('FUT01', 'vox', bass='b', passes=3, master='exact')     # or 'lookback', 'off'

render() takes each channel's dry lane from mix_trace.run() (what the voices play into it over the loop, tails
wrapped), the duck keys and the cc_filter rides from plugin_mix (keys(), ride_points()), and plays `passes` passes of
the loop from silence through render_mix(): each channel's inserts, its lane level, its post-fader sends (and the
thrown voices' direct sends), the groups, the returns, the variant's bus EQ and the club master. Every stage runs on the
stream from its start, causally with a fixed latency (rt_ref): the output is in aligned time, so pass 2 (samples
L..2L) compares with the recipe's loop sample for sample; the engine's output is the same delayed by meta['latency'].

master='exact' plays master.py's chain at the manifest's fixed glue threshold and drive, its two-sided limiter hold and
its zero-phase DC clean (groovecore render's verification mode); master='lookback' plays the real-time master (the
hold looking back, the causal DC blocker); 'off' stops at the bus EQ (meta['premaster'] always holds that point).

render_mix() is the engine's whole mix on any manifest and any dry lanes (the synthetic kit's too): P8's whole-chain
test renders through it, and gates() holds render() to the committed Wet and Club loops (LUFS-I, short-term
loudness, 1/3-octave bands from 40 Hz to 16 kHz, true peak, kick crest).
"""
import math
import os
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads

import numpy as np  # noqa: E402
from scipy import signal  # noqa: E402
from scipy.linalg import hadamard  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402
from tools.sound import phrase as Ph  # noqa: E402
from tools.sound import rt_ref as R  # noqa: E402
from tools.sound import sends as S  # noqa: E402

SR = D.SR
LANES = ('kick', 'snare', 'hats', 'perc', 'bass', 'lead', 'chords', 'vox', 'accents', 'fx')
TICKS = 15360
EXACT_DC_HZ = 10.0               # master.py's zero-phase DC clean (the exact master); the live one is mix.json's dc_hz


# ------------------------------------------------------------------ helpers
def undb(mdb_value):
    return float(D.undb(mdb_value / 1000.0))


def active(ins, variant, ab):
    """Whether a `when` holds: the variant (any, vox, novox) and the A/B side ({'kick': 'a', ...})."""
    w = ins.get('when')
    if not w:
        return True
    if w['variant'] != 'any' and w['variant'] != variant:
        return False
    return w['ab'] == 'none' or ab.get(w['ab'], 'a') == w['is']


def tile(x, passes):
    return np.tile(D.st(np.asarray(x, dtype=float)), (passes, 1))


PRE = SR            # samples of silence every render starts with (dropped): from rest, as the engine starts; a
                    # multiple of the glue's 16- and the level duck's 64-sample blocks, so their grids stay on the
                    # first real sample
POST = 512          # and ends with: what the centred 4x stages ring after the last sample


def rest_pad(x):
    """A stream with PRE samples of silence in front and POST behind (render_mix renders from rest, P8)."""
    x = D.st(np.asarray(x, dtype=float))
    return np.concatenate([np.zeros((PRE, 2)), x, np.zeros((POST, 2))])


def stream(x, passes, loop_len):
    """A dry lane as the stream: one loop tiled `passes` times, or a whole stream (passes x loop_len) as it is."""
    x = D.st(np.asarray(x, dtype=float))
    if len(x) == passes * loop_len and passes > 1:
        return x
    if len(x) != loop_len:
        raise ValueError(f'a dry lane of {len(x)} samples: one loop is {loop_len}')
    return np.tile(x, (passes, 1))


# ------------------------------------------------------------------ the cc_filter
def cc_stream(points, default, n, loop_len, passes, bpm):
    """A CC lane per sample over the stream (float 0-127), as the engine plays a sequenced CC lane: each point holds
    its value and ramps linearly to the next when that comes within a 16th; the default until the first. points:
    (tick in the loop, value), the loop's ticks mapped onto loop_len samples."""
    spt = loop_len / TICKS
    pos, val = [], []
    for p in range(passes):
        for t, v in points:
            pos.append(p * loop_len + int(math.floor(t * spt + 0.5)))
            val.append(float(v))
    out = np.full(n, float(default))
    s16 = 240 * spt
    for i, (a, v) in enumerate(zip(pos, val)):
        b = pos[i + 1] if i + 1 < len(pos) else n
        if a >= n:
            break
        b = min(b, n)
        if i + 1 < len(pos) and pos[i + 1] - a <= s16 + 0.5:
            out[a:b] = v + (val[i + 1] - v) * (np.arange(a, b) - a) / (pos[i + 1] - a)
        else:
            out[a:b] = v
    return out


def svf(x, fc, q, response):
    """dsp48.svf_tpt with the cutoff and Q per sample, mono; the response's output."""
    n = len(x)
    g = np.tan(np.pi * np.clip(fc, 10, SR * 0.49) / SR).tolist()
    k = (1.0 / np.asarray(q, dtype=float)).tolist()
    xs = x.tolist()
    out = [0.0] * n
    s1 = s2 = 0.0
    which = {'lowpass': 0, 'bandpass': 1, 'highpass': 2}[response]
    for i in range(n):
        gi, ki = g[i], k[i]
        h = (xs[i] - (ki + gi) * s1 - s2) / (1.0 + gi * (gi + ki))
        v1 = gi * h
        b = v1 + s1
        s1 = b + v1
        v2 = gi * b
        lo = v2 + s2
        s2 = lo + v2
        out[i] = lo if which == 0 else (b if which == 1 else h)
    return np.array(out)


# ------------------------------------------------------------------ inserts
class Ctx:
    """What the inserts read besides their audio: the stream's length, the variant and A/B sides, the duck keys, the
    channels' key taps (level ducks), this channel's CC lanes, the macros and the tempo."""

    def __init__(self, n, variant, ab, keys, taps, cc, pump, bpm, loop_len, passes):
        self.n, self.variant, self.ab, self.keys, self.taps, self.cc = n, variant, ab, keys, taps, cc
        self.pump, self.bpm, self.loop_len, self.passes = pump, bpm, loop_len, passes


def rbj(kind, f, gain_db, q):
    """RBJ cookbook biquad coefficients (b, a), normalised: peak, low_shelf, high_shelf, highpass, lowpass."""
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f / SR
    cw, al = np.cos(w0), np.sin(w0) / (2 * q)
    if kind == 'peak':
        b, a = [1 + al * A, -2 * cw, 1 - al * A], [1 + al / A, -2 * cw, 1 - al / A]
    elif kind in ('low_shelf', 'high_shelf'):
        sq = 2 * np.sqrt(A) * al
        s = 1 if kind == 'low_shelf' else -1
        b = [A * ((A + 1) - s * (A - 1) * cw + sq), s * 2 * A * ((A - 1) - s * (A + 1) * cw),
             A * ((A + 1) - s * (A - 1) * cw - sq)]
        a = [(A + 1) + s * (A - 1) * cw + sq, -s * 2 * ((A - 1) + s * (A + 1) * cw), (A + 1) + s * (A - 1) * cw - sq]
    elif kind == 'lowpass':
        b, a = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2], [1 + al, -2 * cw, 1 - al]
    elif kind == 'highpass':
        b, a = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2], [1 + al, -2 * cw, 1 - al]
    else:
        raise ValueError(kind)
    b, a = np.array(b), np.array(a)
    return b / a[0], a / a[0]


def run_inserts(inserts, x, ctx, own=None):
    """A chain of inserts on a stream (n, 2), in order, the inactive ones bypassed."""
    for ins in inserts:
        if active(ins, ctx.variant, ctx.ab):
            x = run_insert(ins, x, ctx, own)
    return x


def run_insert(ins, x, ctx, own=None):
    t = ins['type']
    if t == 'gain':
        return x * undb(ins['gain_mdb'])
    if t == 'pan':
        return Ph.pan(x, ins['pan_pm'] / 1000.0)
    if t == 'width':
        m, s = 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1])
        w = ins['width_pm'] / 1000.0
        s = s * w if not ins['freq_hz'] else s + (w - 1.0) * D.bfilt(s, ins['freq_hz'], 'highpass',
                                                                        order=ins['order'])
        return np.stack([m + s, m - s], 1)
    if t in ('peak', 'low_shelf', 'high_shelf'):
        b, a = rbj(t, ins['freq_hz'], ins['gain_mdb'] / 1000.0, ins['q_pm'] / 1000.0)
        return signal.lfilter(b, a, x, axis=0)
    if t in ('highpass', 'lowpass'):
        def f(u):
            for _ in range(ins['stages']):
                if ins['q_pm']:
                    b, a = rbj(t, ins['freq_hz'], 0.0, ins['q_pm'] / 1000.0)
                    u = signal.lfilter(b, a, u, axis=0)
                else:
                    u = D.bfilt(u, ins['freq_hz'], t, order=ins['order'])
            return u
        return R.in_band(f, x, ins['band']['lo_hz'], ins['band']['hi_hz'])
    if t == 'cc_filter':
        lo, hi = ins['cc74_lo_hz'], ins['cc74_hi_hz']
        v74 = ctx.cc.get(74) if ctx.cc.get(74) is not None else np.full(ctx.n, float(ins['cc74_default']))
        v71 = ctx.cc.get(71) if ctx.cc.get(71) is not None else np.full(ctx.n, float(ins['cc71_default']))
        fc = lo * (hi / lo) ** (v74 / 127.0)
        q = (ins['cc71_lo_pm'] + (ins['cc71_hi_pm'] - ins['cc71_lo_pm']) * v71 / 127.0) / 1000.0
        f = lambda u: np.stack([svf(u[:, c], fc, q, ins['response']) for c in range(2)], 1)  # noqa: E731
        return R.in_band(f, x, ins['band']['lo_hz'], ins['band']['hi_hz'])
    if t == 'mono_below':
        return D.mono_below(x, ins['freq_hz'], order=ins['order'])
    if t == 'clip':
        return R.lane_soft_clip(x, ins['ceiling_mdb'] / 1000.0, ins['knee_mdb'] / 1000.0)
    if t == 'drive':
        return R.lane_drive(x, ins['drive_pm'] / 1000.0, ins['bias_pm'] / 1000.0, ins['output_mdb'] / 1000.0)
    if t == 'duck':
        scale = ctx.pump if ins['pump'] else 1.0
        if ins['mode'] == 'onset':
            pre = int(math.floor(ins['pre_us'] * SR / 1e6 + 0.5))
            onsets = [p - pre for lane, p, v in ctx.keys if lane in ins['key_lanes'] and v >= ins['min_velocity']]
            g = R.onset_db(onsets, ctx.n, scale * ins['depth_mdb'] / 1000.0, ins['ramp_us'] / 1e6,
                           ins['hold_us'] / 1e6, ins['release_us'] / 1e6)
        else:
            key = ctx.taps[ins['key_channel']]
            g = R.level_duck_db(key, scale * ins['depth_mdb'] / 1000.0, ins['range_mdb'] / 1000.0,
                                ins['top_mdb'] / 1000.0, ins['attack_us'] / 1e6, ins['release_us'] / 1e6)
        return R.band_duck(x, g, ins['band']['lo_hz'], ins['band']['hi_hz'])
    if t == 'comp':
        return R.glue(x, ins['threshold_mdb'] / 1000.0, ins['ratio_pm'] / 1000.0, ins['knee_mdb'] / 1000.0,
                      ins['attack_us'] / 1e6, (ins['release_fast_us'] / 1e6, ins['release_slow_us'] / 1e6),
                      ins['makeup_mdb'] / 1000.0)
    if t == 'parallel':
        return x + undb(ins['level_mdb']) * run_inserts(ins['inserts'], x, ctx, own)
    raise ValueError(f'unknown insert type {t!r}')


def latency(inserts):
    """An insert chain's latency in samples (rt_ref.LATENCY): the engine delays the shorter paths to match."""
    out = 0
    for ins in inserts:
        t = ins['type']
        if t == 'parallel':
            out += latency(ins['inserts'])
        elif t == 'clip':
            out += R.LATENCY['lane_clip']
        elif t == 'drive':
            out += R.LATENCY['lane_drive']
        elif t == 'comp':
            out += R.LATENCY['glue']
    return out


# ------------------------------------------------------------------ the returns (sends.py, from the manifest)
def space(x, cfg, n_tail=0, t0=0):
    """sends.space with the manifest's delays and input signs (the PCG64 draws) in place of the seed. t0: the
    absolute time of x's first sample, in samples (the shimmer's taps read absolute time)."""
    s = cfg
    x = D.bfilt(D.mono(x), s['send_hp_hz'], 'highpass', order=s['send_order']) if s['send_hp_hz'] else D.mono(x)
    pd = int(round(s['predelay_us'] * SR / 1e6))
    n = len(x) + n_tail
    xin = np.zeros(n)
    xin[pd:min(n, pd + len(x))] = x[:max(0, n - pd)]
    d = np.array(s['delays_frames'], dtype=int)
    inj = np.array(s['signs'], dtype=float) / np.sqrt(8)
    Hm = hadamard(8) / np.sqrt(8)
    t60, t60_low = s['t60_ms'] / 1000.0, (s['t60_low_ms'] / 1000.0) or None
    g = 10 ** (-3 * d / (t60 * SR))
    g_low = 10 ** (-3 * d / (t60_low * SR)) if t60_low else g
    a = np.exp(-2 * np.pi * s['damp_hz'] / SR)
    a_low = np.exp(-2 * np.pi * s['low_hz'] / SR)
    z_low = np.zeros((8, 1))
    dmax, B = int(d.max()), int(d.min())
    lines = np.zeros((8, n + dmax))
    z_damp = np.zeros((8, 1))
    shimmer = s['shimmer_pm'] / 1000.0
    window = s['window_frames']
    sos = signal.butter(4, (s['shimmer_lo_hz'], s['shimmer_hi_hz']), 'bandpass', fs=SR, output='sos')
    z_lp = np.zeros((sos.shape[0], 8, 2))
    hist = np.zeros((8, n + window))
    for st in range(0, n, B):
        e = min(st + B, n)
        taps = np.stack([lines[i, dmax + st - d[i]:dmax + e - d[i]] for i in range(8)])
        taps, z_damp = signal.lfilter([1 - a], [1, -a], taps, axis=1, zi=z_damp)
        if t60_low:
            lo, z_low = signal.lfilter([1 - a_low], [1, -a_low], taps, axis=1, zi=z_low)
            taps = taps + (g_low / g - 1)[:, None] * lo
        fb = taps
        if shimmer:
            h, z_lp = signal.sosfilt(sos, taps, axis=1, zi=z_lp)
            hist[:, window + st:window + e] = h
            t = np.arange(st, e)
            up = sum(w * hist[:, window + t - dk] for dk, w in S.octave_taps(t + t0, window))
            fb = np.sqrt(1 - shimmer ** 2) * taps + shimmer * up
        lines[:, dmax + st:dmax + e] = Hm @ (fb * g[:, None]) + inj[:, None] * xin[st:e][None, :]
    out = lines[:, dmax:]
    y = np.stack([S.OUT_L @ out, S.OUT_R @ out], 1) / np.sqrt(8)
    return D.mono_below(y, s['mono_hz']) if s['mono_hz'] else y


def echo(x, cfg, bpm, t0=0):
    """sends.echo at the manifest's wow and flutter phases; the delay is `steps` 16ths at bpm. t0: the absolute time
    of x's first sample, in samples (the wow and flutter run on absolute time)."""
    s = cfg
    x = D.mono(x)
    n = len(x)
    t = (np.arange(n) + t0) / SR
    p1, p2 = s['phases_mrad'][0] / 1000.0, s['phases_mrad'][1] / 1000.0
    wobble = (s['wow_ppm'] / 1e6) * np.sin(2 * np.pi * (s['wow_mhz'] / 1000.0) * t + p1) \
        + (s['flutter_ppm'] / 1e6) * np.sin(2 * np.pi * (s['flutter_mhz'] / 1000.0) * t + p2)
    delay_s = s['steps'] * 60.0 / bpm / 4
    dly = delay_s * SR * (1 + wobble)
    B = int(np.floor(dly.min())) - 1
    sos = np.vstack([signal.butter(2, s['lp_hz'], 'lowpass', fs=SR, output='sos'),
                     signal.butter(2, s['hp_hz'], 'highpass', fs=SR, output='sos')])
    za, zb = np.zeros((2, 2)), np.zeros((2, 2))
    a, b, out = np.zeros(n + 1), np.zeros(n + 1), np.zeros((n, 2))
    drive_amount = s['drive_pm'] / 1000.0
    feedback = s['feedback_pm'] / 1000.0
    sat = lambda u: np.tanh(drive_amount * u) / drive_amount  # noqa: E731
    for st in range(0, n, B):
        e = min(st + B, n)
        pos = np.arange(st, e) - dly[st:e]
        ta, za = signal.sosfilt(sos, sat(S._read(a, pos)), zi=za)
        tb, zb = signal.sosfilt(sos, sat(S._read(b, pos)), zi=zb)
        out[st:e, 0], out[st:e, 1] = ta, tb
        b[st:e] = feedback * ta
        a[st:e] = x[st:e] + feedback * tb
    return out


def resonator(x, cfg):
    if not cfg['tones_mhz']:
        return np.zeros((len(x), 2))
    return S.resonator(x, [f / 1000.0 for f in cfg['tones_mhz']], tau=cfg['tau_ms'] / 1000.0,
                       send_hp=cfg['send_hp_hz'])


# ------------------------------------------------------------------ the club master
def master_chain(x, mcfg, variant, mode):
    """The club master on the bus (aligned): 'exact' is master.py's chain at the fixed values (the two-sided hold, the
    zero-phase DC clean before and after the limiter, no trim); 'lookback' the real-time one (the hold looking back,
    the causal DC blocker before the limiter). Returns (audio, latency in samples)."""
    v = mcfg[variant]
    g = mcfg['glue']
    y = D.bfilt(D.st(x), mcfg['lowcut_hz'], 'highpass', order=mcfg['lowcut_order'])
    if mcfg['mono_below_hz']:
        y = D.mono_below(y, mcfg['mono_below_hz'])
    y = R.glue(y, v['glue_threshold_mdb'] / 1000.0, g['ratio_pm'] / 1000.0, g['knee_mdb'] / 1000.0,
               g['attack_us'] / 1e6, (g['release_fast_us'] / 1e6, g['release_slow_us'] / 1e6))
    y = y * undb(v['drive_mdb'])
    c, lim = mcfg['clip'], mcfg['limiter']
    y = R.soft_clip(y, c['ceiling_mdb'] / 1000.0, c['knee_mdb'] / 1000.0)
    args = (lim['ceiling_mdb'] / 1000.0, lim['lookahead_us'] / 1e6, lim['hold_us'] / 1e6, lim['release_us'] / 1e6)
    if mode == 'exact':
        y = D.bfilt(y, EXACT_DC_HZ, 'highpass', order=4, zero_phase=True)
        y = R.limit(y, *args, hold='two_sided')
        y = D.bfilt(y, EXACT_DC_HZ, 'highpass', order=4, zero_phase=True)
        hold = 'two_sided'
    elif mode == 'lookback':
        y = R.dc_block(y, mcfg['dc_hz'])
        y = R.limit(y, *args, hold='lookback')
        hold = 'lookback'
    else:
        raise ValueError(f"master is 'exact', 'lookback' or 'off', not {mode!r}")
    lat = R.LATENCY['glue'] + R.LATENCY['soft_clip'] + R.limit_latency(args[1], args[2], hold)
    return y, lat


# ------------------------------------------------------------------ the mix
def _order(channels):
    """Channels in an order where every level duck's key channel comes first."""
    ids = [c['id'] for c in channels]
    deps = {c['id']: {i['key_channel'] for i in _all_inserts(c['inserts'])
                      if i['type'] == 'duck' and i['mode'] == 'level'} for c in channels}
    out, done = [], set()
    while len(out) < len(ids):
        progressed = False
        for c in channels:
            if c['id'] not in done and deps[c['id']] <= done:
                out.append(c)
                done.add(c['id'])
                progressed = True
        if not progressed:
            raise ValueError('a level-duck cycle among the channels')
    return out


def _all_inserts(inserts):
    for i in inserts:
        yield i
        if i['type'] == 'parallel':
            yield from _all_inserts(i['inserts'])


def static_gain(inserts, variant, ab):
    """The product of a channel's active gain inserts: what a thrown voice's direct send passes (approved option a)."""
    g = 1.0
    for ins in inserts:
        if ins['type'] == 'gain' and active(ins, variant, ab):
            g *= undb(ins['gain_mdb'])
    return g


def render_mix(mix, dry, *, variant, loop_len, bpm, keys=(), ccs=None, throws=None, ab=None, passes=3,
               master='exact', lane_db=None, mutes=(), macros=None, taps=()):
    """The engine's mix of one loop played `passes` times from silence (aligned time).

    mix: a mix manifest (the parsed JSON). dry: {channel id: (loop_len,) or (loop_len, 2)}, what the voices play into
    each channel over one loop (tails wrapped), or a whole stream of passes x loop_len samples. keys: (lane, sample in
    the loop, velocity) duck keys. ccs: {channel id: {74: [(tick, value)], 71: [...]}} the cc_filter rides over the
    loop. throws: {channel id: [(part, cc91, cc94)]} thrown voices among a channel's dry lane. ab: {'kick': 'a'|'b',
    'bass': ..., 'stab': ...}. lane_db: {lane: dB} lane levels; mutes: lanes muted. macros: {'space', 'echo', 'pump'}:
    0-2 (1 = 100 %). taps: extra meta to keep ('channels', 'returns', 'groups').

    Every stream starts from rest: PRE samples of silence in front, POST behind, dropped from what comes back (the
    centred 4x stages ring before the first sample, the limiter holds for what comes, as the engine does); the echo's
    wow and the shimmer's taps count time from the first real sample.

    Returns (audio (passes * loop_len, 2), meta): meta['premaster'] (the bus after its EQ), meta['latency'] ({'mix':
    the longest insert path, 'master': the master's, 'total'}), and the taps asked for."""
    ab = {'kick': 'b', 'bass': 'b', 'stab': 'a', **(ab or {})}
    macros = {'space': 1.0, 'echo': 1.0, 'pump': 1.0, **(macros or {})}
    lane_db = lane_db or {}
    real = passes * loop_len
    n = PRE + real + POST
    stream_keys = tuple((lane, PRE + p * loop_len + pos, vel) for p in range(passes) for lane, pos, vel in keys)
    ccs = ccs or {}
    throws = throws or {}
    keytap, outs = {}, {}
    sends_in = {k: np.zeros(n) for k in ('space', 'echo', 'resonator')}
    groups = {'drums': np.zeros((n, 2)), 'music': np.zeros((n, 2)), 'none': np.zeros((n, 2))}
    path_lat = {}
    for c in _order(mix['channels']):
        cid = c['id']
        x = rest_pad(stream(dry[cid], passes, loop_len)) if cid in dry else np.zeros((n, 2))
        cc = {}
        for ins in c['inserts']:
            if ins['type'] == 'cc_filter':
                pts = ccs.get(cid, {})
                cc = {}
                for num in (74, 71):
                    default = ins['cc74_default'] if num == 74 else ins['cc71_default']
                    if pts.get(num):
                        cc[num] = np.concatenate([np.full(PRE, float(default)),
                                                  cc_stream(pts[num], default, real, loop_len, passes, bpm),
                                                  np.full(POST, float(default))])
                    else:
                        cc[num] = None
        ctx = Ctx(n, variant, ab, stream_keys, keytap, cc, macros['pump'], bpm, loop_len, passes)
        y = run_inserts(c['inserts'], x, ctx, cid) if np.any(x) else x
        keytap[cid] = y
        lvl = 0.0 if c['lane'] in mutes else float(D.undb(lane_db.get(c['lane'], 0.0)))
        y = y * lvl
        outs[cid] = y
        groups[c['group']] += y
        for j, s in enumerate(('space', 'echo', 'resonator')):
            if c['sends_pm'][j]:
                sends_in[s] += D.mono(y) * (c['sends_pm'][j] / 1000.0)
        for part, cc91, cc94 in throws.get(cid, ()):
            g = static_gain(c['inserts'], variant, ab) * lvl
            p = D.mono(rest_pad(stream(part, passes, loop_len)))
            for j, (s, v) in enumerate((('space', cc91), ('echo', cc94))):
                base = c['sends_pm'][j] / 1000.0
                sends_in[s] += p * g * (1.0 - base) * v / 127.0
        path_lat[cid] = latency([i for i in c['inserts']])
    ctx = Ctx(n, variant, ab, stream_keys, keytap, {}, macros['pump'], bpm, loop_len, passes)
    bus = groups['none'].copy()
    gtaps = {}
    for g in ('drums', 'music'):
        y = run_inserts(mix['groups'][g]['inserts'], groups[g], ctx) if np.any(groups[g]) else groups[g]
        gtaps[g] = y
        bus += y
    rets = {}
    for r, fn, macro in (('space', lambda u, cfg: space(u, cfg, t0=-PRE), 'space'),
                         ('echo', lambda u, cfg: echo(u, cfg, bpm, t0=-PRE), 'echo'),
                         ('resonator', lambda u, cfg: resonator(u, cfg), 'space')):
        cfg = mix['returns'][r]
        if not np.any(sends_in[r]):
            continue
        y = fn(sends_in[r], cfg)
        y = run_inserts(cfg['inserts'], y, ctx)
        y = y * undb(cfg['return_mdb']) * macros[macro]
        rets[r] = y
        bus += y
    bus = run_inserts(mix['bus_eq'][variant], bus, ctx)
    mix_lat = max([path_lat[c] + latency(mix['groups'][ch['group']]['inserts'] if ch['group'] != 'none' else [])
                   for ch in mix['channels'] for c in (ch['id'],)] + [0]) + latency(mix['bus_eq'][variant])
    crop = lambda y: y[PRE:PRE + real]  # noqa: E731
    meta = {'premaster': crop(bus)}
    if master == 'off':
        out, mlat = bus, 0
    else:
        out, mlat = master_chain(bus, mix['master'], variant, master)
    meta['latency'] = {'mix': int(mix_lat), 'master': int(mlat), 'total': int(mix_lat + mlat)}
    if 'channels' in taps:
        meta['channels'] = {k: crop(v)[loop_len:2 * loop_len] if passes >= 2 else crop(v) for k, v in outs.items()}
    if 'returns' in taps:
        meta['returns'] = {k: crop(v) for k, v in rets.items()}
    if 'groups' in taps:
        meta['groups'] = {k: crop(v) for k, v in gtaps.items()}
    return crop(out), meta


# ------------------------------------------------------------------ a kit's loop
def render(kit, variant, bass='b', passes=3, master='exact', mix=None, taps=(), **kw):
    """The kit's loop for a variant ('vox' or 'novox') from the recipe's dry lanes (mix_trace.run) and its mix
    manifest (or `mix`): (audio, meta), aligned. bass 'a' plays the recipe's A-side dry lane where the trace has one
    (FUT01, FUT04)."""
    from tools.sound import mix_trace as T
    from tools.sound import plugin_mix as PM
    kit = kit.upper()
    m = mix if mix is not None else PM.load(kit)
    _, cap = T.run(kit, variant)
    dry = dict(cap.dry)
    if bass == 'a':
        for (ch, side), x in cap.sides.items():
            if side == 'a' and ch in ('bass', 'bass_stem'):
                dry[ch] = x
        if not any(side == 'a' and ch in ('bass', 'bass_stem') for ch, side in cap.sides):
            raise ValueError(f'{kit}: the trace holds no bass A dry lane')
    L, bpm = PM.loop_info(kit)
    out, meta = render_mix(m, dry, variant=variant, loop_len=L, bpm=bpm, keys=PM.keys(kit, variant),
                           ccs=PM.ride_points(kit), throws=cap.throws, ab={'bass': bass}, passes=passes,
                           master=master, taps=taps, **kw)
    meta['loop_len'] = L
    return out, meta


# ------------------------------------------------------------------ the gates
GATES = {'lufs_i': 0.3, 'lufs_s': 0.5, 'bands': 0.75, 'true_peak': 0.3, 'kick_crest': 0.5}
BAND_LO, BAND_HI = 40.0, 16000.0
def r5_dir(kit):
    """The kit's round 5 render folder (build/<kit>-r5: the FLAC masters and qa_report.json): under
    $WUBANSIA_R5_BUILD, else the repo's build/; None when absent."""
    for base in (os.environ.get('WUBANSIA_R5_BUILD'), str(Path(__file__).resolve().parents[2] / 'build')):
        if base and (Path(base) / f'{kit.lower()}-r5' / 'qa_report.json').exists():
            return Path(base) / f'{kit.lower()}-r5'
    return None


def reference(kit, item):
    """(audio, item report) of a committed round 5 loop: its FLAC master and its qa_report entry."""
    import json
    import soundfile as sf
    d = r5_dir(kit)
    rep = {i['name']: i for i in json.loads((d / 'qa_report.json').read_text())['items']}[item]
    y, sr = sf.read(str(d / rep['files'][0]['file']), dtype='float64')
    if sr != SR:
        raise ValueError(f'{item}: {sr} Hz')
    return D.st(y), rep


def third_octave_db(x, lo=BAND_LO, hi=BAND_HI):
    """Each base-2 third octave's power (L and R summed), dB, from one FFT over the loop: {centre: dB}."""
    from tools.sound import mixmeter as MM
    f, X = MM._spectrum(x, True)
    P = (np.abs(X) ** 2).sum(1)
    return {round(fc, 1): float(10 * np.log10(P[(f >= fc / 2 ** (1 / 6)) & (f < fc * 2 ** (1 / 6))].sum() + 1e-30))
            for fc in MM.third_octaves(lo, hi)}


def measure(x, kicks):
    from tools.sound import mixmeter as MM
    return {'lufs_i': MM.lufs_i(x), 'lufs_s': MM.lufs_s(x), 'bands': third_octave_db(x),
            'true_peak': MM.true_peak_db(x), 'kick_crest': MM.hit_crest_db(x, kicks)}


def compare(got, want, kicks):
    """The gate readings of a render against a reference loop (both one loop, aligned): the worst difference per
    measure, and pass/fail."""
    a, b = measure(got, kicks), measure(want, kicks)
    diff = {'lufs_i': a['lufs_i'] - b['lufs_i'],
            'lufs_s': float(np.max(np.abs(a['lufs_s'] - b['lufs_s']))),
            'bands': max(((fc, a['bands'][fc] - b['bands'][fc]) for fc in a['bands']), key=lambda t: abs(t[1])),
            'true_peak': a['true_peak'] - b['true_peak'],
            'kick_crest': float(max(np.abs(np.array(a['kick_crest']) - np.array(b['kick_crest']))))}
    ok = {'lufs_i': abs(diff['lufs_i']) <= GATES['lufs_i'], 'lufs_s': diff['lufs_s'] <= GATES['lufs_s'],
          'bands': abs(diff['bands'][1]) <= GATES['bands'], 'true_peak': abs(diff['true_peak']) <= GATES['true_peak'],
          'kick_crest': diff['kick_crest'] <= GATES['kick_crest']}
    return {'diff': diff, 'pass': ok, 'got': {'lufs_i': a['lufs_i'], 'true_peak': a['true_peak']},
            'want': {'lufs_i': b['lufs_i'], 'true_peak': b['true_peak']}}


def full_kicks(kit):
    from tools.sound import plugin_mix as PM
    return [p for lane, p, v in PM.keys(kit) if lane == 'kick' and v == 127]


def items(kit, variant):
    """The (Wet, Club) items of the kit's round 5 loop in a variant (the kit module's R5_LOOPS)."""
    from tools.sound import mix_trace as T
    return T.kit_module(kit.upper()).R5_LOOPS[variant]


def wet_file(kit, premaster, item_report, mix):
    """The Wet file as render.py writes it from the pre-master loop: the item's sub-sonic high-pass (render.subsonic,
    round the loop) unless the mix's bus EQ already runs that filter (FUT01's club input does), then its one gain."""
    from tools.sound import render as Rn
    from tools.sound import mix_trace as T
    hp = T.kit_module(kit.upper()).item_hp_hz(item_report['name'])
    has_hp = hp is not None and any(i['type'] == 'highpass' and i['freq_hz'] == round(hp) and i['order'] == 4
                                    for i in mix['bus_eq']['vox'])
    x = premaster if hp is None or has_hp else Rn.subsonic(premaster, hp, True)
    return x * D.undb(item_report['gain_db'])


def gates(kit, variant, master='exact', passes=3):
    """engine_ref against the kit's committed Wet and Club loops on the second pass: {'wet': compare(), 'club':
    compare(), 'lookback': the look-back master's difference from the exact one}."""
    L, _ = __import__('tools.sound.plugin_mix', fromlist=['loop_info']).loop_info(kit)
    wet_item, club_item = items(kit, variant)
    from tools.sound import plugin_mix as PM
    mix = PM.load(kit)
    out, meta = render(kit, variant, passes=passes, master=master, mix=mix)
    kicks = full_kicks(kit)
    wet_ref, wet_rep = reference(kit, wet_item)
    club_ref, club_rep = reference(kit, club_item)
    pre = meta['premaster'][L:2 * L]
    res = {'wet': compare(wet_file(kit, pre, wet_rep, mix), wet_ref, kicks),
           'club': compare(out[L:2 * L] * D.undb(club_rep['gain_db']), club_ref, kicks),
           'latency': meta['latency']}
    return res


def master_difference(kit, variant):
    """The look-back master against the exact one on the same mix (pass 2): compare()'s readings, and each one's
    loudness and true peak."""
    from tools.sound import plugin_mix as PM
    L, _ = PM.loop_info(kit)
    _, meta = render(kit, variant, master='off')
    m = PM.load(kit)
    pre = meta['premaster']
    exact, _ = master_chain(pre, m['master'], variant, 'exact')
    look, lat = master_chain(pre, m['master'], variant, 'lookback')
    r = compare(look[L:2 * L], exact[L:2 * L], full_kicks(kit))
    from tools.sound import mixmeter as MM
    r['lookback'] = {'lufs_i': MM.lufs_i(look[L:2 * L]), 'true_peak': MM.true_peak_db(look[L:2 * L]),
                     'latency': lat}
    r['exact'] = {'lufs_i': MM.lufs_i(exact[L:2 * L]), 'true_peak': MM.true_peak_db(exact[L:2 * L])}
    return r


def main(argv=None):
    import argparse
    import json
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True)
    ap.add_argument('--variant', choices=('vox', 'novox'), action='append')
    ap.add_argument('--master', default='exact', choices=('exact', 'lookback'))
    a = ap.parse_args(argv)
    for v in a.variant or ('vox', 'novox'):
        print(json.dumps({v: gates(a.kit, v, a.master)}, indent=1, default=float))
    return 0


if __name__ == '__main__':
    sys.exit(main())
