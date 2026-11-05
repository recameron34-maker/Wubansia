"""Mix meters for loops and masters: the measures of the modern-sound standard (docs/plans/S1-modern-sound.md) as one
tested module. The club master (master.py) and the club loop checks (qa.py) read their numbers here.

- Loudness per ITU-R BS.1770-4, as qa.lufs measures it (the same K-weighting coefficients, qa.K1 and qa.K2; 400 ms
  blocks every 100 ms; gated at -70 LUFS and -10 LU): integrated (lufs_i), momentary (lufs_m, 400 ms) and short-term
  (lufs_s, 3 s) curves. A mono file counts as L = R, as the engine plays it.
- True peak, 4x oversampled with dsp48's FIR (true_peak_db); PLR (true peak minus LUFS-I) and PSR (true peak minus the
  loudest short-term window), in loudness().
- Octave bands, base-2 centres 31.25 Hz to 16 kHz (named 31.5 to 16k), edges at the centre / and x sqrt(2), from one
  FFT over the whole file: per band the level against the 1 kHz band (L and R power summed), side over mid
  (M = (L + R) / 2, S = (L - R) / 2) and the L/R correlation (bands, balance, width, side_mid_db), and the standard's
  shape checks on a balance (shape).
- Punch: the kick crest (qa.kick_crest_db, ADR 0011), the same crest per hit in a loop (hit_crest_db), the attack
  time of a hit, and the jump of a band at each onset in a loop against the 60-10 ms before it (jump_db, the
  standard's own measure). The jump compares a hit's loudest 10 ms with the bed before it, so it can't see a peak
  flattened onto the hit's own body; the per-hit crest can.
- Left/right lean per third octave (lean_db): a side over mid reading counts a lean to one side as width.
- A reference track (reference(), or `python -m tools.sound.mixmeter --reference <file>`): the standard's S4, the
  loudest 16 s of a released record measured the same way, as a file. A 44.1 or 96 kHz file is resampled to 48 kHz
  for measuring only (the no-resampling rule is about masters, not meters).

Loops (loop=True, the default) are measured as they play: every window and filter runs round the seam, so a reading
doesn't depend on where the file starts; an FFT over exactly one loop needs no window. loop=False measures a file as
a file: lufs_i and true_peak_db are then qa.lufs and dsp48.true_peak_db exactly. Every function takes float audio at
48 kHz, (n,) or (n, 2)."""
import argparse
import json
import math
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import tools.sound  # noqa: E402,F401  first: it pins numpy's SIMD level before numpy loads

import numpy as np  # noqa: E402
from scipy import signal  # noqa: E402

from tools.sound import dsp48 as D  # noqa: E402
from tools.sound import qa  # noqa: E402

SR = D.SR
CENTRES = (31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0)
BANDS = ('31.5', '63', '125', '250', '500', '1k', '2k', '4k', '8k', '16k')
BLOCK_S, SHORT_S, HOP_S = 0.4, 3.0, 0.1      # momentary and short-term windows, both every 100 ms (BS.1770, Tech 3341)
RUN_IN_S = 0.5                               # a loop's K-weighting starts this long before it: the filters settle in
                                             # about 0.15 s (the RLB high-pass's poles, 0.995)
TP_PAD = 64                                  # wrapped samples each side of a loop for its true peak: more than the 4x
                                             # FIR's reach (24 samples)
FLOOR = 1e-30                                # powers: a band with nothing in it reads about -300 dB, not -inf


def wrap(x, before, after):
    """A loop with `before` samples of its own end in front and `after` of its start behind, as it plays round (whole
    copies first when the loop is shorter)."""
    L = len(x)
    pre, post = -(-before // L), -(-after // L)
    xx = np.concatenate([x] * (pre + 1 + post))
    return xx[pre * L - before:(pre + 1) * L + after]


def _db(a, b):
    return float(10 * np.log10((a + FLOOR) / (b + FLOOR)))


# ------------------------------------------------------------------ loudness
def _kw(u):
    return signal.lfilter(*qa.K2, signal.lfilter(*qa.K1, u, axis=0), axis=0)


def kweighted(x, loop=True):
    """The K-weighted signal, stereo (L = R for mono). A loop comes out in its steady state: the filters run in from
    RUN_IN_S of its own end."""
    x = D.st(x)
    if not loop:
        return _kw(x)
    n = int(RUN_IN_S * SR)
    return _kw(wrap(x, n, 0))[n:]


def power_windows(x, seconds, loop=True):
    """The mean square of the K-weighted signal, channels summed, in windows of `seconds` every 100 ms. A loop: one
    window starting at every 100 ms of it, each running round the seam. A file: windows that fit in it (one,
    zero-padded, if it is shorter), as qa.lufs blocks it."""
    x = D.st(x)
    L = len(x)
    w, h = int(round(seconds * SR)), int(round(HOP_S * SR))
    if loop:
        n = int(RUN_IN_S * SR)
        y = _kw(wrap(x, n, w))[n:]
        starts = np.arange(0, L, h)
    else:
        y = _kw(x)
        if len(y) < w:
            y = np.concatenate([y, np.zeros((w - len(y), 2))])
        starts = np.arange(0, len(y) - w + 1, h)
    c = np.concatenate([[0.0], np.cumsum(np.sum(y ** 2, 1))])
    return (c[starts + w] - c[starts]) / w + 1e-20


def _lk(ms):
    return -0.691 + 10 * np.log10(ms)


def lufs_i(x, loop=True):
    """Integrated loudness, LUFS: 400 ms blocks, gated at -70 LUFS and at -10 LU under the ungated blocks' mean."""
    z = power_windows(x, BLOCK_S, loop)
    lv = _lk(z)
    g = z[lv > -70]
    if not len(g):
        return -70.0
    rel = _lk(g.mean()) - 10
    return float(_lk(z[(lv > -70) & (lv > rel)].mean()))


def lufs_m(x, loop=True):
    """Momentary loudness every 100 ms (400 ms windows), LUFS."""
    return _lk(power_windows(x, BLOCK_S, loop))


def lufs_s(x, loop=True):
    """Short-term loudness every 100 ms (3 s windows), LUFS. A loop shorter than 3 s is read round more than once."""
    return _lk(power_windows(x, SHORT_S, loop))


def true_peak_db(x, loop=True):
    """True peak, dBTP: the peak of the 4x oversampled signal (dsp48's FIR). A loop is read round its seam, where a file
    would meet silence."""
    x = np.asarray(x, dtype=float)
    if not loop:
        return D.true_peak_db(x)
    u = D.up(wrap(x, TP_PAD, TP_PAD), 4)[4 * TP_PAD:4 * (TP_PAD + len(x))]
    return float(D.db(np.abs(u).max()))


def loudness(x, loop=True):
    """LUFS-I, the loudest short-term (3 s) and momentary (400 ms) windows, the true peak (dBTP), the sample peak
    (dBFS), PLR (true peak minus LUFS-I) and PSR (true peak minus the loudest short-term window), dB."""
    i, tp = lufs_i(x, loop), true_peak_db(x, loop)
    s = float(lufs_s(x, loop).max())
    return {'lufs_i': i, 'lufs_s_max': s, 'lufs_m_max': float(lufs_m(x, loop).max()), 'true_peak': tp,
            'sample_peak': float(D.db(np.abs(np.asarray(x, dtype=float)).max())), 'plr': tp - i, 'psr': tp - s}


# ------------------------------------------------------------------ octave bands and width
def _spectrum(x, loop):
    """One FFT of the stereo signal over the whole file: a loop's exact period; a file zero-padded to 1 s at least."""
    x = D.st(x)
    n = len(x) if loop else max(len(x), SR)
    return np.fft.rfftfreq(n, 1 / SR), np.fft.rfft(x, n=n, axis=0)


def band_edges(fc):
    return fc / math.sqrt(2), fc * math.sqrt(2)


def bands(x, loop=True):
    """Per octave band: {'level': dB against the 1 kHz band (L and R power summed), 'width': side over mid, dB,
    'corr': the L/R correlation}. A band with nothing in it reads about -300 dB; width about -300 where L = R."""
    f, X = _spectrum(x, loop)
    XL, XR = X[:, 0], X[:, 1]
    PL, PR = np.abs(XL) ** 2, np.abs(XR) ** 2
    M, S = np.abs(0.5 * (XL + XR)) ** 2, np.abs(0.5 * (XL - XR)) ** 2
    C = np.real(XL * np.conj(XR))
    rows = {}
    for fc, name in zip(CENTRES, BANDS):
        lo, hi = band_edges(fc)
        k = (f >= lo) & (f < hi)
        rows[name] = (PL[k].sum() + PR[k].sum(), _db(S[k].sum(), M[k].sum()),
                      float(C[k].sum() / math.sqrt(PL[k].sum() * PR[k].sum() + FLOOR)))
    ref = rows['1k'][0]
    return {name: {'level': _db(p, ref), 'width': w, 'corr': c} for name, (p, w, c) in rows.items()}


def balance(x, loop=True):
    """The octave levels against the 1 kHz band, dB: {'31.5': ..., '16k': ...}."""
    return {name: r['level'] for name, r in bands(x, loop).items()}


def width(x, loop=True):
    """Side over mid per octave band, dB."""
    return {name: r['width'] for name, r in bands(x, loop).items()}


def side_mid_db(x, lo, hi, loop=True):
    """Side over mid from lo to hi Hz (FFT bins, so no filter skirt leaks in): qa.low_side_db's measure for 0-120 Hz."""
    f, X = _spectrum(x, loop)
    k = (f >= lo) & (f < hi)
    return _db(np.sum(np.abs(0.5 * (X[k, 0] - X[k, 1])) ** 2), np.sum(np.abs(0.5 * (X[k, 0] + X[k, 1])) ** 2))


def shape(b):
    """The standard's shape checks on a balance (balance()): the low-mid hole, the 125 and 250 Hz bands against the
    straight line (in octaves) from the sub, the louder of the 31.5 and 63 Hz bands (a sub on E1 reads in the 31.5 Hz
    band), at 63 Hz, to the 500 Hz band; the top slope, dB per octave from 500 Hz to 8 kHz; the sub hump, the sub over
    the 500 Hz band."""
    sub = max(b['31.5'], b['63'])
    line = lambda octaves: sub + (b['500'] - sub) * octaves / 3
    return {'hole_125': b['125'] - line(1), 'hole_250': b['250'] - line(2), 'top_slope': (b['8k'] - b['500']) / 4,
            'sub_hump': sub - b['500']}


# ------------------------------------------------------------------ punch
def crest_db(x):
    """The kick crest (ADR 0011): true peak over the RMS of the loudest 50 ms of the mono sum. A sine reads 3.01 dB."""
    return qa.kick_crest_db(x)


HIT_SPAN_S = 0.200           # hit_crest_db: each hit read over this long from its onset (a kick's peak and body)


def hit_crest_db(x, onsets, span=HIT_SPAN_S, lo=None, hi=None):
    """ADR 0011's crest (qa.kick_crest_db: true peak over the RMS of the loudest 50 ms of the mono sum) of each hit in
    a loop, as the loop plays it: read over `span` seconds from each onset (samples), round the seam; with lo and hi,
    of that band (zero-phase, 4th order, round the loop). The true peak is read with TP_PAD samples of the loop either
    side of the span, as true_peak_db reads a loop, so the 4x FIR doesn't ring on a span cut out of a loud passage. A
    hit alone reads what its own file reads (the span covering it). A limiter or a clipper that pulls a hit's peak
    down onto its body lowers it; the in-loop jump can't see that."""
    x = D.st(np.asarray(x, dtype=float))
    L = len(x)
    if lo and hi:
        x = D.bfilt(np.tile(x, (3, 1)), (lo, hi), 'bandpass', order=4, zero_phase=True)[L:2 * L]
    n = int(round(span * SR))
    out = []
    for o in onsets:
        seg = x[(int(o) - TP_PAD + np.arange(n + 2 * TP_PAD)) % L]
        tp = D.db(np.abs(D.up(seg, 4)[4 * TP_PAD:4 * (TP_PAD + n)]).max())
        out.append(float(qa.kick_crest_db(seg[TP_PAD:TP_PAD + n], peak_db=tp)))
    return out


def third_octaves(lo=100.0, hi=16000.0):
    """Base-2 third-octave centres from lo to hi, Hz (1 kHz is one)."""
    k = np.arange(math.ceil(3 * math.log2(lo / 1000.0)), math.floor(3 * math.log2(hi / 1000.0)) + 1)
    return tuple(float(1000.0 * 2 ** (i / 3)) for i in k)


def lean_db(x, loop=True, lo=100.0, hi=16000.0):
    """Right over left per third octave, dB (positive: the band leans right), {centre Hz: dB}, from one FFT over the
    whole file (a loop's exact period). Side over mid reads a lean as width; this tells them apart: a balanced wide
    sound reads 0 in every band."""
    f, X = _spectrum(x, loop)
    PL, PR = np.abs(X[:, 0]) ** 2, np.abs(X[:, 1]) ** 2
    out = {}
    for fc in third_octaves(lo, hi):
        k = (f >= fc / 2 ** (1 / 6)) & (f < fc * 2 ** (1 / 6))
        out[round(fc, 1)] = _db(PR[k].sum(), PL[k].sum())
    return out


def _env_db(m, win):
    """Centred moving RMS, dB re full scale, `win` seconds."""
    w = max(1, int(win * SR))
    c = np.concatenate([[0.0], np.cumsum(np.concatenate([np.zeros(w // 2), m ** 2, np.zeros(w - w // 2)]))])
    return 10 * np.log10((c[w:] - c[:-w])[:len(m)] / w + 1e-30)


def attack_ms(x):
    """A hit's attack: on the mono sum's 1 ms RMS envelope, from its first point within 30 dB of the peak (the onset)
    to the peak, ms."""
    e = _env_db(D.mono(x), 0.001)
    k = int(np.argmax(e))
    return 1000.0 * (k - int(np.flatnonzero(e >= e[k] - 30.0)[0])) / SR


def jump_db(x, onsets, lo=None, hi=None, before=(0.060, 0.010), after=0.030, win=0.010):
    """The standard's in-loop punch: for each onset (samples), the loudest `win` RMS of the band in the `after` seconds
    from it against the band's mean power from before[0] to before[1] seconds ahead of it, dB. The loop is read round
    its seam; lo, hi: a zero-phase 4th-order band (or a high- or low-pass with one of them None), full band without."""
    m = D.mono(x)
    L = len(m)
    if lo or hi:
        kind, fc = ('bandpass', (lo, hi)) if lo and hi else (('highpass', lo) if lo else ('lowpass', hi))
        m = D.bfilt(np.tile(m, 3), fc, kind, order=4, zero_phase=True)[L:2 * L]
    e = _env_db(np.tile(m, 3), win)[L:2 * L]
    p = 10 ** (e / 10)
    out = []
    for o in onsets:
        pre = np.arange(o - int(before[0] * SR), o - int(before[1] * SR)) % L
        post = np.arange(o, o + int(after * SR)) % L
        out.append(float(e[post].max() - 10 * np.log10(p[pre].mean() + 1e-30)))
    return out


# ------------------------------------------------------------------ a reference track (the standard's S4)
REFERENCE_S = 16.0           # a reference's section: its loudest 16 s, by K-weighted loudness
REFERENCE_HOP_S = 0.5        # ...searched every half second


def read_reference(path):
    """A reference file as float stereo at 48 kHz: a 44.1 or 96 kHz file is resampled to 48 kHz (resample_poly), for
    measuring only; a mono file counts as L = R; channels past the second are left out."""
    import soundfile as sf
    x, sr = sf.read(str(path), dtype='float64', always_2d=True)
    if sr != SR:
        g = math.gcd(SR, int(sr))
        x = signal.resample_poly(x, SR // g, int(sr) // g, axis=0)
    return np.repeat(x, 2, 1) if x.shape[1] == 1 else x[:, :2]


def loudest_start(x, seconds=REFERENCE_S, hop=REFERENCE_HOP_S):
    """The first sample of the loudest `seconds` of a file (K-weighted mean square, every `hop`); 0 if it is
    shorter."""
    x = D.st(x)
    w, h = int(round(seconds * SR)), int(round(hop * SR))
    if len(x) <= w:
        return 0
    y = _kw(x)
    c = np.concatenate([[0.0], np.cumsum(np.sum(y ** 2, 1))])
    starts = np.arange(0, len(x) - w + 1, h)
    return int(starts[int(np.argmax(c[starts + w] - c[starts]))])


def reference(x, start_s=None, seconds=REFERENCE_S):
    """A reference track measured as the standard's tables read a loop, but as a file (it doesn't loop): its loudest
    `seconds` (or from start_s), with its loudness, PLR and PSR, the octave bands re 1 kHz, side over mid and
    correlation per band, the shape checks, and the lean per third octave. x: audio at 48 kHz, or a path
    (read_reference)."""
    if isinstance(x, (str, Path)):
        x = read_reference(x)
    x = D.st(np.asarray(x, dtype=float))
    a = loudest_start(x, seconds) if start_s is None else int(round(start_s * SR))
    seg = x[a:a + int(round(seconds * SR))]
    b = bands(seg, loop=False)
    bal = {k: r['level'] for k, r in b.items()}
    return {'section_s': [round(a / SR, 3), round((a + len(seg)) / SR, 3)], 'loudness': loudness(seg, loop=False),
            'bands': b, 'shape': shape(bal), 'corr': float(qa.stereo_corr(seg)), 'lean': lean_db(seg, loop=False)}


def _report_text(path, r):
    lo = r['loudness']
    bal = {k: v['level'] for k, v in r['bands'].items()}
    lines = [f"{path}: {r['section_s'][0]:.1f}-{r['section_s'][1]:.1f} s",
             f"LUFS-I {lo['lufs_i']:.2f}, LUFS-S max {lo['lufs_s_max']:.2f}, true peak {lo['true_peak']:.2f} dBTP, "
             f"PLR {lo['plr']:.2f}, PSR {lo['psr']:.2f} dB",
             'octaves re 1 kHz: ' + ', '.join(f'{k} {v:+.1f}' for k, v in bal.items()),
             'side over mid: ' + ', '.join(f"{k} {v['width']:.1f}" for k, v in r['bands'].items()),
             f"hole 125 {r['shape']['hole_125']:+.1f}, hole 250 {r['shape']['hole_250']:+.1f}, top slope "
             f"{r['shape']['top_slope']:+.2f} dB/oct, sub hump {r['shape']['sub_hump']:+.1f}; correlation "
             f"{r['corr']:.2f}"]
    return '\n'.join(lines)


def main(argv=None):
    ap = argparse.ArgumentParser(description='Measure a reference track as the modern-sound standard reads the kits '
                                             '(its S4): the loudest 16 s, as a file.')
    ap.add_argument('--reference', required=True, nargs='+', help='WAV or FLAC files (44.1, 48 or 96 kHz)')
    ap.add_argument('--start', type=float, help='the section start, s (default: the loudest 16 s)')
    ap.add_argument('--seconds', type=float, default=REFERENCE_S)
    ap.add_argument('--json', action='store_true', help='print JSON instead of text')
    a = ap.parse_args(argv)
    out = {}
    for path in a.reference:
        out[path] = reference(path, a.start, a.seconds)
        if not a.json:
            print(_report_text(path, out[path]))
    if a.json:
        print(json.dumps(out, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
