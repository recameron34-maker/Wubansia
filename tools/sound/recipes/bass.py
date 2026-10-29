"""FUT01 bass for the first listen: the neuro reese (dry multisamples on F#1, A1 and C#2), the same reese talking
through a stand-in of the engine's lane filter, and two growl shots from the resample pass.

The mud gate (plan section 5.2) shapes all of it. Every bass is a clean mono sine sub under 120 Hz plus mids that start
at 450 Hz, high-passed at about 96 dB per octave, so 150-400 Hz stays empty even where the mids are the loud part. (A
wall at 250 Hz, 24 dB per octave, would fill 150-400 Hz: the reese then read -8 to -13 dB against the sub, against the
gate's -20.) Above the wall the reese dips at 650 Hz and snarls at 1.8 and 2.6 kHz, so its weight sits over the boxy
450 Hz-1 kHz zone rather than in it, and its combs ring on the kit's alloy ratios.

The reese loops are exactly periodic: every oscillator sits on a grid of whole cycles per loop (about 2 s, one full turn
of the detune), so the loop joins by itself; a short crossfade only absorbs what's left of the attack. The drive runs on
each saw before the stack is summed. That keeps the stack free of intermodulation, every partial on its own voice's
harmonic series, so the alias check still means something; the grit that intermodulation adds belongs to the engine's
lane drive, which ReeseTalk plays.

Provenance: the reese grows out of the S1 research prototype (recipes.py: neuro, 2026-10-29) and the legacy reese engine
(legacy/web-pack/audio/reese.py: three detuned saws a side, the separate sine sub). The resample chain is the one of
plan section 2.2: growl, reese, multiband, comb/formant, twice.
"""
import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound.item import Item, Out, Take
from tools.sound.kits import fut01 as K

SR = D.SR
MID_HP = 450.0                      # the mids start here; 150-400 Hz stays empty


# ------------------------------------------------------------------ shared pieces (music.py uses them too)
def _t(n):
    return np.arange(n) / SR


def raised(n, length):
    """A raised-cosine rise over `length` seconds, then 1."""
    a = max(1, int(length * SR))
    e = np.ones(n)
    e[:a] = 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    return e


def frac_delay(x, d):
    """x delayed by d samples (fractional, linear interpolation), silent before the start."""
    i = int(np.floor(d))
    fr = d - i
    y = np.zeros_like(x)
    y[i:] = (1 - fr) * x[:len(x) - i]
    y[i + 1:] += fr * x[:len(x) - i - 1]
    return y


def hp_steep(x, fc):
    """Two 8th-order Butterworth high-passes in a row: about 96 dB per octave under fc."""
    return D.bfilt(D.bfilt(x, fc, 'highpass', order=8), fc, 'highpass', order=8)


def close_loop(y, a0, L, xfade=0.02, search=0.02):
    """Loop points for a sound that repeats every L samples from about a0 on: the start is the sample in the next
    `search` seconds with the smallest step into it (no saw edge on the seam), and the loop's last `xfade` seconds fade
    into the samples just before the start, so it also joins when the attack hasn't quite settled. Returns the file
    (attack plus one loop) and the loop points."""
    d = np.abs(np.diff(y[a0 - 1:a0 + int(search * SR)], axis=0)).max(1)
    a = a0 + int(np.argmin(d))
    X = int(xfade * SR)
    w = np.linspace(0.0, 1.0, X)[:, None]
    y = np.array(y[:a + L])
    y[a + L - X:] = (1 - w) * y[a + L - X:] + w * y[a - X:a]
    return y, (a, a + L)


# ------------------------------------------------------------------ the reese
REESE_NOTES = (30, 33, 37)          # F#1, A1, C#2
REESE_LAYERS = (0.6, 1.0)
LOOP_S = 2.0                        # one turn of the detune
ATTACK_S = 0.5                      # attack and settle before the loop
DETUNE = ((-1, 0, 2), (-2, 0, 1))   # grid steps per side (13-19 cents a step); the centre voice is shared
REESE_COMBS = (K.ALLOY[2], K.ALLOY[3])  # comb taps one period of f x 1.932 and of f x 2.546 back (was 3 f)
REESE_COMB_GAIN = 0.25                 # at 0.3 the A1 zone read -21.5 dB on the mud gate; 0.25 leaves 7 dB


def loop_grid(f0, seconds=LOOP_S):
    """A loop of about `seconds` holding a whole number of f0 cycles: (length in samples, the grid step in Hz, the
    pitch on the grid). Every oscillator on the grid repeats exactly once per loop."""
    cycles = round(f0 * seconds)
    L = int(round(cycles * SR / f0))
    return L, SR / L, cycles * SR / L


def reese_voice(f, n, ph0, vel):
    """One saw with its own drive and fold, at 4x, hit a little harder for the first ~100 ms (the bite)."""
    k = 4
    bite = 1.0 + 0.5 * vel * np.exp(-np.arange(n * k) / (SR * k) / 0.07)
    u = D.polyblep_saw(f, n, ph0, k=k) * bite
    return D.down(D.sinefold_adaa(D.tanh_adaa(u, 1.5 + 1.5 * vel), 1.0 + 0.4 * vel), k)


def reese_mids(f, grid, n, vel, r):
    """Detuned stacks, two comb taps on the kit's alloy ratios (an inharmonic hollowness, the kit's metal in the
    bass), a dip at 650 Hz and peaks at 1.8 and 2.6 kHz (the snarl sits above the boxy 450 Hz-1 kHz zone, not in it),
    the steep high-pass, the up/down compressor on a slow detector (it evens the phasing without following the beats),
    and velocity on the brightness."""
    steps = sorted({d for dets in DETUNE for d in dets})
    voice = {d: reese_voice(f + d * grid, n, r.uniform(), vel) for d in steps}
    y = np.stack([sum(voice[d] for d in dets) / len(dets) for dets in DETUNE], 1)
    for ratio in REESE_COMBS:
        y = y + REESE_COMB_GAIN * frac_delay(y, SR / (ratio * f))
    y = D.peaking(y, 650.0, -4.0, 1.0)
    y = D.peaking(y, 1800.0, 3.0, 1.0)
    y = D.peaking(y, 2600.0, 3.0, 2.0)
    y = hp_steep(y, MID_HP)
    y = D.updown3(y, xover=(700.0, 3000.0), down_db=-22.0, up_db=(-40.0, -34.0, -32.0), max_up_db=6.0,
                  attack=0.04, release=0.3, rms=0.12)
    y = D.bfilt(y, 4500.0 + 9000.0 * vel, 'lowpass', order=2)
    return hp_steep(y, MID_HP)


def reese(take):
    """The dry reese: mids about 2 LU over the sub on the hard layer; the soft layer is darker and leans on the sub."""
    L, grid, f = loop_grid(take.f0)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    vel = take.vel
    ph = 2 * np.pi * D.phase(f, n)
    sub = (np.sin(ph) + 0.1 * np.sin(2 * ph)) * raised(n, 0.004)
    mids = reese_mids(f, grid, n, vel, take.rng('saws')) * raised(n, 0.004)[:, None]
    y = D.mono_below(0.5 * vel ** 0.6 * D.st(sub) + 2.2 * vel ** 1.5 * mids, 120.0)
    y, loop = close_loop(y, A0, L)
    f0s = sorted({f + d * grid for dets in DETUNE for d in dets})  # every voice's fundamental, for the alias check
    return Out(y, loop=loop, f0s=tuple(f0s))


# ------------------------------------------------------------------ the talking phrase
LOOP_STEPS = 32                     # 2 bars of 16ths
TALK_NOTES = ((0, 3, 1), (4, 2, 0), (7, 3, 0), (10, 2, 1), (13, 2, 0),   # (16th, length in 16ths, accent)
              (16, 4, 1), (22, 2, 0), (24, 3, 1), (28, 2, 0), (30, 1, 0))
CC74 = ((0, 40), (1, 112), (3, 58), (4, 50), (5, 92), (7, 44), (8, 100), (10, 66), (11, 122), (13, 52), (14, 86),
        (16, 34), (18, 127), (20, 68), (22, 58), (23, 96), (24, 48), (25, 116), (27, 62), (28, 80), (30, 104),
        (32, 40))
CC1 = ((0, 0), (3, 92), (6, 30), (10, 127), (13, 48), (16, 8), (20, 112), (24, 20), (27, 100), (30, 60), (32, 0))
VOWEL_PATH = ((0, 520.0, 900.0), (42, 650.0, 1080.0), (85, 760.0, 1500.0), (127, 560.0, 2200.0))  # CC1, F1, F2 (Hz)


def loop_len(steps=LOOP_STEPS):
    return int(round(steps * K.STEP * SR))


def cc_curve(points, L, smooth=0.008):
    """A CC lane as the engine gets it: breakpoints (16th, value) joined linearly, sent as 7-bit values, then the
    parameter smoother (one-pole, `smooth` seconds). Circular over the loop, so the curve joins at the seam."""
    steps = np.array([p[0] for p in points], dtype=float) * K.STEP * SR
    raw = np.round(np.interp(np.arange(L), steps, [p[1] for p in points]))
    a = np.exp(-1.0 / (smooth * SR))
    return signal.lfilter([1 - a], [1, -a], np.tile(raw, 2), zi=[a * raw[-1]])[0][L:]


def formants(cc1):
    """F1 and F2 per sample along the vowel path, interpolated in log frequency."""
    c = [p[0] for p in VOWEL_PATH]
    return tuple(2 ** np.interp(cc1, c, np.log2([p[j] for p in VOWEL_PATH])) for j in (1, 2))


def lane_filter(x, cc74, cc1, drive=1.8):
    """Python stand-in for the engine's reese lane filter. Under 120 Hz the sub passes untouched (LR4 crossover, so
    the bands sum flat). Above it: a TPT SVF low-pass on CC74 (300 Hz to 19 kHz), a pair of formant band-passes moved
    along a vowel path by CC1, the lane's drive at 4x, and the steep high-pass again, because the drive's difference
    tones would land in the low mids."""
    lo = D.lr4(x, 120.0, 'lowpass')
    hi = D.lr4(x, 120.0, 'highpass')
    fc = 300.0 * 2 ** (6.0 * cc74 / 127.0)
    f1, f2 = formants(cc1)
    out = []
    for ch in range(hi.shape[1]):
        lp = D.svf_tpt(hi[:, ch], fc, q=1.6)[0]
        b1 = D.svf_tpt(lp, f1, q=5.0)[1] / 5.0     # SVF band-pass peaks at q: scaled to unity
        b2 = D.svf_tpt(lp, f2, q=8.0)[1] / 8.0
        out.append(0.12 * lp + 1.2 * b1 + 0.9 * b2)
    hi = D.oversampled(lambda u: D.tanh_adaa(u, drive), np.stack(out, 1), 4)
    return lo + hp_steep(hi, MID_HP)


REESE_ATTACK = 0.010                # the stand-ins start reese notes over 10 ms: the engine's lane is a mono legato voice
                                    # that doesn't retrigger hard, and a 2 ms start splatters the sub into 150-400 Hz


def sampler_voice(out, n_on, attack=0.002, release=0.02):
    """What the engine's sampler does with one held note: play from the start, loop while the key is down, then a
    raised-cosine release."""
    a, b = out.loop
    m = n_on + int(release * SR)
    i = np.arange(m)
    y = out.audio[np.where(i < b, i, a + (i - a) % (b - a))]
    return y * (raised(m, attack) * D.taper(m, n_on / SR, release))[:, None]


def reese_talk(take):
    """A 2-bar phrase on the zone's note: the dry Reese multisample (accents play the hard layer) through the sampler
    and the lane filter, with CC74 and CC1 drawn by hand. Tails wrap and the lane runs over two passes of the loop,
    keeping the second, so the phrase loops."""
    L = loop_len()
    dry = {lay: reese(Take(take.kit, REESE, lay, 1, take.note)) for lay in (1, 2)}
    x = np.zeros((L, 2))
    for step, length, accent in TALK_NOTES:
        v = sampler_voice(dry[2 if accent else 1], int(round(length * K.STEP * SR)) - int(0.01 * SR), REESE_ATTACK)
        np.add.at(x, (int(round(step * K.STEP * SR)) + np.arange(len(v))) % L, v)
    y = lane_filter(np.tile(x, (2, 1)), np.tile(cc_curve(CC74, L), 2), np.tile(cc_curve(CC1, L), 2))[L:]
    return Out(D.mono_below(y, 120.0))


# ------------------------------------------------------------------ growl shots from the resample pass
GROWL_NOTES = (30,)
SHOT_S = 0.6
GROWL_SHIFT_HZ = 0.8                # the reese stage: a copy shifted up on the left, down on the right. Each side
                                    # beats against its copy, null at 1 / (2 shift): 0.63 s, past the shot (at 2 Hz the
                                    # null fell at 0.25 s and took the whole growl with it)
GROWL_WIDTH = 0.5                   # the mids' side, scaled: the two sides drift apart in phase as they beat
LIFT_HP = 1400.0                    # where the mids move after the lift: clear of 400 Hz-1 kHz
# vowels as (F1, F2, F3) Hz, voiced in the F2-F3 range so the move is still heard above LIFT_HP
OO, AH, EH, EE = (300, 900, 2600), (700, 1500, 2600), (530, 1850, 2800), (270, 2300, 3000)
MOVES = {
    # FM index (seconds, index), the vowel path (seconds, formants), the lift out of 400 Hz-1 kHz (start, end), the
    # fade (start, length)
    'rise': dict(index=((0.0, 1.2), (0.12, 3.5), (0.3, 5.0)), vowels=((0.0, OO), (0.08, AH), (0.18, EH), (0.30, EE)),
                 lift=(0.05, 0.12), fade=(0.30, 0.10)),
    'chew': dict(index=((0.0, 4.5), (0.04, 1.5), (0.08, 5.5), (0.12, 2.0), (0.16, 7.0), (0.24, 5.5), (0.34, 3.0)),
                 vowels=((0.0, AH), (0.04, EE), (0.08, AH), (0.12, EE), (0.16, AH), (0.26, EE), (0.34, EH)),
                 lift=(0.07, 0.13), fade=(0.28, 0.08)),
}


def growl_source(f, n, move):
    """The growl the shots are resampled from: 2-op FM (modulator at 2 f, so every partial is harmonic) on the move's
    index curve, then a sine fold, at 4x."""
    k = 4
    t = np.arange(n * k) / (SR * k)
    pts = MOVES[move]['index']
    idx = np.interp(t, [p[0] for p in pts], [p[1] for p in pts])
    ph = 2 * np.pi * f * t
    return D.down(D.sinefold_adaa(1.4 * np.sin(ph + idx * np.sin(2 * ph)), 1.6), k)


def reese_stage(x, r):
    """Reese: each side gets a copy of itself frequency-shifted, up on the left and down on the right."""
    x = D.st(x)
    return np.stack([x[:, 0] + D.freq_shift(x[:, 0], GROWL_SHIFT_HZ),
                     x[:, 1] + D.freq_shift(x[:, 1], -GROWL_SHIFT_HZ)], 1)


def multiband_stage(x, r):
    """Multiband: fold and drive the mids, clip the top, at 4x; then the up/down compressor."""
    lo = D.lr4(x, 300.0, 'lowpass')
    mid = D.lr4(D.lr4(x, 300.0, 'highpass'), 2500.0, 'lowpass')
    hi = D.lr4(x, 2500.0, 'highpass')
    mid = D.oversampled(lambda u: D.sinefold_adaa(D.tanh_adaa(u, 2.5), 1.3), mid, 4)
    hi = D.oversampled(lambda u: D.hardclip_adaa(3.0 * u, 1.0), hi, 4)
    return D.updown3(lo + mid + 0.5 * hi, xover=(300.0, 2500.0), down_db=-20.0, up_db=-36.0)


def comb_formant_stage(x, f, vowels):
    """A comb on the note's period (it rings on the harmonics) and one on the kit's alloy ratio 1.483 (an inharmonic
    ring), then the move's vowel path, timed in seconds across the shot."""
    y = x + 0.55 * frac_delay(x, SR / f)
    y = y + 0.3 * frac_delay(y, SR / (f * K.ALLOY[1]))
    t = np.linspace(0.0, len(x) / SR, 64)
    pts = np.array([p for _, p in vowels], dtype=float)
    track = np.stack([np.interp(t, [s for s, _ in vowels], pts[:, j]) for j in range(3)], 1)
    return np.stack([D.formant_stft(y[:, c], track) for c in range(2)], 1)


def lift_gain(y, a):
    """The gain that brings the band above LIFT_HP up to the level of the band above MID_HP, over the 50 ms from sample
    a: the lift moves the growl up out of the low mids without dropping it 15-18 dB."""
    w = slice(a, a + int(0.05 * SR))
    return float(np.sqrt(np.sum(hp_steep(y, MID_HP)[w] ** 2) / (np.sum(hp_steep(y, LIFT_HP)[w] ** 2) + 1e-30)))


def growl(take, move):
    """Render the growl, resample it twice through the chain, then cut the shot: the mids lift from 450 Hz to 1.4 kHz,
    level-matched, so 400 Hz-1 kHz falls inside the gate while the growl and its vowel move go on above it for another
    ~0.25 s; a clip (it moves the level up), then the same high-passes again, because the clip's difference tones would
    land in the low mids; and a clean sine sub that holds, then drops, by 0.45 s."""
    f = take.f0
    n = int(SHOT_S * SR)
    t = _t(n)
    m = MOVES[move]
    stages = (reese_stage, multiband_stage, lambda x, r: comb_formant_stage(x, f, m['vowels']))
    y = D.resample_pass(growl_source(f, n, move), stages, passes=2, key=(take.kit, take.sound, 'resample'))
    y = y * (raised(n, 0.004) * D.taper(n, *m['fade']))[:, None]
    low = D.taper(n, m['lift'][0], m['lift'][1] - m['lift'][0])[:, None]
    split = lambda u, g=1.0: low * hp_steep(u, MID_HP) + (1 - low) * g * hp_steep(u, LIFT_HP)
    mids = split(y, lift_gain(y, int(m['lift'][0] * SR)))
    mids = split(D.oversampled(lambda u: D.tanh_adaa(2.0 * u, 2.0), mids, 4))
    M, S = D.ms(mids[:, 0], mids[:, 1])
    mids = np.stack([M + GROWL_WIDTH * S, M - GROWL_WIDTH * S], 1)
    sub = np.sin(2 * np.pi * D.phase(f, n)) * raised(n, 0.003) * np.exp(-(t / 0.2) ** 2) * D.taper(n, 0.32, 0.12)
    return D.mono_below(1.4 * mids + 0.45 * D.st(sub), 120.0)


def growl_rise(take):
    return growl(take, 'rise')


def growl_chew(take):
    return growl(take, 'chew')


# The growl shots move from start to end: no steady tone for the alias check. Measured (2026-10-29, this chain):
# GrowlChew -29.6 dB at 4x and -29.4 dB with every stage at 16x, GrowlRise -24.5 dB at both (so not aliasing); with the
# index and the vowel held still, -61.3 and -69.8 dB.
SHOT_QA = {'tonal': False, 'why': 'a moving shot has no steady tone for the alias check (measured: not aliasing)'}

REESE = Item('Bass', 'Reese', 'bass', reese, layers=REESE_LAYERS, notes=REESE_NOTES, loop=True, sets=('s1a',),
             about='neuro reese, dry: driven detuned saws, combs on the alloy ratios, a 650 Hz dip and 1.8/2.6 kHz '
                   'peaks, up/down compression; mids from 450 Hz over a mono sine sub; loops of one detune turn (2 s)')
ITEMS = [
    REESE,
    Item('Loops', 'ReeseTalk', 'loop', reese_talk, notes=REESE_NOTES, loop=True, sets=('s1a',),
         qa={'mud': 'gate', 'why': 'a bass loop: gated like the bass'},
         about='2-bar phrase: the dry Reese through the sampler and a stand-in of the engine lane filter '
               '(SVF on CC74, formant pair on CC1, drive)'),
    Item('Bass', 'GrowlRise', 'bass', growl_rise, notes=GROWL_NOTES, sets=('s1a',), qa=SHOT_QA,
         about='growl shot, rise: an FM growl resampled twice (reese, multiband, comb/formant), oo to ee, '
               'lifting out of the low mids at the same level'),
    Item('Bass', 'GrowlChew', 'bass', growl_chew, notes=GROWL_NOTES, sets=('s1a',), qa=SHOT_QA,
         about='growl shot, chew: three ah-ee chews from the same resample chain, the last above the low mids'),
]
