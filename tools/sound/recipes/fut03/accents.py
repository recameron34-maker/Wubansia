"""FUT03 round 4 synth accents and phrases: one-shots a producer drops into a garage-house or deep tech house track, and
original 2- and 4-bar synth riffs, every one in G minor at 128 BPM on the kit's swing, each with its root stated.

**Accents** (one-shots; their roots in the names and `root`):
- AccentDubChordCm9 (root C4): the electric piano's Cm9 (C4 G4 Bb4 D5 Eb5, the kit's open voicing on the iv chord) with
  its dub tail baked in: a dotted-8th ping-pong that darkens over four repeats, into a short room.
- AccentOrganHitGm7 (root G3): a drawbar organ chord, Gm7 (G3 D4 F4 Bb4): the 8', 4', 2 2/3', 2' and 1' bars, a
  percussion on the third harmonic that decays in 0.15 s, a key click, a soft valve drive.
- AccentRubberStabG1 (root G1): the rubber bass as a stab: a big boing, the pitch dropping from G2 to G1 in 60 ms over
  the sine sub; A clean (the strict mud gate), short enough for every beat.
- AccentPluckChordDm7 (root D4): Dm7 (D4 A4 C5 F5) on tuned plucked strings (Karplus-Strong, every string exactly in
  tune), strummed down 11 ms a string, in a short room.
- AccentNoiseSweepUp (root G4, its resonance): 2 beats of noise through a band-pass rising from 400 Hz to 9 kHz and a comb
  tuned to G4, swelling into the downbeat (cut_end).
- AccentZapG4 (root G4): a perc synth zap: FM falling from G7 to G4 in about 15 ms, then a short G4 tone and a click.
- AccentReverseChordGm9 (root G3): the dub stab's Gm9 through a long plate, reversed: a swell ending on the downbeat
  (cut_end).
- AccentVoxSynthGm (root G3): a vocal-synth stab: Gm (G3 D4 Bb4) on saws through three static "ah" formants, a soft
  attack and a short release.
- AccentGlassPingD6 (root D6): the kit's glass bell struck on D6 with an 8th-note ping-pong: the glass, as a colour.

**Phrases** (loops, on the kit's grid and swing):
- PhraseRubberRiff (2 bars): a rolling bass riff on the rubber bass, A clean, resting on every kick (rubber.PHRASES).
- PhraseDubChords (4 bars): the dub stab's rhythm moving Gm9 | Gm9 | Cm9 | Gm9 through the dub echo, a room, a low-pass
  ride and the pump.
- PhraseKeysRiff (2 bars): an electric-piano riff, call and answer, on the KeysEP multisample's voice.
- PhraseKeysArp (4 bars): a 16th arpeggio over Gm9 and Cm9 in turn on the same keys, with an 8th echo.
- KeysEP: the electric piano as a playable multisample (G3 D4 G4 D5 G5, two layers), what the keys phrases play.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K
from tools.sound.recipes.fut03 import chords, rubber

SR = D.SR
SETS = ('r4',)
RATIOS = np.array(K.RATIOS)
GM9 = chords.CHORD                               # G3 D4 F4 A4 Bb4
CM9 = tuple(n + 5 for n in GM9)                  # C4 G4 Bb4 D5 Eb5: the same open voicing on the iv chord


def ep_chord(notes, v, r, n, hold=0.26, release=0.18):
    """The electric piano (chords.ep_note) on a chord: each note its own soft drive at 4x, spread a little across the
    field, gated and released. Returns (audio (n, 2), the notes' frequencies)."""
    t = P.times(n)
    y = np.zeros((n, 2))
    fs = []
    for k, note in enumerate(notes):
        f = float(D.midi_hz(note))
        fs.append(f)
        x = D.oversampled(lambda w: D.tanh_adaa(w, chords.EP_DRIVE), chords.ep_note(f, n, v, r), 4)
        y += Ph.pan(x, (-0.5 + k / max(1, len(notes) - 1)) * 0.6) / np.sqrt(2)
    u = np.clip((t - hold) / release, 0.0, 1.0)
    return y * (P.raised(n, 0.0015) * (0.5 + 0.5 * np.cos(np.pi * u)))[:, None] / len(notes), fs


def ping_pong(x, n, steps, taps, fb, lp, hp, darken=0.8):
    """A ping-pong echo, `steps` 16ths (straight) a repeat, alternating sides, each darker; the dry not included."""
    d = int(np.floor(steps * K.STEP * SR + 0.5))
    m = D.mono(x)
    out = np.zeros((n, 2))
    tap = m
    for k in range(1, taps + 1):
        tap = D.bfilt(D.bfilt(tap, lp, 'lowpass'), hp, 'highpass') * (1.0 if k == 1 else fb)
        lp *= darken
        side = np.array([1.0, 0.3]) if k % 2 else np.array([0.3, 1.0])
        a = k * d
        if a < n:
            seg = tap[:n - a]
            out[a:a + len(seg)] += seg[:, None] * side
    return out


def room(x, n, key, t60=1.2, size=0.5, damp=7000.0, hp=350.0):
    rv = D.fdn_reverb(D.bfilt(D.mono(x), hp, 'highpass'), K.space_rng(key), t60=t60, size=size, damp_hz=damp,
                      n_tail=0)
    rv = rv[:n]
    mid, side = 0.5 * (rv[:, 0] + rv[:, 1]), 0.5 * (rv[:, 0] - rv[:, 1])
    return np.stack([mid + 0.6 * side, mid - 0.6 * side], 1)


def finish(y, hp=110.0):
    return D.mono_below(D.bfilt(y, hp, 'highpass', order=4), 150.0)


# ------------------------------------------------------------------ the accents
def dub_chord_cm9(take):
    n = int(2.4 * SR)
    r = D.rng(take.kit, take.item.name, 'ep')
    dry, fs = ep_chord(CM9, 1.0, r, n)
    echo = ping_pong(dry, n, 3, 4, 0.5, 5000.0, 400.0)
    y = dry + 0.55 * echo + 0.30 * room(echo, n, 'accent dub')      # the room on the echoes only, as a dub send
    y = y * D.taper(n, 2.0, 0.4)[:, None]
    return Out(finish(y), f0s=tuple(sorted(fs)), meta={'root': 'C4', 'chord': 'Cm9', 'echo_steps': 3})


ORGAN_BARS = ((1, 1.0), (2, 0.7), (3, 0.5), (4, 0.45), (8, 0.25))   # (harmonic, level): 8' 4' 2 2/3' 2' 1'
GM7 = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 22, K.ROOT + 27)        # G3 D4 F4 Bb4


def organ_note(f, n, r):
    t = P.times(n)
    ph = r.uniform(0, 2 * np.pi)
    x = sum(a * np.sin(h * (2 * np.pi * f * t + ph)) for h, a in ORGAN_BARS if f * h < 18000)
    x = x + 0.5 * np.sin(3 * (2 * np.pi * f * t + ph)) * np.exp(-t / 0.15)          # the percussion
    return D.oversampled(lambda w: D.tanh_adaa(w, 1.4), 0.35 * x, 4)


def organ_hit(take):
    n = int(0.9 * SR)
    t = P.times(n)
    r = D.rng(take.kit, take.item.name, 'organ')
    y = np.zeros((n, 2))
    fs = []
    for k, note in enumerate(GM7):
        f = float(D.midi_hz(note))
        fs.append(f)
        y += Ph.pan(organ_note(f, n, r), (-0.4 + 0.8 * k / 3)) / np.sqrt(2)
    click = D.bfilt(P.hp_noise(r, n, 1500.0, order=2), 6000.0, 'lowpass') * D.env_exp(n, 0.0015, 0.0002) * 0.15
    u = np.clip((t - 0.32) / 0.12, 0.0, 1.0)
    env = P.raised(n, 0.002) * (0.5 + 0.5 * np.cos(np.pi * u))
    y = (y / len(GM7) + D.st(click)) * env[:, None]
    return Out(finish(y), f0s=tuple(sorted(fs)), meta={'root': 'G3', 'chord': 'Gm7', 'drawbars': ORGAN_BARS})


STAB_VOICE_TAU = 0.022             # the stab's voice dies away fast after its boing (30 dB in about 75 ms)


def rubber_stab(take):
    """The rubber voice (A: from 480 Hz, steep) dropping from G2 to G1 in 60 ms with the full boing and dying away in
    about 75 ms, over a G1 sine sub held as a pedal (a sine that glides splatters into the low mids); gone by 0.32 s."""
    n = int(0.55 * SR)
    t = P.times(n)
    f = float(D.midi_hz(31))
    freq = f * 2.0 ** (12.0 * (1.0 - rubber.smoothstep(t / 0.060)) / 12.0)
    ph = D.phase(freq, n)
    gate = D.taper(n, 0.20, 0.12)                                   # a slow release: a sine stopped fast splatters
    sub = rubber.SUB_LEVEL * np.sin(2 * np.pi * f * t) * P.raised(n, rubber.SUB_ATTACK) * gate
    lag = int(rubber.BITE_LAG * SR)
    env = np.concatenate([np.zeros(lag), P.raised(n - lag, rubber.MID_ATTACK)])
    voice = P.hp_steep(rubber.rubber(freq, n, 1.0, f, ph) * env, rubber.MID_HP) * np.exp(-t / STAB_VOICE_TAU)
    return Out(sub + rubber.VOICE_LEVEL * voice * gate,
               meta={'root': 'G1', 'voice_drops_from': 'G2', 'sub': 'G1 pedal', 'character': 'clean'})


DM7 = (K.ROOT + 19, K.ROOT + 26, K.ROOT + 29, K.ROOT + 34)        # D4 A4 C5 F5
STRUM = 0.011


def pluck_chord(take):
    n = int(1.8 * SR)
    r = D.rng(take.kit, take.item.name, 'strings')
    y = np.zeros((n, 2))
    fs = []
    for k, note in enumerate(DM7):
        f = float(D.midi_hz(note))
        fs.append(f)
        S, disp = 0.45, 0.0
        m = int(0.004 * SR)
        excite = np.zeros(n)
        excite[:m] = r.uniform(-1, 1, m) * np.hanning(m)
        x = D.karplus(excite, P.ks_tuned(f, S, disp), n, decay_s=1.1, S=S, disp=disp)
        a = int(k * STRUM * SR)
        x = np.concatenate([np.zeros(a), x[:n - a]])
        y += Ph.pan(x, -0.35 + 0.7 * k / 3) / np.sqrt(2)
    y = y / len(DM7)
    y = y + 0.2 * room(y, n, 'accent pluck', t60=0.9, size=0.4)
    return Out(finish(y * D.taper(n, 1.4, 0.4)[:, None], 150.0), f0s=tuple(sorted(fs)),
               meta={'root': 'D4', 'chord': 'Dm7', 'strum_s': STRUM})


SWEEP_BEATS = 2


def noise_sweep(take):
    """2 beats of noise: a band-pass rising from 400 Hz to 9 kHz (exponentially, faster at the end) and a feedback comb
    tuned to G4 (its resonances on G4's harmonics), swelling 24 dB into the downbeat."""
    n = int(round(SWEEP_BEATS * K.BEAT * SR))
    t = P.times(n)
    u = t / t[-1]
    r = D.rng(take.kit, take.item.name, 'noise')
    x = r.standard_normal(n)
    d = SR / float(D.midi_hz(K.ROOT + 24))
    comb = x.copy()
    for _ in range(3):
        comb = x + 0.6 * P.frac_delay(comb, d)
    fc = 400.0 * (9000.0 / 400.0) ** (u ** 1.6)
    bp = D.svf_tpt(comb / 3.0, fc, q=2.0)[1] / 2.0
    side = D.svf_tpt(D.rng(take.kit, take.item.name, 'side').standard_normal(n), fc, q=2.0)[1] / 10.0
    env = D.undb(-24.0 * (1.0 - u))
    y = np.stack([bp + side, bp - side], 1) * env[:, None]
    return Out(finish(y, 200.0), meta={'root': 'G4 (the comb)', 'beats': SWEEP_BEATS, 'end_aligned': True})


def zap(take):
    n = int(0.5 * SR)
    t = P.times(n)
    f0 = float(D.midi_hz(K.ROOT + 24))                              # G4
    freq = f0 * (1.0 + 7.0 * np.exp(-t / 0.004))                    # from G7, settled by 40 ms
    ph = 2 * np.pi * D.phase(freq, n)
    idx = 3.0 * np.exp(-t / 0.006)
    x = np.sin(ph + idx * np.sin(2 * ph)) * np.exp(-t / 0.09)
    r = D.rng(take.kit, take.item.name, 'click')
    click = P.hp_noise(r, n, 3000.0, order=2) * D.env_exp(n, 0.0008, 0.0001) * 0.4
    y = D.oversampled(lambda w: D.tanh_adaa(w, 1.5), x + click, 4)
    return Out(finish(D.st(y) * D.taper(n, 0.26, 0.10)[:, None], 150.0), meta={'root': 'G4', 'from': 'G7'})


def reverse_chord(take):
    n0 = int(0.62 * SR)
    r = D.rng(take.kit, take.item.name, 'ep')
    dry, fs = ep_chord(GM9, 0.9, r, n0)
    n = int(2.2 * SR)
    wet = np.zeros((n, 2))
    wet[:n0] += 0.5 * dry
    wet += 0.9 * room(np.concatenate([dry, np.zeros((n - n0, 2))]), n, 'accent reverse', t60=2.6, size=0.8,
                      damp=6500.0)
    y = wet[::-1].copy()
    return Out(finish(y), meta={'root': 'G3', 'chord': 'Gm9', 'reversed': True, 'end_aligned': True})


AH = ((730.0, 1.0, 5.0), (1090.0, 0.6, 6.0), (2440.0, 0.35, 8.0))   # "ah": formant Hz, gain, q
GM_TRIAD = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 27)                # G3 D4 Bb4


def vox_synth(take):
    n = int(1.0 * SR)
    t = P.times(n)
    y = np.zeros((n, 2))
    fs = []
    for k, note in enumerate(GM_TRIAD):
        f = float(D.midi_hz(note))
        fs.append(f)
        x = D.additive_saw(f, n, 0.1 * k, fmax=12000.0)
        v = 0.15 * x
        for fc, g, q in AH:
            v = v + g * D.svf_tpt(x, np.full(n, fc), q=q)[1] / q
        y += Ph.pan(v, -0.4 + 0.4 * k) / np.sqrt(2)
    u = np.clip((t - 0.34) / 0.25, 0.0, 1.0)
    env = P.raised(n, 0.015) * (0.5 + 0.5 * np.cos(np.pi * u))
    return Out(finish(y * env[:, None] / len(GM_TRIAD), 120.0), f0s=tuple(sorted(fs)),
               meta={'root': 'G3', 'chord': 'Gm', 'vowel': 'ah'})


PING_LEVEL = (1.0, 0.30, 0.40, 0.55, 0.35, 0.28, 0.22)
PING_DECAY = (0.35, 0.22, 0.20, 0.24, 0.12, 0.09, 0.06)


def glass_ping(take):
    n = int(1.6 * SR)
    t = P.times(n)
    f0 = float(D.midi_hz(K.ROOT + 43))                              # D6
    ph = D.rng(take.kit, take.item.name, 'modes')
    x = np.zeros(n)
    for rt, a, d in zip(RATIOS, PING_LEVEL, PING_DECAY):
        if f0 * rt < 19000:
            x += a * np.sin(2 * np.pi * f0 * rt * t + ph.uniform(0, 2 * np.pi)) * np.exp(-t / d)
    x *= P.raised(n, 0.0003)
    y = D.st(x) + 0.45 * ping_pong(x, n, 2, 4, 0.5, 9000.0, 800.0, 0.85)
    return Out(finish(y * D.taper(n, 1.2, 0.4)[:, None], 150.0), meta={'root': 'D6', 'echo_steps': 2})


# ------------------------------------------------------------------ the keys multisample
KEYS_NOTES = (K.ROOT + 12, K.ROOT + 19, K.ROOT + 24, K.ROOT + 31, K.ROOT + 36)   # G3 D4 G4 D5 G5
KEYS_LAYERS = (0.5, 1.0)


def keys_note(f, v, n, r, gate=1.2):
    """One electric-piano note: chords.ep_note, a soft drive at 4x, released over 0.25 s after `gate`."""
    t = P.times(n)
    x = D.oversampled(lambda w: D.tanh_adaa(w, chords.EP_DRIVE), chords.ep_note(f, n, v, r), 4)
    u = np.clip((t - gate) / 0.25, 0.0, 1.0)
    return x * P.raised(n, 0.0015) * (0.5 + 0.5 * np.cos(np.pi * u))


def keys_ep(take):
    n = int(1.6 * SR)
    r = D.rng(take.kit, take.item.name, take.note, 'ep')
    x = keys_note(take.f0, take.vel, n, r)
    return Out(finish(D.st(x) * P.vel_gain(take.vel, 6.0, KEYS_LAYERS[0])), f0s=(take.f0,))


# ------------------------------------------------------------------ the phrases
def phrase_rubber_riff(take):
    out = rubber.line_out('Riff', rubber.CLEAN)
    return Out(out.audio, hits=out.hits, meta={**out.meta, 'phrase': 'Riff'})


DUB_BARS = (GM9, GM9, CM9, GM9)
DUB_RHYTHM = ((3, 0.85), (6, 0.55), (11, 0.8), (14, 0.6))         # per bar
DUB_STEPS = 64


def phrase_dub_chords(take):
    G = Ph.Grid(K, DUB_STEPS)
    L = G.length
    n = int(0.62 * SR)
    y = np.zeros((L, 2))
    played = []
    for b, chord in enumerate(DUB_BARS):
        for k, (s, v) in enumerate(DUB_RHYTHM):
            r = D.rng(take.kit, take.item.name, b, k)
            x, _ = ep_chord(chord, v, r, n)
            Ph.place(y, x, G.at(16 * b + s))
            played.append((16 * b + s, chord[0], v))
    wet = chords.dub_echo(y, L)
    send = D.bfilt(D.mono(y + wet), 400.0, 'highpass')
    rv = Ph.two_pass(lambda u: D.fdn_reverb(u, K.space_rng('phrase dub'), t60=1.6, size=0.6, damp_hz=6000.0),
                     send, 0.02)[:L]
    z = y + wet + 0.22 * rv
    u = np.arange(L) / L
    z = chords.lp_ride(z, 800.0 * (6000.0 / 800.0) ** (0.5 - 0.5 * np.cos(2 * np.pi * u)))
    beats = [G.at(s) for s in range(0, DUB_STEPS, 4)]
    z = z * D.undb(Ph.pump_db(beats, L, *chords.PUMP))[:, None]
    z = Ph.circular(lambda w: D.mono_below(D.bfilt(w, 110.0, 'highpass', order=4), 150.0), z)
    return Out(z, meta={'steps': DUB_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'bars': ['Gm9', 'Gm9', 'Cm9', 'Gm9'],
                        'hits': [[s, D.note_name(m), v] for s, m, v in played]})


# (16th, MIDI, length in 16ths, velocity): G4 67, A4 69, Bb4 70, C5 72, D5 74, F5 77, G5 79
KEYS_RIFF = (
    (0, 74, 1.5, 0.9), (3, 77, 1.0, 0.7), (6, 74, 1.0, 0.8), (8, 72, 2.0, 0.85), (11, 70, 1.0, 0.6),
    (14, 67, 2.0, 0.75),                                            # bar 1, the call: D5 F5 D5 C5 Bb4 G4
    (16, 74, 1.5, 0.9), (19, 77, 1.0, 0.7), (22, 79, 1.5, 0.85), (25, 77, 1.0, 0.6), (27, 74, 1.0, 0.7),
    (30, 69, 2.0, 0.7),                                             # bar 2, the answer: up to G5, down to A4
)
ARP_CHORDS = ((67, 70, 74, 77, 81), (72, 75, 79, 82, 86)) * 2     # Gm9 over G4, Cm9 over C5: a bar each, twice
ARP_ORDER = ((0, 1, 2, 3, 4, 3, 2, 1),) * 3 + ((4, 3, 2, 1, 0, 1, 2, 3),)   # up and down; the last bar down first
ARP_STEPS = 64


def keys_line(notes, steps, key, gate_factor=1.0):
    G = Ph.Grid(K, steps)
    y = np.zeros((G.length, 2))
    n = int(1.0 * SR)
    for i, (s, m, ln, v) in enumerate(notes):
        r = D.rng(K.KIT, key, i)
        x = keys_note(float(D.midi_hz(m)), v, n, r, gate=ln * gate_factor * G.s16 / SR)
        Ph.place(y, Ph.pan(x, 0.0), G.at(s))
    return y, G


def phrase_keys_riff(take):
    y, G = keys_line(KEYS_RIFF, 32, 'keys riff')
    z = y + 0.6 * chords.dub_echo(y, G.length)                      # round the loop: the echoes wrap
    z = Ph.circular(lambda w: D.mono_below(D.bfilt(w, 110.0, 'highpass', order=4), 150.0), z)
    return Out(z, meta={'steps': 32, 'bpm': K.BPM, 'swing': K.SWING,
                        'notes': [[s, D.note_name(m), ln, v] for s, m, ln, v in KEYS_RIFF]})


def arp_notes():
    out = []
    for b, chord in enumerate(ARP_CHORDS):
        for k in range(16):
            out.append((16 * b + k, chord[ARP_ORDER[b][k % 8]], 0.7, 0.85 if k % 4 == 0 else (0.6 if k % 2 else 0.7)))
    return tuple(out)


ARP = arp_notes()


def phrase_keys_arp(take):
    y, G = keys_line(ARP, ARP_STEPS, 'keys arp')
    L = G.length
    d = int(np.floor(2 * K.STEP * SR + 0.5))
    echo = np.zeros((L, 2))
    Ph.place(echo, 0.4 * D.bfilt(y, 4000.0, 'lowpass') * np.array([0.3, 1.0]), d)
    z = Ph.circular(lambda w: D.mono_below(D.bfilt(w, 110.0, 'highpass', order=4), 150.0), y + echo)
    return Out(z, meta={'steps': ARP_STEPS, 'bpm': K.BPM, 'swing': K.SWING, 'chords': ['Gm9', 'Cm9', 'Gm9', 'Cm9'],
                        'notes': [[s, D.note_name(m), ln, v] for s, m, ln, v in ARP]})


ONE = dict(layers=(1.0,), rr=1, sets=SETS)
MOVING = {'tonal': False, 'why': 'a moving shot (a boing and a pitch drop) has no steady tone for the alias check'}
ITEMS = [
    Item('Stabs', 'AccentDubChordCm9', 'stab', dub_chord_cm9, root=K.ROOT + 17, **ONE,
         qa={'sustain': (0.10, 0.34), 'why': 'the alias check reads the stab itself (100-340 ms): its echoes are gated '
                                             'copies of it, whose bursts and gaps spread every partial'},
         about='accent: the electric piano\'s Cm9 (C4 G4 Bb4 D5 Eb5) with its dub tail baked in, a dotted-8th '
               'ping-pong darkening over four repeats and a short room; root C4'),
    Item('Stabs', 'AccentOrganHitGm7', 'stab', organ_hit, root=K.ROOT + 12, **ONE,
         about='accent: a drawbar organ hit, Gm7 (G3 D4 F4 Bb4): five bars, a percussion on the 3rd harmonic, a key '
               'click, a soft valve drive; root G3'),
    Item('Bass', 'AccentRubberStabG1', 'bass', rubber_stab, root=K.KICK_NOTE, qa=MOVING, **ONE,
         about='accent: the rubber bass as a stab, the full boing, dropping from G2 to G1 in 60 ms over the sine sub; A '
               'clean, the strict gate; root G1'),
    Item('Stabs', 'AccentPluckChordDm7', 'stab', pluck_chord, root=K.ROOT + 19, **ONE,
         qa={'tonal': False, 'why': "plucked strings: the Karplus-Strong loop filter's phase delay moves the upper "
                                    "partials off the exact harmonics, which the alias check reads as aliasing"},
         about='accent: Dm7 (D4 A4 C5 F5) on tuned plucked strings, strummed down 11 ms a string, in a short room; '
               'root D4'),
    Item('FX', 'AccentNoiseSweepUp', 'fx', noise_sweep, flags={'cut_end'}, **ONE,
         about='accent: 2 beats of noise through a band-pass rising from 400 Hz to 9 kHz and a comb tuned to G4, '
               'swelling into the downbeat; root G4 (the comb)'),
    Item('Perc', 'AccentZapG4', 'perc', zap, root=K.ROOT + 24, qa={'tune_window': (0.05, 0.15),
         'why': 'a zap: its pitch falls onto G4 within 40 ms, read over 50-150 ms'}, **ONE,
         about='accent: a perc synth zap, FM falling from G7 to G4 in about 15 ms, a short G4 tone and a click; '
               'root G4'),
    Item('FX', 'AccentReverseChordGm9', 'fx', reverse_chord, flags={'cut_end'}, **ONE,
         about='accent: the dub stab\'s Gm9 through a long plate, reversed, swelling into the downbeat; root G3'),
    Item('Stabs', 'AccentVoxSynthGm', 'stab', vox_synth, root=K.ROOT + 12, **ONE,
         about='accent: a vocal-synth stab, Gm (G3 D4 Bb4) on saws through three static "ah" formants; root G3'),
    Item('Perc', 'AccentGlassPingD6', 'perc', glass_ping, root=K.ROOT + 43, qa={'tune_window': (0.03, 0.30),
         'why': "a glass ping rings short: its pitch is read over 30-300 ms"}, **ONE,
         about='accent: the kit\'s glass bell struck on D6 with an 8th-note ping-pong; root D6'),
    Item('Lead', 'KeysEP', 'lead', keys_ep, layers=KEYS_LAYERS, notes=KEYS_NOTES, sets=SETS,
         about='electric piano keys: the dub stab\'s FM tine voice as a playable multisample, G3 D4 G4 D5 G5, two '
               'layers; the keys phrases play it'),
    Item('Loops', 'PhraseRubberRiff', 'loop', phrase_rubber_riff, loop=True, sets=SETS,
         qa={'mud': 'gate', 'why': 'A, clean: a bass phrase under the strict gate, resting on every kick (declared)'},
         about='phrase: a rolling 2-bar bass riff on the rubber bass, A clean: G1 pops, F2 sliding to D2, Bb1 sliding '
               'to G1; bar 2 a held D2, C2 lifting to D2, C2 sliding down to G1'),
    Item('Loops', 'PhraseDubChords', 'loop', phrase_dub_chords, loop=True, sets=SETS,
         qa={'mud': 'lowcut', 'why': 'a chord phrase: the low cut, like the stab'},
         about='phrase: 4 bars of dub chords, Gm9 Gm9 Cm9 Gm9 on the electric piano, the dub echo, a room, a low-pass '
               'ride and the pump'),
    Item('Loops', 'PhraseKeysRiff', 'loop', phrase_keys_riff, loop=True, sets=SETS,
         qa={'mud': 'lowcut', 'why': 'a keys phrase: the low cut, like the keys'},
         about='phrase: a 2-bar electric-piano riff, D5 F5 D5 C5 Bb4 G4 answered up to G5 and down to A4, with a '
               'dotted-8th echo'),
    Item('Loops', 'PhraseKeysArp', 'loop', phrase_keys_arp, loop=True, sets=SETS,
         qa={'mud': 'lowcut', 'why': 'a keys phrase: the low cut, like the keys'},
         about='phrase: a 4-bar 16th arpeggio on the keys, Gm9 over G4 and Cm9 over C5 in turn, up and down (the last '
               'bar down first), with an 8th echo'),
]
