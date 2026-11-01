"""FUT01 bass for listening round 2, the fix round: the reese rebuilt as A, clean and B, warm, and two tempo-synced
growl moves. The three basslines that play the reese against the 2-step kick are in basslines.py.

Round 1 gave the reese Cool 2 ("reeses needs the most work of all"; in the context loop it "doesnt make me want to
dance"). Its mids started at 450 Hz to pass the strict mud gate, behind a 650 Hz dip and two combs on the kit's
inharmonic ratios, so it was hollow on small speakers and blurred in pitch. The rebuild keeps one voice and gives it two
low ends:

- The stack: five driven saws, three a side with the centre one shared, detuned -18 to +18 cents. Each outer saw's
  detune drifts by a few cents, one to three turns per 2 bars, so the phasing speeds up and slows down instead of
  repeating one beat. The saws stay harmonic (no combs), so the note reads clearly.
- The movement: a low-pass that opens on the attack and breathes once per 2 bars, and a vowel pair (oh to ah, F1
  500-750 Hz, F2 900-1260 Hz) that turns once a bar. On a held note you hear the vowel turn; in a line every note
  opens the filter on its own velocity.
- The kit's metal: a short tine on the alloy ratios over each attack, 5 octaves over the note, as on the chrome pluck.
- The sub: a mono sine on the note under the whole stack, so the stack's own fundamentals never beat in the sub band.
- A, ReeseClean (the strict mud gate, the category default): the stack starts at 450 Hz, about 96 dB per octave, so
  150-400 Hz stays empty; its weight sits on the vowel pair above.
- B, ReeseWarm (the proposed bass-body rule, ADR 0011): A exactly, plus a body band: the stack's own harmonics from
  120 Hz up to A's 450 Hz wall, through the same movement, at WARM_BODY of the stack's level, so they fill 150-400 Hz up
  to a plain saw's level; and the sine sub carries a quiet mono second harmonic. Everything above 450 Hz is A's, at
  A's level and on A's fast attack, so the A/B is the body alone.
- The onsets are shaped for a line played in the gaps between kicks, where QA reads every 20 ms of each note's start:
  the sub rises over 18-35 ms (higher zones longer, since a fast start splatters a sine into 150-400 Hz, the more the
  nearer it is to 150 Hz); above the 450 Hz wall the stack starts 3 ms later and rises over 8 ms, its splatter walled
  off, so the pluck stays quick; B's body rises with the sub, a little after it. A sampler can start the files within
  1 ms.

The multisamples (F#1, A1, C#2, two layers) loop over one 2-bar turn: every saw sits on a grid of whole cycles per
loop and each drift and the vowel turn a whole number of times in it, so the loop joins by itself. Both characters
draw the same saw and drift phases, so A and B differ only in their low end.

The growls keep round 1's resample chain (an FM growl, then reese, multiband and comb/formant stages, twice) and get
what round 1 missed, a musical gesture on the kit's grid at 134 BPM, on F#:
- GrowlTalk: 2 beats of swung 16ths, a vowel per 16th (wo-wee-wo-wah...), brighter step by step, accented like a
  drum part (the beats loudest, then the off-beat 8ths, the swung 16ths 4-5 dB down), the last step open.
- GrowlDive: 2 beats of triplet 8ths that dive an octave, F#2 to F#1, on the first, then roll "yow-yow-yow" down to
  dark, each yow 2-3.5 dB under the dive and the last one lifting.
Each hit is set to its accent after the chain, measured on the mono sum, so a dark vowel still speaks and the accents
hold on a mono system. The chain runs in mono: its reese stage puts the shifted copy in the mid only, its slow beat's
null after the move. The width comes last, a side at least 8 dB under the mid, so the mono sum is the growl whole.
The mids start at 450 Hz for the whole move, so the vowels keep their F1 (EH 530, AH 700 Hz) and OO its F2 (900 Hz),
over a clean sine sub, with 150-400 Hz empty. Held through the move, they take the bass-body rule (ADR 0011: no falls)
instead of the gate, whose 400 Hz-1 kHz fall made round 2's first take lift its mids to 1.3 kHz after the first 16th,
where little more than a flicker of brightness was left of the vowels.

Provenance: the reese grows out of round 1's (this file in S1a, from the S1 research prototype and the legacy reese
engine, legacy/web-pack/audio/reese.py); the growl chain is round 1's, plan section 2.2.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

SR = D.SR
LOOP_STEPS = 32                     # 2 bars of 16ths: the reese's loop and the basslines (basslines.py reads it)
MID_HP = 450.0                      # the stack's wall: A, clean, has nothing under it, so 150-400 Hz stays empty
WARM_HP = 120.0                     # B, warm: its body band, the stack's own WARM_HP-MID_HP, over the mono sine sub


# ------------------------------------------------------------------ the reese voice
REESE_NOTES = (30, 33, 37)          # F#1, A1, C#2
REESE_LAYERS = (0.6, 1.0)
LOOP_S = LOOP_STEPS * K.STEP        # one turn of the low-pass, two of the vowel: 3.58 s
ATTACK_S = 0.5                      # attack and settle before the loop
DETUNE = ((-10.0, 0.0, 18.0), (-18.0, 0.0, 10.0))   # cents, left and right; the centre voice is shared
VOICES = (-18.0, -10.0, 0.0, 10.0, 18.0)
DRIFT_CENTS = 4.0                   # each outer saw's detune moves this far either way...
DRIFT_TURNS = {-18.0: 1, -10.0: 3, 0.0: 0, 10.0: 2, 18.0: 1}   # ...this many times per 2 bars; the centre holds
CHARACTERS = ('clean', 'warm')
SUB_LEVEL = 0.5
MID_LEVEL = 2.6                     # the stack over the sub, A and B alike
WARM_BODY = 0.52                    # B's body band against the stack above the wall: the body level of round 2's first
                                    # B (1.35 / 2.6, its whole stack's level then); the bass-body rule's -2 dB window
                                    # limit leaves little more
WARM_H2 = 0.2                       # B's sub: a mono second harmonic, 14 dB down
LP_HZ = (700.0, 1500.0)             # the low-pass at rest: soft layer, hard layer; it opens on the attack
TINE = 32.0                         # the tine sits 5 octaves over the note, on the alloy ratios
# The onsets, so a line's notes pass the gap checks where they start (in the gaps between kicks): a sine that starts
# fast splatters into 150-400 Hz, the more the nearer its note is to 150 Hz, so each zone's sub rises over its own time
# (long enough for the zone played up to a fourth higher). Above MID_HP the stack starts MID_ONSET[0] after the sub and
# rises over MID_ONSET[1], then a wall takes its splatter (and rings clear of 150-400 Hz); B's body, the stack under
# the wall (low-passed as steeply), rises with the sub, a little after it. A sampler can start these files within 1 ms.
SUB_ONSET = {30: 0.018, 33: 0.024, 37: 0.035}   # seconds, per zone
MID_ONSET = (0.003, 0.008)
BODY_ONSET = (0.006, 1.5)           # B's body: starts 6 ms after the sub, rises over 1.5 x its time


def drift_phases(kit):
    """Each saw's start phase and its drift's phase, the same for every zone, layer and character: A and B then differ
    only in their low end, and a line sounds like the multisamples."""
    r = D.rng(kit, 'Reese', 'phases')
    return {c: (r.uniform(), r.uniform(0, 2 * np.pi)) for c in VOICES}


def saw(freq, n, ph0, drive, bite, k=4):
    """One saw with its own drive at 4x (no intermodulation, so every partial stays on its own harmonic series);
    bite: the drive's push, one value per sample."""
    u = D.polyblep_saw(freq, n, ph0, k=k) * np.repeat(bite, k)
    return D.down(D.tanh_adaa(u, drive), k)


def stack(freqs, n, phases, vel, bite):
    """The detuned stack, (n, 2): freqs[c] is saw c's frequency (Hz, one value per sample)."""
    drive = 1.3 + 1.4 * vel
    v = {c: saw(freqs[c], n, phases[c][0], drive, bite) for c in VOICES}
    return np.stack([sum(v[c] for c in side) / len(side) for side in DETUNE], 1)


def tone(x, vel, char, turns, opening):
    """A band of the stack, then the movement: a low-pass that breathes once per 2 bars and opens with `opening` (0..1
    per sample: an attack, a note's pluck), and the vowel pair turning twice per 2 bars. turns: the position in 2-bar
    turns, one value per sample. char 'clean': the stack above the wall, walled again after the movement (the upper
    band of both characters); 'warm': B's body band, from WARM_HP, no wall."""
    hp = MID_HP if char == 'clean' else WARM_HP
    x = P.hp_steep(x, hp)
    breath = 0.5 - 0.5 * np.cos(2 * np.pi * turns)
    vowel = 0.5 - 0.5 * np.cos(4 * np.pi * turns)
    fc = P.lerp(LP_HZ, vel) * (1.0 + 0.6 * breath) * (1.0 + (1.5 + 1.5 * vel) * opening)
    f1, f2 = 500.0 * 1.5 ** vowel, 900.0 * 1.4 ** vowel
    out = []
    for ch in range(2):
        lp = D.svf_tpt(x[:, ch], fc, q=0.9)[0]
        out.append(lp + 0.9 * D.svf_tpt(lp, f1, q=3.0)[1] / 3.0 + 0.6 * D.svf_tpt(lp, f2, q=4.0)[1] / 4.0)
    y = np.stack(out, 1)
    return P.hp_steep(y, hp) if char == 'clean' else y


def tine(f, n, vel, r):
    """A short metal tine on the kit's alloy ratios, 5 octaves over the note, high-passed steeply at 1.2 kHz (its
    0.4 ms strike has nothing left in the low mids): gone in ~30 ms."""
    decays = [0.007 / (1 + 0.3 * k) for k in range(len(K.RATIOS))]
    ir = D.modal_ir(f * TINE, K.RATIOS, decays, [1 / (1 + k) for k in range(len(K.RATIOS))], n, r)
    return P.hp_steep(ir * P.raised(n, 0.0004), 1200.0) * (0.04 + 0.08 * vel)


def sub_wave(cycles, char):
    """The mono sine sub from its running phase in cycles; B adds a quiet second harmonic."""
    ph = 2 * np.pi * cycles
    return np.sin(ph) + (WARM_H2 * np.sin(2 * ph) if char == 'warm' else 0.0)


def lp_steep(x, fc):
    """An 8th-order Butterworth low-pass run forward and back: about 96 dB per octave over fc, like parts.hp_steep,
    and no phase shift. B's body is the stack under the wall cut this way. The stack minus its high-passed copy leaves
    a residue above the wall (the high-pass's phase), which added 2 dB to A's upper band; a causal low-pass turns the
    stack's own second harmonic against the sub's, which on the C#2 zone played up a fourth added 1.5 dB to the low
    mids."""
    return D.bfilt(x, fc, 'lowpass', order=8, zero_phase=True)


def mix(sub, mids, vel):
    """Sub and mids, mono under 150 Hz. The soft layer leans on the sub."""
    return D.mono_below(SUB_LEVEL * vel ** 0.6 * D.st(sub) + MID_LEVEL * vel ** 1.5 * mids, 150.0)


def reese(take, char):
    """One zone and layer, looped: an attack, then one 2-bar turn of the drift, the low-pass and the vowel."""
    L, grid, f = P.loop_grid(take.f0, LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    t = np.arange(n)
    vel = take.vel
    ph = drift_phases(take.kit)
    freqs, f0s = {}, {f}
    for c in VOICES:
        g = f + round(f * (2 ** (c / 1200) - 1) / grid) * grid       # on the loop's grid
        depth = f * (2 ** (DRIFT_CENTS / 1200) - 1)
        freqs[c] = g + depth * np.sin(2 * np.pi * DRIFT_TURNS[c] * (t - A0) / L + ph[c][1])
        f0s.add(g)
    bite = 1.0 + 0.6 * vel * np.exp(-t / SR / 0.06)
    d = int(MID_ONSET[0] * SR)
    later = lambda delay, rise: np.concatenate([np.zeros(int(delay * SR)), P.raised(n - int(delay * SR), rise)])
    sub_rise = P.raised(n, SUB_ONSET[take.note])[:, None]
    mid_rise = later(*MID_ONSET)[:, None]
    body_rise = later(BODY_ONSET[0], BODY_ONSET[1] * SUB_ONSET[take.note])[:, None]
    x = stack(freqs, n, ph, vel, bite)
    turns, opening = (t - A0) / L, np.exp(-t / SR / 0.12)
    mids = tone(P.hp_steep(x * mid_rise, MID_HP), vel, 'clean', turns, opening)    # A's mids, in both characters
    if char == 'warm':                                                             # B: plus the body under the wall
        mids = mids + WARM_BODY * tone(lp_steep(x * body_rise, MID_HP), vel, 'warm', turns, opening)
    mids = mids + D.st(np.concatenate([np.zeros(d), tine(f, n - d, vel, D.rng(take.kit, 'Reese', 'tine'))]))
    y = mix(sub_wave(D.phase(f, n), char) * sub_rise[:, 0], mids, vel)
    y, loop = P.close_loop(y, A0, L)
    return Out(y, loop=loop, f0s=tuple(sorted(f0s)))


def reese_clean(take):
    return reese(take, 'clean')


def reese_warm(take):
    return reese(take, 'warm')


# ------------------------------------------------------------------ growl moves on the grid
GROWL_NOTE = 30                     # F#1
MOVE_STEPS = 8                      # 2 beats of 16ths
SHOT_TAIL = 0.12                    # rendered past the move, for the last vowel's fall
GROWL_SHIFT_HZ = 0.25               # the reese stage: the mid plus a copy of itself shifted up this far; their slow
                                    # beat's null, at 1 / (2 x shift) = 2 s, falls after the move (round 2's first take
                                    # shifted 0.8 Hz up on the left and down on the right, so some hits were mostly side
                                    # and lost up to 14 dB in mono)
GROWL_SIDE = (0.4, 0.8)             # the width, added after the chain: the mid's copies shifted up and down 0.8 Hz,
                                    # their difference (in quadrature with the mid) at 0.4 of it, so the side stays at
                                    # least 8 dB under the mid and the mono sum is the growl whole
# vowels as (F1, F2, F3) Hz; over the 450 Hz wall the talk keeps EH's and AH's F1 and every F2 (oo dark, ah and eh
# open, ee bright); OO's and EE's F1 sit under it, with the sub
OO, AH, EH, EE = (300, 900, 2600), (700, 1500, 2600), (530, 1850, 2800), (270, 2300, 3000)
TALK_VOWELS = (OO, EE, OO, AH, OO, EE, EH, EE)       # one per 16th: wo-wee-wo-wah-wo-wee-weh-wee
DIVE_HITS = 6                       # triplet 8ths over 2 beats
DIVE_GLIDE = 0.090                  # the dive from F#2 lands on F#1 within the first triplet
# each hit's level against the first, dB: the talk accents the beats, then the off-beat 8ths, the swung 16ths softer,
# the last one open; the dive's yows fall away after the dive and the last one lifts
ACCENTS = {'talk': (0.0, -5.0, -2.0, -5.0, -1.0, -4.0, -1.5, 0.0), 'dive': (0.0, -2.0, -2.5, -3.0, -3.5, -2.0)}
LEVEL_PASSES = 3
HIT_STEP = 0.005


def step_times(steps):
    """The onsets of 16ths 0..steps-1 on the kit's swing, in seconds (odd 16ths land late, as on every lane)."""
    return np.array([(s + (2 * K.SWING - 1) * (s % 2)) * K.STEP for s in range(steps)])


def move_plan(move, f):
    """Per hit: onset (s), the FM index it starts on, its vowel path [(seconds after onset, formants)] and its gate
    length (s); and the pitch track (Hz per sample, at the output rate) and the move's length (s)."""
    beat2 = 2 * K.BEAT
    if move == 'talk':
        on = step_times(MOVE_STEPS)
        idx = np.linspace(2.0, 6.5, MOVE_STEPS)              # brighter step by step: the shape
        prev = (EH,) + TALK_VOWELS[:-1]
        vowels = [((0.0, p), (0.03, v)) for p, v in zip(prev, TALK_VOWELS)]
        gates = [0.7 * K.STEP] * (MOVE_STEPS - 1) + [beat2 - on[-1]]   # the last step opens to the end
        glide = None
    else:
        on = np.arange(DIVE_HITS) * K.BEAT / 3
        idx = np.linspace(6.5, 2.5, DIVE_HITS)               # darker hit by hit: the dive's fall
        vowels = [((0.0, EE), (0.05, AH), (0.12, OO))] * DIVE_HITS   # each hit a "yow"
        gates = [0.8 * K.BEAT / 3] * (DIVE_HITS - 1) + [K.BEAT / 3]
        glide = DIVE_GLIDE
    n = int((beat2 + SHOT_TAIL) * SR)
    t = P.times(n)
    track = np.full(n, f)
    if glide:
        u = np.clip(t / glide, 0.0, 1.0)
        track = f * 2.0 ** (0.5 + 0.5 * np.cos(np.pi * u))           # F#2 down to F#1, a raised-cosine dive
    return [dict(on=o, idx=i, vowels=v, gate=g) for o, i, v, g in zip(on, idx, vowels, gates)], track, beat2


def index_curve(hits, n, k):
    """The FM index at k x: each hit starts on its own index, kicks up 60% and settles over 40 ms."""
    t = np.arange(n * k) / (SR * k)
    idx = np.zeros(n * k)
    for h in hits:
        tau = t - h['on']
        idx = np.where(tau >= 0, h['idx'] * (1.0 + 0.6 * np.exp(-np.maximum(tau, 0) / 0.04)), idx)
    return idx


def growl_source(track, hits):
    """The growl the move is resampled from: 2-op FM (modulator at 2 f, so every partial is harmonic) on the pitch
    track and the hits' index curve, then a sine fold, at 4x."""
    k = 4
    n = len(track)
    ph = 2 * np.pi * np.concatenate([[0.0], np.cumsum(np.repeat(track, k)[:-1] / (SR * k))])
    return D.down(D.sinefold_adaa(1.4 * np.sin(ph + index_curve(hits, n, k) * np.sin(2 * ph)), 1.6), k)


def vowel_track(hits, n, frames=512):
    """(frames, 3) formants over the shot: each hit's vowel path from its onset, held until the next hit."""
    t = np.linspace(0.0, n / SR, frames)
    out = np.zeros((frames, 3))
    for i, h in enumerate(hits):
        a, b = h['on'], hits[i + 1]['on'] if i + 1 < len(hits) else np.inf
        sel = (t >= a) & (t < b) if i else (t < b)
        pts = np.array([p for _, p in h['vowels']], dtype=float)
        times = [a + s for s, _ in h['vowels']]
        out[sel] = np.stack([np.interp(t[sel], times, pts[:, j]) for j in range(3)], 1)
    return out


def reese_stage(x, r):
    """Reese, mid-safe: the mid plus a copy of itself shifted up GROWL_SHIFT_HZ, on both sides, so the chain stays mono
    (a stage that folds or drives each side would turn a side into level the mono sum loses) and the copies' slow
    beat has its null after the move."""
    m = D.mono(x)
    return D.st(m + D.freq_shift(m, GROWL_SHIFT_HZ))


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
    ring), then the move's vowels, frame by frame."""
    y = x + 0.55 * P.frac_delay(x, SR / f)
    y = y + 0.3 * P.frac_delay(y, SR / (f * K.RATIOS[1]))
    return np.stack([D.formant_stft(y[:, c], vowels) for c in range(2)], 1)


def talk_gate(hits, n):
    """The vowel rhythm's level: each hit opens over 4 ms, holds its gate and falls 18 dB over 12 ms before the next."""
    t = P.times(n)
    g = np.full(n, D.undb(-18.0))
    for h in hits:
        tau = t - h['on']
        rise = np.clip(tau / 0.004, 0.0, 1.0)
        fall = np.clip((tau - h['gate']) / 0.012, 0.0, 1.0)
        e = np.where(tau < 0, 0.0, (0.5 - 0.5 * np.cos(np.pi * rise)) * (0.5 + 0.5 * np.cos(np.pi * fall)))
        g = np.maximum(g, e)
    return g


def hit_curve(hits, gains, n):
    """A gain per sample: each hit's gain from its onset to the next, the steps raised-cosine over the 5 ms before each
    onset, where the vowel rhythm's gate sits 18 dB down."""
    g = np.full(n, gains[0])
    t = P.times(n)
    for h, prev, cur in zip(hits[1:], gains, gains[1:]):
        u = np.clip((t - h['on'] + HIT_STEP) / HIT_STEP, 0.0, 1.0)
        g = np.where(t >= h['on'] - HIT_STEP, prev + (cur - prev) * (0.5 - 0.5 * np.cos(np.pi * u)), g)
    return g


def hit_db(x, hits, n):
    """Each hit's level: dB, RMS of the mono sum over its gate, so the accents hold on a mono system."""
    x = D.mono(x)
    ons = [int(h['on'] * SR) for h in hits] + [n]
    return np.array([D.db(np.sqrt(np.mean(x[a:min(b, a + int(h['gate'] * SR))] ** 2)) + 1e-12)
                     for h, a, b in zip(hits, ons, ons[1:])])


def width(mid):
    """The growl's side, after the chain: the mid's copies shifted up and down GROWL_SIDE[1] Hz, their difference at
    GROWL_SIDE[0] of the mid. It is the mid in quadrature, swelling and fading over the move, so the image moves while
    the mono sum (L + R) / 2 stays the mid exactly."""
    level, hz = GROWL_SIDE
    return level * 0.5 * (D.freq_shift(mid, hz) - D.freq_shift(mid, -hz))


def growl(take, move):
    """Render the move's growl on its pitch track, resample it twice through the chain (in mono) with the move's vowels,
    put the vowel rhythm on it, then cut the shot: the mids from 450 Hz for the whole move; a clip (it moves the level
    up), then the same high-pass again, because the clip's difference tones would land in the low mids; each hit set
    to its level in the move's accent shape (ACCENTS, measured on the mono sum after the clip, so a dark vowel still
    speaks); the width (a side at least 8 dB under the mid); and a clean sine sub on the same pitch track that holds
    through the move and falls after it."""
    f = take.f0
    hits, track, length = move_plan(move, f)
    n = len(track)
    vowels = vowel_track(hits, n)
    stages = (reese_stage, multiband_stage, lambda x, r: comb_formant_stage(x, f, vowels))
    y = D.resample_pass(growl_source(track, hits), stages, passes=2, key=(take.kit, take.sound, 'resample'))
    y = D.mono(y) * P.raised(n, 0.004) * talk_gate(hits, n) * D.taper(n, length, 0.06)
    mids = P.hp_steep(y, MID_HP)

    def clip(g):
        return P.hp_steep(D.oversampled(lambda v: D.tanh_adaa(2.0 * v, 2.0), mids * g, 4), MID_HP)

    gains = np.ones(len(hits))
    for _ in range(LEVEL_PASSES):     # each hit at its level in the move's accent shape, measured as it leaves
        got = hit_db(clip(hit_curve(hits, gains, n)), hits, n)
        gains[1:] *= D.undb((np.array(ACCENTS[move]) - (got - got[0]))[1:])
    mid = clip(hit_curve(hits, gains, n))
    side = width(mid)
    sub = np.sin(2 * np.pi * D.phase(track, n)) * P.raised(n, 0.003) * D.taper(n, length - 0.04, 0.08)
    levels = hit_db(mid, hits, n)
    meta = {'move': move, 'bpm': K.BPM, 'hits_s': [round(float(h['on']), 4) for h in hits],
            'hit_db': [round(float(v), 2) for v in levels - levels[0]], 'hit_db_measured_on': 'the mono sum',
            'length_s': round(length, 4), 'swing': K.SWING if move == 'talk' else None}
    return Out(D.mono_below(1.4 * np.stack([mid + side, mid - side], 1) + 0.45 * D.st(sub), 120.0), meta=meta)


def growl_talk(take):
    return growl(take, 'talk')


def growl_dive(take):
    return growl(take, 'dive')


# ------------------------------------------------------------------ round 1 (set s1a), as history
# The S1a reese, unchanged and in set s1a only. Round 2's context loop (context.py) plays the picked bassline
# (basslines.lane) instead; round 1's talking phrase (ReeseTalk) and growls have left.
S1A_DETUNE = ((-1, 0, 2), (-2, 0, 1))
S1A_COMBS = (K.ALLOY[2], K.ALLOY[3])
S1A_COMB_GAIN = 0.25
S1A_LOOP_S = 2.0


def s1a_voice(f, n, ph0, vel):
    k = 4
    bite = 1.0 + 0.5 * vel * np.exp(-np.arange(n * k) / (SR * k) / 0.07)
    u = D.polyblep_saw(f, n, ph0, k=k) * bite
    return D.down(D.sinefold_adaa(D.tanh_adaa(u, 1.5 + 1.5 * vel), 1.0 + 0.4 * vel), k)


def s1a_mids(f, grid, n, vel, r):
    steps = sorted({d for dets in S1A_DETUNE for d in dets})
    voice = {d: s1a_voice(f + d * grid, n, r.uniform(), vel) for d in steps}
    y = np.stack([sum(voice[d] for d in dets) / len(dets) for dets in S1A_DETUNE], 1)
    for ratio in S1A_COMBS:
        y = y + S1A_COMB_GAIN * P.frac_delay(y, SR / (ratio * f))
    y = D.peaking(y, 650.0, -4.0, 1.0)
    y = D.peaking(y, 1800.0, 3.0, 1.0)
    y = D.peaking(y, 2600.0, 3.0, 2.0)
    y = P.hp_steep(y, MID_HP)
    y = D.updown3(y, xover=(700.0, 3000.0), down_db=-22.0, up_db=(-40.0, -34.0, -32.0), max_up_db=6.0,
                  attack=0.04, release=0.3, rms=0.12)
    y = D.bfilt(y, 4500.0 + 9000.0 * vel, 'lowpass', order=2)
    return P.hp_steep(y, MID_HP)


def s1a_reese(take):
    L, grid, f = P.loop_grid(take.f0, S1A_LOOP_S)
    A0 = int(ATTACK_S * SR)
    n = A0 + int(0.02 * SR) + L
    vel = take.vel
    ph = 2 * np.pi * D.phase(f, n)
    sub = (np.sin(ph) + 0.1 * np.sin(2 * ph)) * P.raised(n, 0.004)
    mids = s1a_mids(f, grid, n, vel, take.rng('saws')) * P.raised(n, 0.004)[:, None]
    y = D.mono_below(0.5 * vel ** 0.6 * D.st(sub) + 2.2 * vel ** 1.5 * mids, 120.0)
    y, loop = P.close_loop(y, A0, L)
    f0s = sorted({f + d * grid for dets in S1A_DETUNE for d in dets})
    return Out(y, loop=loop, f0s=tuple(f0s))


S1A_REESE = Item('Bass', 'Reese', 'bass', s1a_reese, layers=REESE_LAYERS, notes=REESE_NOTES, loop=True,
                 sets=('s1a',), about='round 1 neuro reese, dry: mids from 450 Hz over a mono sine sub; 2 s loops')


# ------------------------------------------------------------------ the items
# The growl moves change from start to end (no steady tone for the alias check) and hold their vowels through the move
# (the bass-body rule for a bass shot, ADR 0011: no falls; the groove loop's gap and before-hit checks still run).
SHOT_QA = {'mud': 'bass-body', 'tonal': False,
           'why': 'a growl keeps its vowels (F1 and F2 from 450 Hz) through the move: the bass-body rule for a bass '
                  'shot (ADR 0011), no falls, 150-400 Hz empty; a moving shot (index, vowels, the dive) has no steady '
                  'tone for the alias check'}
WARM = {'mud': 'bass-body', 'why': "B, warm: the proposed bass-body rule (ADR 0011); A's stack plus its own 120-450 Hz "
                                   'over a mono sine sub'}
REESE_LUFS = -13.2                  # A's and B's loudest takes at the same LUFS-I, so the A/B is by character, not by
                                    # level
REESE_ABOUT = ('reese, 5 driven saws (-18 to +18 cents) drifting a few cents per 2 bars, a low-pass that opens and '
               'breathes, an oh-ah vowel pair turning once a bar, an alloy tine on the attack, a mono sine sub that '
               'leads each onset; 2-bar loops')
ITEMS = [
    Item('Bass', 'ReeseClean', 'bass', reese_clean, layers=REESE_LAYERS, notes=REESE_NOTES, loop=True, sets=('r2',),
         level=REESE_LUFS, about=f'A, clean: {REESE_ABOUT}; the stack from 450 Hz (the strict mud gate)'),
    Item('Bass', 'ReeseWarm', 'bass', reese_warm, layers=REESE_LAYERS, notes=REESE_NOTES, loop=True, sets=('r2',),
         level=REESE_LUFS, qa=WARM,
         about=f"B, warm: {REESE_ABOUT}; A's stack plus its own 120-450 Hz body (bass-body)"),
    Item('Bass', 'GrowlTalk', 'bass', growl_talk, notes=(GROWL_NOTE,), sets=('r2',), qa=SHOT_QA,
         about='growl move, talk: 2 beats of swung 16ths on F#1, a vowel per 16th, brighter step by step, accented '
               'on the beats, the last open; the resample chain in mono, mids from 450 Hz over a sine sub'),
    Item('Bass', 'GrowlDive', 'bass', growl_dive, notes=(GROWL_NOTE,), sets=('r2',), qa=SHOT_QA,
         about='growl move, dive: 2 beats of triplet 8ths, an octave dive F#2 to F#1 on the first, then yow-yow-yow '
               'darker each hit, the last lifting; the resample chain in mono, mids from 450 Hz over a sine sub'),
    S1A_REESE,
]
