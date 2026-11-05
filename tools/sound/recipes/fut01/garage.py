"""FUT01 round 4 drums (set r4): a modern UK garage kit with classic drum-machine and sampler punch, the kit's alloy
colour kept only as a hint in the perc.

- KickGarage, kick B: round 1's split design kept (a clean F#1 sub, a short knock, grit only on top) and retimed for
  2-step: a snappier pitch drop, a short sub (-20 dB by about 190 ms, under two 16ths at 134 BPM), a knock on the
  sweep's own second harmonic over its first 45 ms (100-170 Hz, phase-locked to the sub) plus round 1's driven knock
  above 110 Hz, and a drum-machine click on top: a short pulse and a noise burst through a driven stage that only ever
  sounds above 1 kHz. Kick A stays KickSplit (drums.py), round 1's pick, for the A/B.
- ClapGarage: a crisp clap layered with a snappy snare in a short bright room. Four hands of band noise inside 24 ms
  (the first within 1.2 dB of the loudest), a snare body on F#3 and C#4 that drops into pitch (150-300 Hz, gone in
  about 60 ms), snappy wires, and a 0.6 s bright room sent from 700 Hz up; then a gentle soft clip.
- HatClosed, HatOpen: classic metal hats: six square waves on inharmonic pitches (rendered at 4x) and a little noise,
  band-passed high; the closed one gone inside a 16th, the open one ringing for an 8th or so (the loops choke it on
  the next closed hat, as a sampler's hat group does).
- Tambourine: the 16th layer that carries the shuffle: jingle pairs struck within a few ms of each other (three
  inharmonic modes each, 5-11 kHz) over a short burst of band noise; harder strokes strike more jingles, brighter.
- RimShot: a short rim click for ghost notes: two damped modes and a click, with one mode on the kit's alloy ratio
  (1.483) as the hint of the alloy colour.

Every drum has velocity layers that move brightness and transient, not only level, and round robins that move the
noise and the timing a little. The kick passes the mud gate; everything else is high-passed far above the kick and sub.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound.item import Item
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import drums as Dr

SR = D.SR
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
F_KICK = float(D.midi_hz(K.KICK_NOTE))   # F#1, 46.25 Hz
ALLOY = np.array(K.RATIOS)


# ------------------------------------------------------------------ kick B (round 4): garage
GK_SWEEP = dict(f_hi=(300.0, 620.0), tau1=(0.0062, 0.0046), tau2=0.016, w2=0.26)   # soft to hard hits
GK_SUB = dict(tau=(0.138, 0.146), power=3.0, taper=(0.22, 0.08), level=1.08)        # exp(-(t / tau)^power)
GK_KNOCK = dict(level=0.56, on=0.006, rise=0.008, hold=0.034, tau=0.011, band=(85.0, 170.0))
GK_DUCK = 0.45               # the sub's dip under the knock, so the knock never tops the click
GK_SPLIT_KNOCK = (0.45, 0.25)  # round 1's driven knock (drums.split_knock): a + b v
GK_TOP = (0.40, 0.88)        # the click's level: a + b v; on the top layer it sets the peak (in the first
                             # ms), 1-2 dB over the knock's, for a crest of about 6 dB


def garage_top(v, r, n):
    """The kick's top: a 0.3 ms pulse and a noise burst (1.5-3 kHz up), driven at 4x, then nothing under 1 kHz and a
    low-pass that opens with velocity; (n,), its first 60 ms sounding."""
    nt = int(0.06 * SR)
    w = 14
    pulse = np.zeros(nt)
    pulse[:w] = np.hanning(w)
    nb = P.hp_noise(r, nt, 1500.0 + 1500.0 * v, order=2) * D.env_exp(nt, 0.0010 + 0.0012 * v, 0.0001)
    tok = np.sin(2 * np.pi * 1650.0 * P.times(nt)) * D.env_exp(nt, 0.0025, 0.0002)   # a short wooden "tok"
    x = (0.9 + 0.6 * v) * pulse + (0.35 + 0.45 * v) * nb + 0.25 * tok
    grit = D.oversampled(lambda u: D.tanh_adaa(u, 2.0 + 3.0 * v), x, 4)
    top = D.bfilt(D.lr4(grit, 1000.0, 'highpass'), 7000.0 + 7000.0 * v, 'lowpass')
    top *= D.taper(nt, 0.035, 0.02)
    out = np.zeros(n)
    out[:nt] = top
    return out


def kick_garage(take):
    """Kick B for round 4: a clean F#1 sub on a snappy sweep that holds, then drops by about 190 ms; a phase-locked
    100-170 Hz knock over its first 45 ms and round 1's driven knock above 110 Hz (gone by 95 ms); a drum-machine click
    that only ever sounds above 1 kHz."""
    v = take.vel
    r = take.rng('garage')
    n = int(0.6 * SR)
    t = P.times(n)
    sw = GK_SWEEP
    f_hi = P.lerp(sw['f_hi'], v) * 2 ** (r.normal(0, 10) / 1200)
    ph = P.sweep(n, f_hi, P.lerp(sw['tau1'], v) * (1 + 0.04 * r.uniform(-1, 1)), sw['tau2'], sw['w2'], F_KICK)
    s = np.sin(ph)
    k = GK_KNOCK
    rise = np.clip((t - k['on']) / k['rise'], 0.0, 1.0)
    knock_env = (0.5 - 0.5 * np.cos(np.pi * rise)) * P.gate(t, k['hold'], k['tau']) * D.taper(n, 0.06, 0.04)
    a_sub = np.exp(-(t / P.lerp(GK_SUB['tau'], v)) ** GK_SUB['power']) * D.taper(n, *GK_SUB['taper'])
    sub = GK_SUB['level'] * D.lr4(s * a_sub * (1.0 - GK_DUCK * knock_env), Dr.SPLIT_XOVER, 'lowpass')
    h2 = D.bfilt(D.bfilt(np.sin(2 * ph) * knock_env, k['band'][0], 'highpass', order=2), k['band'][1], 'lowpass',
                 order=4)
    y = sub + P.lerp(GK_SPLIT_KNOCK, v) * Dr.split_knock(s, t, v, n) + k['level'] * (0.4 + 0.6 * v) * h2 \
        + P.lerp(GK_TOP, v) * garage_top(v, take.rng('garage', 'top'), n)
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the garage clap: clap + snappy snare + short room
CLAP_HANDS = ((0.0, -1.2), (0.0072, -0.6), (0.0150, -0.9), (0.0236, 0.0))   # (s at velocity 0.6, dB): 4 hands
CLAP_HAND_TAU = 0.0030
CLAP_TAIL_TAU = 0.055        # the last hand's tail
CLAP_BAND = (850.0, 5200.0, 1250.0)   # the hands' band and its presence peak at velocity 0 (both rise with velocity)
SNARE_TONES = (float(D.midi_hz(54)), float(D.midi_hz(61)))   # F#3 and C#4: the body, 185 and 277 Hz
GARAGE_MIX = dict(clap=0.62, tail=0.30, body=0.26, wires=0.26, snap=0.20, room=0.17)   # the body: a 150-400 Hz
                             # share of about -9 dB, inside the standard's -15 to -8
GARAGE_ROOM = dict(t60=0.6, size=0.30, damp_hz=12000.0, send_hp=700.0)
GARAGE_FADE = (0.33, 0.14)   # gone by 0.47 s: one 400 ms loudness block
GARAGE_CLIP = 2.2


def clap_garage(take):
    """The garage clap-snare: four hands of band noise inside 24 ms with a short tail, a snare body on F#3 and C#4
    dropping into pitch, snappy wires, a short bright room, soft-clipped. Harder hits: tighter hands, a brighter band,
    more snap and wires, a deeper pitch drop."""
    v = take.vel
    hands = lambda *extra: D.rng(take.kit, take.item.name, take.rr, *extra)   # per round robin: every layer claps
    r = hands('timing')                                                        # the same noise
    n = int(1.2 * SR)
    t = P.times(n)
    spread = P.lerp((1.15, 0.9), v)
    env = np.zeros(n)
    for at, g in CLAP_HANDS:
        a = int(at * spread * SR * (1 + 0.04 * r.uniform(-1, 1)))
        env[a:] += D.undb(g) * D.env_exp(n - a, CLAP_HAND_TAU, 0.0003)
    env[a:] += GARAGE_MIX['tail'] * D.env_exp(n - a, CLAP_TAIL_TAU, 0.001)
    lo, hi, peak = CLAP_BAND
    nz = [D.peaking(P.band_noise(hands('hand', ch), n, lo * (1 + 0.15 * v), hi * (1 + 0.6 * v)),
                    peak * (0.9 + 0.3 * v), 5.0, 1.4) for ch in (0, 1)]
    clap = np.stack([nz[0], 0.72 * nz[0] + 0.28 * nz[1]], 1) * env[:, None]
    # the snare: a body that drops into pitch, wires and a snap
    bend = (0.12 + 0.25 * v) * np.exp(-t / 0.007)
    body = sum(a * np.sin(2 * np.pi * D.phase(f * (1 + bend), n)) * P.gate(t, 0.003, d)
               for f, a, d in zip(SNARE_TONES, (1.0, 0.55), (0.032 + 0.008 * v, 0.022)))
    body = body * D.env_exp(n, 1.0, 0.0004)
    wires = D.bfilt(P.band_noise(take.rng('wires'), n, 1800.0, 9000.0 + 4000.0 * v), 4500.0, 'highpass', order=1)
    wires = wires * D.env_exp(n, 0.060 + 0.030 * v, 0.0006)
    snap = P.hp_noise(take.rng('snap'), n, 4000.0, order=2) * D.env_exp(n, 0.0025, 0.0002)
    m = GARAGE_MIX
    y = m['clap'] * (0.8 + 0.2 * v) * clap + D.st(m['body'] * (0.7 + 0.3 * v) * body
                                                  + m['wires'] * (0.6 + 0.4 * v) * wires
                                                  + m['snap'] * (0.3 + 0.7 * v) * snap)
    room_in = D.bfilt(D.mono(y), GARAGE_ROOM['send_hp'], 'highpass', order=4)
    room = D.fdn_reverb(room_in, K.space_rng('garage room'), t60=GARAGE_ROOM['t60'], size=GARAGE_ROOM['size'],
                        damp_hz=GARAGE_ROOM['damp_hz'])[:n]
    y = y + m['room'] * room
    y = D.oversampled(lambda u: P.soft_clip(u, GARAGE_CLIP), y)
    return P.low_clean(y) * D.taper(n, *GARAGE_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the metal hats: closed and open
HAT_TONES = (263.0, 401.0, 527.0, 678.0, 811.0, 1043.0)   # six squares, inharmonic (Hz)
HAT_SHAPES = {'closed': dict(tau=(0.016, 0.026), n=0.30, fade=(0.16, 0.08)),
              'open': dict(tau=(0.140, 0.220), n=0.9, fade=(0.55, 0.25))}


def squares(n, r, k=4):
    """The six squares at k x (each the difference of two saws half a cycle apart), summed: (n * k,)."""
    out = np.zeros(n * k)
    for f in HAT_TONES:
        ph = r.uniform()
        out += D.polyblep_saw(f, n, ph, k=k) - D.polyblep_saw(f, n, (ph + 0.5) % 1.0, k=k)
    return out / len(HAT_TONES)


def metal_hat(take, shape):
    """Squares and noise, band-passed high at 4x, then down; each side its own noise. Harder hits: brighter, a sharper
    tick and a longer ring."""
    v = take.vel
    sh = HAT_SHAPES[shape]
    n = int(sh['n'] * SR)
    r = D.rng(take.kit, take.item.name, 'tones')       # the same metal on every take: only the noise moves
    k = 4
    metal = D.down(D.bfilt(squares(n, r, k), (7000.0, 16000.0), 'bandpass', order=2), k)
    sides = []
    for ch in (0, 1):
        nz = P.hp_noise(take.rng('noise', ch), n, 7000.0, order=2)
        sides.append(0.7 * metal + 0.3 * (0.6 * P.hp_noise(take.rng('noise'), n, 7000.0, order=2) + 0.4 * nz))
    x = np.stack(sides, 1)
    x = D.bfilt(D.bfilt(x, 6500.0 - 1000.0 * v, 'highpass', order=4), 9000.0 + 8000.0 * v, 'lowpass', order=2)
    tick = D.bfilt(take.rng('tick').uniform(-1, 1, n), (8000.0, 16000.0), 'bandpass', order=2)
    tick *= D.env_exp(n, 0.0010, 0.0002)
    env = D.env_exp(n, P.lerp(sh['tau'], v), 0.0004)
    x = level_by_env(x, env)
    tick = tick / (np.sqrt(np.mean(tick[:int(0.002 * SR)] ** 2)) + 1e-12)
    y = x * env[:, None] + 0.02 * (0.3 + 0.7 * v) * D.st(tick)
    return y * D.taper(n, *sh['fade'])[:, None] * P.vel_gain(v, 13.0)


def level_by_env(x, env):
    """x (n, 2) set to one level under env (its envelope-weighted RMS), so round robins differ in their noise, not
    their level."""
    w = env ** 2
    return x / np.sqrt(np.sum(w[:, None] * x ** 2) / (2 * np.sum(w)) + 1e-20)


def hat_closed(take):
    return metal_hat(take, 'closed')


def hat_open(take):
    return metal_hat(take, 'open')


# ------------------------------------------------------------------ tambourine: the 16ths
TAMB_LAYERS = (0.3, 0.55, 0.8, 1.0)
TAMB_JINGLE = (5200.0, 1.37, 1.92)    # each jingle's first mode (Hz, before its own spread) and the other two ratios
TAMB_S = 0.32


def tambourine(take):
    """One tambourine stroke: 8-18 jingle pairs struck inside 6-9 ms (harder strokes more and sooner), each three
    damped inharmonic modes from about 5 kHz, over a burst of 6-15 kHz noise; each side strikes its own jingles a
    little apart, so the stroke is wide but its mono sum whole."""
    v = take.vel
    r = take.rng('jingles')                                  # each stroke's timing and spread
    metal = D.rng(take.kit, take.item.name, 'metal')         # the jingles themselves, the same on every stroke
    n = int(TAMB_S * SR)
    t = P.times(n)
    count = int(round(8 + 10 * v))
    f_up = 0.9 + 0.25 * v                     # harder strokes ring higher
    spread = P.lerp((0.009, 0.006), v)
    out = np.zeros((n, 2))
    f1, a2, a3 = TAMB_JINGLE
    for j in range(count):
        f = f1 * f_up * (1 + 0.35 * metal.uniform())
        at = int(spread * SR * r.uniform() ** 1.5)
        d = (0.012 + 0.018 * v) * (0.7 + 0.6 * metal.uniform())  # short: a stroke clears its 16th
        ir = D.modal_ir(f, (1.0, a2, a3), (d, 0.7 * d, 0.5 * d), (1.0, 0.6, 0.35), n - at, r)
        pan = 0.5 + 0.35 * r.uniform(-1, 1)
        out[at:, 0] += pan * ir
        out[at:, 1] += (1 - pan) * ir
    env = D.env_exp(n, 0.012 + 0.018 * v)
    out = level_by_env(out, env)
    burst = P.band_noise(take.rng('burst'), n, 6000.0, 15000.0) * D.env_exp(n, 0.004 + 0.006 * v, 0.0008)
    burst = level_by_env(D.st(burst), D.env_exp(n, 0.004 + 0.006 * v))
    att = P.lerp((0.004, 0.0015), v)
    y = level_by_env(out + (0.15 + 0.55 * v) * burst, env) * P.raised(n, att)[:, None]
    y = D.bfilt(y, 3500.0, 'highpass', order=4)
    return D.mono_below(y, 150.0) * D.taper(n, TAMB_S - 0.09, 0.08)[:, None] * P.vel_gain(v, 12.0, TAMB_LAYERS[0])


# ------------------------------------------------------------------ rim: the ghosts, with a hint of alloy
RIM_F = 1720.0               # the rim's main mode (Hz); a second mode at 0.29 of it, one on the alloy ratio 1.483
RIM_MODES = ((1.0, 1.0, 0.011), (0.29, 0.55, 0.016), (ALLOY[1], 0.32, 0.007))   # (ratio, level, decay s)


def rim_shot(take):
    """A short rim click: three damped modes (the third on the kit's alloy ratio 1.483) struck by a 0.4 ms pulse, a
    click of noise over them, a touch of drive at 4x; gone in about 40 ms. Harder hits: a brighter strike, more click,
    a little more drive."""
    v = take.vel
    r = take.rng('rim')
    n = int(0.25 * SR)
    t = P.times(n)
    f0 = RIM_F * (1 + 0.004 * r.uniform(-1, 1))
    phases = D.rng(take.kit, take.item.name, 'phases')
    body = sum(a * np.exp(-t / (d * (0.85 + 0.3 * v))) * np.sin(2 * np.pi * f0 * ratio * t + phases.uniform(0, 6.283))
               for ratio, a, d in RIM_MODES)
    body *= D.env_exp(n, 1.0, 0.0002)
    click = P.hp_noise(take.rng('rim', 'click'), n, 3000.0, order=2) * D.env_exp(n, 0.0012, 0.0001)
    x = body + (0.25 + 0.5 * v) * click
    x = D.oversampled(lambda u: D.tanh_adaa(u, 1.2 + 1.5 * v) / np.tanh(1.2 + 1.5 * v), x, 4)
    x = D.bfilt(D.bfilt(x, 350.0, 'highpass', order=4), 8000.0 + 8000.0 * v, 'lowpass', order=2)
    y = np.stack([x, 0.92 * x + 0.08 * np.roll(x, 12)], 1)
    return y * D.taper(n, 0.12, 0.08)[:, None] * P.vel_gain(v, 12.0)


R4 = ('r4',)
ITEMS = [
    Item('Drums', 'KickGarage', 'kick', kick_garage, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=R4,
         about='kick B, garage: a clean F#1 sub on a snappy sweep, gone by about 190 ms; a phase-locked 100-170 Hz '
               'knock over its first 45 ms and a driven knock gone by 95 ms; a drum-machine click above 1 kHz'),
    Item('Drums', 'ClapGarage', 'snare', clap_garage, layers=LAYERS, rr=2, sets=R4,
         about='garage clap-snare: four hands inside 24 ms, a snare body on F#3 and C#4, snappy wires, a short bright '
               'room (0.6 s), soft-clipped'),
    Item('Drums', 'HatClosed', 'hat', hat_closed, layers=LAYERS, rr=3, sets=R4,
         about='closed hat: six squares on inharmonic pitches and noise, band-passed high at 4x, gone inside a 16th'),
    Item('Drums', 'HatOpen', 'hat', hat_open, layers=LAYERS, rr=2, sets=R4,
         about='open hat: the closed hat\'s metal ringing for about an 8th; the loops choke it on the next closed hat'),
    Item('Perc', 'Tambourine', 'hat', tambourine, layers=TAMB_LAYERS, rr=4, sets=R4,
         about='tambourine for swung 16ths: 8-18 struck jingles of three inharmonic modes from 5 kHz over a noise '
               'burst, wide, its mono sum whole'),
    Item('Perc', 'RimShot', 'perc', rim_shot, layers=LAYERS, rr=2, sets=R4,
         about='rim click for ghost notes: three damped modes (one on the alloy ratio 1.483) and a click, gone in '
               'about 40 ms'),
]
