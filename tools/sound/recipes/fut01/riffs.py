"""FUT01 round 4 synth phrases (set r4): four original riffs in F# minor at 134 BPM, each a loop on the kit's grid and
swing, played on the kit's own files, and the organ lead they need.

- OrganLead: a drawbar organ for single-note riffs, C4 to C6 every 3 semitones, two layers: drawbar harmonics 1-4, 6
  and 8, a percussion partial and a key click, a short release; additive, so every partial is a harmonic.
- OrganRiff (4 bars, on OrganLead): a syncopated organ riff on the 16ths, F#-A-C#-E around F#4 with a call in bars
  1-2 and its answer in bars 3-4, stepping up to B4 and G#4 at the ends.
- WarpRiff (2 bars, on WarpClean, bass A): a busier warp bass riff on the 16ths: octave pops, two slides and three
  pitch dips, the lane filter wobbling once an 8th (context4's), no kick; walled at 450 Hz after the dips.
- PluckRiff (4 bars, the chrome pluck at exact pitches, as the round 1 phrase plays it): a plucky garage lead riff,
  a 3-3-2 call on C#5-A4-F#4 and its answer climbing to E5 and F#5.
- StabRhythm (2 bars, on EPianoStab): the e-piano stab rhythm, F#m9 then Bm9, pushed onto the swung 16ths.

Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide), as midi.py's phrases take them.
"""
import numpy as np

from tools.sound import dsp48 as D
from tools.sound import parts as P
from tools.sound import phrase as Ph
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K
from tools.sound.recipes.fut01 import basslines as B, context4 as C, keys as Ky, music

SR = D.SR

# ------------------------------------------------------------------ the organ lead
LEAD_NOTES = (60, 63, 66, 69, 72, 75, 78, 81, 84)     # C4-C6 every 3 semitones, as the chrome pluck
LEAD_LAYERS = (0.6, 1.0)


def organ_lead(take):
    """One organ note: drawbar harmonics (the hard layer brighter), the percussion partial (3rd harmonic, decaying in
    60 ms) and a click; held 0.9 s, then released exponentially (45 ms). A sampler loops nothing: riffs are short."""
    v = take.vel
    n = int(1.5 * SR)                # 0.4 s of silence after the release, for the renderer's high-pass to settle
    t = P.times(n)
    f = take.f0
    up = (v - LEAD_LAYERS[0]) / (LEAD_LAYERS[1] - LEAD_LAYERS[0])
    ph = 2 * np.pi * f * t
    y = sum(a * (0.7 + 0.3 * up) ** (h - 1) * np.sin(h * ph) for h, a in Ky.DRAWBARS if h * f < 16000)
    y = y + 0.45 * v * np.sin(3 * ph) * np.exp(-t / 0.06)
    click = P.hp_noise(take.rng('click'), n, 3000.0, order=2) * D.env_exp(n, 0.002, 0.0002)
    y = (y + 0.06 * v * click) * P.raised(n, 0.003) * P.gate(t, 0.9, 0.045) * D.taper(n, 1.35, 0.1)
    rot = 0.12 * np.sin(2 * np.pi * 5.5 * t)
    out = np.stack([y * (1 + rot), y * (1 - rot)], 1)
    return Out(D.mono_below(D.bfilt(out, 150.0, 'highpass', order=4), 150.0) * (0.25 + 0.2 * v), f0s=(f,))


# ------------------------------------------------------------------ the riffs
F4, GS4, A4, B4, CS5, E5, FS5, E4 = 66, 68, 69, 71, 73, 76, 78, 64
ORGAN_RIFF = (   # 4 bars: the call (bars 1-2) and its answer (bars 3-4)
    (0, F4, 1, 1.0, False), (3, A4, 1, 0.7, False), (6, CS5, 1, 0.9, False), (8, F4, 1, 0.6, False),
    (10, E5, 2, 1.0, False), (13, CS5, 1, 0.7, False), (16, B4, 1, 0.9, False), (19, A4, 1, 0.7, False),
    (22, F4, 1, 0.9, False), (24, A4, 1, 0.6, False), (26, B4, 3, 1.0, False),
    (32, F4, 1, 1.0, False), (35, A4, 1, 0.7, False), (38, CS5, 1, 0.9, False), (40, F4, 1, 0.6, False),
    (42, FS5, 2, 1.0, False), (45, E5, 1, 0.7, False), (48, CS5, 1, 0.9, False), (51, B4, 1, 0.7, False),
    (54, A4, 1, 0.9, False), (56, GS4, 2, 0.8, False), (59, A4, 1, 0.6, False), (61, CS5, 2, 0.9, False),
)
PLUCK_RIFF = (   # 4 bars on the chrome pluck: (16th, note, length, velocity, slide)
    (0, CS5, 2, 1.0, False), (3, A4, 2, 0.75, False), (6, F4, 2, 0.85, False), (8, A4, 1, 0.6, False),
    (10, CS5, 2, 0.9, False), (12, B4, 1, 0.6, False), (14, A4, 2, 0.75, False),
    (16, CS5, 2, 1.0, False), (19, A4, 2, 0.75, False), (22, F4, 2, 0.85, False), (24, E4, 1, 0.6, False),
    (26, F4, 2, 0.9, False), (29, GS4, 2, 0.7, False),
    (32, CS5, 2, 1.0, False), (35, A4, 2, 0.75, False), (38, F4, 2, 0.85, False), (40, A4, 1, 0.6, False),
    (42, E5, 2, 0.95, False), (44, CS5, 1, 0.6, False), (46, B4, 2, 0.75, False),
    (48, CS5, 2, 1.0, False), (51, E5, 2, 0.8, False), (54, FS5, 3, 1.0, False), (58, E5, 1, 0.6, False),
    (59, CS5, 2, 0.75, False), (62, A4, 2, 0.6, False),
)
F1, A1, B1, CS2, D2, E2, F2 = 30, 33, 35, 37, 38, 40, 42
WARP_RIFF = (    # 2 bars of 16ths on the warp bass: (16th, length, note, velocity, slide) as context4's lines
    (0, 1.0, F1, 1.0, False), (2, 0.75, F2, 0.6, False), (3, 1.0, F1, 0.8, False), (5, 0.5, F2, 0.7, False),
    (6, 0.5, CS2, 0.7, 'dip'), (7, 1.0, F2, 0.7, 'dip'), (9, 1.0, E2, 0.8, False), (11, 1.0, F1, 0.9, False),
    (13, 0.75, A1, 0.7, False), (14, 1.75, CS2, 0.8, 'slide'),
    (16, 1.0, B1, 1.0, False), (18, 0.75, F2, 0.6, False), (19, 1.0, B1, 0.8, False), (21, 0.5, F2, 0.7, False),
    (22, 0.5, D2, 0.7, 'dip'), (23, 1.0, F2, 0.7, 'dip'), (25, 1.0, E2, 0.8, False), (27, 1.0, CS2, 0.9, False),
    (29, 0.5, E2, 0.7, False), (30, 0.5, CS2, 0.7, 'dip'), (31, 0.75, E2, 0.7, 'dip'),
)
STAB_RHYTHM = (  # 2 bars on the e-piano stab: (16th, zone, velocity)
    (0, 54, 0.9), (3, 54, 0.7), (6, 54, 1.0), (10, 54, 0.8), (13, 54, 0.6),
    (16, 59, 0.9), (19, 59, 0.7), (22, 59, 1.0), (25, 59, 0.6), (27, 59, 0.8), (30, 59, 0.6),
)
STAB_LEN = 1.5                      # the stabs' gate on the grid, in 16ths (the file holds 0.3 s; the riff cuts it)


def lead_lane(name, notes, steps):
    """A riff on a zoned lead: each note plays the zone at or below it, repitched up (4-point Hermite), its layer the
    nearer one, gated to its length plus a 40 ms release."""
    item, got, _ = Ph.kit_files(K, name)
    g = Ph.Grid(K, steps)
    buf = np.zeros((g.length, 2))
    for step, note, length, vel, _ in notes:
        zone = max(z for z in item.notes if z <= note)
        i = min(range(len(item.layers)), key=lambda j: (abs(item.layers[j] - vel), -j))
        src = got[(zone, i + 1, 1)].audio
        ratio = 2.0 ** ((note - zone) / 12.0)
        m = int(len(src) / ratio) - 4
        y = B.hermite(src, np.arange(m) * ratio, (len(src) - 4, len(src) - 1))
        n_on = int(length * g.s16)
        y = y[:n_on + int(0.04 * SR)] * D.taper(min(m, n_on + int(0.04 * SR)), n_on / SR, 0.04)[:, None]
        Ph.place(buf, y * (vel / item.layers[i]) ** 1.5, g.at(step))
    return buf


def organ_riff(take):
    return Out(lead_lane('OrganLead', ORGAN_RIFF, 64), meta={'bars': 4, 'bpm': K.BPM, 'swing': K.SWING})


def pluck_riff(take):
    """The pluck riff at its exact pitches on the chrome pluck's own voice (music.pluck_note, as the round 1 phrase),
    each note damped 120 ms after its length; tails wrap."""
    g = Ph.Grid(K, 64)
    y = np.zeros((g.length, 2))
    n = int(1.6 * SR)
    for i, (step, note, length, vel, _) in enumerate(PLUCK_RIFF):
        x = music.pluck_note(float(D.midi_hz(note)), vel, n, take.rng('note', i)) * D.taper(n, length * K.STEP,
                                                                                            0.12)[:, None]
        Ph.place(y, x, g.at(step))
    return Out(y, meta={'bars': 4, 'bpm': K.BPM, 'swing': K.SWING})


def warp_riff(take):
    """The warp riff on bass A's files: the sampler voice with its slides and dips (context4.bass_voice), the wall
    again at 450 Hz, the lane filter's 8th-note wobble, mono under 450 Hz."""
    g = Ph.Grid(K, 32)
    files = C.bass_files('clean')
    buf = np.zeros((g.length, 2))
    for on, run, n_on in C.line_voices(WARP_RIFF, g, 1):
        Ph.place(buf, C.bass_voice(files, run, n_on), on)
    x = C.wobble(C.wall(buf, 'clean'), g)
    return Out(C.circ(lambda u: D.mono_below(u, C.MONO_BELOW), x), meta={'bars': 2, 'bpm': K.BPM})


def stab_rhythm(take):
    """The e-piano stab rhythm: each stab its zone's file, cut to STAB_LEN 16ths with a 30 ms release."""
    item, got, _ = Ph.kit_files(K, 'EPianoStab')
    g = Ph.Grid(K, 32)
    buf = np.zeros((g.length, 2))
    n_on = int(STAB_LEN * g.s16)
    for step, zone, v in STAB_RHYTHM:
        i = min(range(len(item.layers)), key=lambda j: (abs(item.layers[j] - v), -j))
        x = got[(zone, i + 1, 1)].audio[:n_on + int(0.03 * SR)]
        x = x * D.taper(len(x), n_on / SR, 0.03)[:, None] * (v / item.layers[i]) ** 1.5
        Ph.place(buf, x, g.at(step))
    return Out(buf, meta={'bars': 2, 'bpm': K.BPM, 'swing': K.SWING})


def stab_notes():
    """The stab rhythm as MIDI notes for the e-piano stab sample: one note per stab on its chord's file (F#3 plays
    F#m9, B3 Bm9), as the riff plays it. The sample holds the whole chord, so a note per chord tone would stack it."""
    return [(s, z, STAB_LEN, v, False) for s, z, v in STAB_RHYTHM]


R4 = ('r4', 'r5')                  # round 5 carries every one on, unchanged
LEAD = {'mud': 'lowcut', 'why': 'a lead or stab loop: the low cut, like its sound'}
PHRASE_LUFS = -16.0                 # every phrase loop at one loudness
ITEMS = [
    Item('Lead', 'OrganLead', 'lead', organ_lead, layers=LEAD_LAYERS, notes=LEAD_NOTES, sets=R4,
         qa={'sustain': (0.10, 0.85), 'why': 'the alias check reads the held note: its exponential release starts at '
                                            '0.9 s'},
         about='organ lead for riffs: drawbar harmonics, a percussion partial and a click, C4 to C6 every 3 semitones'),
    Item('Loops', 'OrganRiff', 'loop', organ_riff, loop=True, level=PHRASE_LUFS, sets=R4, qa=LEAD,
         about='phrase, 4-bar organ riff in F# minor at 134 BPM on the organ lead: a syncopated call and its answer'),
    Item('Loops', 'WarpRiff', 'loop', warp_riff, loop=True, level=PHRASE_LUFS, sets=R4,
         qa={'mud': 'gate', 'why': 'bass A: the strict mud gate over the riff'},
         about='phrase, 2-bar warp bass riff in F# minor at 134 BPM on bass A: 16ths, octave pops, slides and dips, '
               'the lane filter wobbling once an 8th'),
    Item('Loops', 'PluckRiff', 'loop', pluck_riff, loop=True, level=PHRASE_LUFS, sets=R4, qa=LEAD,
         about='phrase, 4-bar chrome pluck riff in F# minor at 134 BPM: a 3-3-2 call and an answer climbing to F#5'),
    Item('Loops', 'StabRhythm', 'loop', stab_rhythm, loop=True, level=PHRASE_LUFS, sets=R4, qa=LEAD,
         about='phrase, 2-bar e-piano stab rhythm at 134 BPM: F#m9 then Bm9, pushed onto the swung 16ths'),
]
