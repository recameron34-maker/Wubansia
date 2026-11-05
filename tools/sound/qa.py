"""Sample QA for 48 kHz masters: plan section 5.2, with the mud gate.

Every check returns a Check: a name, ok (True, False, or None when it doesn't apply), the measured value, the limit and
a short detail. check_take() runs the per-file checks on one sound; check_layers(), check_loop(), check_gains(),
check_content() and check_transcript() run the per-instrument ones. A sound declares its category (and overrides) in a
Spec; the category picks the rules: CATEGORY_RULES below. ADR 0011 (proposed) adds two: the kick crest floor and the
bass-body mode of the low-end rule. ADR 0014 (proposed) adds the checks of a mastered loop, tag 'club', against its
genre's targets (CLUB) and against what its master did (the cap, the hits' crest), and the crest floor on every
full-velocity kick a loop declares (top_hits).

Provenance: lufs, dc_db, click_at, stereo and harmonic_asr are ported from the S1 research prototype (qa.py), the mud
measures from the kick mud meter (mud.py) and the 2-step loop from the kick judge (judge_loops.py, loopcheck.py), all
written during planning on 2026-10-29. Method of the mud measures, as in docs/listening.md round 0: 4th-order
Butterworth band-pass, zero-phase; 5 ms RMS envelope; a fall is counted from the band's own peak, until it stays down.
"""
import re
from dataclasses import dataclass, field

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
KICK_CREST_MIN_DB = 4.0      # kicks, the top layer: true peak at least this over the loudest 50 ms (RMS). A sine reads
                             # 3.01; KickSplit 5.8-6.4, and 3.5 driven 6 dB into a limiter; KickTight 7.9-8.9, and
                             # 4.2-5.0 at 6 dB, 3.1-3.5 at 9 dB: a spikier kick fails only when limited harder
KICK_CREST_WINDOW = 0.050
KICK_OWN_S = 0.080           # each kick owns its first 80 ms: the gates start after it, the bass-body duck covers it
BEFORE_HIT = (0.040, 0.015)  # the before-hit window: from 40 to 15 ms before each hit, seconds
BODY_VS_SUB_DB = -2.0        # bass-body: 150-400 Hz against 30-120 Hz in every 20 ms window (a plain saw on F#2 reads
                             # -3.3, on F#1 -8.0; low mids outlasting the sub read +6)
BODY_OVER_SAW_DB = 5.0       # bass-body: no partial in 150-400 Hz more than this over a saw's roll-off (-6 dB per
                             # octave) from the sound's own partials in 30-120 Hz (a saw reads 0, a resonant low-pass
                             # 3-3.5, a +9 dB low-mid bump 6.2)
BODY_SHARE_DB = -5.0         # bass-body: 150-400 Hz share of the sound (a plain saw on G2 reads -6.3)
BODY_DUCK_DB = -4.0          # bass-body phrases with kick hits: 150-400 Hz over each kick's first 80 ms against the
                             # bass's own level in the gap after it, mean power both sides. A steady 6 dB duck reads
                             # -4.7 to -5.5 on F#1-G2 saws, 3 dB about -2, no duck 0 to +1, resting under the kick
                             # -26; a sidechain needs about 8 dB
DUCK_FLOOR_DB = -30.0        # body_duck: a gap more than this under the phrase's loudest gap counts at that floor, so a
                             # bass that sits out a kick and the gap after it isn't read against silence
MUD_MODES = ('gate', 'bass-body', 'lowcut', 'off')
STRICT = ('kick', 'tom')     # these keep the strict gate: 'bass-body', 'lowcut' and 'off' are refused (item.py)
BODY_CATEGORIES = ('bass', 'loop')   # the categories bass-body may be declared on, with a reason (item.py)
LOUDNESS = {'texture': (-20.0, 1.0), 'riser': (-14.0, 2.0), 'wet': (-16.0, 1.0), 'dry': (-18.0, 1.0)}  # LUFS-I, +-
# The mastered 'club' loop's targets per genre (a kit's DIRECTION), from the modern-sound standard
# (docs/plans/S1-modern-sound.md sections 1.1, 1.2 and 1.4; ADR 0014, proposed), every one read round the loop:
# - lufs: LUFS-I, (target, +- LU);
# - plr_min: true peak minus LUFS-I at least this (the target and the -1 dBTP ceiling leave 0.5 dB over it);
# - bands: each octave's window, dB re the 1 kHz octave. 'sub' is the louder of the 31.5 and 63 Hz octaves, read
#   together because the kick's key decides which one holds it (E1, 41 Hz, sits in the 31.5 Hz octave), in the 63 Hz
#   window.
# The loudness targets rest on measured medians of released records; the bands and widths are conventions built on a
# measured chart average (the standard's section 0): S4 replaces them with measured references.
CLUB = {
    'future garage / 2-step': dict(lufs=(-9.0, 1.0), plr_min=7.5, bands={
        'sub': (6.0, 12.0), '125': (2.0, 6.0), '250': (1.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-5.0, -2.0), '8k': (-8.0, -4.0), '16k': (-14.0, -8.0)}),
    'sci-fi club': dict(lufs=(-8.5, 1.0), plr_min=7.0, bands={
        'sub': (6.0, 12.0), '125': (1.0, 6.0), '250': (0.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-5.0, -2.0), '8k': (-8.0, -4.0), '16k': (-14.0, -8.0)}),
    'hyper-clean future house': dict(lufs=(-8.0, 1.0), plr_min=6.5, bands={
        'sub': (6.0, 12.0), '125': (1.0, 5.0), '250': (0.0, 3.0), '500': (0.0, 3.0), '2k': (-2.0, 1.0),
        '4k': (-4.0, -1.0), '8k': (-7.0, -3.0), '16k': (-12.0, -7.0)}),
    'dark industrial / techno': dict(lufs=(-8.0, 1.0), plr_min=6.5, bands={
        'sub': (4.0, 12.0), '125': (1.0, 6.0), '250': (0.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-6.0, -2.0), '8k': (-10.0, -5.0), '16k': (-17.0, -10.0)}),
    # round 4 (FUT03's move to garage-house and rolling deep tech house): warmer and less bright than future house,
    # a little quieter, fuller low mids; conventions, like the others, until measured references replace them
    'garage-house / deep tech house': dict(lufs=(-8.5, 1.0), plr_min=7.0, bands={
        'sub': (6.0, 12.0), '125': (1.0, 6.0), '250': (0.0, 4.0), '500': (0.0, 3.0), '2k': (-3.0, 0.0),
        '4k': (-5.0, -2.0), '8k': (-8.0, -4.0), '16k': (-14.0, -8.0)}),
}
CLUB_LOW_WIDTH_MAX_DB = -10.0  # side over mid in the 125 and 250 Hz octaves (under 120 Hz is the low_side check)
CLUB_MAX_REDUCTION_DB = 4.0    # the master's peak reduction, clipper and limiter together (the standard's cap, C): a
                               # club loop mastered with more declares its cap (qa club_max_reduction_db, with a why)
CLUB_CREST_LOSS_MAX_DB = 1.5   # what the master may cost the loop's hits: the median loss of ADR 0011's crest, hit by
                               # hit, from the master's input to the club loop, the worse of the kicks and the claps
                               # (the A&R review's proposal, to be calibrated by ear); more is declared (qa
                               # club_crest_loss_db, with a why)
CLUB_BANDS = ('sub', '125', '250', '500', '2k', '4k', '8k', '16k')   # every genre's windowed octaves

CATEGORIES = ('kick', 'tom', 'snare', 'hat', 'perc', 'bass', 'pad', 'stab', 'lead', 'vox', 'fx', 'texture', 'loop')
FLAGS = ('deliberate_alias', 'explicit', 'hf_synthetic', 'cut_end')
# per category: low-end rule ('gate' = mud gate, 'bass-body', 'lowcut' = nothing under ~100 Hz, 'off'), alias check on
# the sustain, tuning tolerance in cents when the sound declares a pitch (None = not checked), the groove loop check
# (one-shots only) and the kick crest floor (the top layer). The 2-step boom is a 'kick'. Toms get the kick's rules
# until S1c decides theirs (ADR 0009), but not its crest floor.
CATEGORY_RULES = {
    'kick': dict(mud='gate', tonal=False, tune_cents=10.0, loop_check=True, crest=True),
    'tom': dict(mud='gate', tonal=False, tune_cents=10.0, loop_check=True, crest=False),
    'snare': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False, crest=False),
    'hat': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False, crest=False),
    'perc': dict(mud='lowcut', tonal=False, tune_cents=3.0, loop_check=False, crest=False),
    'bass': dict(mud='gate', tonal=True, tune_cents=3.0, loop_check=True, crest=False),
    'pad': dict(mud='lowcut', tonal=True, tune_cents=3.0, loop_check=False, crest=False),
    'stab': dict(mud='lowcut', tonal=True, tune_cents=3.0, loop_check=False, crest=False),
    'lead': dict(mud='lowcut', tonal=True, tune_cents=3.0, loop_check=False, crest=False),
    'vox': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False, crest=False),
    'fx': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False, crest=False),
    'texture': dict(mud='lowcut', tonal=False, tune_cents=None, loop_check=False, crest=False),
    'loop': dict(mud='off', tonal=False, tune_cents=None, loop_check=False, crest=False),
}
# the kick patterns the groove loop check plays a kit's kicks and bass shots in (its GROOVE): (16th, velocity), 2 bars
TWO_STEP = ((0, 1.0), (10, 0.7), (16, 1.0), (23, 0.4), (26, 0.7))  # the kick judge's loop: 3 16ths is its tightest gap
FOUR_FLOOR = tuple((4 * k, 1.0 if k % 2 == 0 else 0.8) for k in range(8))  # every beat, the off-beats a little softer
GROOVES = {'2step': TWO_STEP, '4x4': FOUR_FLOOR}


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
    mud: str = None                   # 'gate', 'bass-body', 'lowcut' or 'off'
    crest: bool = None                # the kick crest floor (kicks: on)
    top: bool = True                  # the instrument's hardest velocity layer: the crest floor checks it only
    flags: frozenset = frozenset()
    tags: tuple = ()
    splices: tuple = ()               # sample positions the recipe spliced
    loop: tuple = None                # (start, end), end exclusive; loops default to the whole file
    hits: tuple = ()                  # kick onsets in a loop that loops from sample 0: the mud gate runs per gap too;
                                      # in a bass-body phrase, the kicks it plays against (tag 'with_kick' when the
                                      # kicks are in the file)
    genre: str = None                 # a 'club' loop's genre, the kit's DIRECTION: its targets in CLUB
    club_bands: object = None         # a 'club' loop's octave windows: None (all), False (off) or the names of the
                                      # bands exempted (an override, with its why); the rest stay gated
    club_max_reduction_db: float = None   # a 'club' loop's declared cap on peak reduction (CLUB_MAX_REDUCTION_DB)
    club_crest_loss_db: float = None      # a 'club' loop's declared crest loss (CLUB_CREST_LOSS_MAX_DB)
    master: dict = field(default=None, compare=False)   # a 'club' loop's master report (master.master)
    top_hits: tuple = ()              # the onsets of a loop's full-velocity kicks: the crest floor reads each one

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


def kick_crest_db(x, peak_db=None):
    """The kick crest: true peak (4x) over the RMS level of the loudest 50 ms of the mono sum, dB. A limiter or a
    clipper pulls the peak down onto the body; a sine (whole cycles in the window) reads 3.01 dB. peak_db: x's true
    peak read by the caller, for a span cut from a loop (mixmeter.hit_crest_db reads it with the loop round it)."""
    m = D.mono(x)
    w = min(len(m), int(KICK_CREST_WINDOW * SR))
    c = np.concatenate([[0.0], np.cumsum(m ** 2)])
    tp = D.true_peak_db(x) if peak_db is None else float(peak_db)
    return float(tp - 10 * np.log10((c[w:] - c[:-w]).max() / w + 1e-30))


def over_saw_db(x, floor_db=-60.0):
    """Bass-body: the worst spectral peak in 150-400 Hz above a saw's roll-off (-6 dB per octave) from the strongest of
    the sound's own partials in 30-120 Hz, dB. A plain saw reads 0 on any note (its low mids are its own harmonics), a
    filtered one less, a low-mid bump about its height. Long-term spectrum: Welch, Hann, 4x zero-padded so a peak reads
    within 0.1 dB; peaks are local maxima 3 dB proud of their surroundings and within floor_db of the loudest partial
    in 30 Hz-1 kHz. None when nothing sits in 150-400 Hz; inf when nothing sits in 30-120 Hz (low mids with no sub)."""
    m = D.mono(x)
    n = min(2 ** 15, 2 ** int(np.log2(len(m))))
    f, P = signal.welch(m, SR, window='hann', nperseg=n, nfft=4 * n)
    P = 10 * np.log10(P + 1e-30)
    pk, _ = signal.find_peaks(P, prominence=3.0)
    pk = pk[P[pk] > P[(f >= 30) & (f <= 1000)].max() + floor_db]
    subs, lows = pk[(f[pk] >= 30) & (f[pk] <= 120)], pk[(f[pk] >= 150) & (f[pk] <= 400)]
    if not len(lows):
        return None
    if not len(subs):
        return float('inf')
    trend = (P[subs][None, :] - 20 * np.log10(f[lows][:, None] / f[subs][None, :])).max(1)
    return float((P[lows] - trend).max())


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


CHOKE_FADE_S = 0.005         # two_step_loop(choke=True): each hit fades out over this, ending on the next hit


def two_step_loop(layers, bpm, hits, bars=2, choke=False):
    """The 2-bar groove loop the mud gate also runs on, at the kit's BPM in its groove (hits: GROOVES[kit.GROOVE]): each
    hit takes the layer nearest its velocity, all hits sum and tails wrap to the start, as a looping clip plays.
    choke: each hit ends where the next one starts (a 5 ms fade into it), as a mono voice plays a bass shot: a move
    longer than the gap is cut, not summed with the next copy of itself. layers: {velocity: audio}. Returns (loop,
    onsets)."""
    s16 = 60.0 / bpm / 4 * SR
    L = int(round(16 * bars * s16))
    loop = np.zeros((L, 2))
    starts = [int(round(step * s16)) for step, _ in hits]
    onsets = []
    for i, (step, v) in enumerate(hits):
        y = D.st(layers[min(layers, key=lambda k: (abs(k - v), -k))])
        p = starts[i]
        if choke:
            gap = (starts[(i + 1) % len(starts)] - p) % L or L
            y = y[:gap].copy()
            f = min(int(CHOKE_FADE_S * SR), len(y))
            if len(y) == gap and f:
                y[-f:] *= np.linspace(1.0, 0.0, f)[:, None]
        np.add.at(loop, (p + np.arange(len(y))) % L, y)
        onsets.append(p)
    return loop, onsets


def loop_mud(loop, onsets):
    """Per gap between hits, a dict:
    - vs_sub_db: 150-400 against 30-120 Hz in the gap, from 80 ms after the hit to 20 ms before the next, the worst
      window;
    - before_hit_db: the 150-400 level 40-15 ms before the next hit against the loop's 150-400 envelope peak;
    - duck_db, for a bass phrase played against the hits: the mean 150-400 power over the hit's own first 80 ms
      against its mean power over the gap after it, the bass's own level there. A gap more than 30 dB under the
      loudest gap counts at that floor.
    Bands are filtered circularly (the loop wraps)."""
    m = D.mono(loop)
    L = len(m)
    mid = band_zp(np.tile(m, 3), 150, 400)[L:2 * L]
    sub = band_zp(np.tile(m, 3), 30, 120)[L:2 * L]
    peak = env_db(np.tile(mid, 3))[L:2 * L].max()
    sub_peak = peak_window(sub)
    own_n = int(KICK_OWN_S * SR)
    spans = []
    for i, p in enumerate(onsets):
        nxt = onsets[i + 1] if i + 1 < len(onsets) else onsets[0] + L
        gap = np.arange(p + own_n, max(p + own_n + 1, nxt - int(GAP_END_S * SR))) % L
        pre = np.arange(nxt - int(BEFORE_HIT[0] * SR), nxt - int(BEFORE_HIT[1] * SR)) % L
        spans.append((np.arange(p, p + own_n) % L, gap, pre))
    power = lambda idx: float(np.mean(mid[idx] ** 2))
    floor = max(power(gap) for _, gap, _ in spans) * 10 ** (DUCK_FLOOR_DB / 10)
    return [{'vs_sub_db': vs_sub_db(mid[gap], sub[gap], sub_peak),
             'before_hit_db': float(10 * np.log10(power(pre) + 1e-20) - peak),
             'duck_db': float(10 * np.log10((power(own) + 1e-30) / max(power(gap), floor + 1e-30)))}
            for own, gap, pre in spans]


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
    if mode == 'bass-body':
        return _check_body(x, spec)
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


def _check_body(x, spec):
    """The bass-body mode of the low-end rule (ADR 0011, proposed): a bass keeps its own harmonics in 150-400 Hz, up to a
    plain saw's level on its note, but no bump over them, no window where the low mids rival the sub, a capped share,
    and, in a phrase that declares the kicks it plays against, low mids that duck under each kick and clear before the
    next. No falls: a held bass note doesn't decay."""
    m = mud(x)
    out = [na('body_vs_sub', 'a phrase with kicks: checked in every gap') if spec.hits else
           _le('body_vs_sub', m['vs_sub_db'], BODY_VS_SUB_DB, 'dB', '150-400 Hz against 30-120 Hz after 80 ms, worst '
               '20 ms window (a sub that beats or fades fails it too)'),
           _check_over_saw(x),
           _le('body_share', m['share_db'], BODY_SHARE_DB, 'dB', '150-400 Hz share of the sound')]
    if spec.hits:
        rows = loop_mud(x, sorted(spec.hits))
        out += [_le('loop_body_vs_sub', max(r['vs_sub_db'] for r in rows), BODY_VS_SUB_DB, 'dB',
                    f'worst of {len(rows)} gaps, from 80 ms after each kick'),
                _le('loop_mud_before_hit', max(r['before_hit_db'] for r in rows), LOOP_BEFORE_HIT_DB, 'dB',
                    'worst 40-15 ms before a kick, re the 150-400 Hz peak'),
                na('body_duck', 'the kicks are in the file (with_kick)') if 'with_kick' in spec.tags else
                _le('body_duck', max(r['duck_db'] for r in rows), BODY_DUCK_DB, 'dB',
                    "worst kick: 150-400 Hz over its first 80 ms against the gap after it (mean power)")]
    return out


def _check_over_saw(x):
    v = over_saw_db(x)
    if v is None:
        return Check('body_over_saw', True, None, f'<= {BODY_OVER_SAW_DB:g} dB', 'nothing in 150-400 Hz')
    return _le('body_over_saw', v, BODY_OVER_SAW_DB, 'dB', 'no partial in 30-120 Hz under the low mids' if np.isinf(v)
               else "150-400 Hz partials over a saw's roll-off from the sound's own sub")


def _check_crest(x, spec):
    if not spec.rule('crest'):
        return na('kick_crest', 'not a kick' if spec.category != 'kick' else 'off (an override)')
    if not spec.top:
        return na('kick_crest', 'a softer layer: the floor is for the hardest hits')
    v = kick_crest_db(x)
    return Check('kick_crest', bool(v >= KICK_CREST_MIN_DB), v, f'>= {KICK_CREST_MIN_DB:g} dB',
                 'true peak over the loudest 50 ms (RMS), the top layer')


def _check_loop_crest(x, spec):
    """The kick crest floor on every full-velocity kick a loop declares (Spec.top_hits), as the loop plays it: ADR
    0011's measure over 200 ms from each onset (mixmeter.hit_crest_db), so a kick lane clipped flat in the mix fails
    even when the kick's own file passes."""
    if not spec.top_hits:
        return na('loop_kick_crest', 'no full-velocity kicks declared (Out.top_hits)')
    from tools.sound import mixmeter as MM
    v = MM.hit_crest_db(x, sorted(spec.top_hits))
    return Check('loop_kick_crest', bool(min(v) >= KICK_CREST_MIN_DB), min(v), f'>= {KICK_CREST_MIN_DB:g} dB',
                 f'the lowest of {len(v)} full-velocity kicks (median {float(np.median(v)):.2f}): true peak over the '
                 'loudest 50 ms in the 200 ms from each')


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
    out.append(_check_crest(x, spec))
    out.append(_check_loop_crest(x, spec))
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
    out += _check_club(x, spec)
    return out


def _check_club(x, spec):
    """A mastered loop's checks, tag 'club' (ADR 0014, proposed), against its genre's targets (CLUB), every one read
    round the loop as it plays (mixmeter, which builds on this module's measures): LUFS-I in the genre's window, PLR
    over its floor, the true peak at the ceiling round the seam too, every octave in its window, and the low mids'
    width; and from its master's report (Spec.master): the peak reduction within the cap, every full-velocity kick
    over the crest floor, and the crest the master cost the hits. Other items get none of them."""
    if 'club' not in spec.tags:
        return []
    from tools.sound import mixmeter as MM
    t = CLUB.get(spec.genre)
    if t is None:
        return [Check('club_genre', False, spec.genre, f'one of {tuple(CLUB)}',
                      "a club loop is checked against its genre: the kit's DIRECTION")]
    lo = MM.loudness(x)
    target, tol = t['lufs']
    b = MM.bands(x)
    return [Check('club_loudness', bool(abs(lo['lufs_i'] - target) <= tol), lo['lufs_i'], f'{target:g} +-{tol:g} LUFS',
                  f'{spec.genre}: LUFS-I round the loop'),
            Check('club_plr', bool(lo['plr'] >= t['plr_min']), lo['plr'], f">= {t['plr_min']:g} dB",
                  'true peak minus LUFS-I, round the loop'),
            _le('club_true_peak', lo['true_peak'], CEILING_DBTP, 'dBTP', '4x, round the loop: the seam too'),
            na('club_bands', 'off (an override)') if spec.club_bands is False else
            _club_bands(b, t['bands'], spec.club_bands or ()),
            _le('club_low_width', max(b['125']['width'], b['250']['width']), CLUB_LOW_WIDTH_MAX_DB, 'dB',
                f"side over mid, the worse of 125 Hz ({b['125']['width']:.1f}) and 250 Hz ({b['250']['width']:.1f})")
            ] + check_master(spec.master, spec.club_max_reduction_db, spec.club_crest_loss_db)


def _club_bands(b, windows, exempt=()):
    """Every octave in its window, but the bands an override exempts: the value is the worst distance outside one, dB
    (negative: the smallest margin inside); the detail lists them all, the exempted ones as such. 'sub' reads the
    louder of the 31.5 and 63 Hz octaves."""
    worst, rows = -np.inf, []
    for name, (lo, hi) in windows.items():
        if name == 'sub':
            band = '31.5' if b['31.5']['level'] > b['63']['level'] else '63'
            label = f'sub ({band})'
        else:
            band = label = name
        v = b[band]['level']
        d = max(lo - v, v - hi)
        if name in exempt:
            rows.append(f'{label} {v:+.1f} in {lo:+g}..{hi:+g}' + (' (exempt, out)' if d > 0 else ' (exempt)'))
            continue
        worst = max(worst, d)
        rows.append(f'{label} {v:+.1f} in {lo:+g}..{hi:+g}' + (' OUT' if d > 0 else ''))
    if worst == -np.inf:
        return na('club_bands', 'every band exempted (an override)')
    return Check('club_bands', bool(worst <= 0), float(worst), '<= 0 dB outside its window, every octave' +
                 (f" but {', '.join(exempt)} (an override)" if exempt else ''),
                 'octaves re 1 kHz: ' + ', '.join(rows))


def check_master(report, cap_db=None, crest_loss_db=None):
    """What a club loop's master did, from its report (master.master, in the loop's metadata):
    - club_reduction: the cap the master ran with is the standard's CLUB_MAX_REDUCTION_DB, or the cap the item
      declares (qa club_max_reduction_db, an exception listed with its why), and the peak reduction stayed within it;
    - club_kick_crest: every full-velocity kick (its 'kick' hits) keeps ADR 0011's floor in the club loop;
    - club_crest_loss: the median crest the master cost the kicks, and the claps, against its input, at most
      CLUB_CREST_LOSS_MAX_DB or the declared crest_loss_db; the value is the worse of the two.
    The two crest checks need the report's punch section: the master given the loop's hits."""
    if not report:
        return [Check(n, False, None, '', "no master report: a club loop puts master.master()'s report in its "
                                         "metadata ('master')") for n in ('club_reduction', 'club_kick_crest',
                                                                          'club_crest_loss')]
    allowed = CLUB_MAX_REDUCTION_DB if cap_db is None else float(cap_db)
    ran = float(report['settings']['max_reduction_db'])
    pr = float(report['peak_reduction_db'])
    how = 'the standard\'s cap' if cap_db is None else 'the cap the item declares (an override)'
    out = [Check('club_reduction', bool(abs(ran - allowed) < 1e-9 and pr <= allowed + 1e-6), pr,
                 f'<= {allowed:g} dB, {how}',
                 f'the clipper and limiter together; the master ran with a cap of {ran:g} dB' +
                 ('' if abs(ran - allowed) < 1e-9 else f', not the {allowed:g} dB declared: declare the cap it runs '
                                                       'with (qa club_max_reduction_db, with a why)'))]
    punch = report.get('punch') or {}
    kick, clap = punch.get('kick'), punch.get('clap')
    if not kick:
        return out + [Check(n, False, None, '', "the master was given no kicks: master(x, s, hits={'kick': ..., "
                                                "'clap': ...})") for n in ('club_kick_crest', 'club_crest_loss')]
    out.append(Check('club_kick_crest', bool(kick['min_out_db'] >= KICK_CREST_MIN_DB), kick['min_out_db'],
                     f'>= {KICK_CREST_MIN_DB:g} dB',
                     f"the lowest of {len(kick['onsets'])} full-velocity kicks in the club loop (in the master's input "
                     f"{min(kick['in_db']):.2f}): true peak over the loudest 50 ms in the 200 ms from each"))
    limit = CLUB_CREST_LOSS_MAX_DB if crest_loss_db is None else float(crest_loss_db)
    losses = {'kicks': kick['median_loss_db']}
    if clap:
        losses['claps'] = clap['median_loss_db']
    worst = max(losses.values())
    detail = 'median loss of crest, input to club loop: ' + ', '.join(f'{k} {v:.2f}' for k, v in losses.items())
    if clap:
        detail += f" (the claps in {clap['band_hz'][0]:g}-{clap['band_hz'][1]:g} Hz: {clap['band_median_loss_db']:.2f})"
    out.append(Check('club_crest_loss', bool(worst <= limit), worst,
                     f"<= {limit:g} dB" + ('' if crest_loss_db is None else ', declared (an override)'), detail))
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


def check_loop(layers, bpm, hits, mode='gate', choke=False):
    """The mud gate on the 2-bar groove loop at the kit's BPM in its groove (hits: GROOVES[kit.GROOVE]). mode
    'bass-body': its gap limit instead of the gate's (a bass shot). choke: each hit ends on the next (two_step_loop),
    for a bass shot a mono voice plays (qa loop_choke). layers: {velocity: audio}. Returns (checks, loop audio)."""
    loop, onsets = two_step_loop(layers, bpm, hits, choke=choke)
    rows = loop_mud(loop, onsets)
    worst_sub = max(r['vs_sub_db'] for r in rows)
    worst_pre = max(r['before_hit_db'] for r in rows)
    name, limit = ('loop_body_vs_sub', BODY_VS_SUB_DB) if mode == 'bass-body' else ('loop_mud_vs_sub', MUD_VS_SUB_DB)
    return [_le(name, worst_sub, limit, 'dB', 'worst gap, from 80 ms after each hit'),
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
