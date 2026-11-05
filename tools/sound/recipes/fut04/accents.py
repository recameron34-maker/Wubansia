"""FUT04 round 5 synth accents and phrases (set r5): one-shots to drop into a dark industrial techno track and original
2- and 4-bar synth riffs, every one in E minor at 136 BPM on the kit's swing, each with its root stated (in its name and
its `root`).

**Accents**:
- AccentRaveStabEm (root E4): a dark rave stab, Em (E4 B4 E5 G5) on detuned saws (the root one saw, undetuned),
  dropping a semitone into pitch over 25 ms, a filter blip that closes onto 1.1 kHz, each note through its own drive,
  into a short dark room.
- AccentMetalChordEm (root E4): a metal chord hit: each note of Em struck as the kit's iron (its seven modes on
  K.RATIOS, the warbling pairs beating) over a saw chord under a falling low-pass, a clank on the strike.
- AccentAcidStabE4 (root E4): the kit's acid voice as a stab: the saw through the 4-pole ladder (resonance 0.9) at 4x,
  the filter snapping open to 6 kHz and closing onto 1.1 kHz in 90 ms, the furnace grit after it, E5 an octave above;
  a gentle high-pass at 380 Hz thins its fundamental.
- AccentDistChordEm (root E4): a distorted chord, Em(add9) (E4 B4 E5 F#5 G5: the 9 and the minor third a semitone apart
  at the top), each note through its own hard drive and fold at 4x (no intermodulation, so it stays in tune), gated at
  0.3 s into the dark room.
- AccentVoxChordEm (root E3): the vox synth (voxsynth.py) on Em (E3 B3 E4 G4), "oh" closing to "oo", gated, driven,
  in the kit's dark hall.
- AccentZapE5 (root E5): a feedback zap: a sine falling from E8 onto E5 in about 25 ms through a feedback comb tuned to
  E5 (the ring of a delay at its edge), driven, a click on the front.
- AccentSubDropE2 (root E2): a clean sine on E2 held 120 ms, then dropping two octaves over 1.2 s, a soft thump on its
  onset: the drop into a section (the only accent in the low end).
- AccentNoiseSweepE (root E3, its comb): an industrial noise sweep, 1 bar into the downbeat: noise through a comb tuned
  to E3 and a band-pass rising from 400 Hz to 9 kHz, crushed (a sample-and-hold whose rate rises with it), 18 dB up to
  its last sample (cut_end).
- AccentReverseHitEm (root E4): the metal chord hit through the dark hall, reversed: it swells over 3 beats into the
  beat it ends on (cut_end).

**Phrases** (loops on the kit's grid and swing, and MIDI phrases on channel 4, recipes/fut04/midi.py):
- PhraseAcidStabs (2 bars): the acid stab in a syncopated E minor riff, E4 G4 E4 D4 F#4, answered up to A4 and B4.
- PhraseMetalChords (4 bars): the metal chord hit on the off-beats, Em Em Am Bm (the Em sample a fourth and a fifth up),
  into a dub echo and the dark hall.
- PhraseRaveRiff (2 bars): the rave stab, a hypnotic 3-3-2 on Em, turning to Am and Bm at the end of bar 2.

Everything but the sub drop is high-passed far above the kick (200-400 Hz) and mono under 150 Hz. The tonal accents are
tuned on their root (+-3 cents); where a sound has inharmonic or driven parts by design, its alias check is off with the
reason.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K
from tools.sound.recipes.fut04 import bass as B, voxsynth as V

SR = D.SR
R5 = ('r5',)
RATIOS = np.array(K.RATIOS)
EM = (64, 71, 76, 79)               # E4 B4 E5 G5: Em, open
AM = tuple(n + 5 for n in EM)       # the same shape a fourth up: Am (A4 E5 A5 C6)
BM = tuple(n + 7 for n in EM)       # a fifth up: Bm (B4 F#5 B5 D6)


def hz(note):
    return float(D.midi_hz(note))


def finish(y, hp=250.0, order=4):
    """High-passed (hp, Butterworth) and mono under 150 Hz."""
    return D.mono_below(D.bfilt(D.st(y), hp, 'highpass', order=order), 150.0)


def room(y, n, key, t60=1.4, size=0.6, damp=3500.0, send_hp=450.0):
    """The kit's dark concrete room on y, returned alone (its space seed), mono under 150 Hz, n samples long."""
    send = D.bfilt(D.mono(y), send_hp, 'highpass', order=4)
    rv = D.fdn_reverb(send, K.space_rng(key), t60=t60, size=size, damp_hz=damp, n_tail=max(0, n - len(send)))[:n]
    return D.mono_below(rv, 150.0)


# ------------------------------------------------------------------ the rave stab
RAVE_DETUNE = (-9.0, 0.0, 9.0)      # cents: three saws a note, the root one alone


def rave_voice(notes, n, v, r):
    """The rave stab before its room: three band-limited saws a note (the root one, undetuned), the chord dropping a
    semitone into pitch over 25 ms, a filter blip (to 6-8.5 kHz, closing onto 1.1 kHz in 90 ms), each note through its
    own tanh at 4x (driven, no intermodulation); a smooth release from 0.18 s. Returns (audio, fundamentals)."""
    t = P.times(n)
    drop = 2.0 ** (1.0 / 12.0 * np.exp(-t / 0.008))
    fc = 1100.0 + (5000.0 + 2500.0 * v) * np.exp(-t / 0.09)
    y = np.zeros((n, 2))
    f0s = []
    for i, note in enumerate(notes):
        note_y = np.zeros((n, 2))
        for c, cents in enumerate(RAVE_DETUNE):
            if i == 0 and c != 1:
                continue
            f = hz(note) * 2 ** (cents / 1200)
            f0s.append(f)
            osc = D.additive_saw(f * drop, n, r.uniform(), fmax=14000.0)
            pan = (0.7, 0.7) if c == 1 else ((0.40, 0.12) if c == 0 else (0.12, 0.40))
            note_y += np.outer(osc, pan)
        note_y = np.stack([D.svf_tpt(note_y[:, c], fc, q=1.1)[0] for c in range(2)], 1)
        y += D.oversampled(lambda u: D.tanh_adaa(u, 2.0), 0.8 * note_y, 4)
    y = y / len(notes) * (P.raised(n, 0.001) / (1.0 + (t / 0.18) ** 8))[:, None]
    return y, f0s


def rave_stab(take):
    n = int(1.2 * SR)
    y, f0s = rave_voice(EM, n, 1.0, D.rng(take.kit, take.item.name, 'saws'))
    y = (y + 0.30 * room(y, n, 'rave stab')) * D.taper(n, 0.85, 0.3)[:, None]
    return Out(finish(y), f0s=tuple(sorted(f0s)), meta={'root': 'E4', 'chord': 'Em', 'notes': 'E4 B4 E5 G5'})


# ------------------------------------------------------------------ the metal chord hit, and its reverse
METAL_AMPS = (1.0, 0.55, 0.55, 0.35, 0.25, 0.25, 0.15)   # each pair at one level, so it beats
METAL_DECAYS = (0.40, 0.22, 0.20, 0.12, 0.09, 0.08, 0.05)


def metal_voice(notes, n, v, r):
    """The metal chord before its room: each note struck as the kit's iron (its modes on RATIOS, the strike bending
    them 0.6 % sharp for 6 ms) over a saw chord under a low-pass falling from 6 kHz to 700 Hz, a clank (the first 30 ms
    driven at 4x, over 1.5 kHz) and a click. Returns (audio, fundamentals)."""
    t = P.times(n)
    iron = np.zeros(n)
    saws = np.zeros(n)
    for note in notes:
        f = hz(note)
        bend = 0.006 * np.exp(-t / 0.006)
        iron += P.modes(n, f, RATIOS, METAL_DECAYS, METAL_AMPS, bend, r)
        saws += D.additive_saw(f, n, r.uniform(), fmax=12000.0)
    iron /= len(notes)
    saws = D.svf_tpt(saws / len(notes), 700.0 + 5300.0 * np.exp(-t / 0.06), q=1.2)[0] * np.exp(-t / 0.25)
    head = iron * P.gate(t, 0.01, 0.012) * D.taper(n, 0.03, 0.015)
    clank = D.bfilt(D.oversampled(lambda u: D.tanh_adaa(u, 3.0 + 3.0 * v, bias=0.2), head, 4), 1500.0, 'highpass',
                    order=4)
    click = P.hp_noise(r, n, 3000.0, order=2) * D.env_exp(n, 0.0015, 0.0002) * 0.2
    x = (0.8 * iron + 0.45 * saws + 0.35 * clank + click) * P.raised(n, 0.0005)
    side = D.bfilt(P.allpass(x, 61, 0.6), 1200.0, 'highpass', order=2) * 0.35
    return np.stack([x + side, x - side], 1), [hz(m) for m in notes]


def metal_chord(take):
    n = int(1.8 * SR)
    y, _ = metal_voice(EM, n, 1.0, D.rng(take.kit, take.item.name, 'iron'))
    y = (y + 0.30 * room(y, n, 'metal chord')) * D.taper(n, 1.3, 0.45)[:, None]
    return Out(finish(y), meta={'root': 'E4', 'chord': 'Em', 'notes': 'E4 B4 E5 G5'})


REVERSE_BEATS = 3


def reverse_hit(take):
    """The metal chord hit and 2.4 s of the dark hall, reversed, the last 3 beats kept (a raised-cosine fade-in over
    the first beat): it swells into the beat it ends on."""
    n0 = int(0.7 * SR)
    hit, _ = metal_voice(EM, n0, 1.0, D.rng(take.kit, take.item.name, 'iron'))
    n = int(2.4 * SR)
    wet = np.zeros((n, 2))
    wet[:n0] += 0.45 * hit
    wet += 0.9 * room(np.concatenate([hit, np.zeros((n - n0, 2))]), n, 'reverse hit', t60=3.2, size=1.0, damp=3000.0,
                      send_hp=400.0)
    y = wet[::-1]
    m = int(round(REVERSE_BEATS * K.BEAT * SR))
    y = y[-m:] * P.raised(m, K.BEAT)[:, None]
    return Out(finish(y), meta={'root': 'E4', 'chord': 'Em', 'reversed': True, 'beats': REVERSE_BEATS,
                                'end_aligned': True})


# ------------------------------------------------------------------ the acid stab
ACID_STAB = (64, 76)                # E4, and E5 an octave up at a third of its level
ACID_HP = (380.0, 2)                # a gentle high-pass (Hz, order) under the root: the saw's strong fundamental
                                    # thinned 4-6 dB, so the stab's body sits over the low mids


def acid_stab_voice(note, n, v, length_s=0.22):
    """The acid voice (bass.acid: the band-limited saw into the ladder at 4x, resonance bass.RES, the grit) on the note
    and its octave, the filter snapping to 6 kHz and closing onto 1.1 kHz in 90 ms (harder hits open further); gated."""
    t = P.times(n)
    fc = 1100.0 + (3000.0 + 2000.0 * v) * np.exp(-t / 0.09)
    y = np.zeros(n)
    for k, g in ((0, 1.0), (12, 0.33)):
        y += g * B.acid(np.full(n, hz(note + k)), fc)
    gate = P.raised(n, 0.002) * D.taper(n, length_s, 0.04)
    return y * gate


def acid_stab(take):
    n = int(0.6 * SR)
    x = acid_stab_voice(ACID_STAB[0], n, 1.0)
    side = D.bfilt(P.allpass(x, 47, 0.6), 1000.0, 'highpass', order=2) * 0.25
    y = np.stack([x + side, x - side], 1)
    y = y + 0.18 * room(y, n, 'acid stab', t60=1.0, size=0.5)
    return Out(finish(y, *ACID_HP) * D.taper(n, 0.45, 0.12)[:, None], f0s=tuple(hz(m) for m in ACID_STAB),
               meta={'root': 'E4', 'octave': 'E5', 'voice': 'bass.acid'})


# ------------------------------------------------------------------ the distorted chord
DIST = (64, 71, 76, 78, 79)         # E4 B4 E5 F#5 G5: Em(add9)


def dist_chord(take):
    """Each note of Em(add9) on two saws (+-6 cents), through its own hard drive and a fold at 4x, so the chord grinds
    with no intermodulation; low-passed at 6 kHz, gated at 0.30 s, into the dark room."""
    n = int(1.3 * SR)
    t = P.times(n)
    r = D.rng(take.kit, take.item.name, 'saws')
    y = np.zeros((n, 2))
    f0s = []
    for i, note in enumerate(DIST):
        f = hz(note)
        pair = []
        for cents in (-6.0, 6.0):
            fk = f * 2 ** (cents / 1200)
            f0s.append(fk)
            pair.append(D.additive_saw(fk, n, r.uniform(), fmax=6000.0))
        for c in range(2):
            u = 0.75 * pair[c] + 0.25 * pair[1 - c]
            u = D.oversampled(lambda w: D.sinefold_adaa(D.tanh_adaa(w, 4.0), 1.6), 0.9 * u, 4)
            y[:, c] += u
    y = D.bfilt(y / len(DIST), 6000.0, 'lowpass', order=2)
    y = y * (P.raised(n, 0.002) * D.taper(n, 0.30, 0.05))[:, None]
    y = y + 0.30 * room(y, n, 'dist chord', t60=1.2, size=0.6)
    return Out(finish(y, 300.0), f0s=tuple(sorted(f0s)), meta={'root': 'E4', 'chord': 'Em(add9)',
                                                                'notes': 'E4 B4 E5 F#5 G5'})


# ------------------------------------------------------------------ the vox chord stab
VOX_CHORD = (52, 59, 64, 67)        # E3 B3 E4 G4: Em


def vox_chord(take):
    n = int(3.4 * SR)
    hold, rel = 0.22, 0.05
    y = np.zeros((n, 2))
    for k, note in enumerate(VOX_CHORD):
        r = D.rng(take.kit, take.item.name, note)
        x = V.voice(hz(note), 1.0, n, 'oh', hold, rel, r, drive=2.4, window=(0.05, 0.22))
        y += Ph.pan(x, (-0.3 + 0.2 * k)) / np.sqrt(2) * (1.4 if k == 0 else 1.0)
    y = y / len(VOX_CHORD)
    y = y + V.HALL['level'] * V.hall(y, n, 'vox chord')
    return Out(V.finish(y * D.taper(n, 2.6, 0.7)[:, None]), meta={'root': 'E3', 'chord': 'Em', 'vowel': 'oh'})


# ------------------------------------------------------------------ the zap
def zap(take):
    """A sine falling from E8 onto E5 (most of the way in 25 ms) into a feedback comb tuned to E5 (feedback 0.82: the
    ring of a delay at its edge), driven at 4x, a click on the front; gone by 0.4 s."""
    n = int(1.3 * SR)
    t = P.times(n)
    f0 = hz(76)
    freq = f0 * 2.0 ** (3.0 * np.exp(-t / 0.008))
    x = np.sin(2 * np.pi * D.phase(freq, n)) * np.exp(-t / 0.10)
    d = SR / f0
    comb = x.copy()
    for _ in range(6):
        comb = x + 0.82 * P.frac_delay(comb, d)
    r = D.rng(take.kit, take.item.name, 'click')
    click = P.hp_noise(r, n, 4000.0, order=2) * D.env_exp(n, 0.0008, 0.0001) * 0.5
    y = D.oversampled(lambda u: D.tanh_adaa(u, 2.5), 0.35 * comb + click, 4)
    y = D.st(y) * D.taper(n, 0.28, 0.12)[:, None]
    y = (y + 0.20 * room(y, n, 'zap', t60=1.0, size=0.5)) * D.taper(n, 0.8, 0.45)[:, None]
    return Out(finish(y, 300.0), meta={'root': 'E5', 'from': 'E8', 'comb': 'E5'})


# ------------------------------------------------------------------ the sub drop
SUB_DROP = dict(note=40, hold=0.12, octaves=2.0, tau=0.40, length=1.6)


def sub_drop(take):
    """A clean sine on E2 held 120 ms, then dropping two octaves (exponentially, most of it in the first half second),
    its level falling slowly: a pure sub with nothing in the low mids; a soft thump of 1-4 kHz noise on the onset."""
    s = SUB_DROP
    n = int((s['length'] + 0.6) * SR)
    t = P.times(n)
    f = hz(s['note']) * 2.0 ** (-s['octaves'] * (1 - np.exp(-np.maximum(t - s['hold'], 0) / s['tau'])))
    y = np.sin(2 * np.pi * D.phase(f, n)) * P.raised(n, 0.012) * np.exp(-np.maximum(t - 0.3, 0) / 0.7)
    y = y * D.taper(n, s['length'] - 0.5, 0.45)
    thump = P.band_noise(take.rng('thump'), n, 1000.0, 4000.0) * D.env_exp(n, 0.003, 0.0003)
    return D.st(0.7 * y + 0.03 * thump)


# ------------------------------------------------------------------ the noise sweep
SWEEP_BARS = 1


def noise_sweep(take):
    """1 bar of noise through a comb tuned to E3 (feedback 0.6, its resonances on E's harmonics) and a band-pass whose
    centre rises from 400 Hz to 9 kHz, crushed by a sample-and-hold whose rate rises from 2 to 24 kHz with it (the
    industrial grit, aliasing on purpose), the level rising 18 dB to the last sample: the downbeat (cut_end)."""
    n = int(round(SWEEP_BARS * 16 * K.STEP * SR))
    t = P.times(n)
    u = t / t[-1]
    r = D.rng(take.kit, take.item.name, 'noise')
    x = r.standard_normal(n)
    d = SR / hz(52)
    comb = x.copy()
    for _ in range(4):
        comb = x + 0.6 * P.frac_delay(comb, d)
    fc = 400.0 * (9000.0 / 400.0) ** (u ** 1.5)
    band = D.svf_tpt(comb / 2.5, fc, q=1.6)[1]
    rate = 2000.0 * (24000.0 / 2000.0) ** u                  # the sample-and-hold's rate
    held = np.floor(np.cumsum(rate / SR)).astype(int)
    first = np.concatenate([[0], np.flatnonzero(np.diff(held)) + 1])
    crushed = band[first[np.searchsorted(first, np.arange(n), side='right') - 1]]
    y = (0.6 * band + 0.4 * crushed) * D.undb(-18.0 * (1 - u) ** 1.3) * P.raised(n, 0.4)
    side = D.bfilt(D.svf_tpt(D.rng(take.kit, take.item.name, 'side').standard_normal(n), fc, q=1.6)[1], 1500.0,
                   'highpass') * 0.25 * u
    out = np.stack([y + side, y - side], 1)
    return Out(finish(out, 300.0), meta={'root': 'E3 (the comb)', 'bars': SWEEP_BARS, 'end_aligned': True})


# ------------------------------------------------------------------ the phrases
PHRASE_LUFS = -16.0                 # every phrase loop at one loudness
# (16th, MIDI, length in 16ths, velocity): E4 64, G4 67, D4 62, F#4 66, A4 69, B4 71
ACID_RIFF = ((2, 64, 0.75, 1.0), (5, 64, 0.5, 0.6), (7, 67, 0.75, 0.9), (10, 64, 0.75, 1.0), (13, 62, 0.5, 0.7),
             (14, 66, 1.5, 0.9),
             (18, 64, 0.75, 1.0), (21, 64, 0.5, 0.6), (23, 69, 0.75, 0.9), (26, 67, 0.75, 1.0), (28, 66, 0.5, 0.7),
             (29, 64, 0.5, 0.6), (30, 71, 1.5, 1.0))


def phrase_acid_stabs(take):
    """The acid stab riff at its exact pitches on the stab's own voice (each note restarts the filter), gated to its
    length; tails wrap."""
    G = Ph.Grid(K, 32)
    y = np.zeros((G.length, 2))
    n = int(0.5 * SR)
    for step, note, length, vel in ACID_RIFF:
        x = acid_stab_voice(note, n, vel, length_s=length * K.STEP)
        side = D.bfilt(P.allpass(x, 47, 0.6), 1000.0, 'highpass', order=2) * 0.25
        Ph.place(y, np.stack([x + side, x - side], 1) * vel ** 1.2, G.at(step))
    y = Ph.circular(lambda w: finish(w, *ACID_HP), y)
    return Out(y, meta={'bars': 2, 'bpm': K.BPM, 'swing': K.SWING, 'sample_root': 'E4',
                        'notes': [[s, D.note_name(m), ln, v] for s, m, ln, v in ACID_RIFF]})


CHORD_BARS = (EM, EM, AM, BM)
CHORD_NAMES = ('Em', 'Em', 'Am', 'Bm')
CHORD_RHYTHM = ((2, 0.9), (6, 0.6), (10, 0.85), (13, 0.55))   # per bar: the off-beats, and a swung push into beat 4


def phrase_metal_chords(take):
    G = Ph.Grid(K, 64)
    L = G.length
    n = int(0.7 * SR)
    y = np.zeros((L, 2))
    for b, chord in enumerate(CHORD_BARS):
        for k, (s, v) in enumerate(CHORD_RHYTHM):
            x, _ = metal_voice(chord, n, v, D.rng(take.kit, take.item.name, b, k))
            Ph.place(y, x * v ** 1.3 * D.taper(n, 0.4, 0.25)[:, None], G.at(16 * b + s))
    echo = Ph.two_pass(lambda u: S.echo(u, K.space_rng('metal chords echo'), S.ECHO_STEPS * K.STEP,
                                        **{**S.ECHO, 'lp_hz': 2200.0, 'hp_hz': 400.0}), D.mono(y), 0.02)
    space = {**S.SPACE, 't60': 3.6, 'damp_hz': 3000.0, 'send_hp': 400.0, 'predelay': K.STEP / 4, 'shimmer': 0.03}
    hall = Ph.two_pass(lambda u: S.space(u, K.space_rng('metal chords'), **space), D.mono(y), 0.02)
    z = y + 0.35 * echo + 0.25 * hall
    z = Ph.circular(lambda w: finish(w), z)
    return Out(z, meta={'bars': 4, 'bpm': K.BPM, 'swing': K.SWING, 'chords': list(CHORD_NAMES),
                        'rhythm': [list(h) for h in CHORD_RHYTHM]})


RAVE_RIFF = ((2, EM, 0.95), (5, EM, 0.7), (8, EM, 0.85), (11, EM, 0.7), (14, EM, 0.9),
             (18, EM, 0.95), (21, EM, 0.7), (24, EM, 0.85), (27, AM, 0.8), (30, BM, 0.95))


def phrase_rave_riff(take):
    G = Ph.Grid(K, 32)
    y = np.zeros((G.length, 2))
    n = int(0.5 * SR)
    for k, (s, chord, v) in enumerate(RAVE_RIFF):
        x, _ = rave_voice(chord, n, v, D.rng(take.kit, take.item.name, k))
        Ph.place(y, x * v ** 1.3 * D.taper(n, 0.24, 0.06)[:, None], G.at(s))
    y = y + 0.30 * Ph.two_pass(lambda u: room(u, len(u), 'rave riff'), y, 0.02)
    y = Ph.circular(lambda w: finish(w), y)
    return Out(y, meta={'bars': 2, 'bpm': K.BPM, 'swing': K.SWING,
                        'hits': [[s, {EM: 'Em', AM: 'Am', BM: 'Bm'}[c], v] for s, c, v in RAVE_RIFF]})


def chord_root(chord):
    return chord[0]


def midi_phrases():
    """The three phrases as midi.py's PARTS['phrases'] entries (each on its own sample, rooted as its file)."""
    return [
        dict(name='Acid stab riff', sample='Accent Acid Stab E4', zones='E4',
             notes=[(s, m, ln, v, False) for s, m, ln, v in ACID_RIFF], bars=2),
        dict(name='Metal chords, Em Em Am Bm', sample='Accent Metal Chord Em', zones='E4 (A4 plays Am, B4 Bm)',
             notes=[(16 * b + s, chord_root(chord), 1.0, v, False) for b, chord in enumerate(CHORD_BARS)
                    for s, v in CHORD_RHYTHM], bars=4),
        dict(name='Rave stab riff', sample='Accent Rave Stab Em', zones='E4 (A4 plays Am, B4 Bm)',
             notes=[(s, chord_root(c), 1.0, v, False) for s, c, v in RAVE_RIFF], bars=2),
    ]


ONE = dict(layers=(1.0,), rr=1, sets=R5)
DRIVEN = {'tonal': False, 'why': 'a driven, folded chord: the alias check reads its grind, not aliasing; tuned on its '
                                 'root'}
ITEMS = [
    Item('Stabs', 'AccentRaveStabEm', 'stab', rave_stab, root=64, **ONE,
         qa={'tune_window': (0.05, 0.20), 'tonal': False,
             'why': 'a stab: read over its body, 50-200 ms; each note driven (the drive spreads its partials past the '
                    "alias check's 8 Hz in the fast release)"},
         about='accent, dark rave stab on E4: Em (E4 B4 E5 G5) on detuned saws, dropping a semitone into pitch, a '
               'filter blip, each note driven, a dark room'),
    Item('Stabs', 'AccentMetalChordEm', 'stab', metal_chord, root=64, **ONE,
         qa={'tonal': False, 'tune_window': (0.05, 0.30),
             'why': "struck iron: the kit's modal ratios are inharmonic by design; tuned on its root"},
         about="accent, metal chord hit on E4: Em, each note struck as the kit's iron over a saw chord under a falling "
               'low-pass, a clank'),
    Item('Stabs', 'AccentAcidStabE4', 'stab', acid_stab, root=64, **ONE,
         qa={'tonal': False, 'tune_window': (0.05, 0.22),
             'why': "the acid voice's ladder sweeps its resonance through the partials: read over the gate, 50-220 ms"},
         about="accent, acid stab on E4: the kit's acid voice and its octave, the ladder snapping open and shut, the "
               'grit'),
    Item('Stabs', 'AccentDistChordEm', 'stab', dist_chord, root=64, **ONE, qa={**DRIVEN, 'tune_window': (0.05, 0.28)},
         about='accent, distorted chord on E4: Em(add9) (E4 B4 E5 F#5 G5), each note hard-driven and folded, gated at '
               '0.3 s, a dark room'),
    Item('Stabs', 'AccentVoxChordEm', 'stab', vox_chord, root=52, **ONE,
         qa={'tonal': False, 'tune_window': (0.05, 0.22),
             'why': 'the vox synth: breath between the harmonics; read over its gate, 50-220 ms'},
         about='accent, vox-synth chord stab on E3: Em (E3 B3 E4 G4), "oh" closing to "oo", gated, driven, in the dark '
               'hall; wordless'),
    Item('Perc', 'AccentZapE5', 'perc', zap, root=76, **ONE,
         qa={'tune_window': (0.05, 0.15), 'why': 'a zap: its pitch falls onto E5 within 40 ms, read over 50-150 ms'},
         about='accent, feedback zap on E5: a sine falling from E8 into a feedback comb tuned to E5, driven, a click'),
    Item('Bass', 'AccentSubDropE2', 'bass', sub_drop, root=40, **ONE,
         qa={'tonal': False, 'tune_window': (0.02, 0.11), 'loop_check': False,
             'why': 'a pitch drop: on E2 for its first 120 ms (the tuning window), then gliding, so no steady tone for '
                    'the alias check; an accent for a section change, not a groove bass shot'},
         about='accent, sub drop from E2: a clean sine held 120 ms, then dropping two octaves over 1.2 s'),
    Item('FX', 'AccentNoiseSweepE', 'fx', noise_sweep, root=52, level=-15.0, flags={'cut_end'}, tags=('riser',), **ONE,
         about='accent, industrial noise sweep on E: 1 bar of noise through a comb tuned to E3 and a rising band-pass, '
               'crushed, rising 18 dB into the downbeat'),
    Item('FX', 'AccentReverseHitEm', 'fx', reverse_hit, root=64, flags={'cut_end'}, **ONE,
         about='accent, reverse hit on E4: the metal chord hit and the dark hall reversed, swelling over 3 beats into '
               'the beat it ends on'),
    Item('Loops', 'PhraseAcidStabs', 'loop', phrase_acid_stabs, loop=True, level=PHRASE_LUFS, sets=R5,
         qa={'mud': 'lowcut', 'why': 'a stab phrase: the low cut, like its stab'},
         about='phrase, 2-bar acid stab riff in E minor at 136 BPM: E4 G4 E4 D4 F#4, answered up to A4 and B4'),
    Item('Loops', 'PhraseMetalChords', 'loop', phrase_metal_chords, loop=True, level=PHRASE_LUFS, sets=R5,
         qa={'mud': 'lowcut', 'why': 'a chord phrase: the low cut, like its stab'},
         about='phrase, 4 bars of metal chord hits on the off-beats, Em Em Am Bm, a dub echo and the dark hall'),
    Item('Loops', 'PhraseRaveRiff', 'loop', phrase_rave_riff, loop=True, level=PHRASE_LUFS, sets=R5,
         qa={'mud': 'lowcut', 'why': 'a stab phrase: the low cut, like its stab'},
         about='phrase, 2-bar rave stab riff: a 3-3-2 on Em, turning to Am and Bm at the end of bar 2'),
]
