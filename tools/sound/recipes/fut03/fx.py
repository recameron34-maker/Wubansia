"""FUT03 effects for round 2: the one-bar build fill, the 8-bar riser and the impact it lands on.

Every one is tempo-synced at 128 BPM and pitched to G minor. The fill and the riser are end-aligned: each file ends on
the next downbeat and stays loud to it (cut_end), so the drop lands where it stops. The impact starts on that downbeat.

- BuildFill: one bar, a tom roll that speeds up and climbs while the kit's own clap (its files as written) doubles it:
  8ths on beats 1 and 2, 16ths on beat 3, 32nds on beat 4, on the kit's swing. The glass toms climb the G minor
  pentatonic an octave, G3 Bb3 | C4 D4 | F4 G4, two hits a note on the 8ths and 16ths and four on the 32nds, every
  glass mode they ring in the key, while
  the clap's velocity layers rise from v2 to v5 and a high-pass opens from 120 to 450 Hz: a rhythm, a pitch climb and
  a crescendo into the drop.
- Riser: 8 bars, three layers on one rising level (24 dB over the 8 bars): a noise wash whose band climbs from 300 Hz
  to 10 kHz; the same noise pulsed on the grid, 8ths for four bars, 16ths for two, 32nds for the last two (a roll in
  noise, louder each step); and a glass chord, the kit's minor-triad ratios as band-limited saws, gliding up two
  octaves from G3 to G5 and landing on G at the downbeat, the glide speeding up at the end.
- Impact: the hit on the downbeat. A sub drop that falls from 700 Hz onto G1 in 100 ms and holds a beat (a clean sine,
  the kick's note), a glass bell struck on G6 on the kit's ratios, and a crash of noise above 2 kHz in the kit's
  space. Everything is gone two bars after the downbeat. It is mud-gated like a kick, since it carries a sub.

Everything but the impact's sub is high-passed above the kick and sub, and mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
RATIOS = np.array(K.RATIOS)
S16 = K.STEP * SR                         # samples per 16th: 5625 at 128 BPM
BAR = int(round(16 * S16))                # 90000 samples


def swung(s):
    """A position in 16ths (fractional: 32nds) as a sample index on the kit's swing: the first 16th of each 8th spans
    2 x K.SWING of a 16th and the second the rest, so the odd 16ths land where phrase.Grid puts them and a 32nd inside
    a swung 16th shrinks with it."""
    base = np.floor(s / 2) * 2
    f = s - base
    u = f * K.SWING if f <= 1 else K.SWING + (f - 1) * (1 - K.SWING)
    return int(np.floor((base + 2 * u) * S16 + 0.5))


def svf_sweep(x, fc, kind, q=0.6):
    """A time-varying 2-pole TPT filter per channel: 'lp', 'bp' or 'hp'."""
    i = {'lp': 0, 'bp': 1, 'hp': 2}[kind]
    x = D.st(x)
    return np.stack([D.svf_tpt(x[:, c], fc, q=q)[i] for c in range(2)], 1)


# ------------------------------------------------------------------ the build fill
G3, BB3, C4, D4, F4, G4 = (K.ROOT + 12, K.ROOT + 15, K.ROOT + 17, K.ROOT + 19, K.ROOT + 22, K.ROOT + 24)
# (16th, tom note, velocity): 8ths, then 16ths, then 32nds; the clap takes the layer nearest the velocity
FILL = ((0, G3, 0.40), (2, G3, 0.45), (4, BB3, 0.50), (6, BB3, 0.55),
        (8, C4, 0.62), (9, C4, 0.62), (10, D4, 0.70), (11, D4, 0.70),
        (12, F4, 0.78), (12.5, F4, 0.80), (13, F4, 0.82), (13.5, F4, 0.85),
        (14, G4, 0.90), (14.5, G4, 0.93), (15, G4, 0.97), (15.5, G4, 1.0))
FILL_HP = (120.0, 450.0)     # the high-pass on the whole fill opens over the bar
TOM_CLAP = (1.0, 0.75)       # tom and clap levels in the fill


def in_key(f):
    """Whether a frequency's nearest note is in the kit's key."""
    return int(np.floor(69 + 12 * np.log2(f / 440.0) + 0.5)) % 12 in K.SCALE


def tom_modes(f0, v):
    """The glass modes a tom on f0 rings: (Hz, level) at 2 f0 times the bell's four low ratios. A mode whose note is out
    of the key is left out: the bell's minor third rings in key over G, C and D, but over Bb3 and F4 it would ring Db5
    and Ab5, a semitone under the D5 and A5 the clap's tone rings on the same hits, so those two toms ring their fifth
    and double octave only."""
    return [(2 * f0 * rt, a if in_key(2 * f0 * rt) else 0.0)
            for a, rt in zip((0.0, 0.30 * v, 0.22 * v, 0.16 * v), RATIOS[:4])]


def glass_tom(f0, v, n, r):
    """A tuned glass tom: a sine body that drops onto f0 from a fifth up in about 10 ms, the bell's next three modes
    (the kit's ratios) an octave up, short, every one in the key (tom_modes; the phases stay those of the full bell),
    and a stick tick. Harder hits drop further, ring brighter and tick more."""
    t = P.times(n)
    f = f0 * (1 + (0.3 + 0.3 * v) * np.exp(-t / 0.006))
    body = np.sin(2 * np.pi * D.phase(f, n)) * P.gate(t, 0.010, 0.085 + 0.03 * v)
    amps = [a for _, a in tom_modes(f0, v)]
    glass = P.modes(n, 2 * f0, RATIOS[:4], [0.05, 0.035, 0.03, 0.025], amps, 0.01 * np.exp(-t / 0.005), r)
    tick = P.hp_noise(r, n, 3000.0, order=2) * D.env_exp(n, 0.0008, 0.0001)
    return (body + glass + (0.05 + 0.15 * v) * tick) * P.raised(n, 0.0005)


def build_fill(take):
    """The roll on the bar (end-aligned: the file is exactly one bar, ending on the next downbeat). Each hit's tom and
    clap start together; tails run on under the next hits and are cut where the bar ends."""
    n = BAR
    y = np.zeros((n, 2))
    item, claps, _ = Ph.kit_files(K, 'Clap')
    m = int(0.4 * SR)
    for i, (step, note, v) in enumerate(FILL):
        pos = swung(step)
        tom = TOM_CLAP[0] * glass_tom(float(D.midi_hz(note)), v, m, take.rng('tom', i)) * D.undb(14.0 * (v - 1.0))
        layer = 1 + min(range(len(item.layers)), key=lambda k: (abs(item.layers[k] - v), -k))
        clap = TOM_CLAP[1] * claps[(None, layer, 1 + i % item.rr)].audio
        for x in (D.st(tom) * np.array([1.0, 1.0]), clap):
            end = min(n, pos + len(x))
            y[pos:end] += x[:end - pos]
    u = np.arange(n) / n
    y = svf_sweep(y, FILL_HP[0] * (FILL_HP[1] / FILL_HP[0]) ** u, 'hp', q=0.7)
    y = D.mono_below(D.bfilt(y, 110.0, 'highpass', order=4), 150.0)
    return Out(y, meta={'bars': 1, 'bpm': K.BPM, 'swing': K.SWING, 'end_aligned': True,
                        'hits': [[s, D.note_name(nt), v] for s, nt, v in FILL]})


# ------------------------------------------------------------------ the riser
RISER_BARS = 8
RISER_N = int(round(RISER_BARS * 16 * S16))     # 720000 samples, 15 s
RISE_DB = 24.0                                  # the level climbs this much over the 8 bars
PULSES = ((0, 4, 2.0), (4, 6, 1.0), (6, 8, 0.5))   # (from bar, to bar, pulse length in 16ths): 8ths, 16ths, 32nds
GLIDE = (K.ROOT + 12, 2)                        # the glass chord: from G3, up this many octaves (to G5)


def pulse_gate(n):
    """The noise roll: a pulse on every 8th, then 16th, then 32nd (PULSES), on the kit's swing, each a fast attack
    and a decay over 60 % of its length, every pulse a little louder than the one before."""
    g = np.zeros(n)
    bar = 16
    for b0, b1, step in PULSES:
        s = b0 * bar
        while s < b1 * bar - 1e-9:
            a, b = swung(s), min(n, swung(s + step) if s + step < RISER_BARS * bar else n)
            k = b - a
            tt = np.arange(k) / SR
            g[a:b] = (1 - np.exp(-tt / 0.002)) * np.exp(-tt / (0.6 * k / SR))
            s += step
    return g


def glass_glide(n, r):
    """The kit's four low ratios (a minor triad and its octave) as band-limited saws on one gliding root, G3 up two
    octaves to G5, the glide speeding up toward the end; each side a voice 6 cents off the other."""
    u = np.arange(n) / n
    f_root = float(D.midi_hz(GLIDE[0])) * 2 ** (GLIDE[1] * u ** 1.6)
    y = np.zeros((n, 2))
    for k, rt in enumerate(RATIOS[:4]):
        for c, cents in enumerate((-3.0, 3.0)):
            f = f_root * rt * 2 ** (cents / 1200)
            y[:, c] += D.additive_saw(f, n, r.uniform(), fmax=16000.0) / (1 + 0.5 * k)
    return y, f_root


def riser(take):
    n = RISER_N
    u = np.arange(n) / n
    r = take.rng('noise')
    noise = np.stack([r.standard_normal(n), r.standard_normal(n)], 1)
    noise = 0.8 * noise[:, :1] + 0.45 * noise           # mostly common: a centred wash with some width
    wash = svf_sweep(noise, 300.0 * (10000.0 / 300.0) ** (u ** 1.3), 'bp', q=1.1)
    roll = D.bfilt(noise, (1500.0, 9000.0), 'bandpass', order=2) * pulse_gate(n)[:, None]
    roll *= (0.25 + 0.75 * u ** 2)[:, None]               # the roll comes forward as it speeds up
    tone, _ = glass_glide(n, take.rng('glide'))
    tone = D.bfilt(tone, 9000.0, 'lowpass', order=2)
    y = 0.55 * wash / np.std(wash) + 0.35 * roll / np.std(roll) + 0.45 * tone / np.std(tone)
    y *= D.undb(-RISE_DB + RISE_DB * u ** 1.3)[:, None]
    y = D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)
    return Out(y, meta={'bars': RISER_BARS, 'bpm': K.BPM, 'end_aligned': True,
                        'glide': [D.note_name(GLIDE[0]), D.note_name(GLIDE[0] + 12 * GLIDE[1])]})


# ------------------------------------------------------------------ the impact
IMPACT_N = 2 * BAR                         # two bars: the tail is gone when the second bar ends
F_SUB = float(D.midi_hz(K.KICK_NOTE))      # G1
BELL_NOTE = K.ROOT + 48                    # G6, 1568 Hz: every bell mode sits above 1 kHz, clear of the low mids
IMPACT_MIX = (1.0, 0.30, 0.22)             # sub drop, bell, crash


def impact(take):
    n = IMPACT_N
    t = P.times(n)
    r = take.rng('impact')
    # 1. the sub drop: a sine falling from 700 Hz onto G1 (fast snap, then a 40 ms glide), held a beat, gone in 1.3 s
    f = F_SUB + (700.0 - F_SUB) * (0.75 * np.exp(-t / 0.006) + 0.25 * np.exp(-t / 0.040))
    sub = np.sin(2 * np.pi * D.phase(f, n)) * P.gate(t, K.BEAT, 0.32) * D.taper(n, 0.9, 0.4) * P.raised(n, 0.0005)
    # 2. the glass bell on G6, every mode above 1 kHz
    decays = [1.1 / (1 + 0.45 * k) for k in range(len(RATIOS))]
    amps = [1.0 / (1 + 0.5 * k) for k in range(len(RATIOS))]
    bell = P.modes(n, float(D.midi_hz(BELL_NOTE)), RATIOS, decays, amps, 0.006 * np.exp(-t / 0.01),
                   D.rng(take.kit, take.item.name, 'bell'))
    side = P.modes(n, float(D.midi_hz(BELL_NOTE)) * 1.0012, RATIOS, decays, amps, 0.006 * np.exp(-t / 0.01),
                   D.rng(take.kit, take.item.name, 'bell side'))
    bell = np.stack([bell + 0.4 * side, bell - 0.4 * side], 1) * P.raised(n, 0.0003)[:, None]
    # 3. the crash: noise above 2 kHz, fast attack, half-second decay, into the kit's space
    hit = P.hp_steep(r.standard_normal(n), 2000.0) * D.env_exp(n, 0.45, 0.001)
    room = D.fdn_reverb(hit * 0.5, K.space_rng('impact'), t60=2.6, size=1.1, damp_hz=9000.0)[:n]
    crash = D.st(hit) + 0.8 * room
    crash = np.stack([P.hp_steep(crash[:, c], 1500.0) for c in range(2)], 1)
    g_sub, g_bell, g_crash = IMPACT_MIX
    hi = g_bell * bell + g_crash * crash / np.std(crash[:int(0.5 * SR)])
    hi = D.mono_below(hi, 150.0)
    y = g_sub * D.st(sub) + hi * D.taper(n, 2.6, IMPACT_N / SR - 2.6)[:, None]
    return Out(y, meta={'bpm': K.BPM, 'starts': 'on the downbeat', 'sub': D.note_name(K.KICK_NOTE),
                        'bell': D.note_name(BELL_NOTE)})


ITEMS = [
    Item('FX', 'BuildFill', 'fx', build_fill, root=G3, sets=('r2', 'r3'), flags={'cut_end'}, tags=('fill',),
         qa={'why': 'end-aligned: it ends on the next downbeat, its last 32nd still sounding, where the drop takes '
                    'over'},
         about='one-bar build fill at 128 BPM: glass toms climbing G minor pentatonic G3 to G4 under the kit clap, '
               '8ths, 16ths, then 32nds, louder and opening'),
    Item('FX', 'Riser', 'fx', riser, level=-14.0, sets=('r2', 'r3'), flags={'cut_end'}, tags=('riser',),
         qa={'why': 'end-aligned: it stays loud to the downbeat, where the impact takes over'},
         about='8-bar riser at 128 BPM, end-aligned: a climbing noise band, a noise roll in 8ths, 16ths then 32nds, '
               'and a glass chord gliding from G3 to G5'),
    Item('FX', 'Impact', 'fx', impact, root=K.KICK_NOTE, sets=('r2', 'r3'),
         qa={'mud': 'gate', 'why': 'it carries a sub drop onto G1: gated like a kick, not low-cut'},
         about='impact on the downbeat: a sub drop onto G1 held a beat, a glass bell on G6, a crash above 2 kHz in '
               'the kit space; gone in two bars'),
]
