"""The club master: a deterministic stand-in for the engine's master bus, for loops (ADR 0014, proposed). The engine's
master (S1e) must match it, as the engine's sends must match sends.py. A kit's mastered "club" loop runs its mix
through it.

    from tools.sound import master as MS
    y, report = MS.master(mix, MS.settings(K))      # K: the kit module; its DIRECTION picks the genre (qa.CLUB)

The chain. Every stage runs round the loop: a loop plays round and round, so each filter and envelope runs in its
steady state and the seam joins.
1. Low cut: a 4th-order Butterworth high-pass at 25 Hz.
2. Mono under 150 Hz: the side high-passed, 8th order (dsp48.mono_below, the samples' own rule, ADR 0007).
3. Glue: a feed-forward bus compressor, stereo-linked: 2:1, a 6 dB soft knee, a 30 ms attack and an auto release (a
   150 ms envelope, and under it a 1.5 s one that follows sustained compression). Its threshold is found so the
   loudest moment takes 2.5 dB of gain reduction.
4. Drive: the gain into the clipper and limiter, found so the output reaches the target loudness, but never past
   max_reduction_db of peak reduction (the driven true peak over the ceiling, the clipper and limiter together).
5. Soft clip, 4x oversampled: linear up to 1 dB under the limiter's ceiling, then a tanh curve toward 0.5 dB over it,
   with first-order ADAA on the clipped excess only (what stays under the knee passes untouched).
6. DC clean: a 10 Hz zero-phase high-pass. The clipper and the limiter both leave sub-sonic ripple at the kick rate
   on asymmetric kick cycles (-44 to -55 dBFS on the round 2 loops, over the DC check's -60); any causal high-pass
   after the limiter moves the peaks 0.5-2 dB over the ceiling. A zero-phase filter is offline only: the engine's
   real-time master needs its own answer, which S1e decides (ADR 0014 lists the options).
7. True-peak limiter, stereo-linked: 4x peak detection, 5 ms lookahead, a 60 ms hold (a held note's gain stays still,
   so it adds no sidebands) and an 80 ms release, to the ceiling. The hold reaches both ways, so the gain starts down
   lookahead + hold before a peak: the limiter's latency (limit_latency(), 3145 samples, 65.5 ms at the defaults), which
   an engine that matches it reports to the host.
8. DC clean again, then a trim if the true peak, read round the loop or as a file, is over the ceiling.

master() returns the loop and a report of what it did (JSON-ready): the input's and output's loudness, the glue's
threshold and gain reduction, the drive, the clipper's peak reduction, the limiter's gain reduction, the trim, the
latency, whether it reached the target or how far short it stopped and, given the loop's main hits, what it did to
each (punch(): ADR 0011's crest of every kick and clap, in and out).
"""
import dataclasses
import math

import numpy as np
from scipy import ndimage, signal

from tools.sound import dsp48 as D
from tools.sound import mixmeter as MM
from tools.sound import qa

SR = D.SR
RUN_IN = SR                  # samples of the loop's own end each causal filter runs in from (1 s)
CLIP_PAD = 64                # wrapped samples round the clipper's oversampling: more than the FIR's 24
BLOCK = 16                   # the glue's control rate: samples per block
GLUE_RMS_S = 0.005           # the glue's detector: RMS over about 5 ms
SLOW_ATTACK_S = 0.5          # the auto release: how fast the slow envelope follows sustained compression
SOLVE_LU = 0.01              # the drive is solved to this close to the target
TP_REACH = (len(D.os_fir(4)) - 1) // 8   # samples the 4x true-peak FIR reaches each side (24): its share of the
                                         # limiter's latency
PUNCH_SPANS = {'kick': 0.200, 'clap': 0.100}   # punch(): each kind of hit read over this long from its onset
CLAP_BAND = (1000.0, 5000.0)                   # punch(): a clap also read in its own band, where the standard reads
                                               # its jump (in the four-on-the-floor kits a kick sits under it)


@dataclasses.dataclass(frozen=True)
class Settings:
    """One master's settings. settings() fills them from a genre's defaults, the kit's own MASTER dict and overrides."""
    genre: str                            # the kit's DIRECTION (kits.DIRECTIONS): QA's windows (qa.CLUB)
    target_lufs: float                    # LUFS-I of the mastered loop, read round it
    plr_min: float                        # never louder than ceiling_dbtp - plr_min (the genre's PLR floor)
    ceiling_dbtp: float = D.TP_TARGET     # the true-peak ceiling: 20 mdB under QA's -1.0, for the 24-bit rounding
    max_reduction_db: float = 4.0         # peak reduction, the clipper and the limiter together (the standard's cap)
    lowcut_hz: float = 25.0
    lowcut_order: int = 4
    mono_hz: float = 150.0
    glue_gr_db: float = 2.5               # the glue's gain reduction at the loudest moment; 0 turns the glue off
    glue_ratio: float = 2.0
    glue_knee_db: float = 6.0
    glue_attack_s: float = 0.030
    glue_release_s: tuple = (0.15, 1.5)   # the auto release: the fast envelope's, the slow one's
    clip_db: float = 0.5                  # the clipper's ceiling over the limiter's, dB
    clip_knee_db: float = 1.5             # the clipper is linear up to this far under its ceiling
    clip_os: int = 4
    limit_lookahead_s: float = 0.005
    limit_hold_s: float = 0.060
    limit_release_s: float = 0.080
    dc_hz: float = 10.0                   # the zero-phase DC clean after the clipper and the limiter

    def __post_init__(self):
        bad = []
        if self.genre not in qa.CLUB:
            bad.append(f'genre {self.genre!r} is not one of {tuple(qa.CLUB)}')
        if not self.ceiling_dbtp < 0:
            bad.append('ceiling_dbtp must be under 0 dBTP')
        if self.max_reduction_db < 0 or self.glue_gr_db < 0 or self.clip_db < 0 or self.clip_knee_db <= 0:
            bad.append('max_reduction_db, glue_gr_db and clip_db must be >= 0, clip_knee_db > 0')
        if self.glue_ratio <= 1 or self.glue_knee_db < 0:
            bad.append('glue_ratio must be over 1, glue_knee_db >= 0')
        times = (self.glue_attack_s, *self.glue_release_s, self.limit_lookahead_s, self.limit_hold_s,
                 self.limit_release_s)
        if len(self.glue_release_s) != 2 or min(times) <= 0:
            bad.append('glue_release_s is (fast, slow); every time must be over 0 s')
        if self.clip_os not in (2, 4, 8):
            bad.append('clip_os must be 2, 4 or 8')
        if not 0 < self.lowcut_hz < self.mono_hz < 1000 or self.lowcut_order not in (1, 2, 3, 4, 5, 6, 7, 8):
            bad.append('lowcut_hz < mono_hz < 1 kHz, lowcut_order 1-8')
        if not 0 < self.dc_hz < self.lowcut_hz:
            bad.append('dc_hz must be over 0 and under lowcut_hz')
        if bad:
            raise ValueError('master settings: ' + '; '.join(bad))


FIELDS = tuple(f.name for f in dataclasses.fields(Settings))


def settings(kit=None, genre=None, **overrides):
    """A master's settings: the genre's defaults (its target loudness and PLR floor from qa.CLUB; the rest of the chain
    is the same for every genre), then the kit module's own MASTER dict, if it has one, then the overrides. kit: a kit
    module (its DIRECTION is the genre); or genre: one of kits.DIRECTIONS."""
    if kit is not None:
        genre = kit.DIRECTION
        own = dict(getattr(kit, 'MASTER', None) or {})
    else:
        own = {}
    if genre is None:
        raise ValueError('settings() needs a kit module or a genre')
    if genre not in qa.CLUB:
        raise ValueError(f'genre {genre!r} is not one of {tuple(qa.CLUB)}')
    values = {'genre': genre, 'target_lufs': qa.CLUB[genre]['lufs'][0], 'plr_min': qa.CLUB[genre]['plr_min'],
              **own, **overrides}
    unknown = sorted(set(values) - set(FIELDS))
    if unknown:
        raise ValueError(f'unknown master settings {unknown}; known: {FIELDS}')
    if 'glue_release_s' in values:
        values['glue_release_s'] = tuple(values['glue_release_s'])
    return Settings(**values)


# ------------------------------------------------------------------ the stages, each round the loop
def _round(fn, x, before, after=0):
    """fn over the loop with `before` samples of its own end in front and `after` of its start behind; the loop's
    span of the result."""
    return fn(MM.wrap(x, before, after))[before:before + len(x)]


def lowcut(x, hz=25.0, order=4):
    """The low cut: a Butterworth high-pass (causal, as the engine's), run in from a second of the loop's own end."""
    sos = signal.butter(order, hz, 'highpass', fs=SR, output='sos')
    return _round(lambda u: signal.sosfilt(sos, u, axis=0), D.st(x), RUN_IN)


def mono_below(x, hz=150.0):
    """Everything under hz in the centre: the side high-passed, 8th order (dsp48.mono_below)."""
    return _round(lambda u: D.mono_below(u, hz), D.st(x), RUN_IN)


def dc_clean(x, hz=10.0):
    """The sub-sonic ripple the clipper and the limiter leave, removed with no phase shift: a zero-phase 4th-order
    high-pass (8th order in all), round the loop. At 25 Hz it takes 0.003 dB, at 40 Hz nothing measurable."""
    return _round(lambda u: D.bfilt(u, hz, 'highpass', order=4, zero_phase=True), D.st(x), 2 * SR, 2 * SR)


def _blocks(n):
    """The glue's control blocks over a loop of n samples: their starts and lengths (the last one may be short)."""
    starts = np.arange(0, n, BLOCK)
    return starts, np.diff(np.append(starts, n))


def _detector_db(x):
    """The glue's level per block, dB: the mean power of L and R (stereo-linked), RMS-smoothed over about 5 ms in
    its steady state round the loop."""
    starts, lens = _blocks(len(x))
    p = np.add.reduceat(np.mean(x ** 2, 1), starts) / lens
    a = math.exp(-BLOCK / (GLUE_RMS_S * SR))
    passes = 2 + int(math.ceil(20 * GLUE_RMS_S * SR / len(x)))
    pp = signal.lfilter([1 - a], [1, -a], np.tile(p, passes))[-len(p):]
    return 10 * np.log10(pp + 1e-20), lens


def _reduction_db(level, lens, threshold, ratio, knee, attack, release, length):
    """The glue's gain reduction per block (dB, >= 0) in its steady state round the loop: the static curve (a soft
    knee), then a fast envelope (attack, release[0]) and a slow one that follows it (SLOW_ATTACK_S, release[1]); the
    larger of the two applies, so sustained compression releases slowly and a lone hit quickly."""
    over = level - threshold
    tgt = (1 - 1 / ratio) * np.where(over <= -knee / 2, 0.0, np.where(
        over >= knee / 2, over, (over + knee / 2) ** 2 / (2 * knee) if knee > 0 else over))
    coef = lambda tau: np.exp(-lens / (tau * SR)).tolist()
    aa, rf, sa, rs = coef(attack), coef(release[0]), coef(SLOW_ATTACK_S), coef(release[1])
    passes = 1 + int(math.ceil(8 * max(release[1], SLOW_ATTACK_S) * SR / length))
    t = tgt.tolist()
    nb = len(t)
    f = s = 0.0
    out = [0.0] * nb
    for _ in range(passes):
        for i in range(nb):
            ti = t[i]
            f = ti + (aa[i] if ti > f else rf[i]) * (f - ti)
            s = f + (sa[i] if f > s else rs[i]) * (s - f)
            out[i] = f if f > s else s
    return np.array(out)


def _per_sample(r_blocks, n):
    """Block values (at block centres) to one per sample, linear in between, round the loop."""
    starts, lens = _blocks(n)
    c = starts + 0.5 * lens - 0.5
    return np.interp(np.arange(n), np.concatenate([c - n, c, c + n]), np.tile(r_blocks, 3))


def glue(x, gr_db=2.5, ratio=2.0, knee_db=6.0, attack_s=0.030, release_s=(0.15, 1.5)):
    """The glue compressor (see the module docstring), its threshold found so the loudest moment takes gr_db of gain
    reduction (Illinois, to 1e-4 dB). Returns (audio, {'threshold_dbfs', 'gr_max_db', 'gr_mean_db'})."""
    x = D.st(x)
    if gr_db <= 0:
        return x, {'threshold_dbfs': None, 'gr_max_db': 0.0, 'gr_mean_db': 0.0}
    level, lens = _detector_db(x)
    red = lambda thr: _reduction_db(level, lens, thr, ratio, knee_db, attack_s, release_s, len(x))
    f = lambda thr: float(red(thr).max()) - gr_db
    hi = float(level.max()) + knee_db / 2                       # over the loudest block: no reduction at all
    lo = hi - knee_db - 4 * gr_db / (1 - 1 / ratio)             # past gr_db, unless the loudest moment is so short
    fa = f(lo)                                                  # that the attack hardly reaches it: then go lower
    while fa <= 0 and hi - lo < 200.0:
        lo -= hi - lo
        fa = f(lo)
    a, b, fb = lo, hi, -gr_db
    side = 0
    for _ in range(60):
        thr = (a * fb - b * fa) / (fb - fa)
        ft = f(thr)
        if abs(ft) < 1e-4:
            break
        if (ft > 0) == (fa > 0):
            a, fa = thr, ft
            if side == 1:
                fb /= 2
            side = 1
        else:
            b, fb = thr, ft
            if side == -1:
                fa /= 2
            side = -1
    r = red(thr)
    g = _per_sample(r, len(x))
    return x * D.undb(-g)[:, None], {'threshold_dbfs': float(thr), 'gr_max_db': float(r.max()),
                                     'gr_mean_db': float(g.mean())}


def soft_clip(x, ceiling_db, knee_db=1.5, os=4):
    """The soft clipper, os x oversampled, round the loop: linear up to knee_db under its ceiling (dBFS), then a tanh
    curve toward it, C1-smooth at the knee. First-order ADAA on the clipped excess only, so a signal that stays under
    the knee comes out as the oversampling's FIR leaves it, with no ADAA averaging (that would take 0.3 dB off the
    16 kHz band). Returns (audio, the share of oversampled samples over the knee)."""
    C = float(D.undb(ceiling_db))
    T = C * float(D.undb(-knee_db))
    k = C - T

    def excess(u):
        a = np.abs(u)
        return np.where(a <= T, 0.0, np.sign(u) * (T + k * np.tanh((a - T) / k)) - u)

    def excess_int(u):                                          # its antiderivative
        a = np.abs(u)
        v = np.maximum((a - T) / k, 0.0)
        logcosh = v + np.log1p(np.exp(-2 * v)) - math.log(2.0)
        return np.where(a <= T, 0.0, T * a + k * k * logcosh - 0.5 * a * a - 0.5 * T * T)

    over = []

    def run(u):
        uu = D.up(u, os)
        over.append(float(np.mean(np.abs(uu[os * CLIP_PAD:len(uu) - os * CLIP_PAD]) > T)))
        return D.down(uu + D._adaa(uu, excess, excess_int), os)

    y = _round(run, D.st(x), CLIP_PAD, CLIP_PAD)
    return y, over[0]


def _sliding_min(a, before, after):
    """min of a[n - before .. n + after], round the loop (after - before even)."""
    m = ndimage.minimum_filter1d(a, size=before + after + 1, mode='wrap')
    return np.roll(m, -(after - before) // 2)


def _mean_back(a, w):
    """mean of a[n - w .. n], round the loop."""
    c = np.concatenate([[0.0], np.cumsum(np.concatenate([a[-w:], a]))])
    n = np.arange(len(a))
    return (c[n + w + 1] - c[n]) / (w + 1)


def limit_latency(lookahead_s=0.005, hold_s=0.060):
    """How far ahead of its input the limiter's gain looks, samples: the lookahead's ramp and the hold (the gain holds
    the lowest need from hold_s before a sample to lookahead_s + hold_s after it), plus the sample either side the peak
    detector reads and the 4x FIR's reach. An engine that reproduces limit() delays its audio this long, and reports it
    to the host as latency. At the defaults, 240 + 2880 + 1 + 24 = 3145 samples, 65.5 ms."""
    return 2 * int(round(lookahead_s * SR / 2)) + int(round(hold_s * SR)) + 1 + TP_REACH


def limit_gain_db(x, ceiling_db, lookahead_s=0.005, hold_s=0.060, release_s=0.080):
    """The limiter's gain reduction per sample, dB (>= 0), round the loop (limit())."""
    x = D.st(x)
    n = len(x)
    c = float(D.undb(ceiling_db))
    u = D.up(MM.wrap(x, CLIP_PAD, CLIP_PAD), 4)[4 * CLIP_PAD:4 * (CLIP_PAD + n)]
    p = np.abs(u).max(1)
    pp = np.concatenate([p[-4:], p, p[:4]])
    idx = 4 * np.arange(n) + 4
    pk = np.max(np.stack([pp[idx + d] for d in range(-4, 5)]), 0)
    need = np.minimum(1.0, c / (pk + 1e-30))
    w = 2 * int(round(lookahead_s * SR / 2))
    h = int(round(hold_s * SR))
    g = _mean_back(_sliding_min(need, h, w + h), w)
    r2 = -20 * np.log10(np.minimum(g, 1.0))
    la = -1.0 / (release_s * SR)                               # ln of the release's per-sample factor
    passes = 1 + int(math.ceil(10 * release_s * SR / n))
    rr = np.tile(r2, passes)
    k = np.arange(len(rr))
    with np.errstate(divide='ignore'):
        acc = np.maximum.accumulate(np.log(np.maximum(rr, 1e-300)) - k * la)
    r = np.maximum(np.exp(k * la + acc), rr)[-n:]             # r[k] = max(r2[k], r[k - 1] e^la), in steady state
    return np.where(r < 1e-9, 0.0, r)


def limit(x, ceiling_db, lookahead_s=0.005, hold_s=0.060, release_s=0.080):
    """The true-peak limiter, stereo-linked, round the loop. The gain each sample needs comes from the 4x oversampled
    peak within a sample of it; the gain holds the lowest need from hold_s before to lookahead_s + hold_s after, ramps
    down over the lookahead (a moving average, so it is never over the need), and releases exponentially (release_s)
    in dB. The gain so starts down limit_latency() samples before a peak. Returns (audio, {'gr_max_db', 'gr_mean_db',
    'active_pct': the share of samples taking over 0.1 dB})."""
    x = D.st(x)
    r = limit_gain_db(x, ceiling_db, lookahead_s, hold_s, release_s)
    return x * D.undb(-r)[:, None], {'gr_max_db': float(r.max()), 'gr_mean_db': float(r.mean()),
                                     'active_pct': float(100 * np.mean(r > 0.1))}


# ------------------------------------------------------------------ the master
def _loud(x):
    lo = MM.loudness(x)
    return {'lufs_i': lo['lufs_i'], 'lufs_s_max': lo['lufs_s_max'], 'true_peak': lo['true_peak'],
            'true_peak_file': MM.true_peak_db(x, loop=False), 'plr': lo['plr'], 'psr': lo['psr']}


def _chain(g, drive_db, s):
    """Stages 4-8 on the glued loop at a drive: (audio, the clipper's report, the limiter's report, trim dB)."""
    u = g * D.undb(drive_db)
    clipped, over = soft_clip(u, s.ceiling_dbtp + s.clip_db, s.clip_knee_db, s.clip_os)
    clip = {'reduction_db': max(0.0, MM.true_peak_db(u) - MM.true_peak_db(clipped)) if over else 0.0,
            'active_pct': 100 * over}
    y, lim = limit(dc_clean(clipped, s.dc_hz), s.ceiling_dbtp, s.limit_lookahead_s, s.limit_hold_s,
                   s.limit_release_s)
    y = dc_clean(y, s.dc_hz)
    trim = min(0.0, s.ceiling_dbtp - max(MM.true_peak_db(y), MM.true_peak_db(y, loop=False)))
    return y * D.undb(trim), clip, lim, float(trim)


def punch(x, y, hits):
    """What a master did to a loop's main hits: for each kind ({kind: onsets in samples}, e.g. 'kick' and 'clap'),
    ADR 0011's crest of every hit as the loop plays it (mixmeter.hit_crest_db over PUNCH_SPANS[kind] from its onset)
    in x, the master's input, and in y, its output; the loss per hit and its median, and the lowest crest out. A clap
    is also read in CLAP_BAND, its own band, where a kick under it doesn't count. JSON-ready."""
    out = {}
    for kind in sorted(hits):
        onsets = sorted(int(o) for o in hits[kind])
        span = PUNCH_SPANS.get(kind, MM.HIT_SPAN_S)
        reads = {'': (MM.hit_crest_db(x, onsets, span), MM.hit_crest_db(y, onsets, span))}
        if kind == 'clap':
            reads['band_'] = (MM.hit_crest_db(x, onsets, span, *CLAP_BAND),
                              MM.hit_crest_db(y, onsets, span, *CLAP_BAND))
        row = {'onsets': onsets, 'span_s': span}
        for pre, (a, b) in reads.items():
            loss = [u - v for u, v in zip(a, b)]
            row.update({f'{pre}in_db': [round(v, 3) for v in a], f'{pre}out_db': [round(v, 3) for v in b],
                        f'{pre}median_loss_db': float(np.median(loss)) if loss else None,
                        f'{pre}min_out_db': float(min(b)) if b else None})
        if kind == 'clap':
            row['band_hz'] = list(CLAP_BAND)
        out[kind] = row
    return out


PEAK_NEAR_S = 0.030          # peaks(): a moment this close after a declared hit is that hit's
PEAK_TOP = 3                 # peaks(): the loudest moments away from the hits it lists


def peaks(u, s, hits):
    """Where the driven loop (u: the glued loop times the drive, what the clipper meets) goes over the clipper's knee:
    for each kind of hit, how far over it each hit's first 30 ms goes (the median and the most), and the PEAK_TOP
    loudest moments more than 30 ms after any hit (their time and dB over the knee), so the report shows whether the
    master's cap is set by the transients or by tonal content between them. dB over the knee; negative: under it."""
    n = len(u)
    knee = s.ceiling_dbtp + s.clip_db - s.clip_knee_db
    v = D.up(MM.wrap(D.st(u), CLIP_PAD, CLIP_PAD), 4)[4 * CLIP_PAD:4 * (CLIP_PAD + n)]
    over = 20 * np.log10(np.abs(v).max(1).reshape(n, 4).max(1) + 1e-30) - knee
    near = int(PEAK_NEAR_S * SR)
    out = {'knee_dbfs': float(knee)}
    taken = np.zeros(n, bool)
    for kind in sorted(hits):
        per = []
        for o in sorted(int(h) for h in hits[kind]):
            idx = (o + np.arange(near)) % n
            per.append(float(over[idx].max()))
            taken[idx] = True
        out[kind] = {'median_db': float(np.median(per)) if per else None, 'max_db': float(max(per)) if per else None}
    rest, top = np.where(taken, -np.inf, over), []
    for _ in range(PEAK_TOP):
        k = int(np.argmax(rest))
        if not np.isfinite(rest[k]):
            break
        top.append({'s': round(k / SR, 4), 'db': float(rest[k])})
        rest[(k + np.arange(-int(0.05 * SR), int(0.05 * SR))) % n] = -np.inf
    out['away_from_hits'] = top
    return out


def master(x, s, hits=None):
    """Master a loop (float, (n,) or (n, 2), 48 kHz; it plays from sample 0 to the end and round again) with settings
    s (settings()). hits: {kind: onsets (samples)} of its main hits, e.g. {'kick': ..., 'clap': ...}: the report then
    says what the master did to each (punch()); a club loop's QA needs it. Returns (the mastered loop, (n, 2), L = R
    when the input is mono; the report)."""
    x = D.st(np.asarray(x, dtype=float))
    report = {'genre': s.genre, 'settings': dataclasses.asdict(s), 'silent': not np.any(x), 'input': _loud(x),
              'latency': {'samples': limit_latency(s.limit_lookahead_s, s.limit_hold_s),
                          'ms': round(1000.0 * limit_latency(s.limit_lookahead_s, s.limit_hold_s) / SR, 3)}}
    target = min(s.target_lufs, s.ceiling_dbtp - s.plr_min)
    if report['silent']:
        report.update(glue=None, drive_db=0.0, clip=None, limit=None, peak_reduction_db=0.0, trim_db=0.0,
                      output=report['input'], target={'lufs': target, 'asked': s.target_lufs, 'reached': False,
                                                      'short_lu': None})
        if hits is not None:
            report['punch'] = punch(x, x, hits)
        return x.copy(), report
    a = mono_below(lowcut(x, s.lowcut_hz, s.lowcut_order), s.mono_hz)
    g, glue_rep = glue(a, s.glue_gr_db, s.glue_ratio, s.glue_knee_db, s.glue_attack_s, s.glue_release_s)
    tp, lu = MM.true_peak_db(g), MM.lufs_i(g)
    quiet = s.ceiling_dbtp + min(0.0, s.clip_db - s.clip_knee_db) - tp   # up to this drive neither the clipper (its
    top = s.ceiling_dbtp + s.max_reduction_db - tp                       # knee) nor the limiter acts; top: the cap
    run = lambda d: _chain(g, d, s)
    if target - lu <= quiet:                                    # reached before anything acts: gain only
        drive = target - lu
        out = run(drive)
    else:
        lo_d, hi_d = quiet, top
        out = run(hi_d)
        f_hi = MM.lufs_i(out[0]) - target
        drive = hi_d
        if f_hi > 0:                                            # reachable: solve between the knee and the cap
            f_lo = lu + lo_d - target
            a_, fa, b_, fb = lo_d, f_lo, hi_d, f_hi
            side = 0
            for _ in range(40):
                drive = (a_ * fb - b_ * fa) / (fb - fa)
                out = run(drive)
                fd = MM.lufs_i(out[0]) - target
                if abs(fd) < SOLVE_LU:
                    break
                if fd < 0:
                    a_, fa = drive, fd
                    if side == 1:
                        fb /= 2
                    side = 1
                else:
                    b_, fb = drive, fd
                    if side == -1:
                        fa /= 2
                    side = -1
    y, clip, lim, trim = out
    o = _loud(y)
    report.update(glue=glue_rep, drive_db=float(drive), clip=clip, limit=lim,
                  peak_reduction_db=float(max(0.0, tp + drive - s.ceiling_dbtp)), trim_db=trim, output=o,
                  target={'lufs': float(target), 'asked': float(s.target_lufs),
                          'reached': bool(o['lufs_i'] >= target - 2 * SOLVE_LU),
                          'short_lu': float(max(0.0, target - o['lufs_i']))})
    if hits is not None:
        report['punch'] = punch(x, y, hits)
        report['peaks'] = peaks(g * D.undb(drive), s, hits)
    return y, report
