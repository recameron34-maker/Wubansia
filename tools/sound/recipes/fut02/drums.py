"""FUT02 drums for rounds 2 and 3: the split kick, the electro clap, the closed and open hats and their 2-bar preview
loop. Round 3 changes the hats only (their tick and their width, see below); the kick and the clap are round 2's, the
same bytes.

Every drum has 5 velocity layers (2 round robins, the hats 3). Velocity moves the brightness, the transient and the
pitch envelope, not only the level.

- KickSplit: round 1's pick (FUT01's A, "Split") rebuilt on this kit: a clean sine sub on A1 (the key's fifth), a
  short knock above 110 Hz with only a gentle saturation on it, and a top above 1 kHz that carries all the drive: a
  chrome FM click, a 9-to-2 kHz laser tick (the kit's pitch-drop gesture in miniature) and a ring of the kit's
  stiff-wire partials on A6. The sub holds for about 60 ms and is 20 dB down by about 0.12 s, so the rolling bass's
  first note of each beat (on the e, 115 ms after the kick) has the low end to itself; a quiet tail, 30-40 dB down,
  carries the A1 on and is exactly zero at 0.40 s, inside the 461 ms beat of four on the floor.
- ElectroClap: four flams of band noise with a short tail in the kit's room, layered with a short snare: a body on D4
  (the key's root) with the wire's partials, dropping into pitch, and bright wires.
- ClosedHat and OpenHat: one metal for both, six square oscillators on the stiff wire's partials 5, 8, 11, 14, 17 and
  20 over a 120 Hz string, where its stiffness has pulled them apart (1 : 1.62 : 2.28 : 2.97 : 3.72 : 4.53, the way
  the classic six-oscillator hat uses inharmonic squares), over noise. The closed hat is 30 dB down inside a 16th;
  the open one sizzles for a beat. They share a choke group: a closed hat cuts the open one (meta 'choke'). Round 3:
  the tick on the front is spread over about 3 ms instead of a 1 ms needle that stood 11 dB over the metal (the hats
  read about 4 dB louder at the same peak, crest 20.7 and 15.2 dB against 23.7 and 18.3), and the open hat spreads
  (side over mid about -4 dB at 8 kHz, the closed hat about -13), its side a second noise of its own, so it sits in the
  middle (round 3's first pass took the mid's own noise out of the left and doubled it on the right: the open hat
  read 2.6 dB louder on the right).
- HatLoop: a 2-bar preview of the pair, closed 16ths around an open hat on every off-beat, played on the files as
  written, each open hat choked by the next closed one (a 4 ms fade, the sampler's choke).

Everything but the kick is high-passed far above the kick and sub (low_clean) and mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K

SR = D.SR
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
RATIOS = np.array(K.RATIOS)
F_KICK = float(D.midi_hz(K.KICK_NOTE))  # A1, 55 Hz
# the stiffness B of the wire behind RATIOS (partial 7 = 7 sqrt((1 + 49 B) / (1 + B))), to reach its higher partials
STIFF = ((RATIOS[6] / 7) ** 2 - 1) / (49 - (RATIOS[6] / 7) ** 2)


def wire(n):
    """The stiff wire's partial n, as a ratio to its fundamental: RATIOS for n = 1..7, and on up the same curve."""
    n = np.asarray(n, dtype=float)
    return n * np.sqrt((1 + STIFF * n * n) / (1 + STIFF))


# ------------------------------------------------------------------ the split kick
XOVER = 110.0                # sub / knock crossover (LR4, sums flat)
# the sub holds, then drops: exp(-(t / tau) ** 4), tau = 82-92 ms soft to hard, so it is 20 dB down by about 0.12 s,
# before the rolling bass's first note on the e (115 ms after the kick) has swelled in. Under the drop a quiet tail
# (7% of the sub, about 23 dB down at the start, decaying with 0.15 s) carries the A1 on, about 30-40 dB down, so the
# pitch still reads where QA tunes a kick (0.10-0.25 s); a raised-cosine fade ends it at exactly 0 by 0.40 s, inside
# the 461 ms beat. The first render's sub (exp(-(t / 0.175) ** 2), still 6 dB down at 115 ms) rang under every e-note
SUB_TAU = (0.082, 0.010)     # tau = SUB_TAU[0] + SUB_TAU[1] * velocity, seconds
SUB_POW = 4.0
SUB_TAIL = (0.07, 0.15)      # the tail: its share of the sub, and its time constant (s)
SUB_END = (0.30, 0.10)       # the fade: from 0.30 s, exactly 0 at 0.40 s
RING_SCALE = 0.25            # the wire ring against the click before the top's drive


def sub_env(t, v):
    """The split kick's sub envelope at times t (s) for velocity v: the hold-then-drop, the quiet tail under it, the
    fade."""
    k, tau2 = SUB_TAIL
    hold = np.exp(-(t / (SUB_TAU[0] + SUB_TAU[1] * v)) ** SUB_POW)
    return ((1 - k) * hold + k * np.exp(-t / tau2)) * D.taper(len(t), *SUB_END)


def kick_split(take):
    """Three layers shaped apart, so no drive sits on the low end: the clean sub on the sweep, a gently saturated
    knock above 110 Hz that is gone by 85 ms, and the top (FM click, laser tick, noise, wire ring) whose drive is
    high-passed at 1 kHz."""
    v = take.vel
    r = take.rng('split')
    n = int(0.6 * SR)
    t = P.times(n)

    # sweep: harder hits start higher and snap faster; the round robin moves the start and the snap, never the end
    f_hi = (300.0 + 340.0 * v) * 2 ** (r.normal(0, 10) / 1200)
    tau1 = (0.0080 - 0.0020 * v) * (1 + 0.04 * r.uniform(-1, 1))
    s = np.sin(P.sweep(n, f_hi, tau1, 0.014, 0.26, F_KICK))

    # 1. sub: clean, hold-then-drop under a quiet tail (sub_env), exactly zero from 0.40 s
    a_sub = sub_env(t, v)
    sub = D.lr4(s * a_sub, XOVER, 'lowpass')

    # 2. knock: the same phase, a gentle biased tanh at 4x (no fold: the body is never hard-driven), high-passed,
    #    dipped at 255 Hz, gone by 85 ms
    kd = D.oversampled(lambda u: D.tanh_adaa(u, 1.5 + 2.5 * v, bias=0.08), s * np.exp(-t / 0.045), 4)
    a_k = P.gate(t, 0.009 + 0.004 * v, 0.020 + 0.005 * v) * D.taper(n, 0.040, 0.045)
    knock = D.peaking(D.lr4(kd * a_k, XOVER, 'highpass'), 255.0, -10.0, 0.75)
    knock = D.bfilt(knock, 4500.0 + 1500.0 * v, 'lowpass')

    # 3. top: chrome FM click (modulator on the wire's third partial), a laser tick falling from 7-9 kHz to 2 kHz in a
    #    few ms, a noise burst and a ring of wire partials on A6, through the drive chain at 8x, then nothing under
    #    1 kHz
    nt = int(0.2 * SR)
    tt = P.times(nt)
    fcar = 3100.0
    idx = (1.5 + 4.5 * v) * np.exp(-tt / 0.0015)
    fm = np.sin(2 * np.pi * fcar * tt + idx * np.sin(2 * np.pi * fcar * RATIOS[2] * tt))
    fm *= np.exp(-tt / (0.0012 + 0.0012 * v)) * D.taper(nt, 0.006, 0.004)
    f_las = 2000.0 + (5000.0 + 2000.0 * v) * np.exp(-tt / 0.0012)
    las = np.sin(2 * np.pi * D.phase(f_las, nt)) * np.exp(-tt / 0.003) * D.taper(nt, 0.010, 0.005)
    nb = P.hp_noise(r, nt, 3000.0, order=2) * D.env_exp(nt, 0.0008 + 0.0008 * v, 0.0002)
    f_ring = float(D.midi_hz(K.KICK_NOTE + 60)) * (1 + 0.002 * r.uniform(-1, 1))
    decays = [(0.015 + 0.020 * v) / (1 + 0.45 * k) for k in range(len(RATIOS))]
    amps = [1.0 / (1 + 0.7 * k) for k in range(len(RATIOS))]
    w = int(0.0015 * SR)
    ring = np.convolve(r.uniform(-1, 1, w) * np.hanning(w), D.modal_ir(f_ring, RATIOS, decays, amps, nt, r))[:nt]
    x = ((0.45 + 0.55 * v) * fm + (0.20 + 0.40 * v) * las + (0.20 + 0.35 * v) * nb
         + (0.25 + 0.30 * v) * RING_SCALE * ring)

    def chain(u):
        u = D.tanh_adaa(u, 3.0 + 5.0 * v, bias=0.25)
        u = D.sinefold_adaa(u, 1.0 + 0.5 * v)
        return D.hardclip_adaa(u, 1.3)
    top = D.oversampled(chain, x, 8)
    top *= np.exp(-tt / (0.020 + 0.018 * v)) * D.taper(nt, 0.08, 0.05)
    top = D.bfilt(D.lr4(top, 1000.0, 'highpass'), 5500.0 + 8500.0 * v, 'lowpass')
    top_full = np.zeros(n)
    top_full[:nt] = top

    y = sub + (0.50 + 0.25 * v) * knock + (0.10 + 0.16 * v) * top_full
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the electro clap
CLAP_FLAMS = (0.0, 0.0085, 0.0175, 0.0270)   # seconds, at velocity 0.6: tighter on harder hits
SHORT_FADE = (0.32, 0.15)    # snares and hats: gone by 0.47 s, so a file is one 400 ms loudness block (LUFS-I can't
                             # jump at the relative gate, ADR 0009)
SNARE_NOTE = K.ROOT + 24     # D4, 293.66 Hz: the snare body on the key's root
CLAP_MIX = (0.55, 0.30, 0.26, 0.10)   # clap, snare body, wires, room


def electro_clap(take):
    """An electro clap (four flams of 0.9-2.8 kHz noise, soft-clipped, the last carrying the tail) layered with a short
    snare: a body on D4 with the wire's next two partials, dropping into pitch, and bright wires; a little of the
    kit's room on the clap. Harder hits tighten the flams, raise and widen the band, drop the body further and add
    crack."""
    v = take.vel
    r = take.rng('clap')
    n = int(0.6 * SR)
    t = P.times(n)

    # clap: four flams of band noise, the last one carrying the tail; each side its own noise, mostly shared
    spread = P.lerp((1.25, 0.80), v)
    lo, hi = 900.0 * (1 + 0.20 * v), 2800.0 * (1 + 0.35 * v)
    nz = [P.band_noise(take.rng('clap', 'noise', ch), n, lo, hi) for ch in (0, 1)]
    env = np.zeros(n)
    for k, at in enumerate(CLAP_FLAMS):
        a = int(at * spread * SR * (1 + 0.05 * r.uniform(-1, 1)))
        env[a:] += (0.75 + 0.08 * k) * D.env_exp(n - a, 0.0032, 0.0003)
    env[a:] += 0.50 * D.env_exp(n - a, 0.070 + 0.020 * v, 0.001)
    clap = np.stack([nz[0], 0.88 * nz[0] + 0.40 * nz[1]], 1) * env[:, None]
    clap = D.oversampled(lambda u: P.soft_clip(u, 1.2 + 1.3 * v), clap)

    # snare: a body on D4 with the wire's 2nd and 3rd partials, starting (15-60%) sharp, held 2 ms, gone by 60 ms
    bend = (0.15 + 0.45 * v) * np.exp(-t / 0.005)
    ph = 2 * np.pi * D.phase(float(D.midi_hz(SNARE_NOTE)) * (1 + bend), n)
    body = (np.sin(ph) + 0.40 * v * np.sin(RATIOS[1] * ph) + 0.25 * v * np.sin(RATIOS[2] * ph))
    body *= P.gate(t, 0.002, 0.014 + 0.006 * v) * D.env_exp(n, 1.0, 0.0004)
    wires = D.bfilt(D.peaking(P.hp_noise(take.rng('clap', 'wires'), n, 2500.0), 6000.0, 3.0, 0.9),
                    7000.0 + 7000.0 * v, 'lowpass', order=2)
    wires = wires * D.env_exp(n, 0.040 + 0.020 * v, 0.0005)
    crack = (0.2 + 0.8 * v) * P.hp_noise(take.rng('clap', 'crack'), n, 5000.0, order=2) * D.env_exp(n, 0.0018, 0.0002)

    # the kit's room on the clap, its send high-passed at 1.5 kHz so the room stays out of the low mids
    send = D.bfilt(D.mono(clap), 1500.0, 'highpass', order=4)
    room = D.fdn_reverb(send, K.space_rng('room'), t60=0.5, size=0.4, damp_hz=8000.0)[:n]

    g_clap, g_body, g_wire, g_room = CLAP_MIX
    y = (g_clap * (0.7 + 0.3 * v) * clap + g_body * (0.6 + 0.4 * v) * D.st(body)
         + g_wire * D.st((0.5 + 0.5 * v) * wires + 0.4 * crack) + g_room * room)
    return P.low_clean(y, 150.0) * D.taper(n, *SHORT_FADE)[:, None] * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the hats: one metal, two lengths
HAT_STRING = 120.0                     # Hz: the wire the hat's six squares sit on
HAT_PARTIALS = (5, 8, 11, 14, 17, 20)  # 605-2741 Hz, 1 : 1.62 : 2.28 : 2.97 : 3.72 : 4.53
CHOKE = {'group': 'hats', 'closed': 'ClosedHat', 'open': 'OpenHat', 'fade_s': 0.004}
# the tick: its energy over the file as an RMS, its envelope (decay, attack, s) and how it grows with velocity
# (a + b v^p). Round 2's tick was a 1 ms needle (0.0087, decay 1 ms, attack 0.2 ms, 0.15 + 0.85 v) whose peak stood
# 11 dB over the metal's: the closed hat's crest read 23.7 dB, and in the loop the hats alone reached 0.83 of full
# scale on the open hat at 16th 46. Spread over about 3 ms and 1.5 dB lower it is still a tick, and the hat reads about
# 4 dB louder at the same peak, so it no longer eats the mix's headroom; growing with the square of the velocity (a
# soft stroke barely ticks), every layer stays brighter than the one under it (the spread tick on round 2's linear
# curve left the centroid falling between two layers in two of the three round robins)
TICK_RMS = 0.0087 * float(D.undb(-1.5))
TICK_ENV = (0.0024, 0.0005)
TICK_VEL = (0.05, 0.95, 2.0)
# the width: the side is a second noise of the hat's own (hat()), orthogonal to its mid noise, at this share of the
# mid noise's level. The closed hat stays near the middle (side/mid about -13 dB at 8 kHz); the open hat on the offbeat
# spreads (about -4 dB). Round 3's first pass made the side width x (second noise - mid noise), which took the mid's
# noise out of the left channel and doubled it in the right: the open hat read 2.6 dB louder on the right, its noise
# 7.6 dB, and that lean counted as width. The side alone is the second noise now: the closed hat's at sqrt(2) times the
# old share (the same side over mid), the open hat's 1.2 times that again, so the balanced loop keeps round 3's width
# at 8 kHz (the club loop's side over mid -7.5 dB against -7.9; -8.4 at sqrt(2))
HAT_WIDTH = {'closed': 0.57, 'open': 1.35}


def hat_metal(n, v, ph0, k=4):
    """Six band-limited squares (two PolyBLEP saws half a cycle apart, at k x) on the wire's partials, each starting a
    little sharp and settling: the pitch envelope, deeper on harder hits. ph0: the squares' start phases, shared by
    every take so the round robins differ by their noise only."""
    t = P.times(n)
    bend = (0.004 + 0.016 * v) * np.exp(-t / 0.006)
    y = np.zeros(n * k)
    for i, (p, a) in enumerate(zip(HAT_PARTIALS, ph0)):
        f = HAT_STRING * float(wire(p)) * (1 + bend)
        y += (D.polyblep_saw(f, n, a, k) - D.polyblep_saw(f, n, (a + 0.5) % 1.0, k)) / (1 + 0.15 * i)
    return D.down(y, k) / len(HAT_PARTIALS)


def unit_against(x, basis):
    """x with its projection on each of `basis` removed, scaled to unit energy: a take's noise that adds no level of
    its own by chance. On a hit this short (a 16th's worth of a few kHz of noise), the noise's energy and its
    correlation with the metal moved the round robins by up to 1.3 dB."""
    for b in basis:
        x = x - b * (np.dot(x, b) / (np.dot(b, b) + 1e-30))
    return x / (np.sqrt(np.dot(x, x)) + 1e-30)


def hat(take, decay, fade, width):
    """The metal and noise through a band that opens with velocity, a tick on the front, on one decay; the side is a
    second noise (`width` of the mid noise's level), so the mono sum keeps every partial and both channels carry the
    same level. Every noise is made orthogonal to what is already there and given a fixed energy, so the round robins
    differ in grain, not level."""
    v = take.vel
    n = int(0.6 * SR)
    ph0 = D.rng(take.kit, 'hats', 'phases').uniform(0, 1, len(HAT_PARTIALS))   # one metal for both hats
    env = D.env_exp(n, decay(v), 0.0003)
    band = lambda x: D.bfilt(D.bfilt(x, 6500.0 - 800.0 * v, 'highpass', order=4), 9000.0 + 8000.0 * v, 'lowpass',
                             order=4)
    metal = band(hat_metal(n, v, ph0)) * env
    e = np.sqrt(np.dot(metal, metal))
    nm = e * unit_against(band(take.rng('hat').standard_normal(n)) * env, (metal,))
    ns = e * unit_against(band(take.rng('hat', 'side').standard_normal(n)) * env, (metal, nm))
    mid = 0.75 * metal + 0.45 * nm
    tick = D.bfilt(take.rng('hat', 'tick').uniform(-1, 1, n), (8000.0, 16000.0), 'bandpass', order=2)
    a, b, p = TICK_VEL
    tick = (a + b * v ** p) * TICK_RMS * np.sqrt(n) * unit_against(tick * D.env_exp(n, *TICK_ENV), (mid,))
    m = mid + tick
    s = width * 0.45 * ns
    y = np.stack([m + s, m - s], 1)
    return P.low_clean(y, 3000.0) * D.taper(n, *fade)[:, None] * P.vel_gain(v, 13.0)


def closed_hat(take):
    """30 dB down in 40-60 ms: inside a 16th at 130 BPM (115 ms), so rolling 16ths stay apart."""
    return Out(hat(take, lambda v: 0.008 + 0.006 * v, SHORT_FADE, HAT_WIDTH['closed']), meta={'choke': CHOKE})


def open_hat(take):
    """A sizzle about a beat long, 30 dB down by 0.3-0.4 s and gone by 0.47 s: choked by the next closed hat."""
    return Out(hat(take, lambda v: 0.085 + 0.035 * v, SHORT_FADE, HAT_WIDTH['open']), meta={'choke': CHOKE})


# ------------------------------------------------------------------ the 2-bar hat preview loop
# (16th, velocity) on the kit's grid: closed 16ths around an open hat on every off-beat 8th. The 16th after an open
# hat pushes into the beat and chokes it after one 16th; in beats 2 and 6 it is left out, so the open hat rings a whole
# 8th until the beat chokes it
HAT_STEPS = 32
HAT_OPEN = tuple((4 * b + 2, 1.0 if b in (3, 7) else 0.8) for b in range(8))
HAT_CLOSED = tuple(sorted([(4 * b, 0.8 if b % 4 == 0 else 0.6) for b in range(8)]
                          + [(4 * b + 1, 0.2 if b % 2 else 0.4) for b in range(8)]
                          + [(4 * b + 3, 0.8) for b in range(8) if b not in (1, 5)]))


def hat_loop(take):
    """The two hats' files exactly as rendered, on the kit's grid and swing; each open hat fades out over 4 ms from the
    next closed hat (the choke), and tails wrap round the loop."""
    G = Ph.Grid(K, HAT_STEPS)
    buf = np.zeros((G.length, 2))
    choke_n = int(CHOKE['fade_s'] * SR)
    closed_at = sorted(G.at(s) for s, _ in HAT_CLOSED)
    for name, hits in (('ClosedHat', HAT_CLOSED), ('OpenHat', HAT_OPEN)):
        item, got, _ = Ph.kit_files(K, name)
        for k, (step, vel) in enumerate(hits):
            layer = 1 + min(range(len(item.layers)), key=lambda i: (abs(item.layers[i] - vel), -i))
            x = np.array(D.st(got[(None, layer, 1 + k % item.rr)].audio))
            p = G.at(step)
            if name == 'OpenHat':
                nxt = min((c - p) % G.length for c in closed_at if c != p)
                g = np.ones(len(x))
                g[nxt:nxt + choke_n] = np.cos(0.5 * np.pi * np.arange(len(g[nxt:nxt + choke_n])) / choke_n) ** 2
                g[nxt + choke_n:] = 0.0
                x *= g[:, None]
            Ph.place(buf, x, p)
    meta = {'grid': {'bpm': K.BPM, 'steps': HAT_STEPS, 'swing': K.SWING}, 'closed': list(HAT_CLOSED),
            'open': list(HAT_OPEN), 'choke': CHOKE}
    return Out(buf, meta=meta)


ITEMS = [
    Item('Drums', 'KickSplit', 'kick', kick_split, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=('r2', 'r3'),
         about='split kick: a clean A1 sub that holds about 60 ms and is 20 dB down by 0.12 s (a quiet tail to '
               '0.40 s), a gently saturated knock gone by 85 ms, a driven chrome top with a laser tick above 1 kHz'),
    Item('Drums', 'ElectroClap', 'snare', electro_clap, layers=LAYERS, rr=2, sets=('r2', 'r3'),
         about='electro clap: four flams of band noise in the kit room, layered with a short snare on D4 (wire '
               'partials, a pitch drop) and bright wires'),
    Item('Drums', 'ClosedHat', 'hat', closed_hat, layers=LAYERS, rr=3, sets=('r2', 'r3'),
         about='closed hat: six squares on the stiff wire partials 5-20 over noise, 30 dB down inside a 16th; chokes '
               'the open hat'),
    Item('Drums', 'OpenHat', 'hat', open_hat, layers=LAYERS, rr=3, sets=('r2', 'r3'),
         about='open hat: the same wire metal as a wide sizzle about a beat long, choked by the closed hat'),
    Item('Loops', 'HatLoop', 'loop', hat_loop, loop=True, sets=('r2', 'r3'), tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a hat loop: the low cut, like the hats'},
         about='2-bar hat preview at 130 BPM: closed 16ths around an open hat on every off-beat, choked by the next '
               'closed hat'),
]
