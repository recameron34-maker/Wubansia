"""FUT04 drums for round 2: the split kick on E1, the gated iron snare, the dark closed and open hats and their 2-bar
preview loop.

Every drum has 5 velocity layers (2 round robins, the hats 3). Velocity moves the brightness, the transient and the
pitch envelope, not only the level.

- KickSplit: round 1's pick (FUT01's A, "Split") rebuilt for a heavy techno kick on E1 (41.2 Hz, the tonic): a clean
  sine sub that blooms and is exactly zero before the next beat (441 ms at 136 BPM), a clean knock above 110 Hz (the
  sweep's own harmonics 2-5, added, not shaped) that is gone by 80 ms, and the 8x oversampled drive chain, high-passed
  at 300 Hz, so no distortion reaches the sub or the knock: the crunch of the body driven, gated short, and an iron
  click and ring above 1 kHz. Its top layer reads 5.9-6.4 dB of crest, Split's range (5.8-6.4), not Tight's (7.9).
- SnareGated: an iron plate on B3 (the anvil's modes from the key's fifth), a three-flam clap and bright wires,
  soft-clipped at 4x, in the kit's dark concrete room (an FDN on K.space_rng, fed above 500 Hz, t60 2.4 s, damped at
  4 kHz) held about 10 dB under the hit and chopped by a gate after 150-185 ms: the industrial gated snare.
- ClosedHat and OpenHat: one dark metal for both, seven squares on the anvil's ratios over E4 (330-1388 Hz, the way the
  classic six-oscillator hat uses inharmonic squares), over a little noise, in a 4.3-14 kHz band that opens with
  velocity. The closed hat is 30 dB down inside a 16th; the open one sizzles for about a beat. They share a choke
  group: a closed hat cuts the open one (meta 'choke').
- HatLoop: a 2-bar preview of the pair on the kit's swing, played on the files as written, each open hat choked by the
  next closed one (a 4 ms fade, the sampler's choke).

Everything but the kick is high-passed far above the kick and sub (low_clean) and mono under 150 Hz.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
LAYERS = (0.2, 0.4, 0.6, 0.8, 1.0)
RATIOS = np.array(K.RATIOS)
F_KICK = float(D.midi_hz(K.KICK_NOTE))  # E1, 41.20 Hz


# ------------------------------------------------------------------ the split kick
XOVER = 110.0                # sub / knock crossover (LR4, sums flat)
DRIVE_HP = 300.0             # the drive chain's output is high-passed here (LR4): no distortion under it
SUB_END = (0.31, 0.12)       # the sub's raised-cosine fade: from 0.31 s, exactly 0 at 0.43 s (the beat is 0.441 s)
RING_SCALE = 0.25            # the iron ring against the click before the top's drive
KNOCK_MIX = (0.40, 0.58)     # the knock against the sub, softest to hardest layer
KNOCK_HARM = ((0.30, 0.55), (0.25, 0.45), (0.15, 0.35), (0.10, 0.25))   # the sweep's harmonics 2-5 in the knock
KNOCK_SPREAD = 5.0           # their Schroeder phases, pi k (k - 1) / KNOCK_SPREAD: in phase they spiked the crest to
                             # 7.4 dB (Tight's range); spread, the same energy reads 5.9-6.6
KNOCK_GATE = ((0.010, 0.016), (0.018, 0.024))   # the knock's hold and fall (s), softest to hardest
CRUNCH_MIX = (0.20, 0.55)    # the driven crunch (all above 300 Hz)
CRUNCH_RISE = 0.003          # seconds: the crunch swells in under the knock, so it thickens the body, not the spike

def kick_split(take):
    """Four parts shaped apart: the clean sub on the sweep, a clean knock above 110 Hz, the crunch (the same sweep
    driven through the 8x chain, nothing under 300 Hz, gone by 75 ms) and the iron top (FM click, noise, a ring of the
    anvil's modes on E6) whose heavy drive is high-passed at 1 kHz."""
    v = take.vel
    r = take.rng('split')
    n = int(0.6 * SR)
    t = P.times(n)

    # sweep: harder hits start higher and snap faster; the round robin moves the start and the snap, never the end
    f_hi = (230.0 + 290.0 * v) * 2 ** (r.normal(0, 10) / 1200)
    tau1 = (0.0095 - 0.0025 * v) * (1 + 0.04 * r.uniform(-1, 1))
    ph = P.sweep(n, f_hi, tau1, 0.014, 0.30, F_KICK)
    s = np.sin(ph)

    # 1. sub: clean, a hold-then-drop that blooms through the beat, exactly zero from 0.43 s
    a_sub = np.exp(-(t / (0.190 + 0.030 * v)) ** 2.0) * D.taper(n, *SUB_END)
    sub = D.lr4(s * a_sub, XOVER, 'lowpass')

    # 2. knock: clean, the sweep's own harmonics 2-5 added (no shaper, so nothing intermodulates), above 110 Hz,
    #    dipped at 240 Hz, gone by 80 ms; harder hits carry more of the upper harmonics
    a_k = P.gate(t, P.lerp(KNOCK_GATE[0], v), P.lerp(KNOCK_GATE[1], v)) * D.taper(n, 0.050, 0.030)
    kn = s + sum(P.lerp(a, v) * np.sin(k * ph + np.pi * k * (k - 1) / KNOCK_SPREAD)
                 for k, a in enumerate(KNOCK_HARM, 2))
    knock = D.lr4(kn * a_k, XOVER, 'highpass')
    knock = D.peaking(D.bfilt(knock, 1500.0 + 1500.0 * v, 'lowpass'), 240.0, -6.0, 0.8)

    # 3. crunch: the sweep's body through the 8x chain (biased tanh, a fold, a clip), then nothing under 300 Hz
    drive, fold = 2.0 + 5.0 * v, 1.0 + 0.6 * v

    def body_chain(u):
        u = D.tanh_adaa(u, drive, bias=0.15)
        u = D.sinefold_adaa(u, fold)
        return D.hardclip_adaa(u, 1.3)
    cr = D.oversampled(body_chain, s * np.exp(-t / 0.040), 8)
    a_c = P.raised(n, CRUNCH_RISE) * P.gate(t, 0.010 + 0.006 * v, 0.012 + 0.006 * v) * D.taper(n, 0.045, 0.030)
    crunch = D.bfilt(D.lr4(cr * a_c, DRIVE_HP, 'highpass'), 3000.0 + 4000.0 * v, 'lowpass')

    # 4. top: an iron FM click (modulator on the anvil ratio 2.4446), a noise burst and a ring of the anvil's modes on
    #    E6, through the heavy chain at 8x, then nothing under 1 kHz
    nt = int(0.2 * SR)
    tt = P.times(nt)
    fcar = 2600.0
    idx = (2.0 + 5.0 * v) * np.exp(-tt / 0.0015)
    fm = np.sin(2 * np.pi * fcar * tt + idx * np.sin(2 * np.pi * fcar * RATIOS[3] * tt))
    fm *= np.exp(-tt / (0.0014 + 0.0014 * v)) * D.taper(nt, 0.008, 0.004)
    nb = P.hp_noise(r, nt, 2500.0, order=2) * D.env_exp(nt, 0.0010 + 0.0008 * v, 0.0002)
    f_ring = float(D.midi_hz(K.KICK_NOTE + 60)) * (1 + 0.002 * r.uniform(-1, 1))
    decays = [(0.018 + 0.022 * v) / (1 + 0.45 * k) for k in range(len(RATIOS))]
    amps = [1.0 / (1 + 0.6 * k) for k in range(len(RATIOS))]
    w = int(0.0015 * SR)
    ring = np.convolve(r.uniform(-1, 1, w) * np.hanning(w), D.modal_ir(f_ring, RATIOS, decays, amps, nt, r))[:nt]
    x = (0.45 + 0.55 * v) * fm + (0.25 + 0.40 * v) * nb + (0.30 + 0.35 * v) * RING_SCALE * ring

    def top_chain(u):
        u = D.tanh_adaa(u, 3.0 + 6.0 * v, bias=0.3)
        u = D.sinefold_adaa(u, 1.0 + 0.7 * v)
        return D.hardclip_adaa(u, 1.3)
    top = D.oversampled(top_chain, x, 8)
    top *= np.exp(-tt / (0.022 + 0.018 * v)) * D.taper(nt, 0.08, 0.05)
    top = D.bfilt(D.lr4(top, 1000.0, 'highpass'), 5000.0 + 8000.0 * v, 'lowpass')
    top_full = np.zeros(n)
    top_full[:nt] = top

    y = sub + P.lerp(KNOCK_MIX, v) * knock + P.lerp(CRUNCH_MIX, v) * crunch + (0.08 + 0.15 * v) * top_full
    return y * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the gated iron snare
SNARE_NOTE = K.ROOT + 19     # B3, 246.94 Hz: the key's fifth; the plate's modes run up to 1.04 kHz
CLAP_FLAMS = (0.0, 0.0065, 0.0140)   # seconds, at velocity 0.6: tighter on harder hits
SNARE_MIX = (0.40, 0.50, 0.40, 3.20)  # plate, clap, wires, room (the room about 10 dB under the hit, then flat)
SNARE_ROOM_WIDTH = 0.5       # the room's side against its mid: the FDN's two outputs are uncorrelated
SNARE_ROOM = dict(t60=2.4, size=0.8, damp_hz=4000.0, send_hp=500.0)   # big and dark; fed only above 500 Hz
SNARE_GATE = ((0.150, 0.185), 0.012)  # the gate: open for 150-185 ms (softest to hardest), then a 12 ms cosine close
SNARE_GRIT = (1.2, 3.0)      # the soft clip on the dry hit, softest to hardest (4x)


def unit_noise(x, basis=()):
    """x with its projection on each of `basis` removed, scaled to unit energy: a take's noise whose level is fixed,
    so round robins differ in grain, not in level."""
    for b in basis:
        x = x - b * (np.dot(x, b) / (np.dot(b, b) + 1e-30))
    return x / (np.sqrt(np.dot(x, x)) + 1e-30)


def snare_gated(take):
    """An iron plate on B3 (the anvil's modes, struck by a Hann pulse, starting sharp), a three-flam clap and bright
    wires, soft-clipped at 4x, then the kit's concrete room, big and dark, chopped by a gate after 150-185 ms: the
    industrial gated snare. Harder hits strike shorter (more upper modes), bend further, tighten the flams, open the
    wires and drive the clip harder; the gate stays open a little longer."""
    v = take.vel
    r = take.rng('snare')
    n = int(0.5 * SR)
    t = P.times(n)

    # plate: the iron modes on B3, the same phases on every take; the strike's width sets how many modes ring
    fk = float(D.midi_hz(SNARE_NOTE)) * RATIOS
    amps = P.hann_pulse_gain(fk, P.lerp((0.0012, 0.0004), v)) / (1 + 0.3 * np.arange(len(RATIOS)))
    amps[0] *= 0.5                                              # the one mode in the low mids
    decays = [0.035 / (1 + 0.25 * k) for k in range(len(RATIOS))]
    bend = (0.03 + 0.09 * v) * np.exp(-t / 0.006)
    plate = P.modes(n, fk[0], RATIOS, decays, amps, bend, D.rng(take.kit, take.item.name, 'phases'))
    plate = plate * D.env_exp(n, 1.0, 0.0004)
    plate = np.sqrt(n) * 0.06 * unit_noise(plate)

    # clap: three flams of 0.9-3.2 kHz noise, the last carrying a short tail
    spread = P.lerp((1.25, 0.80), v)
    env = np.zeros(n)
    for k, at in enumerate(CLAP_FLAMS):
        a = int(at * spread * SR)
        env[a:] += (0.8 + 0.1 * k) * D.env_exp(n - a, 0.0030, 0.0003)
    env[a:] += 0.45 * D.env_exp(n - a, 0.040 + 0.015 * v, 0.001)
    clap = P.band_noise(r, n, 900.0 * (1 + 0.2 * v), 3200.0 * (1 + 0.3 * v)) * env
    clap = np.sqrt(n) * 0.05 * unit_noise(clap, (plate,))

    # wires: noise over 1.8 kHz with a presence peak, brighter on harder hits, and a 2 ms crack
    wires = D.peaking(P.hp_noise(take.rng('snare', 'wires'), n, 1800.0), 4500.0, 4.0, 0.9)
    wires = D.bfilt(wires, 5000.0 + 7000.0 * v, 'lowpass', order=2) * D.env_exp(n, 0.035 + 0.020 * v, 0.0005)
    wires = np.sqrt(n) * 0.04 * unit_noise(wires, (plate, clap))
    crack = (0.2 + 0.8 * v) * P.hp_noise(take.rng('snare', 'crack'), n, 5000.0, order=2) * D.env_exp(n, 0.002, 0.0002)

    g_plate, g_clap, g_wire, g_room = SNARE_MIX
    dry = g_plate * (0.7 + 0.3 * v) * plate + g_clap * (0.6 + 0.4 * v) * clap + g_wire * (0.5 + 0.5 * v) * wires
    dry = D.oversampled(lambda u: P.soft_clip(u, P.lerp(SNARE_GRIT, v)), dry + 0.05 * crack)

    # the concrete room: dark and big, fed above 500 Hz (none of the plate's low mode), its two sides decorrelated
    room_kw = {k: val for k, val in SNARE_ROOM.items() if k != 'send_hp'}
    send = D.bfilt(dry, SNARE_ROOM['send_hp'], 'highpass', order=4)
    room = D.fdn_reverb(send, K.space_rng('snare room'), **room_kw)[:n]
    rm, rs = D.ms(room[:, 0], room[:, 1])
    room = np.stack([rm + SNARE_ROOM_WIDTH * rs, rm - SNARE_ROOM_WIDTH * rs], 1)
    y = D.st(dry) + g_room * room

    # the gate: open, then a short cosine close, nothing after it
    hold, close = P.lerp(SNARE_GATE[0], v), SNARE_GATE[1]
    g = np.cos(0.5 * np.pi * np.clip((t - hold) / close, 0.0, 1.0)) ** 2
    return P.low_clean(y * g[:, None], 150.0) * P.vel_gain(v, 12.0)


# ------------------------------------------------------------------ the hats: one dark metal, two lengths
HAT_BASE = float(D.midi_hz(K.ROOT + 24))   # E4, 329.63 Hz: seven squares on the anvil's ratios over it (330-1388 Hz)
HAT_BAND = ((4300.0, 5200.0), (6000.0, 14000.0))   # high-pass and low-pass (4th order), softest to hardest: dark
CHOKE = {'group': 'hats', 'closed': 'ClosedHat', 'open': 'OpenHat', 'fade_s': 0.004}
HAT_FADE = (0.32, 0.15)      # gone by 0.47 s, so a file is one 400 ms loudness block (LUFS-I can't jump at the
                             # relative gate, ADR 0009)
HAT_DECAY = {'closed': (0.010, 0.008), 'open': (0.090, 0.040)}   # the decay (s): a + b v
HAT_TICK = 0.0035             # the tick's RMS over the file at full level: 7-10 dB under the metal on the hardest hits


def hat_metal(n, v, ph0, k=4):
    """Seven band-limited squares (two PolyBLEP saws half a cycle apart, at k x) on the anvil's ratios over E4, each
    starting a little sharp and settling: the pitch envelope, deeper on harder hits. ph0: the squares' start phases,
    shared by every take so the round robins differ by their noise only."""
    t = P.times(n)
    bend = (0.003 + 0.012 * v) * np.exp(-t / 0.006)
    y = np.zeros(n * k)
    for i, (ratio, a) in enumerate(zip(RATIOS, ph0)):
        f = HAT_BASE * ratio * (1 + bend)
        y += (D.polyblep_saw(f, n, a, k) - D.polyblep_saw(f, n, (a + 0.5) % 1.0, k)) / (1 + 0.1 * i)
    return D.down(y, k) / len(RATIOS)


def hat(take, kind):
    """The iron metal and noise through a dark band that opens with velocity, a tick on the front, on one decay; the
    side is its own noise, so the mono sum keeps every partial."""
    v = take.vel
    n = int(0.6 * SR)
    ph0 = D.rng(take.kit, 'hats', 'phases').uniform(0, 1, len(RATIOS))   # one metal for both hats
    env = D.env_exp(n, P.lerp(HAT_DECAY[kind], v), 0.0003)
    hp, lp = P.lerp(HAT_BAND[0], v), P.lerp(HAT_BAND[1], v)
    band = lambda x: D.bfilt(D.bfilt(x, hp, 'highpass', order=4), lp, 'lowpass', order=4)
    metal = band(hat_metal(n, v, ph0)) * env
    e = np.sqrt(np.dot(metal, metal))
    nm = e * unit_noise(band(take.rng('hat').standard_normal(n)) * env, (metal,))
    ns = e * unit_noise(band(take.rng('hat', 'side').standard_normal(n)) * env, (metal, nm))
    mid = 0.85 * metal + 0.30 * nm
    tick = D.bfilt(take.rng('hat', 'tick').uniform(-1, 1, n), (6000.0, 12000.0), 'bandpass', order=2)
    tick = (0.10 + 0.60 * v) * HAT_TICK * np.sqrt(n) * unit_noise(tick * D.env_exp(n, 0.0010, 0.0002), (mid,))
    m = mid + tick
    s_ = 0.30 * 0.30 * (ns - nm)
    y = np.stack([m + s_, m - s_], 1)
    return P.low_clean(y, 3000.0) * D.taper(n, *HAT_FADE)[:, None] * P.vel_gain(v, 13.0)


def closed_hat(take):
    """30 dB down in 40-60 ms: inside a 16th at 136 BPM (110 ms), so driving 16ths stay apart."""
    return Out(hat(take, 'closed'), meta={'choke': CHOKE})


def open_hat(take):
    """A dark sizzle about a beat long, gone by 0.47 s: choked by the next closed hat."""
    return Out(hat(take, 'open'), meta={'choke': CHOKE})


# ------------------------------------------------------------------ the 2-bar hat preview loop
# (16th, velocity) on the kit's grid and swing. The open hat on every off-beat 8th (louder on beats 2 and 6), the
# closed one driving the 16ths around it: on the beat (louder on beats 1 and 3 of each bar), a ghost after it, a push
# into the next beat that chokes the open hat after one 16th. On beats 4 and 8 the push is left out, so the open hat
# rings a whole 8th into the bar line
HAT_STEPS = 32
HAT_OPEN = tuple((4 * b + 2, 0.8 if b in (1, 5) else 0.6) for b in range(8))
HAT_CLOSED = tuple(sorted([(4 * b, 0.8 if b % 2 else 1.0) for b in range(8)]
                          + [(4 * b + 1, 0.4 if b % 2 else 0.6) for b in range(8)]
                          + [(4 * b + 3, 0.8) for b in range(8) if b not in (3, 7)]))


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
    Item('Drums', 'KickSplit', 'kick', kick_split, layers=LAYERS, rr=2, root=K.KICK_NOTE, sets=('r2',),
         about='split kick on E1: a clean sub zero before the next beat, a clean knock above 110 Hz, the 8x drive '
               'chain only above 300 Hz (a short crunch, an iron click and ring above 1 kHz)'),
    Item('Drums', 'SnareGated', 'snare', snare_gated, layers=LAYERS, rr=2, sets=('r2',),
         about='gated iron snare: the anvil modes on B3, a three-flam clap and bright wires, soft-clipped, in the '
               'dark concrete room chopped by a gate after 150-185 ms'),
    Item('Drums', 'ClosedHat', 'hat', closed_hat, layers=LAYERS, rr=3, sets=('r2',),
         about='closed hat, dark: seven squares on the anvil ratios over E4 in a 4.3-14 kHz band, 30 dB down inside a '
               '16th; chokes the open hat'),
    Item('Drums', 'OpenHat', 'hat', open_hat, layers=LAYERS, rr=3, sets=('r2',),
         about='open hat: the same dark iron metal as a sizzle about a beat long, choked by the closed hat'),
    Item('Loops', 'HatLoop', 'loop', hat_loop, loop=True, sets=('r2',), tags=('preview',),
         qa={'mud': 'lowcut', 'why': 'a hat loop: the low cut, like the hats'},
         about='2-bar hat preview at 136 BPM on the kit swing: closed 16ths driving round an open hat on every '
               'off-beat, choked by the next closed hat'),
]
