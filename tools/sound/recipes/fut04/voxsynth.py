"""FUT04 round 5: the vox synth, a wordless formant "vocal synth" for dark techno, its stabs and its riff (set r5).

Round 3's vocals (three sung lines, their chops and a robot phrase, all text-to-speech resynthesised in WORLD) were the
weak point: "vocals suck, either ditch or rethink". Round 5 drops the words, the speech engine and the models, and
builds the voice from synthesis instead, the vowel stab dark techno uses: a voice-like tone, no singer, no words.

The voice (voice()), every note:
- the source: an exactly band-limited glottal-like pulse train, additive: a saw's 1/h roll-off times a second pole at
  a tilt corner (TILT_HZ, brighter on the hard layer), and a pulse's ripple (PULSE: a 28% pulse mixed into the saw, a
  hollow, reedy colour). Into the note it scoops up from 35 cents flat (SCOOP), then holds; a slow jitter wanders
  1.5 cents (JITTER) and from 0.55 s, on a held note, a vibrato fades in (VIB), the two zero-mean over the tuning
  window;
- a detuned double: two more voices +-7 cents (DOUBLE), 9 and 13 ms late, one on each side, from the second harmonic
  up (so the fundamental never beats);
- breath: noise above 800 Hz into the same tract (BREATH), and an "h" before the vowel;
- the consonant-like onset: the tract starts closed (LIPS: a "w"-"m", F1 240 Hz, F2 600 Hz) and opens into the vowel
  over 35 ms (ONSET_S), with a 6 ms tick of 2-6 kHz noise on the front (a soft "k");
- the vowel: five narrow band-passes (TPT state-variable filters, q 6 to 18: the metallic ring of a vocoder's bands)
  on a dark male "oh" or "oo" (VOWELS5), the upper two or three lifted over the first (the ring sits at 2.3-4.2 kHz),
  closing towards "oo" as the note dies (the dark "ohw"); F1 never under 1.15 times the note (F1_FLOOR); a little of
  the source goes round them (DIRECT), so the note's own fundamental is always there;
- a drive at 4x (DRIVE), then a high-pass at 210 Hz (4th order), a 4 dB dip at 300 Hz and mono under 150 Hz: out of
  the kick and the acid's sub. Its body sits over the low mids: under the "oh" layer, 150-400 Hz reads 3-30 dB
  under the whole file, and what an "oo" on D4 or E4 keeps there is its own fundamental.

The items:
- VoxSynth (Vox): the playable multisample, E3 G3 B3 D4 E4 G4 B4 (every zone in the key), two layers: v1 sings "oo"
  (closed, darker), v2 "oh" (open): velocity opens the vowel. Held 1.1 s, released over 0.25 s; dry, so the sampler's
  sends place it.
- VoxStabOhE3, VoxStabOoB3 (Vox): single stabs, gated (held 160 ms, closed in 40 ms), driven harder, in the kit's dark
  hall (the kit's space seed, 3.6 s, damped from 3 kHz).
- VoxSynthRiff (Loops): the riff, 2 bars at 136 BPM on the kit's swing, played on VoxSynth's files as the sampler
  plays them (each note its zone and layer, gated to its length): the stabs answer the acid line's leaps on the
  off-beat 8ths, on the 16th after them (bar 1: E3 "oh", E3 "oo", G3 "oh"; bar 2: E3, E3, B3, then D4 on the "and"
  of 4 thrown into the hall and a dotted-8th echo, so it spills into the next bar). Notes: RIFF, also a MIDI phrase
  (recipes/fut04/midi.py).

No item here has words, a script or a speech model: QA's tonal check is off for them (the breath sits between the
harmonics, the vibrato and jitter move them), the tuning check is on (+-3 cents on the root, over 0.1-0.6 s; a stab
over its gate, 50-170 ms).
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
VOX_NOTES = (52, 55, 59, 62, 64, 67, 71)   # E3 G3 B3 D4 E4 G4 B4: the zones, every one in E minor
VOX_LAYERS = (0.5, 1.0)                    # v1 "oo", v2 "oh"
LAYER_VOWEL = {1: 'oo', 2: 'oh'}

# the tract: (formant Hz, gain, q) x 5. A dark male "oh" and "oo" (after the Hillenbrand and Peterson-Barney averages,
# F2 pulled down for the dark colour), narrow (q 7-20) for the metallic ring; LIPS: the closed onset
VOWELS5 = {
    'oh': ((500.0, 0.8, 6.0), (850.0, 1.0, 8.0), (2450.0, 3.0, 12.0), (3300.0, 2.2, 16.0), (4200.0, 1.0, 18.0)),
    'oo': ((360.0, 0.5, 6.0), (780.0, 1.0, 8.0), (2300.0, 1.4, 12.0), (3200.0, 0.7, 16.0), (4100.0, 0.25, 18.0)),
}
LIPS = ((240.0, 1.0), (600.0, 0.30), (2100.0, 0.10), (3000.0, 0.05), (4000.0, 0.03))   # (Hz, gain): the closure
ONSET_S = 0.035             # the tract opens from LIPS into the vowel over this
F1_FLOOR = 1.15             # F1 at least this times the note
CLOSE = (0.45, 0.60, 'oo')  # from this far into a held note (a stab: from its release), over this long (s), the tract
                            # closes towards this vowel
DIRECT = 0.06               # the source around the tract: the fundamental always reads
TILT_HZ = (2500.0, 5000.0)  # the source's second pole, soft and hard layer
PULSE = (0.28, 0.45)        # the pulse in the source: its width, its depth
FMAX = 12000.0              # the source's harmonics up to here
H1 = 2.0                    # the first harmonic over a saw's (a glottal source's strong fundamental): the note reads
SCOOP = (-35.0, 0.012)      # cents, time constant: the pitch scoops up into the note
VIB = (5.2, (7.0, 10.0), 0.55, 0.30)   # rate (Hz), depth (cents, soft and hard), onset (s), fade-in (s): on the held
                                       # note only
JITTER = (1.5, 12.0)        # cents RMS, bandwidth (Hz)
DOUBLE = (7.0, (0.009, 0.013), 0.55)   # cents each side, the two doubles' delays (s), their level
BREATH = (0.05, 800.0)      # level against the source (unit RMS each), high-pass (Hz)
ASPIRATE = (0.30, 0.025)    # the "h" before the vowel: level, length (s)
TICK = (0.10, (2000.0, 6000.0), 0.006)   # the "k": level, band (Hz), decay (s)
DRIVE = (1.6, 2.6)          # the tanh at 4x, soft and hard layer (stabs: STAB_DRIVE)
HP = 210.0                  # the high-pass (4th order): the vowel sits over the low mids
LOW_MID_DIP = (300.0, -4.0, 0.8)
TUNE_WINDOW = (0.10, 0.60)  # QA's tuning window: the vibrato and jitter are zero-mean over it (a stab's: STAB_TUNE)


def hz(note):
    return float(D.midi_hz(note))


def source(freq, f0, n, hard, ph0, first=1):
    """The glottal-like source on a pitch curve (Hz per sample), unit RMS: additive harmonics up to FMAX, each 1/h
    times the tilt pole and the pulse's ripple, the first harmonic H1 times over. first: the lowest harmonic (the
    doubles start at 2, so the note's own fundamental never beats)."""
    tilt = P.lerp(TILT_HZ, hard)
    width, depth = PULSE
    top = int(FMAX // float(np.max(freq)))
    h = np.arange(first, top + 1)
    w = (1.0 / h) / np.sqrt(1.0 + (h * f0 / tilt) ** 2) * (1.0 - depth * np.cos(2 * np.pi * h * width))
    w = w * np.where(h == 1, H1, 1.0)
    ph = 2 * np.pi * (D.phase(freq, n) + ph0)
    y = np.zeros(n)
    for k, a in zip(h, w):
        y += a * np.sin(k * ph)
    return y / np.sqrt(0.5 * np.sum(w ** 2))


def pitch_curve(f0, n, hard, r, window=TUNE_WINDOW):
    """The note's pitch, Hz per sample: the scoop, then the delayed vibrato and the jitter, the two together zero-mean
    over `window` (the tuning check's), so the note reads on its pitch."""
    t = P.times(n)
    rate, depth, onset, fade = VIB
    ramp = np.clip((t - onset) / fade, 0.0, 1.0)
    vib = P.lerp(depth, hard) * (0.5 - 0.5 * np.cos(np.pi * ramp)) * np.sin(
        2 * np.pi * rate * np.maximum(t - onset, 0.0) + r.uniform(0, 2 * np.pi))
    rms, bw = JITTER
    j = D.bfilt(r.standard_normal(n + SR), bw, 'lowpass', order=2)[SR:]
    a, b = int(window[0] * SR), min(n, int(window[1] * SR))
    j = j * rms / (np.std(j[a:b]) + 1e-12)
    mod = vib + j
    mod = mod - np.mean(mod[a:b])
    return f0 * 2.0 ** ((SCOOP[0] * np.exp(-t / SCOOP[1]) + mod) / 1200.0)


def tract(vowel, n, hold_s, f0):
    """The formants' frequencies and gains per sample, (5, n) each: LIPS opening into the vowel over ONSET_S, held,
    then closing towards CLOSE's vowel. F1 never sits under F1_FLOOR times the note (a singer's formant tuning on high
    notes: an "oo" on B4 opens its F1 to the note)."""
    t = P.times(n)
    u = 0.5 - 0.5 * np.cos(np.pi * np.clip(t / ONSET_S, 0.0, 1.0))
    start, length, end_v = CLOSE
    at = start * hold_s if hold_s >= 0.5 else hold_s          # a short note (a stab) closes in its release
    c = 0.5 - 0.5 * np.cos(np.pi * np.clip((t - at) / length, 0.0, 1.0))
    fs, gs = [], []
    for k, ((f_l, g_l), (f_v, g_v, _), (f_e, g_e, _)) in enumerate(zip(LIPS, VOWELS5[vowel], VOWELS5[end_v])):
        if k == 0:
            f_v, f_e = max(f_v, F1_FLOOR * f0), max(f_e, F1_FLOOR * f0)
        f_open = f_l * (f_v / f_l) ** u
        fs.append(f_open * (f_e / f_v) ** c)
        gs.append((g_l + (g_v - g_l) * u) * (1.0 + (g_e / g_v - 1.0) * c))
    return np.array(fs), np.array(gs)


def formants(x, vowel, fs, gs):
    """x through the five band-passes (unity peak each, at their own q) on the tract's tracks, plus the direct path."""
    y = DIRECT * x
    for k, (_, _, q) in enumerate(VOWELS5[vowel]):
        y = y + gs[k] * D.svf_tpt(x, fs[k], q=q)[1] / q
    return y


def voice(f0, vel, n, vowel, hold_s, release_s, r, drive=None, tick=True, window=TUNE_WINDOW):
    """One vox-synth note, stereo, n samples: the source with breath, the "h" and the tick, and its two doubles (one
    each side), each through the tract and the drive at 4x on its own (so no intermodulation lands by the
    fundamental), gated (held hold_s, released over release_s); before the high-pass (finish())."""
    hard = float(np.clip((vel - VOX_LAYERS[0]) / (VOX_LAYERS[1] - VOX_LAYERS[0]), 0.0, 1.0))
    t = P.times(n)
    f = pitch_curve(f0, n, hard, r, window)
    main = source(f, f0, n, hard, r.uniform())
    cents, delays, level = DOUBLE
    doubles = []
    for side, d in zip((-1, 1), delays):
        m = int(d * SR)
        x = source(f[:n - m] * 2.0 ** (side * cents / 1200.0), f0, n - m, hard, r.uniform(), first=2)
        doubles.append(np.concatenate([np.zeros(m), x * P.raised(n - m, 0.004)]))
    gate = P.raised(n, 0.006 + 0.006 * (1 - hard)) * D.taper(n, hold_s, release_s)
    level_b, hp_b = BREATH
    breath = level_b * P.hp_noise(r, n, hp_b, order=2)
    a_lvl, a_len = ASPIRATE
    aspirate = a_lvl * P.hp_noise(r, n, 1200.0, order=2) * np.exp(-t / (a_len / 3.0)) * P.raised(n, 0.003)
    fs, gs = tract(vowel, n, hold_s, f0)
    ym = formants((main + breath + aspirate) * gate, vowel, fs, gs)
    if tick:
        lvl, band, decay = TICK
        ym = ym + lvl * (0.5 + 0.5 * vel) * P.band_noise(r, n, *band) * D.env_exp(n, decay, 0.0005)
    ref = np.sqrt(np.mean(ym[: int((hold_s + 0.02) * SR)] ** 2)) * 3.0 + 1e-12
    g = P.lerp(DRIVE, hard) if drive is None else drive
    sat = lambda x: D.oversampled(lambda u: D.tanh_adaa(u, g), x / ref, 4) / np.tanh(g)  # noqa: E731
    y_main = sat(ym)
    ys = [sat(formants(dbl * gate, vowel, fs, gs)) for dbl in doubles]   # each voice driven alone: the doubles'
    y = np.stack([y_main + level * ys[0], y_main + level * ys[1]], 1)   # partials never mix with the note's
    return y / (1.0 + level) * gate[:, None]


def finish(y):
    """The high-pass (HP, 4th order), the low-mid dip and mono under 150 Hz."""
    y = D.peaking(D.bfilt(D.st(y), HP, 'highpass', order=4), *LOW_MID_DIP)
    return D.mono_below(y, 150.0)


# ------------------------------------------------------------------ the multisample
HOLD_S, RELEASE_S = 1.10, 0.25


def vox_synth(take):
    n = int((HOLD_S + RELEASE_S + 0.15) * SR)
    r = D.rng(take.kit, take.sound, take.layer, 'voice')
    y = voice(take.f0, take.vel, n, LAYER_VOWEL[take.layer], HOLD_S, RELEASE_S, r)
    return Out(finish(y) * P.vel_gain(take.vel, 6.0, VOX_LAYERS[0]), f0s=(take.f0,),
               meta={'vowel': LAYER_VOWEL[take.layer], 'zone': D.note_name(take.note)})


# ------------------------------------------------------------------ the stabs: gated, driven, in the dark hall
STAB_HOLD = (0.16, 0.04)    # held, then closed (s): the gate
STAB_DRIVE = 3.2
STAB_TUNE = (0.05, 0.17)    # a stab's tuning window: its gate closes at 0.16-0.20 s
HALL = dict(t60=3.6, size=1.1, damp_hz=3000.0, send_hp=350.0, level=0.35)
STABS = {'VoxStabOhE3': (52, 'oh'), 'VoxStabOoB3': (59, 'oo')}


def hall(y, n, key):
    """The kit's dark hall (its space seed) on y, returned alone, mono under 150 Hz: fed above HALL's send_hp."""
    send = D.bfilt(D.mono(y), HALL['send_hp'], 'highpass', order=4)
    rv = D.fdn_reverb(send, K.space_rng(key), t60=HALL['t60'], size=HALL['size'], damp_hz=HALL['damp_hz'],
                      n_tail=max(0, n - len(send)))[:n]
    return D.mono_below(rv, 150.0)


def vox_stab(take):
    note, vowel = STABS[take.item.name]
    hold, rel = STAB_HOLD
    n = int(3.4 * SR)
    r = D.rng(take.kit, take.item.name, 'voice')
    dry = voice(hz(note), 1.0, n, vowel, hold, rel, r, drive=STAB_DRIVE, window=STAB_TUNE)
    y = dry + HALL['level'] * hall(dry, n, 'vox stab')
    y = y * D.taper(n, 2.6, 0.7)[:, None]
    return Out(finish(y), f0s=(hz(note),), meta={'root': D.note_name(note), 'vowel': vowel, 'hall': HALL})


# ------------------------------------------------------------------ the riff
RIFF_STEPS = 32
# (16th, MIDI, length in 16ths, velocity): v 1.0 sings "oh", 0.5 "oo" (the multisample's layers)
RIFF = ((3, 52, 1.0, 1.0), (7, 52, 0.75, 0.5), (11, 55, 1.0, 1.0),
        (19, 52, 1.0, 1.0), (23, 52, 0.75, 0.5), (27, 59, 1.0, 1.0), (30, 62, 2.0, 1.0))
THROW_STEP = 30             # the riff's last stab, thrown into the hall and the echo, so it spills into the next bar
RELEASE_GRID_S = 0.030      # a riff note's release after its length


def riff_hit(note, length16, vel, s16):
    """One riff note as the sampler plays it: VoxSynth's file for its zone (every riff note is a zone) and the layer
    nearest its velocity, gated to its length plus RELEASE_GRID_S."""
    item, got, _ = Ph.kit_files(K, 'VoxSynth')
    if note not in item.notes:
        raise ValueError(f'riff note {D.note_name(note)} is not a VoxSynth zone')
    layer = 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - vel), -i))
    src = got[(note, layer, 1)].audio
    n_on = int(length16 * s16)
    m = min(len(src), n_on + int(RELEASE_GRID_S * SR))
    return src[:m] * D.taper(m, n_on / SR, RELEASE_GRID_S)[:, None]


def riff_parts(grid, notes, throws=(THROW_STEP,)):
    """The riff's notes on a grid: (the dry stabs, the thrown ones), each (L, 2)."""
    dry, thrown = np.zeros((grid.length, 2)), np.zeros((grid.length, 2))
    for step, note, length, vel in notes:
        Ph.place(thrown if step in throws else dry, riff_hit(note, length, vel, grid.s16), grid.at(step))
    return dry, thrown


RIFF_SENDS = dict(hall=0.25, throw_hall=1.0, throw_echo=1.0)


def vox_riff(take):
    """The riff on VoxSynth's files, a little of the dark hall on every stab and the throw's full hall and echo (the
    kit's dub ping-pong on a dotted 8th), round the loop."""
    G = Ph.Grid(K, RIFF_STEPS)
    L = G.length
    dry, thrown = riff_parts(G, RIFF)
    send_h = D.mono(RIFF_SENDS['hall'] * dry + RIFF_SENDS['throw_hall'] * thrown)
    space = {**S.SPACE, 't60': HALL['t60'], 'damp_hz': HALL['damp_hz'], 'send_hp': HALL['send_hp'],
             'predelay': K.STEP / 4, 'shimmer': 0.03}
    hall_rt = Ph.two_pass(lambda u: S.space(u, K.space_rng('vox riff'), **space), send_h, 0.02)
    echo = Ph.two_pass(lambda u: S.echo(u, K.space_rng('vox riff echo'), S.ECHO_STEPS * K.STEP,
                                        **{**S.ECHO, 'lp_hz': 2200.0, 'hp_hz': 400.0}),
                       D.mono(RIFF_SENDS['throw_echo'] * thrown), 0.02)
    y = dry + thrown + 0.30 * hall_rt + 0.45 * echo
    y = Ph.circular(lambda u: D.mono_below(D.bfilt(u, HP, 'highpass', order=4), 150.0), y)
    return Out(y, meta={'steps': RIFF_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'throw_step': THROW_STEP,
                        'notes': [{'step': s, 'note': D.note_name(m), 'length16': ln, 'vel': v,
                                   'vowel': LAYER_VOWEL[2 if v > 0.75 else 1]} for s, m, ln, v in RIFF]})


def riff_midi():
    """The riff as MIDI notes (16th, note, length, velocity, slide): velocity picks the vowel, as on the multisample."""
    return [(s, m, ln, v, False) for s, m, ln, v in RIFF]


R5 = ('r5',)
VOX_QA = {'tonal': False, 'why': 'a vocal synth: its breath sits between the harmonics and its vibrato and jitter move '
                                 'them, so the alias check would read the voice, not aliasing; tuned on its root'}
ITEMS = [
    Item('Vox', 'VoxSynth', 'lead', vox_synth, layers=VOX_LAYERS, notes=VOX_NOTES, sets=R5, qa=VOX_QA,
         about='vox synth, wordless: a glottal-like pulse, a detuned double, breath and a closed onset through five '
               'narrow formants, "oo" (v1) or "oh" (v2) closing as it dies, vibrato and jitter, driven; E3 G3 B3 D4 E4 '
               'G4 B4'),
] + [
    Item('Vox', name, 'stab', vox_stab, root=note, sets=R5,
         qa={**VOX_QA, 'tune_window': STAB_TUNE, 'why': VOX_QA['why'] + '; a stab is read over its gate, 50-170 ms'},
         about=f'vox synth stab on {D.note_name(note)}, "{vowel}": gated at 160 ms, driven, in the '
               'kit\'s dark hall; wordless')
    for name, (note, vowel) in STABS.items()
] + [
    Item('Loops', 'VoxSynthRiff', 'loop', vox_riff, loop=True, sets=R5,
         qa={'mud': 'lowcut', 'why': 'a vox-synth loop: the low cut, like the vox synth'},
         about='vox synth riff, 2 bars at 136 BPM on the kit swing: E3 "oh", E3 "oo", G3 "oh" answering the acid\'s '
               'off-beat leaps; then E3, E3, B3 and D4 thrown into the hall and a dotted-8th echo'),
]
