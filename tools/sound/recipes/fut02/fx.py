"""FUT02 effects for round 2: the one-bar laser fill and the 8-bar laser riser. Both carry the kit's laser pitch-drop
gesture (perc.laser, the zap's), both sit on the kit's 16th grid and swing at 130 BPM, both are in D minor and both end
exactly on the next downbeat (cut_end): they lead into the drop and stop where it lands.

- LaserFill: one bar. The lasers play a phrase in D minor: a 3-3-2 cell rising D5 F5 A5, its answer from C6 back down
  (C6 on beat 3, A5), then F5 and G5 on the 16ths of beat 4, and a long dive from three octaves up that lands on D5
  exactly at the downbeat. Under it the kit's own closed hat (a 16th crescendo) and electro clap (the backbeat, then a 32nd roll under
  the dive), played on the files exactly as written; the lasers alone go through a little of the kit's room. Round 1
  rated the glitch fill 3 ("a clearer musical gesture"): this one is a rhythm and a line that lands on the root.
- LaserRiser: 8 bars. A staircase of lasers climbs the D minor seventh arpeggio a bar at a time (D4 F4 A4 C5 D5 F5 A5,
  then Bb5 and C6 in the last bar), quarters for two bars, 8ths for four, 16ths for the last two, each falling an
  octave onto its note, quicker as they get denser. A laser rise glides under them from D4 up two octaves, its
  vibrato speeding up from one cycle a beat to one a 16th, onto D6 at the downbeat (the note the staircase leads to).
  Noise opens from a 400 Hz-3 kHz band to 3-15 kHz, and from bar 5 it pumps on the 16ths (ducked under each laser,
  swelling back), harder towards the end. The level rises 20 dB, and a parallel soft clip thickens the sum.

Both are high-passed at 150 Hz and mono under 150 Hz: nothing competes with the kick and sub.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import perc as Z

SR = D.SR
NOTE = {'D4': 62, 'F4': 65, 'G4': 67, 'A4': 69, 'C5': 72, 'D5': 74, 'F5': 77, 'G5': 79, 'A5': 81, 'Bb5': 82,
        'C6': 84, 'D6': 86}


def hz(name):
    return float(D.midi_hz(NOTE[name]))


def swung(G, s):
    """A fractional 16th (32nds, 64ths) as a sample position on the grid's swing: the first 16th of each 8th spans
    2 x swing of it, the second the rest, so a 32nd inside the swung 16th shrinks with it and every whole 16th lands
    where G.at puts it."""
    base = np.floor(s / 2) * 2
    f = s - base
    u = f * G.swing if f <= 1 else G.swing + (f - 1) * (1 - G.swing)
    return int(np.floor((base + 2 * u) * G.s16 + 0.5))


def kit_hit(name, layer, rr):
    """One of the kit's files exactly as written (stereo)."""
    return Ph.kit_files(K, name)[1][(None, layer, rr)].audio


def add(buf, x, pos):
    """Add x at pos; whatever runs past the end of buf is cut (the end is the downbeat)."""
    m = min(len(x), len(buf) - pos)
    if m > 0:
        buf[pos:pos + m] += D.st(x)[:m]


def room(x, t60, size, hp=300.0):
    """The kit's room on a mono send, high-passed so it stays out of the low mids. Stereo."""
    return D.fdn_reverb(D.bfilt(x, hp, 'highpass', order=2), K.space_rng('fx'), t60=t60, size=size,
                        damp_hz=7000.0)[:len(x)]


# ------------------------------------------------------------------ the laser fill
FILL_STEPS = 16
# the lasers: (16th, note, velocity, fall in octaves, fall in seconds, decay in seconds, pan)
FILL_LASERS = (
    (0, 'D5', 1.00, 2.0, 0.055, 0.12, 0.0),
    (3, 'F5', 0.75, 1.5, 0.045, 0.10, -0.35),
    (6, 'A5', 0.85, 1.5, 0.045, 0.10, 0.35),
    (8, 'C6', 0.95, 2.0, 0.050, 0.12, 0.0),
    (11, 'A5', 0.75, 1.5, 0.045, 0.09, 0.35),
    (12, 'F5', 0.80, 1.2, 0.035, 0.08, -0.35),
    (13, 'G5', 0.85, 1.2, 0.035, 0.08, 0.35),
)
FILL_DIVE = (14, 'D5', 3.0, 1.7)       # the dive: from this 16th, onto this note at the downbeat, octaves, curve
FILL_HATS = tuple((s, 1 + min(4, s // 4 + (1 if s % 2 == 0 else 0))) for s in range(14))   # (16th, layer): louder
FILL_CLAPS = ((4, 4), (12, 5), (14.0, 2), (14.5, 3), (15.0, 4), (15.5, 5))                # (16th, layer)
FILL_MIX = (1.0, 0.30, 0.55, 0.22)     # lasers, hats, claps, the lasers' room


def laser_fill(take):
    G = Ph.Grid(K, FILL_STEPS)
    n = G.length
    lasers = np.zeros((n, 2))
    send = np.zeros(n)
    r = take.rng('fill')
    for step, note, v, depth, drop, decay, p in FILL_LASERS:
        m = int((drop + 6 * decay) * SR)
        x = Z.laser(m, hz(note), depth, drop, amps=(1.0, 0.25 + 0.35 * v, 0.10 + 0.30 * v, 0.03 + 0.22 * v),
                    fm=0.4 + 1.6 * v, fm_tau=0.015 + 0.010 * v)
        x = v * (x * D.env_exp(m, decay, 0.0004) * D.taper(m, drop + 4 * decay, 2 * decay) + Z.spark(r, m, v))
        add(lasers, Ph.pan(x, p), G.at(step))
        send[G.at(step):G.at(step) + m] += x[:max(0, n - G.at(step))]
    step, note, depth, curve = FILL_DIVE
    a = G.at(step)
    m = n - a
    dive = Z.laser(m, hz(note), depth, m / SR, curve=curve, amps=(1.0, 0.5, 0.35, 0.2), fm=1.5, fm_tau=0.06)
    dive *= D.undb(-6.0 + 6.0 * np.linspace(0.0, 1.0, m)) * D.fade(np.ones(m), 0.002, 0.0)
    dive += Z.spark(r, m, 1.0)
    add(lasers, Z.widen(dive, 0.35), a)
    send[a:] += dive
    hats = np.zeros((n, 2))
    for k, (s, layer) in enumerate(FILL_HATS):
        add(hats, kit_hit('ClosedHat', layer, 1 + k % 3), G.at(s))
    claps = np.zeros((n, 2))
    for k, (s, layer) in enumerate(FILL_CLAPS):
        add(claps, kit_hit('ElectroClap', layer, 1 + k % 2), swung(G, s))
    g_l, g_h, g_c, g_r = FILL_MIX
    y = g_l * lasers + g_h * hats + g_c * claps + g_r * room(send, 0.7, 0.5)
    y = D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)
    meta = {'grid': {'bpm': K.BPM, 'steps': FILL_STEPS, 'swing': K.SWING}, 'end': 'the next downbeat',
            'lasers': [[s, nt] for s, nt, *_ in FILL_LASERS] + [[FILL_DIVE[0], FILL_DIVE[1] + ' (dive, lands on the '
                                                                  'downbeat)']]}
    return Out(y, meta=meta)


# ------------------------------------------------------------------ the laser riser
RISER_STEPS = 128                      # 8 bars of 16ths
# the staircase, a bar at a time: (first bar, last bar, note, 16ths between lasers)
STAIRS = ((0, 0, 'D4', 4), (1, 1, 'F4', 4), (2, 2, 'A4', 2), (3, 3, 'C5', 2), (4, 4, 'D5', 2), (5, 5, 'F5', 2),
          (6, 6, 'A5', 1), (7, 7, 'Bb5', 1))
STAIRS_LAST = (120, 'C6')              # from this 16th (the last bar's beat 3) the staircase is on C6, leading to D6
STAIR_SHAPE = {4: (0.060, 0.14), 2: (0.045, 0.09), 1: (0.030, 0.05)}   # 16ths apart: (fall, decay) in seconds
RISE = ('D4', 2.0, 1.6)                # the laser rise: from, octaves up (onto D6 at the downbeat), curve
VIBRATO = (1.0, 4.0, 18.0)             # cycles a beat at the start and the end, depth in cents
NOISE_BAND = ((400.0, 3000.0), (3000.0, 15000.0))   # the noise's band at the start and the end, Hz
PULSE = (64, 0.65)                     # from this 16th the noise ducks under each 16th's laser and swells back, up to
                                       # this depth: it pumps on the 16ths, out of the lasers' way
RISER_MIX = (0.55, 0.30, 0.90, 0.30)   # staircase, rise, noise, the room
RISER_RANGE_DB = 20.0
RISER_CURVE = 1.0                      # the level's rise in dB goes as u^this (u: 0 to 1 over the 8 bars)
NOISE_CLIP = 0.45                      # the noise's soft clip (re its RMS): peaks over about 2 sigma are rounded off
STAIR_SPARK = 0.5                      # the staircase's sparks, against the zap's
RISER_DENSITY = (0.6, 2.5)             # a parallel soft clip (4x) on the sum, re its peak: wet share, drive. It takes the
                                       # lasers' spikes down to the noise, so the riser is dense enough for -14 LUFS


def stair_steps():
    """The staircase's lasers: (16th, note, 16ths to the next)."""
    out = []
    for b0, b1, note, every in STAIRS:
        for s in range(16 * b0, 16 * (b1 + 1), every):
            out.append((s, STAIRS_LAST[1] if s >= STAIRS_LAST[0] else note, every))
    return out


def laser_riser(take):
    G = Ph.Grid(K, RISER_STEPS)
    n = G.length
    t = np.arange(n) / SR
    u = t / t[-1]
    r = take.rng('riser')

    # the staircase: each laser falls an octave onto its note, faster and shorter as the steps get denser
    stairs = np.zeros((n, 2))
    send = np.zeros(n)
    steps = stair_steps()
    for k, (s, note, every) in enumerate(steps):
        drop, decay = STAIR_SHAPE[every]
        v = 0.55 + 0.45 * s / RISER_STEPS
        m = int((drop + 6 * decay) * SR)
        x = Z.laser(m, hz(note), 1.0, drop, amps=(1.0, 0.25 + 0.35 * v, 0.10 + 0.30 * v, 0.03 + 0.22 * v),
                    fm=0.4 + 1.6 * v, fm_tau=0.012 + 0.008 * v)
        x = x * D.env_exp(m, decay, 0.0004) * D.taper(m, drop + 4 * decay, 2 * decay) + STAIR_SPARK * Z.spark(r, m, v)
        p = G.at(s)
        add(stairs, Ph.pan(x, 0.4 * (-1) ** k), p)
        send[p:p + m] += x[:max(0, n - p)]

    # the laser rise: D4 up two octaves onto D6 at the downbeat, a vibrato that speeds up with the tempo
    k4 = 4
    tk = np.arange(n * k4) / (SR * k4)
    uk = tk / tk[-1]
    lo, octaves, curve = RISE
    c0, c1, depth = VIBRATO
    rate = (c0 + (c1 - c0) * uk ** 2) / K.BEAT                          # Hz: a beat's cycles, faster towards the end
    vib = depth / 1200 * np.sin(2 * np.pi * np.cumsum(rate) / (SR * k4)) * uk
    f = hz(lo) * 2.0 ** (octaves * uk ** curve + vib)
    rise = Z.wire_tone(f, (1.0, 0.40, 0.22, 0.12), fm=0.3 + 1.2 * uk ** 2, k=k4)
    rise = Z.widen(rise, 0.3)

    # the noise: a band that opens and rises, mid and a partly shared side, pulsing on the 16ths from bar 5
    lo0, hi0 = NOISE_BAND[0]
    lo1, hi1 = NOISE_BAND[1]
    hp = lo0 * (lo1 / lo0) ** u
    lp = hi0 * (hi1 / hi0) ** u
    nz = [r.standard_normal(n), take.rng('riser', 'side').standard_normal(n)]
    band = [D.svf_tpt(D.svf_tpt(z, hp, q=0.7)[2], lp, q=0.7)[0] for z in nz]
    band = [P.soft_clip(b / np.std(b), NOISE_CLIP) for b in band]   # its Gaussian spikes off: a dense wash
    noise = np.stack([band[0] + 0.45 * band[1], band[0] - 0.45 * band[1]], 1)
    p0, pd = PULSE
    pulse = np.ones(n)
    for s in range(p0, RISER_STEPS):
        a, b = G.at(s), (G.at(s + 1) if s + 1 < RISER_STEPS else n)
        d = pd * ((s - p0) / (RISER_STEPS - p0)) ** 0.7
        pulse[a:b] = 1.0 - d * (1.0 - np.linspace(0.0, 1.0, b - a)) ** 2   # ducked under each laser, swelling back
    noise *= (pulse * (0.25 + 0.75 * u ** 1.5))[:, None]

    g_s, g_r, g_n, g_room = RISER_MIX
    y = g_s * stairs + g_r * rise + g_n * 0.15 * noise + g_room * room(send + 0.5 * D.mono(rise), 1.4, 0.8)
    y *= D.undb(-RISER_RANGE_DB + RISER_RANGE_DB * u ** RISER_CURVE)[:, None]
    wet, drive = RISER_DENSITY
    y = y / np.abs(y).max()
    y = (1 - wet) * y + wet * D.oversampled(lambda v: D.tanh_adaa(v, drive), y) / np.tanh(drive)
    y = D.mono_below(D.bfilt(y, 150.0, 'highpass', order=4), 150.0)
    meta = {'grid': {'bpm': K.BPM, 'steps': RISER_STEPS, 'swing': K.SWING}, 'end': 'the next downbeat (D6)',
            'stairs': [[s, nt] for s, nt, _ in steps]}
    return Out(y, meta=meta)


ITEMS = [
    Item('FX', 'LaserFill', 'fx', laser_fill, root=NOTE['D5'], sets=('r2',), flags={'cut_end'}, tags=('fill',),
         qa={'why': 'it ends on the next downbeat: the dive lands on D5 there, where the drop takes over'},
         about='one-bar laser fill at 130 BPM in D minor: lasers D5 F5 A5 | C6 A5 F5 G5 on the swing, the kit hat and '
               'clap under them, a three-octave dive landing on D5 at the downbeat'),
    Item('FX', 'LaserRiser', 'fx', laser_riser, root=NOTE['D6'], level=-14.0, sets=('r2',), flags={'cut_end'},
         tags=('riser',),
         qa={'why': 'end-aligned: it stays loud to the downbeat, where the next section takes over'},
         about='8-bar laser riser at 130 BPM: a laser staircase up the Dm7 arpeggio (quarters, 8ths, 16ths), a laser '
               'rise onto D6 and opening, pulsing noise; ends on the downbeat'),
]
