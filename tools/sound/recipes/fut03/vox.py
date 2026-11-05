"""FUT03 "Glass Current" vocals for listening round 2: three candidate sung lines, the chops cut from the pick, and the
vocoder "drop" phrase on the 16th grid, dry and wet.

Every line is original and clean, written for this kit, 2 bars at 128 BPM in G minor, sung on the kit's swing
(K.SWING, as every lane), in the kit's bright female blend (K.VOICE). Each was title-checked on 2026-10-29 (the exact
phrase with "song" and with "lyrics"): no release with that title or line.
- SungLine1: "Ride the current with me" (Bb4 A4 G4 A4 Bb4, "me" on D5 from the and of 3, falling to C5 over the bar
  line): an invitation to move, the kit's title in it.
- SungLine2: "Clear as glass tonight" (D5 C5, "glass" held on Bb4 from beat 2, "tonight" A4 up to D5, falling to C5),
  sung linked.
- SungLine3: "Hold me in the current" (G4, Bb4 on the swung 16th, "in the" C5 Bb4, "current" D5 falling to C5, held),
  sung linked.
The chops are cut from the pick (PICK). Every line is sung whole once, dry, so Whisper checks it.

The drop phrase, "Let the glass drop" (title-checked the same way): one syllable per slice, on steps 0, 2 and 4 of a
bar and "drop" landing on beat 3, in the kit's one voice blend (K.VOICE, as every Kokoro take: ADR 0008; the first
version used a second, male-leaning blend the brief didn't ask for), on a flat buzzing G3; "drop" falls a fourth to D3
over its vowel (the drop itself). "Here it drops" was the first phrase:
its take ran "here it" together ("Hereton drops") and Whisper heard "here drop" in every variant, so it went. The
vocoder is the take resynthesised three times, on the root, the minor third and the fifth (G3 Bb3 D4: a G minor chord
the formants ride on), the root loudest so the words stay clear. The slice points are fixed by the grid
(SLICES): the sampler fires slice k on its step, so the phrase stays on the grid at any tempo.

Every file is a WORLD resynthesis at 48 kHz from the features of a 24 kHz Kokoro take (ADR 0005, D4: hf_synthetic).
Chops are cut in the frame domain (vocals.chop): each keeps its onset consonant whole and ends in a breath tail, so no
cut lands in the audio. Everything is high-passed at 100 Hz; the wet phrase is mono under 150 Hz.

Rendering needs requirements-vocals.txt and the models (WUBANSIA_MODELS_DIR); importing this module does not.
"""
import functools
from dataclasses import dataclass, replace

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut03 as K

SR = D.SR
HP = 100.0                   # nothing under ~100 Hz competes with the kick and sub
VOX = frozenset({'hf_synthetic'})
WHY = {'why': 'WORLD resynthesis at 48 kHz of a 24 kHz Kokoro take: the top band is synthetic (extend_hf, D4)'}


# ------------------------------------------------------------------ the three candidate lines
@dataclass(frozen=True)
class Line:
    """One candidate line: its words, its notes ((16th step, length, MIDI, word); '~' carries the word on to another
    note) and the Kokoro take it is sung from: the lexicon accent and the draw. Each line's take was picked from draws
    0-5 in both accents as the one Whisper hears word for word as rendered and best under band noise 12 dB down
    (SungLine1: US draw 4, heard whole on 4 of the 6 US draws, 0.74 under the noise; SungLine2: US draw 1, sung
    linked, since a glottal "as" was heard as "his" on every draw and the linked one as "clears" on 8 of 12; SungLine3:
    US draw 4, linked, the only take of 24 where "current" isn't heard as "car" or "end")."""
    name: str
    text: str
    notes: tuple
    accent: str
    draw: int
    about: str
    glottal: bool = True     # a glottal stop before a vowel-initial word after the first (False: linked, legato)


HOW = V.Sing(speed=0.8, consonants=1.3)
PRE = 0.5                    # seconds before step 0: the inhale and the first onset
LENGTH = 30 * K.STEP + 0.4
# G4 67, A4 69, Bb4 70, C5 72, D5 74
LINES = (
    Line('SungLine1', 'Ride the current with me',
         ((0, 3, 70, 'ride'), (3, 1, 69, 'the'), (4, 2, 67, 'current'), (6, 2, 69, '~'), (8, 2, 70, 'with'),
          (10, 8, 74, 'me'), (18, 6, 72, '~')), 'us', 4,
         'Bb4 A4 G4 A4 Bb4, "me" on D5 from the and of 3, falling to C5 over the bar line'),
    Line('SungLine2', 'Clear as glass tonight',
         ((0, 2, 74, 'clear'), (2, 2, 72, 'as'), (4, 4, 70, 'glass'), (8, 2, 69, 'tonight'), (10, 8, 74, '~'),
          (18, 6, 72, '~')), 'us', 1,
         'D5 C5, "glass" held on Bb4 from beat 2, "tonight" A4 up to D5, falling to C5 over the bar line',
         glottal=False),
    Line('SungLine3', 'Hold me in the current',
         ((0, 3, 67, 'hold'), (3, 1, 70, 'me'), (4, 2, 72, 'in'), (6, 2, 70, 'the'), (8, 2, 74, 'current'),
          (10, 12, 72, '~')), 'us', 4,
         'G4, "me" on the swung 16th on Bb4, "in the" C5 Bb4, "current" D5 falling to C5, held from the and of 3',
         glottal=False),
)
LINE_BY_NAME = {ln.name: ln for ln in LINES}
PICK = 'SungLine1'           # the line the chops are cut from


@functools.lru_cache(maxsize=None)
def line_track(name):
    """One line as one sung performance on the kit's swing: its frames before synthesis, the phrase ends not yet
    released (chops are cut from it)."""
    ln = LINE_BY_NAME[name]
    how = replace(HOW, glottal=ln.glottal)
    perf = V.performance(V.sing_text(ln.notes), K.VOICE, how.speed, how.glottal, ln.accent, ln.draw)
    return V.sing_track(perf, ln.notes, K.BPM, how, LENGTH, PRE, D.rng(K.KIT, name, 'sing'), swing=K.SWING)


@functools.lru_cache(maxsize=None)
def line_sung(name):
    """The whole line as sung (phrase ends released) with its inhale, before polish, and the levels polish works
    against on it."""
    tr = V.release_ends(replace(line_track(name), sp=line_track(name).sp.copy()))
    y = V.breaths(V.synth(tr), tr.events, D.rng(K.KIT, name, 'breath'))
    return y, V.polish_levels(y, hp=HP)


def sung_whole(take):
    """A candidate line, dry: the inhale, the line, polished."""
    y, levels = line_sung(take.item.name)
    return V.polish(y, hp=HP, levels=levels)


# ------------------------------------------------------------------ the chops of the pick
# name, first and last syllable (the pick's events: ride 0, the 1, cur 2, rent 3, with 4, me 5), the words, the chop's
# first note (its root), about
CHOPS = (
    ('ChopRide', 0, 0, 'ride', 70, '"ride", Bb4, on the one'),
    ('ChopCurrent', 2, 3, 'current', 67, '"current", G4 up to A4'),
    ('ChopTheCurrent', 1, 3, 'the current', 69, '"the current", A4 G4 A4'),
    ('ChopRideTheCurrent', 0, 3, 'ride the current', 70, '"ride the current", Bb4 A4 G4 A4'),
    ('ChopWithMe', 4, 5, 'with me', 70, '"with me", Bb4 up to D5, falling to C5'),
    ('ChopMe', 5, 5, 'me', 74, '"me", D5 falling to C5, held with vibrato into a breath tail'),
    ('ChopCurrentWithMe', 2, 5, 'current with me', 67, '"current with me", G4 A4 Bb4, then D5 falling to C5'),
)
CHOP_BY_NAME = {c[0]: c for c in CHOPS}


def chop_cut(name):
    _, first, last, _, _, _ = CHOP_BY_NAME[name]
    return V.chop(line_track(PICK), first, last)


def pick_chop(take):
    """One chop of the pick, cut in the frame domain (vocals.chop) and polished with the whole line's levels, so it
    sounds as it did inside the line; the junction where its breath tail starts is logged as a splice."""
    tr, junction = chop_cut(take.item.name)
    return Out(V.polish(V.synth(tr), hp=HP, levels=line_sung(PICK)[1]), splices=(int(np.floor(junction * SR + 0.5)),))


def chop_onset(name):
    """Seconds from the start of a chop's file to its note (where the beat falls, after the onset consonant): place
    the file this much before the 16th. Needs the vocal models."""
    return float(chop_cut(name)[0].events[0]['t_on'])


# ------------------------------------------------------------------ the vocoder drop phrase
ROBOT = 'Let the glass drop'
ROBOT_SYLLABLES = ('let', 'the', 'glass', 'drop')
ROBOT_GRID = ((0, 2), (2, 2), (4, 3), (8, 4))   # (16th step, length) of each syllable, one bar: "drop" on beat 3
ROBOT_TAKE = ('us', 11)      # the Kokoro take (accent, draw) on K.VOICE: of draws 0-11 (US; the British takes were
                             # heard as "gloss"), the only one Whisper hears word for word as rendered and under band
                             # noise 12 and 18 dB down, three noises each (every other draw lost "glass" at least once)
ROBOT_NOTE = K.ROOT + 12     # G3, 196 Hz: the flat pitch...
DROP_TO = K.ROOT + 7         # ...and "drop" falls to D3, a fourth down, over its vowel
CHORD = ((0, 1.0), (3, 0.55), (7, 0.6))   # the vocoder's voices: semitones over the take's pitch, level
ROBOT_LEAD = 0.06            # a slice starts this long before its step: room for the onset consonant
ROBOT_PRE = ROBOT_LEAD + 0.01
ROBOT_LEN = 12 * K.STEP + 0.3
SLICE_QUIET = 0.001          # each slice point is exactly silent from 1 ms before to 1 ms after it (raised-cosine
                             # edges 1 ms more): the grit's 4x linear-phase filter pre-rings the next onset by up to
                             # 15 ms at about -110 dBFS, so a slice would otherwise start on that residue, not silence
ECHO_STEPS = 3               # dotted 8th
ECHO = dict(taps=5, first=0.6, fb=0.5, lp=6000.0, hp=400.0)


def slice_sample(step):
    return int(np.floor((ROBOT_PRE + step * K.STEP - ROBOT_LEAD) * SR + 0.5))


# (nominal 16th step, onset sample in the file, syllable): the slice table for index.json, the same in dry and wet
SLICES = tuple((step, slice_sample(step), syl) for (step, _), syl in zip(ROBOT_GRID, ROBOT_SYLLABLES))
SLICE_NOTE = 'slices (16th step @ onset sample): ' + ', '.join(f'{s}@{n} {w}' for s, n, w in SLICES)
SLICE_META = {'slices': [{'step': step, 'onset': n, 'syllable': syl} for step, n, syl in SLICES],
              'bpm': K.BPM, 'chord': ['G3', 'Bb3', 'D4']}


def drop_track():
    """The robot track on the grid, its last syllable falling from G3 to D3 over its vowel (a raised-cosine glide)."""
    perf = V.performance(ROBOT.lower() + '.', K.VOICE, 1.0, False, *ROBOT_TAKE)
    tr = V.robot_track(perf, ROBOT_GRID, K.BPM, float(D.midi_hz(ROBOT_NOTE)), ROBOT_LEN, ROBOT_PRE, ROBOT_LEAD,
                       buzz=0.05)
    last = tr.events[-1]
    t = np.arange(len(tr.f0)) * tr.fps
    u = np.clip((t - last['t_on']) / max(last['t_end'] - last['t_on'], 1e-3), 0.0, 1.0)
    fall = D.midi_hz(ROBOT_NOTE + (DROP_TO - ROBOT_NOTE) * (0.5 - 0.5 * np.cos(np.pi * u))) / D.midi_hz(ROBOT_NOTE)
    f0 = np.where((tr.ev == len(tr.events) - 1) & (tr.f0 > 0), tr.f0 * fall, tr.f0)
    return replace(tr, f0=f0)


@functools.lru_cache(maxsize=1)
def drop_voice():
    """The vocoder: the drop track resynthesised on each chord tone (the same spectral envelope over three pitches),
    summed, high-passed, de-essed and compressed, then a little 4x oversampled grit, which also keeps the peaks near
    the loudness."""
    tr = drop_track()
    y = sum(g * V.synth(replace(tr, f0=tr.f0 * 2 ** (st / 12))) for st, g in CHORD)
    y = D.bfilt(y, HP, 'highpass', order=4)
    y = V.compress(V.deess(y), -18, 3.0, 0.004, 0.08)
    y = y / (np.max(np.abs(y)) + 1e-12)
    y = 0.5 * y + 0.5 * D.oversampled(lambda u: D.tanh_adaa(u, 2.5), y)
    return D.bfilt(y, HP, 'highpass', order=4) * slice_gate(len(y))


def slices():
    return tuple(n for _, n, _ in SLICES)


def slice_gate(n):
    """1, except exactly 0 within SLICE_QUIET of each slice point, with raised-cosine edges SLICE_QUIET long: the slice
    points' silence, made exact (the onset consonants start 5 ms after them)."""
    h = int(SLICE_QUIET * SR)
    t = np.arange(n)
    g = np.ones(n)
    for p in slices():
        d = np.abs(t - p)
        g = g * np.where(d <= h, 0.0, np.where(d < 2 * h, 0.5 - 0.5 * np.cos(np.pi * (d - h) / h), 1.0))
    return g


def drop_dry(take):
    return Out(drop_voice(), splices=slices(), meta=SLICE_META)


def echo(x):
    """A ping-pong echo on a dotted 8th, each repeat darker and thinner. Mono in, stereo out, the dry not included."""
    d = int(np.floor(ECHO_STEPS * K.STEP * SR + 0.5))
    taps = ECHO['taps']
    out = np.zeros((len(x) + d * taps, 2))
    tap = x
    for k in range(1, taps + 1):
        tap = D.bfilt(D.bfilt(tap, ECHO['lp'], 'lowpass'), ECHO['hp'], 'highpass') * (ECHO['first'] if k == 1
                                                                                      else ECHO['fb'])
        out[k * d:k * d + len(x), (k - 1) % 2] += tap
        out[k * d:k * d + len(x), k % 2] += 0.25 * tap
    return out


def drop_wet(take):
    """The dry vocoder in the kit's space (the FDN on the FUT03/space seed) with the ping-pong echo; the space and the
    echo are mono under 150 Hz."""
    y = drop_voice()
    n_tail = int(4.0 * SR)
    verb = D.fdn_reverb(D.bfilt(y, 300, 'highpass'), K.space_rng(), t60=2.2, size=0.85, damp_hz=7000.0,
                        n_tail=n_tail)
    dly = echo(y)
    out = np.zeros((len(y) + n_tail, 2))
    out[:len(y)] += D.st(y)
    out[:len(dly)] += 0.5 * dly
    out += 0.3 * verb[:len(out)]
    return Out(D.mono_below(D.bfilt(out, HP, 'highpass', order=4), 150.0), splices=slices(), meta=SLICE_META)


# ------------------------------------------------------------------ the items
ITEMS = [
    Item('Vox', ln.name, 'vox', sung_whole, root=ln.notes[0][2], level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'sung'), sets=('r2', 'r3'), script=ln.text,
         about=f'candidate line {ln.name[-1]}{" (the pick: the chops come from it)" if ln.name == PICK else ""}, '
               f'dry: "{ln.text}", G minor, on the kit swing: {ln.about}')
    for ln in LINES
] + [
    Item('Vox', name, 'vox', pick_chop, root=root, level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'chop', 'sung'),
         sets=('r2', 'r3'), script=words, about=f'sung chop {about}, cut from "{LINE_BY_NAME[PICK].text}"')
    for name, first, last, words, root, about in CHOPS
] + [
    Item('Vox', 'VocoderDrop', 'vox', drop_dry, root=ROBOT_NOTE, level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'robot'), sets=('r2', 'r3'), script=ROBOT,
         about=f'vocoder drop phrase "{ROBOT}" on the 16th grid at 128 BPM, dry: a G minor chord on the take, '
               f'"drop" falling G3 to D3; {SLICE_NOTE}'),
    Item('Vox', 'VocoderDropWet', 'vox', drop_wet, root=ROBOT_NOTE, level=-16.0, flags=VOX, qa=WHY,
         tags=('clean', 'wet', 'robot'), sets=('r2', 'r3'), script=ROBOT,
         about=f'vocoder drop phrase "{ROBOT}", wet: the kit space and a dotted-8th ping-pong; {SLICE_NOTE}'),
]
