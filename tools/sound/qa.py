"""Sample QA for 48 kHz masters: plan section 5.2, with the mud gate.

Every check returns a Check: a name, ok (True, False, or None when it doesn't apply), the measured value, the limit and
a short detail. check_take() runs the per-file checks on one sound; check_layers(), check_loop(), check_gains(),
check_content() and check_transcript() run the per-instrument ones. A sound declares its category (and overrides) in a
Spec; the category picks the rules: CATEGORY_RULES below.

Provenance: lufs, dc_db, click_at, stereo and harmonic_asr are ported from the S1 research prototype (qa.py), the mud
measures from the kick mud meter (mud.py) and the 2-step loop from the kick judge (judge_loops.py, loopcheck.py), all
written during planning on 2026-10-29. Method of the mud measures, as in docs/listening.md round 0: 4th-order
Butterworth band-pass, zero-phase; 5 ms RMS envelope; a fall is counted from the band's own peak, until it stays down.
"""
import re
from dataclasses import dataclass

import numpy as np
import soundfile as sf
from scipy import signal

from tools.sound import denylist
from tools.sound import dsp48 as D

SR = D.SR

# limits (plan section 5.2)
CEILING_DBTP = -1.0
DC_MAX_DB = -60.0
FIRST_MAX_DB = -80.0
TAIL_MAX_DB = -70.0          # peak of the last 10 ms of a one-shot
SPLICE_MAX_DB = 12.0
SEAM_MAX_DB = 3.0
ALIAS_MAX_DB = -60.0
CORR_MIN = 0.2
LOW_SIDE_MAX_DB = -20.0      # side under 120 Hz against mid under 120 Hz
LOWCUT_HZ = 100.0            # the low cut: nothing under ~100 Hz competes with the kick and sub
LOWCUT_MAX_DB = -24.0        # share of the sound under LOWCUT_HZ
HF_BAND_MIN_DB = -40.0       # hf_synthetic masters: 14-20 kHz against 6-11 kHz (a plain 2x resample reads about -61)
VEL_STEP_MIN_DB = 1.5
RR_SPREAD_MAX_DB = 0.5
MUD_FALL_MS = 120.0          # 150-400 Hz falls 30 dB within this
MUD_VS_SUB_DB = -20.0        # 150-400 Hz against 30-120 Hz after the first 80 ms, in every 20 ms window
MUD_WINDOW = (0.020, 0.010)  # vs sub: window and hop, seconds
SUB_FLOOR_DB = -30.0         # vs sub: where 30-120 Hz has faded this far under its loudest window, compare with that
                             # floor (so 150-400 Hz must then stay 50 dB under the sub's peak; a near-silent window
                             # between notes can't read as mud, a ring with no sub under it still does)
GAP_END_S = 0.020            # 2-step gaps: vs sub stops this long before the next hit, whose onset the zero-phase bands
                             # smear a few ms ahead (the before-hit check covers 40-15 ms)
MUD_SHARE_DB = -12.0         # 150-400 Hz share of the whole sound
BOX_FALL_MS = 150.0          # 400 Hz-1 kHz falls 30 dB within this
LOOP_BEFORE_HIT_DB = -30.0   # 2-step loop: 150-400 Hz 40-15 ms before each hit, re the loop's 150-400 Hz peak
TRANSCRIPT_MIN = 0.8         # share of script words heard, minus 0.3 x the share of extra words
LOUDNESS = {'texture': (-20.0, 1.0), 'riser': (-14.0, 2.0), 'wet': (-16.0, 1.0), 'dry': (-18.0, 1.0)}  # LUFS-I, +-

CATEGORIES = ('kick', 'tom', 'snare', 'hat', 'perc', 'bass', 'pad', 'stab', 'lead', 'vox', 'fx', 'texture', 'loop')
FLAGS = ('deliberate_alias', 'explicit', 'hf_synthetic', 'cut_end')
# per category: low-end rule ('gate' = mud gate, 'lowcut' = nothing under ~100 Hz, 'off'), alias check on the sustain,
# tuning tolerance in cents when the sound declares a pitch (None = not checked), and the 2-step loop check (one-shots
# only). The 2-step boom is a 'kick'. Toms get the kick's rules until S1c decides theirs (ADR 0009).
CATEGORY_RULES = {
    'kick': dict(mud='gate', tonal=False, tune_cents=10.0, loop_check=True),
    'tom': dict(mud='gate', tonal=False, tune_cents=10.0, loop_check=True),
    'snare': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False),
    'hat': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False),
    'perc': dict(mud='lowcut', tonal=False, tune_cents=3.0, loop_check=False),
    'bass': dict(mud='gate', tonal=True, tune_cents=3.0, loop_check=True),
    'pad': dict(mud='lowcut', tonal=True, tune_cents=3.0, loop_check=False),
    'stab': dict(mud='lowcut', tonal=True, tune_cents=3.0, loop_check=False),
    'lead': dict(mud='lowcut', tonal=True, tune_cents=3.0, loop_check=False),
    'vox': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False),
    'fx': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False),
    'texture': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False),
    'loop': dict(mud='off', tonal=False, tune_cents=None, loop_check=False),
}
TWO_STEP = ((0, 1.0), (10, 0.7), (16, 1.0), (23, 0.4), (26, 0.7))  # (16th, velocity), 2 bars: the kick judge's loop


@dataclass(frozen=True)
class Spec:
    """What QA needs to know about one sound. None means: the category's default."""
    category: str
    oneshot: bool = True              # one-shots get the tail check; loops get the seam check instead
    f0: float = None                  # the pitch it must be tuned to, Hz
    f0s: tuple = ()                   # fundamentals present, for the alias check (default: f0)
    tune_cents: float = None
    tune_window: tuple = None         # seconds; default 0.10-0.25 for kicks, 0.10-0.60 otherwise
    tonal: bool = None
    sustain: tuple = None             # alias-check window, seconds; default 0.10 s to 50 ms before the end (max 1.1 s)
    harm_tol: float = 0.005           # alias check: a bin counts as harmonic within this fraction of k f0
    mud: str = None                   # 'gate', 'lowcut' or 'off'
    flags: frozenset = frozenset()
    tags: tuple = ()
    splices: tuple = ()               # sample positions the recipe spliced
    loop: tuple = None                # (start, end), end exclusive; loops default to the whole file
    hits: tuple = ()                  # kick onsets in a loop that loops from sample 0: the mud gate runs per gap too

    def rule(self, key):
        own = getattr(self, key)
        return CATEGORY_RULES[self.category][key] if own is None else own


@dataclass
class Check:
    name: str
    ok: object                        # True, False, or None (not applicable)
    value: object = None
    limit: str = ''
    detail: str = ''

    def __post_init__(self):
        if self.ok is not None:
            self.ok = bool(self.ok)

    @property
    def status(self):
        return 'n/a' if self.ok is None else ('pass' if self.ok else 'fail')

    def as_dict(self):
        v = self.value
        if isinstance(v, (float, np.floating)):
            v = None if not np.isfinite(v) else round(float(v), 2)
        return {'name': self.name, 'status': self.status, 'value': v, 'limit': self.limit, 'detail': self.detail}


def na(name, why):
    return Check(name, None, None, '', why)


def failed(checks):
    return [c for c in checks if c.ok is False]


# ------------------------------------------------------------------ measures
# ITU-R BS.1770 K-weighting at 48 kHz (stage 1 shelf, stage 2 RLB high-pass)
K1 = ([1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585])
K2 = ([1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621])


def _momentary(x):
    """Mean-square per 400 ms block (100 ms hop) of the K-weighted signal. Mono files count as L = R stereo, which is
    how the engine plays them. Sounds shorter than 400 ms are one zero-padded block."""
    y = signal.lfilter(*K2, signal.lfilter(*K1, D.st(x), axis=0), axis=0)
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    if len(y) < blk:
        y = np.concatenate([y, np.zeros((blk - len(y), 2))])
    return np.array([np.mean(np.sum(y[s:s + blk] ** 2, 1)) for s in range(0, len(y) - blk + 1, hop)]) + 1e-20


def lufs(x):
    """Integrated loudness, LUFS (BS.1770 gating: -70 LUFS absolute, -10 LU relative)."""
    z = _momentary(x)
    lv = -0.691 + 10 * np.log10(z)
    g = z[lv > -70]
    if not len(g):
        return -70.0
    rel = -0.691 + 10 * np.log10(g.mean()) - 10
    return float(-0.691 + 10 * np.log10(z[(lv > -70) & (lv > rel)].mean()))


def dc_db(x, loop=None):
    """True offset, not partial cycles of a low note: zero-phase 5 Hz low-pass on a zero-padded copy, max over the sound.
    A loop (start, end) is measured as it plays: its loop region repeats instead of stopping dead."""
    x = D.st(x)
    if loop and loop[0] == 0:                       # the whole file loops: measure it circularly
        b = loop[1]
        lp = D.bfilt(np.concatenate([x[:b]] * 3), 5.0, 'lowpass', order=4, zero_phase=True)
        return float(D.db(np.abs(lp[b:2 * b]).max()))
    n = len(x)
    if loop:
        a, b = loop
        x = np.concatenate([x[:b], x[a:b], x[a:b]])
        n = b + (b - a)
    lp = D.bfilt(x, 5.0, 'lowpass', order=4, zero_phase=True)
    return float(D.db(np.abs(lp[:n]).max()))


def first_sample_db(x):
    return float(D.db(np.abs(D.st(x)[0]).max()))


def tail_db(x, ms=10.0):
    return float(D.db(np.abs(D.st(x)[-int(ms / 1000 * SR):]).max()))


def click_at(x, positions, win=0.001, ctx=0.05):
    """HF (>= 6 kHz) energy in a 1 ms window centred on each position against the median 1 ms window of the 50 ms around
    it. Returns the worst excess in dB. Only the places a renderer knows it spliced are checked: generic click finders
    false-flag saw resets and clipped edges."""
    h = D.bfilt(D.mono(x), 6000, 'highpass', order=4) ** 2
    w, c = int(win * SR), int(ctx * SR)
    worst = -np.inf
    for p in positions:
        p = int(p)
        a, b = max(0, p - c), min(len(h), p + c)
        frames = [h[k:k + w].mean() for k in range(a, b - w, w)]
        at = h[max(0, p - w // 2):p + w // 2 + 1].mean()
        worst = max(worst, 10 * np.log10((at + 1e-20) / (np.median(frames) + 1e-20)))
    return float(worst)


def seam_db(x, start=0, end=None, ctx=0.002):
    """Loop seam: the jump from the loop's last sample to its first, against the 95th percentile of the sample-to-sample
    steps in the 2 ms on each side; worst channel. 0 dB means as smooth as the material around it."""
    x = D.st(x)
    end = len(x) if end is None else end
    c = int(ctx * SR)
    worst = -np.inf
    for ch in range(x.shape[1]):
        v = x[:, ch]
        steps = np.concatenate([np.abs(np.diff(v[start:start + c + 1])), np.abs(np.diff(v[end - 1 - c:end]))])
        worst = max(worst, D.db(v[start] - v[end - 1]) - D.db(np.percentile(steps, 95)))
    return float(worst)


def stereo_corr(x):
    x = D.st(x)
    if x[:, 0].std() == 0 or x[:, 1].std() == 0:
        return 1.0
    return float(np.corrcoef(x[:, 0], x[:, 1])[0, 1])


def low_side_db(x, fc=120.0):
    """Side against mid under fc (FFT bins, so no filter skirt leaks in from above), dB; None when there is nothing down
    there (under -30 dB of the sound)."""
    x = D.st(x)
    M, S = D.ms(x[:, 0], x[:, 1])
    lo = np.fft.rfftfreq(len(M), 1 / SR) < fc
    sm = np.sum(np.abs(np.fft.rfft(M)[lo]) ** 2) + 1e-20
    ss = np.sum(np.abs(np.fft.rfft(S)[lo]) ** 2) + 1e-20
    total = np.sum(np.abs(np.fft.rfft(M)) ** 2) + np.sum(np.abs(np.fft.rfft(S)) ** 2) + 1e-20
    if 10 * np.log10((sm + ss) / total) < -30:
        return None
    return float(10 * np.log10(ss / sm))


def harmonic_asr(x, f0s, t0, t1, tol=0.005):
    """Energy between the harmonics of the fundamentals f0s (aliases, noise) against energy on them, dB, over t0-t1 s.
    A bin is harmonic within max(8 bins, tol * k f0) of k f0, capped at 0.45 f0. For clean tonal sounds only."""
    m = D.mono(x)[int(t0 * SR):int(t1 * SR)]
    n = len(m)
    Y = np.abs(np.fft.rfft(m * signal.windows.blackmanharris(n))) ** 2
    f = np.fft.rfftfreq(n, 1 / SR)
    band = (f > 0.5 * min(f0s)) & (f < 20000)
    harm = np.zeros(len(f), dtype=bool)
    for f0 in f0s:
        for k in range(1, int(24000 // f0) + 1):
            harm |= np.abs(f - k * f0) <= min(max(8 * SR / n, tol * k * f0), 0.45 * f0)
    return float(10 * np.log10(Y[band & ~harm].sum() / (Y[band & harm].sum() + 1e-30) + 1e-30))


def pitch_hz(x, f0, t0, t1):
    """Mean frequency of the fundamental near f0 over t0-t1 s: zero-phase band f0/1.3..1.3 f0, then the slope of the
    analytic phase (amplitude-weighted fit, 15% trimmed at each edge). Returns (Hz, the band's share of the window, dB)."""
    m = D.mono(x)
    a, b = int(t0 * SR), min(len(m), int(t1 * SR))
    lo = D.bfilt(m, f0 / 1.3, 'highpass', order=4, zero_phase=True)
    w = D.bfilt(lo, f0 * 1.3, 'lowpass', order=4, zero_phase=True)[a:b]
    share = 10 * np.log10(np.sum(w ** 2) / (np.sum(m[a:b] ** 2) + 1e-20) + 1e-20)
    z = signal.hilbert(w)
    e = int(0.15 * len(z))
    z = z[e:len(z) - e]
    t = np.arange(len(z)) / SR
    slope = np.polyfit(t, np.unwrap(np.angle(z)), 1, w=np.abs(z) + 1e-12)[0]
    return float(slope / (2 * np.pi)), float(share)


def band_zp(x, lo, hi):
    return D.bfilt(D.mono(x), (lo, hi), 'bandpass', order=4, zero_phase=True)


def env_db(b, win=0.005):
    n = max(1, int(win * SR))
    return 20 * np.log10(np.sqrt(np.convolve(b ** 2, np.ones(n) / n, mode='same')) + 1e-12)


def window_energy(b, win, hop):
    """Energy of b in windows of `win` samples every `hop` (one window if b is shorter)."""
    if len(b) <= win:
        return np.array([np.sum(b ** 2)])
    c = np.concatenate([[0.0], np.cumsum(b ** 2)])
    s = np.arange(0, len(b) - win + 1, hop)
    return c[s + win] - c[s]


def vs_sub_db(mid, sub, sub_peak):
    """The worst 20 ms window (10 ms hop) of 150-400 Hz against 30-120 Hz, dB. A window where the sub has faded more
    than 30 dB under sub_peak (its loudest window energy) is compared with that floor instead, so a near-silent stretch
    can't read as mud and a ring with no sub under it still does."""
    win, hop = (int(v * SR) for v in MUD_WINDOW)
    em, es = window_energy(mid, win, hop), window_energy(sub, win, hop)
    floor = sub_peak * 10 ** (SUB_FLOOR_DB / 10)
    return float(np.max(10 * np.log10((em + 1e-30) / np.maximum(es, floor + 1e-30))))


def peak_window(b):
    win, hop = (int(v * SR) for v in MUD_WINDOW)
    return float(window_energy(b, win, hop).max())


def fall_ms(b, drop=30.0):
    """Time from the band's envelope peak until it stays `drop` dB under it; inf if it never gets there."""
    e = env_db(b)
    k = int(np.argmax(e))
    thr = e[k] - drop
    if e[-1] >= thr:
        return float('inf')
    return 1000.0 * (np.flatnonzero(e[k:] >= thr)[-1] + 1) / SR


def mud(x):
    """The mud measures of one hit (onset at sample 0)."""
    m = D.mono(x)
    total = np.sum(m ** 2) + 1e-20
    mid, sub, box = band_zp(m, 150, 400), band_zp(m, 30, 120), band_zp(m, 400, 1000)
    t80 = int(0.08 * SR)
    return {
        'fall_150_400_ms': fall_ms(mid),
        'vs_sub_db': vs_sub_db(mid[t80:], sub[t80:], peak_window(sub)),
        'share_db': float(10 * np.log10(np.sum(mid ** 2) / total + 1e-20)),
        'fall_400_1k_ms': fall_ms(box),
    }


def low_share_db(x, fc=None):
    """Share of the sound under fc (default LOWCUT_HZ), dB: 4th-order Butterworth, zero-phase."""
    m = D.mono(x)
    fc = LOWCUT_HZ if fc is None else fc
    return float(10 * np.log10(np.sum(D.bfilt(m, fc, 'lowpass', order=4, zero_phase=True) ** 2)
                               / (np.sum(m ** 2) + 1e-20) + 1e-20))


def hf_band_db(x):
    """Energy at 14-20 kHz against 6-11 kHz (FFT bins), dB. A vocal resynthesised at 48 kHz with extend_hf has a top
    band; a 24 kHz take resampled to 48 kHz has almost nothing above 12 kHz."""
    m = D.mono(x)
    P = np.abs(np.fft.rfft(m)) ** 2
    f = np.fft.rfftfreq(len(m), 1 / SR)
    top, ref = P[(f >= 14000) & (f < 20000)].sum(), P[(f >= 6000) & (f < 11000)].sum()
    return float(10 * np.log10((top + 1e-30) / (ref + 1e-30)))


def centroid_hz(x):
    P = np.abs(np.fft.rfft(D.mono(x))) ** 2
    f = np.fft.rfftfreq(len(D.mono(x)), 1 / SR)
    return float(np.sum(f * P) / (np.sum(P) + 1e-30))


def two_step_loop(layers, bpm=134.0, hits=TWO_STEP, bars=2):
    """The 2-bar 2-step loop the mud gate also runs on: each hit takes the layer nearest its velocity, all hits sum (no
    choke) and tails wrap to the start, as a looping clip plays. layers: {velocity: audio}. Returns (loop, onsets)."""
    s16 = 60.0 / bpm / 4 * SR
    L = int(round(16 * bars * s16))
    loop = np.zeros((L, 2))
    onsets = []
    for step, v in hits:
        y = D.st(layers[min(layers, key=lambda k: (abs(k - v), -k))])
        p = int(round(step * s16))
        np.add.at(loop, (p + np.arange(len(y))) % L, y)
        onsets.append(p)
    return loop, onsets


def loop_mud(loop, onsets):
    """Per gap between hits: 150-400 against 30-120 Hz from 80 ms after the hit to 20 ms before the next one (the
    worst window, vs_sub_db), and the 150-400 level 40-15 ms before the next hit against the loop's 150-400 peak. Bands
    are filtered circularly (the loop wraps)."""
    m = D.mono(loop)
    L = len(m)
    mid = band_zp(np.tile(m, 3), 150, 400)[L:2 * L]
    sub = band_zp(np.tile(m, 3), 30, 120)[L:2 * L]
    peak = env_db(np.tile(mid, 3))[L:2 * L].max()
    sub_peak = peak_window(sub)
    rows = []
    for i, p in enumerate(onsets):
        nxt = onsets[i + 1] if i + 1 < len(onsets) else onsets[0] + L
        a, b = p + int(0.08 * SR), max(p + int(0.08 * SR) + 1, nxt - int(GAP_END_S * SR))
        idx = np.arange(a, b) % L
        pre = np.arange(nxt - int(0.040 * SR), nxt - int(0.015 * SR)) % L
        rows.append({
            'vs_sub_db': vs_sub_db(mid[idx], sub[idx], sub_peak),
            'before_hit_db': float(10 * np.log10(np.mean(mid[pre] ** 2) + 1e-20) - peak),
        })
    return rows


# ------------------------------------------------------------------ per-file checks
def _le(name, value, limit, unit, detail=''):
    return Check(name, bool(value <= limit), value, f'<= {limit:g} {unit}'.strip(), detail)


def _check_format(x, fmt):
    out = []
    ok = fmt['samplerate'] == SR and fmt['subtype'] == 'PCM_24' and fmt['format'] == 'FLAC'
    out.append(Check('format', ok, f"{fmt['format']} {fmt['subtype']} {fmt['samplerate']} Hz", 'FLAC PCM_24 48000 Hz'))
    same = x.ndim == 2 and x.shape[1] == 2 and np.array_equal(x[:, 0], x[:, 1])
    out.append(Check('mono_storage', not same, f"{fmt['channels']} ch", 'mono where L = R',
                     'stereo file with identical channels' if same else ''))
    return out


def _check_tuning(x, spec, dur):
    tol = spec.rule('tune_cents')
    if spec.f0 is None or tol is None:
        return na('tuning', 'no pitch declared' if spec.f0 is None else 'not checked for this category')
    t0, t1 = spec.tune_window or ((0.10, 0.25) if spec.category in ('kick', 'tom') else (0.10, 0.60))
    t1 = min(t1, dur)
    if t1 - t0 < 0.05:
        return na('tuning', 'sound too short for the tuning window')
    hz, share = pitch_hz(x, spec.f0, t0, t1)
    if share < -30:
        return Check('tuning', False, None, f'+-{tol:g} cents',
                     f'no fundamental near {spec.f0:.2f} Hz ({share:.0f} dB)')
    c = float(D.cents(hz, spec.f0))
    return Check('tuning', bool(abs(c) <= tol), c, f'+-{tol:g} cents', f'{hz:.2f} Hz over {t0:g}-{t1:g} s')


def _check_alias(x, spec, dur):
    if not spec.rule('tonal'):
        return na('alias', 'not a tonal sustain')
    if 'deliberate_alias' in spec.flags:
        return na('alias', 'deliberate_alias')
    f0s = spec.f0s or ((spec.f0,) if spec.f0 else ())
    if not f0s:
        return na('alias', 'no pitch declared')
    t0, t1 = spec.sustain or (0.10, min(dur - 0.05, 1.1))
    if t1 - t0 < 0.2:
        return na('alias', 'no sustain of 0.2 s')
    return _le('alias', harmonic_asr(x, f0s, t0, t1, spec.harm_tol), ALIAS_MAX_DB, 'dB', f'{t0:g}-{t1:g} s')


def _check_hf_band(x, spec):
    if 'hf_synthetic' not in spec.flags:
        return na('hf_band', 'not hf_synthetic')
    v = hf_band_db(x)
    return Check('hf_band', bool(v >= HF_BAND_MIN_DB), v, f'>= {HF_BAND_MIN_DB:g} dB',
                 '14-20 kHz against 6-11 kHz: a synthetic top band, not a resample (D4)')


def _check_low_end(x, spec):
    mode = spec.rule('mud')
    if mode == 'off':
        return [na('low_end', 'mud rule off')]
    if mode == 'lowcut':
        return [_le('lowcut_share_below_100hz', low_share_db(x), LOWCUT_MAX_DB, 'dB',
                    'nothing under ~100 Hz may compete with the kick and sub')]
    m = mud(x)
    sustained = not spec.oneshot
    out = [na('mud_150_400_fall', 'sustained sound: no decay') if sustained else
           _le('mud_150_400_fall', m['fall_150_400_ms'], MUD_FALL_MS, 'ms', 'falls 30 dB from its peak'),
           na('mud_vs_sub_after_80ms', 'a loop with kicks: checked in every gap (each kick owns its first 80 ms)')
           if spec.hits else _le('mud_vs_sub_after_80ms', m['vs_sub_db'], MUD_VS_SUB_DB, 'dB',
                                 '150-400 Hz against 30-120 Hz, worst 20 ms window'),
           _le('mud_share', m['share_db'], MUD_SHARE_DB, 'dB', '150-400 Hz share of the sound'),
           na('box_400_1k_fall', 'sustained sound: no decay') if sustained else
           _le('box_400_1k_fall', m['fall_400_1k_ms'], BOX_FALL_MS, 'ms', 'falls 30 dB from its peak')]
    if spec.hits:
        rows = loop_mud(x, sorted(spec.hits))
        out += [_le('loop_mud_vs_sub', max(r['vs_sub_db'] for r in rows), MUD_VS_SUB_DB, 'dB',
                    f'worst of {len(rows)} gaps, from 80 ms after each kick'),
                _le('loop_mud_before_hit', max(r['before_hit_db'] for r in rows), LOOP_BEFORE_HIT_DB, 'dB',
                    'worst 40-15 ms before a kick, re the 150-400 Hz peak')]
    return out


def _check_loudness(x, spec):
    key = 'texture' if spec.category == 'texture' else None
    if spec.category == 'fx' and 'riser' in spec.tags:
        key = 'riser'
    if spec.category == 'vox':
        key = 'wet' if 'wet' in spec.tags else ('dry' if 'dry' in spec.tags else None)
    if key is None:
        return [na('loudness', 'no target for this category')]
    target, tol = LOUDNESS[key]
    v = lufs(x)
    out = [Check('loudness', bool(abs(v - target) <= tol), v, f'{target:g} +-{tol:g} LUFS', key)]
    if key == 'riser':
        q = [10 * np.log10(np.mean(z)) for z in np.array_split(_momentary(x), 4)]
        ok = all(b >= a - 1.0 for a, b in zip(q, q[1:])) and q[-1] - q[0] >= 6.0
        out.append(Check('rising', ok, q[-1] - q[0], '>= 6 dB, no quarter 1 dB under the one before',
                         'quarters: ' + ' '.join(f'{v:.1f}' for v in q)))
    return out


def check_take(x, spec, fmt=None):
    """Every per-file check on one sound (float, (n,) or (n, 2), at 48 kHz). fmt: the file's soundfile info, as a dict."""
    x = np.asarray(x, dtype=float)
    dur = len(x) / SR
    out = _check_format(x, fmt) if fmt else []
    out.append(_le('true_peak', D.true_peak_db(x), CEILING_DBTP, 'dBTP', '4x oversampled'))
    loop = spec.loop or (None if spec.oneshot else (0, len(x)))
    out.append(_le('dc', dc_db(x, loop), DC_MAX_DB, 'dBFS', '5 Hz zero-phase low-pass, max'))
    if loop and loop[0] == 0:
        out.append(na('first_sample', 'loops from sample 0'))
    else:
        out.append(_le('first_sample', first_sample_db(x), FIRST_MAX_DB, 'dBFS'))
    if not spec.oneshot:
        out.append(na('tail', 'loop'))
    elif 'cut_end' in spec.flags:
        out.append(na('tail', 'cut_end'))
    else:
        out.append(_le('tail', tail_db(x), TAIL_MAX_DB, 'dBFS', 'peak of the last 10 ms'))
    out.append(_le('splices', click_at(x, spec.splices), SPLICE_MAX_DB, 'dB', f'{len(spec.splices)} splices')
               if spec.splices else na('splices', 'none logged'))
    out.append(_le('seam', seam_db(x, *loop), SEAM_MAX_DB, 'dB', f'loop {loop[0]}-{loop[1]}') if loop
               else na('seam', 'one-shot'))
    out.append(_check_alias(x, spec, dur))
    c = stereo_corr(x)
    out.append(Check('stereo_corr', bool(c >= CORR_MIN), c, f'>= {CORR_MIN:g}'))
    side = low_side_db(x)
    out.append(na('low_side', 'no low end') if side is None else
               _le('low_side', side, LOW_SIDE_MAX_DB, 'dB', 'side against mid under 120 Hz'))
    out.append(_check_tuning(x, spec, dur))
    out.append(_check_hf_band(x, spec))
    out += _check_low_end(x, spec)
    out += _check_loudness(x, spec)
    return out


def file_format(path):
    i = sf.info(path)
    return {'samplerate': i.samplerate, 'subtype': i.subtype, 'format': i.format, 'channels': i.channels}


def check_file(path, spec):
    x, _ = sf.read(path, dtype='float64')
    return check_take(x, spec, file_format(path))


# ------------------------------------------------------------------ per-instrument checks
def check_layers(rows):
    """Velocity layers and round robins. rows: dicts with layer, rr, note and audio x."""
    out = []
    loud = {(r['note'], r['layer'], r['rr']): lufs(r['x']) for r in rows}
    cent = {(r['note'], r['layer'], r['rr']): centroid_hz(r['x']) for r in rows}
    layers = sorted({r['layer'] for r in rows})
    rrs = sorted({r['rr'] for r in rows})
    notes = sorted({r['note'] for r in rows}, key=lambda n: -1 if n is None else n)
    if len(layers) < 2:
        out += [na('velocity_step', 'one layer'), na('velocity_brighter', 'one layer')]
    else:
        steps, rising = [], True
        for note in notes:
            for rr in rrs:
                lv = [loud[(note, la, rr)] for la in layers]
                cv = [cent[(note, la, rr)] for la in layers]
                steps += list(np.diff(lv))
                rising &= all(b > a for a, b in zip(cv, cv[1:]))
        out.append(Check('velocity_step', bool(min(steps) >= VEL_STEP_MIN_DB), float(min(steps)),
                         f'>= {VEL_STEP_MIN_DB:g} dB per layer', 'smallest step, LUFS-I'))
        out.append(Check('velocity_brighter', bool(rising), None, 'centroid rises with each layer'))
    if len(rrs) < 2:
        out.append(na('rr_spread', 'one round robin'))
    else:
        dev = max(abs(loud[(n, la, rr)] - np.mean([loud[(n, la, r)] for r in rrs]))
                  for n in notes for la in layers for rr in rrs)
        out.append(Check('rr_spread', bool(dev <= RR_SPREAD_MAX_DB), float(dev), f'<= +-{RR_SPREAD_MAX_DB:g} dB',
                         'largest round-robin deviation from its layer mean'))
    return out


def check_loop(layers, bpm=134.0):
    """The mud gate on the 2-bar 2-step loop. layers: {velocity: audio}. Returns (checks, loop audio)."""
    loop, onsets = two_step_loop(layers, bpm)
    rows = loop_mud(loop, onsets)
    worst_sub = max(r['vs_sub_db'] for r in rows)
    worst_pre = max(r['before_hit_db'] for r in rows)
    return [_le('loop_mud_vs_sub', worst_sub, MUD_VS_SUB_DB, 'dB', 'worst gap, from 80 ms after each hit'),
            _le('loop_mud_before_hit', worst_pre, LOOP_BEFORE_HIT_DB, 'dB',
                'worst 40-15 ms before a hit, re the 150-400 Hz peak')], loop


def check_gains(gains_db):
    """One gain per instrument, never per file."""
    g = sorted(set(gains_db))
    return Check('one_gain', len(g) == 1, g[0] if len(g) == 1 else None, 'one gain for every layer and RR',
                 '' if len(g) == 1 else f'{len(g)} different gains')


def check_content(names, strings=(), script=None, flags=frozenset(), tags=(), category='', lists=None):
    """Denylists on every name and string (artist names, slurs), the vocal rating and flags. lists: {list name: digests},
    default the shipped ones (tools/sound/denylists)."""
    lists = lists or {k: denylist.load(k) for k in denylist.LISTS}
    texts = list(names) + list(strings) + ([script] if script else [])
    out = []
    for key, label in (('artists', 'artist_names'), ('slurs', 'slurs')):
        hits = sorted({h for t in texts for h in denylist.hits(t, lists[key])})
        detail = ', '.join(hits) if key == 'artists' else ('not repeated in the report' if hits else '')
        out.append(Check(label, not hits, len(hits), 'none', detail))
    if category != 'vox' or not script:
        out += [na('rating', 'no vocal script'), na('clean_words', 'no vocal script'),
                na('hf_synthetic', 'no vocal script')]
        return out
    explicit, clean = 'explicit' in flags, 'clean' in tags
    out.append(Check('rating', explicit != clean, 'explicit' if explicit else ('clean' if clean else 'unset'),
                     'exactly one of the explicit flag or the clean tag'))
    bad = denylist.hits(script, lists['profanity'])
    out.append(na('clean_words', 'explicit') if explicit else
               Check('clean_words', not bad, len(bad), 'no profanity in a clean line', ', '.join(bad)))
    out.append(Check('hf_synthetic', 'hf_synthetic' in flags, None, 'set on TTS-derived masters (D4)'))
    return out


def words(s):
    return re.sub(r"[^a-z' ]", ' ', s.lower().replace('-', ' ')).replace("'", '').split()


def transcript_score(script, heard):
    """Share of the script's words heard, minus 0.3 x the share of heard words that don't belong (legacy vocals2.score)."""
    want = set(words(script))
    got = words(heard)
    if not want:
        return 0.0
    hit = sum(1 for w in want if w in got) / len(want)
    return hit - 0.3 * sum(1 for g in got if g not in want) / max(1, len(got))


def check_transcript(x, script, asr=None, lists=None):
    """Whisper on a dry vocal file: the transcript must match the script and carry no slur. Wet versions fail ASR by
    design and are not checked. asr(x, sr) -> text; default the Whisper model from WUBANSIA_MODELS_DIR."""
    if asr is None:
        from tools.sound import models
        asr = models.asr
    heard = asr(D.mono(x), SR)
    slurs = denylist.hits(heard, (lists or {}).get('slurs') or denylist.load('slurs'))
    score = transcript_score(script, heard)
    ok = score >= TRANSCRIPT_MIN and not slurs
    detail = '(a slur was heard)' if slurs else f'heard "{heard}"'
    return Check('transcript', bool(ok), score, f'>= {TRANSCRIPT_MIN:g}', detail)
