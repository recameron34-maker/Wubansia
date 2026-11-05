"""FUT02 "Orbit Relay" round 5 synth accents and phrases (set r5): one-shots and 2- and 4-bar riffs a producer drops into
a sci-fi club track, every one in D minor at 130 BPM on a stated root, in the kit's voice (the chrome arp's stretched-
wire partials and its laser pitch drop, the starfield's stars, the vox synth).

**Accents** (one-shots):
- LaserChordStab (zones B-flat 3 and D4, a file a chord): the chrome arp's voice (lead.note_voice: the plucked pulse
  body, the stretched-wire chrome dropping a fourth onto each note) on a chord, its detuned unison on the side, with a
  short laser on top falling two octaves onto the root: D4 plays Dm9 (D4 | A4 C5 E5 F5), B-flat 3 plays B-flat maj9
  (Bb3 | F4 A4 C5 D5). The root sits alone an octave under the rest, so it reads on its pitch.
- StarfieldSweep (D, 1 bar): the pad's stars (its Dm9 two octaves up on the wire's partials 1, 3, 5 and 7) twinkling in
  through a band-pass that rises from 800 Hz to 12 kHz with a noise whoosh, 18 dB up to the downbeat it ends on.
- GlassPing (D6, two layers, two hits): a struck glass on the wire's partials, ringing, with a click, through a
  dotted-8th ping-pong that darkens over three repeats.
- ResonantZap (A5): a saw through a resonant low-pass that falls from 11 times the note onto it in about 60 ms: the
  laser as a filter zap, ending on a short A5.
- SubDrop (D2): a clean sine on D2 held 120 ms, then dropping two octaves over 1.4 s, a soft thump on its onset.
- ReverseChord (D4): the laser chord's Dm9, held, in the kit's hall, reversed: two beats of swell into the beat it
  ends on.
- NoiseUplifter (D): 2 bars of noise through a comb tuned to D4 and a band-pass rising from 400 Hz to 9 kHz, 18 dB up
  to the downbeat.
- ArpStab (D6): the chrome arp flicking up D5 F5 A5 D6 in 32nds, the D6 left ringing: an arp stab.
- VoxSynthStab (D4): the vox synth (voxsynth.sing) on Dm7 (D4 | A4 C5 F5), 'ah', short: a choir stab, no words.

**Phrases** (loops on the kit's grid and swing; their notes are MIDI phrases in midi.py):
- LaserStabRiff (2 bars, on LaserChordStab): Dm9 on the arp's 3-3-2 accents and two answers, then B-flat maj9 on the
  accents and Dm9 back.
- GlassMelody (4 bars, on GlassPing): a sparse glass melody between D6 and F5, call and answer, through its ping-pong.
- VoxChoirChords (4 bars, on VoxSynth): held choir chords, Dm7 ('oh'), Dm9 ('ah'), B-flat maj7 ('ee'), Am7 ('ah'),
  the vibrato blooming on each; the velocity picks the vowel, as on the multisample.

Everything but the sub drop is high-passed far above the kick and mono under 150 Hz.
"""
import functools

import numpy as np
from scipy import signal

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound import sends as S
from tools.sound.item import Item, Out
from tools.sound.kits import fut02 as K
from tools.sound.recipes.fut02 import lead as Ld, voxsynth as Vs

SR = D.SR
R5 = ('r5',)
LAYERS = (0.6, 1.0)
RATIOS = np.array(K.RATIOS)


def hz(note):
    return float(D.midi_hz(note))


def finish(y, hp=150.0, end=0.1):
    """High-passed (4th order), mono under 150 Hz, and faded out over its last `end` seconds (after its decay; 0 for a
    loop)."""
    y = D.mono_below(D.bfilt(D.st(y), hp, 'highpass', order=4), 150.0)
    return y * D.taper(len(y), len(y) / SR - end, end)[:, None] if end else y


# ------------------------------------------------------------------ the laser chord stab, its reverse and the arp stab
STAB_CHORDS = {58: (58, 65, 69, 72, 74), 62: (62, 69, 72, 76, 77)}   # Bb3: Bbmaj9; D4: Dm9 (the root alone below)
STAB_GATE, STAB_REL = 0.18, 0.12


def chrome_chord(notes, vel, n, gate, rel, at=None):
    """The chrome arp's voice on a chord (each tone lead.note_voice, its detuned unison on the side, lead.stereo),
    the root a little louder, each held `gate` s (one each, or one for all), then released on a Gaussian (39 dB down by
    `rel`: spectrally compact, so the harmonics stay clean for the alias check, and falling away faster than an
    exponential, so the file ends in silence). at: each tone's onset in seconds (an arpeggio), default all at 0.
    Returns (audio, f0s)."""
    y, d = np.zeros(n), np.zeros(n)
    f0s = []
    for i, m in enumerate(notes):
        f = hz(m)
        k = 0 if at is None else int(at[i] * SR)
        g = 1.0 if i == 0 else 0.8
        gate_i = gate[i] if isinstance(gate, tuple) else gate
        u = np.maximum(P.times(n - k) - gate_i, 0.0) / (rel / 3.0)
        env = g * np.exp(-0.5 * u * u)
        y[k:] += env * Ld.note_voice(f, vel, n - k, None, 0.0)
        d[k:] += env * Ld.detuned(f, vel, n - k, None, 0.0)
        f0s += [f, *Ld.chrome_partials(f)]
    return Ld.stereo(y / len(notes), d / len(notes)), f0s


def laser_line(f_end, n, vel, octaves=2.0, tau=0.008, decay=0.035):
    """A sine falling `octaves` onto f_end (time constant tau), gone in about 4 decay constants: the laser on top."""
    t = P.times(n)
    f = f_end * 2.0 ** (octaves * np.exp(-t / tau))
    return np.sin(2 * np.pi * D.phase(f, n)) * np.exp(-t / decay) * P.raised(n, 0.0005) * vel


def laser_stab(take):
    n = int(0.9 * SR)
    y, f0s = chrome_chord(STAB_CHORDS[take.note], take.vel, n, STAB_GATE, STAB_REL)
    y = y + 0.12 * D.st(laser_line(2 * take.f0, n, take.vel))
    return Out(finish(y) * (0.45 + 0.55 * take.vel), f0s=tuple(f0s))


REVERSE_BEATS = 2


def reverse_chord(take):
    """The laser chord's Dm9 held, with a second of the kit's hall, reversed: it swells into its end, the chord's own
    attack last, so it lands on the beat where the file ends (cut_end)."""
    n = int(REVERSE_BEATS * K.BEAT * SR)
    y, _ = chrome_chord(STAB_CHORDS[62], take.vel, n, 0.6, 0.3)
    hall = S.space(D.mono(y), K.space_rng('reverse'), t60=2.2, size=0.8, damp_hz=8000.0, predelay=0.012,
                   send_hp=300.0)[:n]
    y = (y + 0.7 * hall)[::-1] * P.raised(n, 0.3)[:, None]
    return finish(y) * (0.45 + 0.55 * take.vel)


ARP_STAB = (74, 77, 81, 86)          # D5 F5 A5 D6, a 32nd apart
ARP_GATES = (0.05, 0.05, 0.05, 0.45)


def arp_stab(take):
    n = int(1.2 * SR)
    at = tuple(i * K.STEP / 2 for i in range(len(ARP_STAB)))
    y, f0s = chrome_chord(ARP_STAB[::-1], take.vel, n, ARP_GATES[::-1], 0.12, at=at[::-1])
    return Out(finish(y, 200.0) * (0.45 + 0.55 * take.vel), f0s=tuple(f0s))


# ------------------------------------------------------------------ the glass ping
GLASS_DECAY = 0.6                     # the first partial's decay (s); each next one faster
GLASS_S = 1.5                         # a note's length, faded out over its last 0.4 s


def glass_note(f, vel, n, r):
    """A struck glass on the wire's partials: harder hits keep more of the upper ones; a click on the strike."""
    decays = [GLASS_DECAY / (1 + 0.9 * k) for k in range(len(RATIOS))]
    amps = [vel ** (0.5 * k) / (1 + 0.6 * k) for k in range(len(RATIOS))]
    x = D.modal_ir(f, RATIOS, decays, amps, n, r) * P.raised(n, 0.0005)
    click = P.hp_noise(r, n, 6000.0) * D.env_exp(n, 0.002)
    return (x + 0.04 * vel * click) * D.taper(n, n / SR - 0.4, 0.4)


def ping_pong(x, steps=3, taps=3, fb=0.45, lp=7000.0, length=None):
    """x and its ping-pong on `steps` 16ths, each repeat darker, alternating sides. length: wrap round a loop of that
    many samples instead of growing."""
    x = D.mono(x)
    d = int(round(steps * K.STEP * SR))
    n = len(x)
    out = np.zeros((length if length else n + taps * d, 2))
    Ph.place(out, x, 0)
    tap = x
    for k in range(1, taps + 1):
        tap = D.bfilt(tap, lp, 'lowpass') * fb
        y = np.zeros((n, 2))
        y[:, (k - 1) % 2] = tap
        y[:, k % 2] = 0.3 * tap
        Ph.place(out, y, k * d)
    return out


def glass_ping(take):
    n = int(GLASS_S * SR)
    x = glass_note(take.f0, take.vel, n, take.rng('modes'))
    return Out(finish(ping_pong(x), 400.0) * (0.4 + 0.6 * take.vel), f0s=tuple(take.f0 * RATIOS))


# ------------------------------------------------------------------ the resonant zap
def resonant_zap(take):
    """A saw on the note through a resonant low-pass (q 6) falling from 11 times the note onto it; the level falls
    in 0.12 s."""
    v = take.vel
    n = int(1.3 * SR)
    t = P.times(n)
    f = take.f0
    saw = D.down(D.polyblep_saw(f, n, take.rng('phase').uniform(), k=4), 4)
    fc = f * (1.0 + (6.0 + 4.0 * v) * np.exp(-t / 0.02))
    y = D.svf_tpt(saw, fc, q=6.0)[0] * np.exp(-t / 0.12) * P.raised(n, 0.001)
    return Out(finish(y, 300.0) * (0.3 + 0.7 * v), f0s=(f,))


# ------------------------------------------------------------------ the sub drop
SUB_DROP = dict(note=K.ROOT, hold=0.12, octaves=2.0, tau=0.45, length=1.8)


def sub_drop(take):
    """A clean sine on D2 held 120 ms, then dropping two octaves (most of it in the first half second), its level
    falling slowly: a pure sub, nothing in the low mids."""
    s = SUB_DROP
    n = int((s['length'] + 0.6) * SR)
    t = P.times(n)
    f = hz(s['note']) * 2.0 ** (-s['octaves'] * (1 - np.exp(-np.maximum(t - s['hold'], 0) / s['tau'])))
    y = np.sin(2 * np.pi * D.phase(f, n)) * P.raised(n, 0.012) * np.exp(-np.maximum(t - 0.3, 0) / 0.8)
    y = y * D.taper(n, s['length'] - 0.5, 0.45)
    thump = P.band_noise(take.rng('thump'), n, 1000.0, 4000.0) * D.env_exp(n, 0.003)
    return D.st(0.7 * y + 0.03 * thump)


# ------------------------------------------------------------------ the uplifter and the starfield sweep
UPLIFT_BARS = 2


def comb(x, f, g):
    """y[k] = x[k] + g y[k - SR/f] (linear-interpolated delay): resonances on f's harmonics."""
    d = SR / f
    i, fr = int(np.floor(d)), d - np.floor(d)
    a = np.zeros(i + 2)
    a[0], a[i], a[i + 1] = 1.0, -g * (1 - fr), -g * fr
    return signal.lfilter([1.0], a, x)


def noise_uplifter(take):
    """Two bars of noise through a comb tuned to D4 and a band-pass whose centre rises from 400 Hz to 9 kHz, its level
    rising 18 dB to the last sample, the downbeat after the two bars (cut_end)."""
    n = int(round(UPLIFT_BARS * 16 * K.STEP * SR))
    u = P.times(n) / (n / SR)
    y = comb(take.rng('noise').standard_normal(n), hz(K.ROOT + 24), 0.55)
    fc = 400.0 * (9000.0 / 400.0) ** (u ** 1.5)
    band = D.svf_tpt(y, fc, q=1.2)[1] * D.undb(-18.0 * (1 - u) ** 1.3) * P.raised(n, 0.5)
    side = D.bfilt(D.svf_tpt(take.rng('side').standard_normal(n), fc, q=1.2)[1], 1500.0, 'highpass') * u
    out = np.stack([band + 0.25 * side, band - 0.25 * side], 1)
    return D.mono_below(D.bfilt(out, 200.0, 'highpass', order=4), 150.0)


STARS = (74, 81, 84, 89, 93, 100)    # the pad's Dm9 two octaves up: D5 A5 C6 F6 A6 E7
STAR_PARTIALS = (0, 2, 4, 6)
SWEEP_STEPS = 16


def starfield_sweep(take):
    """One bar: the stars (each tone of STARS on the wire's partials 1, 3, 5 and 7, each twinkling at its own rate)
    and a noise whoosh, through a band-pass rising from 800 Hz to 12 kHz, 18 dB up to the downbeat (cut_end)."""
    n = int(round(SWEEP_STEPS * K.STEP * SR))
    t = P.times(n)
    u = t / t[-1]
    r = take.rng('stars')
    stars = np.zeros((n, 2))
    for i, note in enumerate(STARS):
        for j, k in enumerate(STAR_PARTIALS):
            f = hz(note) * RATIOS[k]
            if f > 18000:
                continue
            tw = (0.5 + 0.5 * np.sin(2 * np.pi * r.uniform(2.0, 7.0) * t + r.uniform(0, 2 * np.pi))) ** 2
            stars += Ph.pan(tw * np.sin(2 * np.pi * f * t + r.uniform(0, 2 * np.pi)) / (1 + 0.5 * j),
                            r.uniform(-0.8, 0.8))
    fc = 800.0 * (12000.0 / 800.0) ** (u ** 1.3)
    whoosh = D.svf_tpt(take.rng('whoosh').standard_normal(n), fc, q=1.5)[1]
    y = np.stack([D.svf_tpt(stars[:, c], fc, q=0.9)[1] for c in range(2)], 1) / len(STARS)
    y = (y + 0.35 * D.st(whoosh)) * (D.undb(-18.0 * (1 - u) ** 1.2) * P.raised(n, 0.2))[:, None]
    return D.mono_below(D.bfilt(y, 300.0, 'highpass', order=4), 150.0)


# ------------------------------------------------------------------ the vox synth stab
VOX_STAB = (62, 69, 72, 77)          # Dm7: D4 alone under A4 C5 F5


def vox_synth_stab(take):
    n = int(0.75 * SR)
    y = np.zeros((n, 2))
    for i, m in enumerate(VOX_STAB):
        nt = Vs.Note(Vs.PRE, Vs.PRE + 0.22, m, (('ah', Vs.ONSET['open_s']),), 1.0, 0.12)
        y += (1.0 if i == 0 else 0.7) * Vs.sung(nt, n, lambda *k: take.rng('tone', i, *k))
    return Out(y * (0.45 + 0.55 * take.vel), f0s=tuple(hz(m) for m in VOX_STAB))


# ------------------------------------------------------------------ the phrases
STAB_RIFF = ((0, 62, 1.0), (3, 62, 0.7), (6, 62, 0.9), (10, 62, 0.7), (12, 62, 0.6),
             (16, 58, 1.0), (19, 58, 0.7), (22, 58, 0.9), (26, 62, 0.8), (29, 62, 0.7))   # (16th, zone, velocity)
STAB_LEN = 1.5                       # the stabs' gate, in 16ths


def nearest_layer(item, v):
    return 1 + min(range(len(item.layers)), key=lambda j: (abs(item.layers[j] - v), -j))


def stab_riff(take):
    """The laser stab riff on LaserChordStab's files: each stab its chord's file, cut to STAB_LEN 16ths with a 30 ms
    release, its level by velocity."""
    item, got, _ = Ph.kit_files(K, 'LaserChordStab')
    g = Ph.Grid(K, 32)
    buf = np.zeros((g.length, 2))
    n_on = int(STAB_LEN * g.s16)
    for step, zone, v in STAB_RIFF:
        layer = nearest_layer(item, v)
        x = got[(zone, layer, 1)].audio[:n_on + int(0.03 * SR)]
        x = x * D.taper(len(x), n_on / SR, 0.03)[:, None] * (v / item.layers[layer - 1]) ** 1.5
        Ph.place(buf, x, g.at(step))
    return Out(buf, meta={'bars': 2, 'bpm': K.BPM, 'swing': K.SWING})


GLASS_MELODY = (   # (16th, MIDI, velocity), 4 bars: D6 A5 C6 F5 E6 F6 G5
    (0, 86, 0.9), (3, 81, 0.6), (6, 84, 0.8), (10, 77, 0.6), (14, 81, 0.7),
    (16, 86, 0.9), (19, 88, 0.7), (22, 89, 1.0), (26, 84, 0.6), (30, 81, 0.6),
    (32, 86, 0.9), (35, 81, 0.6), (38, 84, 0.8), (42, 77, 0.6), (46, 79, 0.7),
    (48, 81, 0.9), (51, 84, 0.7), (54, 86, 1.0), (58, 89, 0.6), (62, 88, 0.5),
)


def glass_melody(take):
    """The glass melody at exact pitches on the glass voice (glass_note), through its ping-pong, round the loop."""
    g = Ph.Grid(K, 64)
    dry = np.zeros(g.length)
    n = int(GLASS_S * SR)
    for i, (step, m, v) in enumerate(GLASS_MELODY):
        x = glass_note(hz(m), v, n, take.rng('note', i)) * (0.4 + 0.6 * v)
        idx = (g.at(step) + np.arange(n)) % g.length
        np.add.at(dry, idx, x)
    return Out(finish(ping_pong(dry, length=g.length), 400.0, end=0), meta={'bars': 4, 'bpm': K.BPM, 'swing': K.SWING})


AH, EE, OH = Vs.AH, Vs.EE, Vs.OH
CHOIR_BARS = (((62, 69, 72, 77), OH),    # Dm7, 'oh'
              ((69, 72, 76, 77), AH),    # Dm9 (A4 C5 E5 F5), 'ah'
              ((62, 65, 69, 70), EE),    # Bbmaj7 over D (D4 F4 A4 Bb4), 'ee'
              ((69, 72, 76, 79), AH))    # Am7 (A4 C5 E5 G5), 'ah'
CHOIR_LEN = 15                       # 16ths a chord is held


def choir_notes():
    """The choir chords as MIDI notes: (16th, MIDI, length, velocity, slide)."""
    return [(16 * b, m, CHOIR_LEN, v, False) for b, (chord, v) in enumerate(CHOIR_BARS) for m in chord]


def choir_chords(take):
    """Each chord tone sung on the vox synth (voxsynth.sung, its vowel by velocity), held CHOIR_LEN 16ths, released
    over 0.12 s, its tail wrapping round the loop."""
    g = Ph.Grid(K, 64)
    buf = np.zeros((g.length, 2))
    pre = Vs.PRE
    for b, (chord, v) in enumerate(CHOIR_BARS):
        hold = CHOIR_LEN * g.s16 / SR
        n = int((pre + hold + 0.5) * SR)
        vowel = Vs.vowel_of(v)
        level = D.undb(Vs.LAYER_DB[Vs.LAYERS.index(v)])
        for i, m in enumerate(chord):
            nt = Vs.Note(pre, pre + hold, m, ((vowel, Vs.ONSET['open_s']),), 1.0, 0.12)
            y = Vs.sung(nt, n, lambda *k: take.rng('chord', b, i, *k)) * level * 0.6
            Ph.place(buf, y, (g.at(16 * b) - int(pre * SR)) % g.length)
    return Out(buf, meta={'bars': 4, 'bpm': K.BPM, 'chords': ['Dm7', 'Dm9', 'Bbmaj7/D', 'Am7'],
                          'vowels': [Vs.vowel_of(v) for _, v in CHOIR_BARS]})


ONE = dict(layers=(1.0,), rr=1, sets=R5)
MOVING = {'tonal': False, 'why': 'a pitch gesture or a ping-pong: no steady tone for the alias check'}
PHRASE_QA = {'mud': 'lowcut', 'why': 'a synth phrase loop: the low cut, like its sounds'}
PHRASE_LUFS = -16.0
ITEMS = [
    Item('Stabs', 'LaserChordStab', 'stab', laser_stab, layers=LAYERS, notes=tuple(STAB_CHORDS), sets=R5,
         about='accent, laser chord stab: the chrome arp voice on a chord with a laser falling onto it, a file a '
               'chord: D4 plays Dm9 (D4 A4 C5 E5 F5), B-flat 3 plays B-flat maj9 (Bb3 F4 A4 C5 D5)'),
    Item('FX', 'StarfieldSweep', 'fx', starfield_sweep, root=K.ROOT + 36, level=-14.0, sets=R5, flags={'cut_end'},
         tags=('riser',),
         about="accent, starfield sweep on D: 1 bar of the pad's stars (Dm9 two octaves up) and a whoosh through a "
               'band-pass rising from 800 Hz to 12 kHz, 18 dB up to the downbeat'),
    Item('Perc', 'GlassPing', 'perc', glass_ping, layers=LAYERS, rr=2, root=K.ROOT + 48, sets=R5,
         qa={'tune_window': (0.03, 0.30), 'why': 'the glass before its first echo'},
         about="accent, glass ping on D6: a struck glass on the kit's wire partials, a click, a dotted-8th ping-pong"),
    Item('Perc', 'ResonantZap', 'perc', resonant_zap, layers=LAYERS, root=K.ROOT + 43, sets=R5,
         qa={'tune_window': (0.08, 0.20), 'why': 'the zap once its filter has fallen onto the note'},
         about='accent, resonant zap on A5: a saw through a resonant low-pass falling from 11 times the note onto it'),
    Item('Bass', 'SubDrop', 'bass', sub_drop, root=K.ROOT, sets=R5,
         qa={'tonal': False, 'tune_window': (0.02, 0.11), 'loop_check': False,
             'why': 'a pitch drop: on D2 for its first 120 ms (the tuning window), then gliding, so no steady tone for '
                    'the alias check; an accent for a section change, not a groove bass shot'},
         about='accent, sub drop from D2: a clean sine held 120 ms, then dropping two octaves over 1.4 s'),
    Item('FX', 'ReverseChord', 'fx', reverse_chord, layers=LAYERS, root=K.ROOT + 24, sets=R5, flags={'cut_end'},
         about="accent, reverse chord on D4: the laser chord's Dm9 and the kit's hall reversed, two beats of swell "
               'into the beat it ends on'),
    Item('FX', 'NoiseUplifter', 'fx', noise_uplifter, root=K.ROOT + 24, level=-14.0, sets=R5, flags={'cut_end'},
         tags=('riser',),
         about='accent, noise uplifter on D: 2 bars of noise through a comb tuned to D4 and a band-pass rising from '
               '400 Hz to 9 kHz, 18 dB up to the downbeat'),
    Item('Stabs', 'ArpStab', 'stab', arp_stab, layers=LAYERS, root=K.ROOT + 48, sets=R5,
         qa={'sustain': (0.35, 0.60), 'tune_window': (0.35, 0.55),
             'why': 'an arpeggio: the alias check and the tuning read the D6 left ringing once the flick is over'},
         about='accent, arp stab on D6: the chrome arp flicking up D5 F5 A5 D6 in 32nds, the D6 ringing'),
    Item('Stabs', 'VoxSynthStab', 'stab', vox_synth_stab, layers=LAYERS, root=K.ROOT + 24, sets=R5,
         qa={**Vs.CHOP_QA},
         about="accent, vox synth stab on D4: the vox synth on Dm7 (D4 A4 C5 F5), 'ah', short; no words"),
    Item('Loops', 'LaserStabRiff', 'loop', stab_riff, loop=True, level=PHRASE_LUFS, sets=R5, qa=PHRASE_QA,
         about='phrase, 2-bar laser stab riff in D minor at 130 BPM: Dm9 on the 3-3-2 accents, then B-flat maj9'),
    Item('Loops', 'GlassMelody', 'loop', glass_melody, loop=True, level=PHRASE_LUFS, sets=R5, qa=PHRASE_QA,
         about='phrase, 4-bar glass melody in D minor at 130 BPM on the glass ping, through its ping-pong'),
    Item('Loops', 'VoxChoirChords', 'loop', choir_chords, loop=True, level=PHRASE_LUFS, sets=R5, qa=PHRASE_QA,
         about="phrase, 4-bar vox synth choir chords at 130 BPM: Dm7 'oh', Dm9 'ah', B-flat maj7 'ee', Am7 'ah'"),
]
