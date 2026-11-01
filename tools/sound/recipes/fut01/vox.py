"""FUT01 vocals: round 2's three sung candidate lines and the six chops cut from the pick, the round 1 sung phrase
and its chops (set s1a only), and a robot phrase on the 16th grid (both rounds).

Every line is original and clean, written for this kit, 2 bars at 134 BPM in F# minor:
- round 2, three candidates, each title-checked (web search, the exact phrase with "song" and with "lyrics",
  2026-10-29: no release with that title or line), each sung whole once, dry, so Whisper checks it, and all on the
  kit's swing (K.SWING, as every other lane):
  - SungLine1, the pick: "Come find me on the floor" (E4 F#4 A4 G#4 F#4, then "floor" on B4 falling to A4). A plain
    call to the dance floor; "floor" lands on the and of 3 with the 2-step kick and holds over the bar line. The
    glottal stop the chain puts before "on" is sung 15 dB softer (GLOTTAL_DB): as Kokoro spoke it, it came out as a
    bright burst 7 dB over the vowel it starts, the loudest moment of the line
  - SungLine2: "Every signal leads to you" (G#4 A4 C#5 B4 A4 G#4, "you" held on F#4)
  - SungLine3: "Your signal keeps me close" (E4 F#4 A4 C#5 B4, "close" held on A4)
  The six chops (PICK_CHOPS) are cut from SungLine1, and their words are title-checked too (CHOP_CHECKS): a chop is a
  hook, and four pieces of the line are titles of released songs, so no chop sings one or is named after one.
  Whisper hears each chop file alone as its words except "find" (heard "fine": its d is sung through into "me");
  round 1's chops alone read "I'm a cop", "3", "Clow".
- round 1, set s1a only (history): "Copper light, moving slow" (A4 G#4 | F#4 | E4 F#4 | C#4 held), sung straight
  (swing 0.5); round 1 called it a "weird phrase". Round 1's context loop, which played these chops, has left the
  kit (the round 2 loop plays PICK_CHOPS), so these chops and the phrase render in set s1a only, as history;
  chop_onset() gives any chop's note offset, old or new
- robot: "Boost the signal", one syllable per slice on steps 0, 3, 4 and 6 of a bar, monotone F#3

Every file is a WORLD resynthesis at 48 kHz from the features of a 24 kHz Kokoro take in the kit's voice blend (ADR
0005, D4: hf_synthetic). The chops are cut in the frame domain (vocals.chop): each keeps its onset consonant whole and
ends in a breath tail, so no cut lands in the audio. The full sung phrase is here too, dry, so Whisper can check the
line the chops come from. The robot's slice points are fixed by the grid (ROBOT_SLICES): the sampler fires slice k at
its step, REX-style, so the phrase stays on the grid at 128 or 140. Everything is high-passed at 100 Hz; the wet robot
is mono under 150 Hz.

Rendering needs requirements-vocals.txt and the models (WUBANSIA_MODELS_DIR); importing this module does not.
"""
import functools
from dataclasses import dataclass, replace

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut01 as K

SR = D.SR
HP = 100.0                   # nothing under ~100 Hz competes with the kick and sub
VOX = frozenset({'hf_synthetic'})
WHY = {'why': 'WORLD resynthesis at 48 kHz of a 24 kHz Kokoro take: the top band is synthetic (extend_hf, D4)'}

# ------------------------------------------------------------------ the sung phrase and its chops
SUNG = 'Copper light, moving slow'
SUNG_NOTES = ((0, 2, 69, 'copper'), (2, 2, 68, '~'), (4, 6, 66, 'light'), (12, 3, 64, 'moving'), (15, 3, 66, '~'),
              (18, 10, 61, 'slow'))            # (16th step, length, MIDI, word); '~' carries the word on
SUNG_HOW = V.Sing(speed=0.8, consonants=1.3)
SUNG_ACCENT = 'us'           # US phonemes in the kit's voice: Whisper hears the British /kɒpə/ as "copa", 6 of 6 times
SUNG_DRAW = 2                # the take: of draws 0-5 the only one Whisper hears word for word padded, 10 dB down and
                             # unpolished alike (the others: "cover light", or "copperlight" as one word)
SUNG_PRE = 0.5               # seconds before step 0: the inhale and the first onset
SUNG_LEN = 30 * K.STEP + 0.4
# name, first and last syllable (one per note, so syllable i sings SUNG_NOTES[i]), the word it is cut from, about
CHOPS = (
    ('ChopCo', 0, 0, 'copper', '"co" of copper, A4'),
    ('ChopPer', 1, 1, 'copper', '"pper" of copper, G#4'),
    ('ChopCopper', 0, 1, 'copper', '"copper", A4 to G#4'),
    ('ChopLight', 2, 2, 'light', '"light", F#4, its t released into breath'),
    ('ChopMo', 3, 3, 'moving', '"mo" of moving, E4'),
    ('ChopVing', 4, 4, 'moving', '"ving" of moving, F#4'),
    ('ChopMoving', 3, 4, 'moving', '"moving", E4 to F#4'),
    ('ChopSlow', 5, 5, 'slow', '"slow", C#4 held with vibrato, a fall and a breath tail'),
)
CHOP_BY_NAME = {c[0]: c for c in CHOPS}


@functools.lru_cache(maxsize=1)
def sung_track():
    """The one sung performance every chop is cut from: its frames before synthesis, the phrase ends not yet
    released."""
    perf = V.performance(V.sing_text(SUNG_NOTES), K.VOICE, SUNG_HOW.speed, SUNG_HOW.glottal, SUNG_ACCENT, SUNG_DRAW)
    # straight (0.5), as rendered for S1a; swinging the line on K.SWING is a sound change for the fix round's own commit
    return V.sing_track(perf, SUNG_NOTES, K.BPM, SUNG_HOW, SUNG_LEN, SUNG_PRE, D.rng(K.KIT, 'SungPhrase', 'sing'),
                        swing=0.5)


@functools.lru_cache(maxsize=1)
def sung_line():
    """The whole line as sung (phrase ends released) with its inhale, before polish, and the levels polish works
    against on it."""
    tr = V.release_ends(replace(sung_track(), sp=sung_track().sp.copy()))
    y = V.breaths(V.synth(tr), tr.events, D.rng(K.KIT, 'SungPhrase', 'breath'))
    return y, V.polish_levels(y, hp=HP)


def sung_phrase(take):
    """The whole line, dry: the inhale, the phrase, polished."""
    y, levels = sung_line()
    return V.polish(y, hp=HP, levels=levels)


def sung_chop(take):
    """One chop: its syllables cut from the sung frames, a breath tail, polished exactly as inside the phrase (the
    phrase's levels, not the chop's own peak). The junction where the tail starts is logged as a splice."""
    _, first, last, _, _ = CHOP_BY_NAME[take.item.name]
    tr, junction = V.chop(sung_track(), first, last)
    return Out(V.polish(V.synth(tr), hp=HP, levels=sung_line()[1]), splices=(int(np.floor(junction * SR + 0.5)),))


# ------------------------------------------------------------------ round 2: three candidate lines, chops from the pick
@dataclass(frozen=True)
class Line:
    """One candidate line: its words, its notes ((16th step, length, MIDI, word); '~' carries the word on) and the
    Kokoro take it is sung from: the lexicon accent and the draw. Of draws 0-5 in either accent, each line's take is
    one that Whisper hears word for word as rendered and again under band noise 12 and 6 dB down (SungLine1: GB, its
    draws 0-5 all clean, draw 3 the one that survives the noise; US heard "Tony floor". With its glottal stop softened
    (GLOTTAL_DB) Whisper still hears draw 3 word for word. SungLine2: US, draws 0-5 clean where GB lost the z of
    "leads" on half of them; draw 4 holds its notes best. SungLine3: any; GB draw 1 holds its notes best)."""
    name: str
    text: str
    notes: tuple
    accent: str
    draw: int
    about: str
    glottal: bool = True     # a glottal stop before a vowel-initial word after the first (False: linked, legato)


LINE_HOW = V.Sing(speed=0.8, consonants=1.3)   # as round 1's line
LINE_PRE = 0.5               # seconds before step 0: the inhale and the first onset
LINE_LEN = 30 * K.STEP + 0.4
LINE_SWING = K.SWING         # the kit's one swing: the line's odd 16ths land with the hats' (round 1 sang straight)
LINES = (
    Line('SungLine1', 'Come find me on the floor',
         ((0, 2, 64, 'come'), (2, 2, 66, 'find'), (4, 3, 69, 'me'), (7, 1, 68, 'on'), (8, 2, 66, 'the'),
          (10, 8, 71, 'floor'), (18, 6, 69, '~')), 'gb', 3,
         'E4 F#4 A4 G#4 F#4, "floor" on the and of 3 (with the 2-step kick), B4 falling to A4 over the bar line'),
    Line('SungLine2', 'Every signal leads to you',
         ((0, 2, 68, 'every'), (2, 1, 69, '~'), (3, 3, 73, 'signal'), (6, 2, 71, '~'), (8, 3, 69, 'leads'),
          (11, 3, 68, 'to'), (14, 10, 66, 'you')), 'us', 4,
         'G#4 A4, "signal" pushed onto the swung 16th on C#5 B4, A4 G#4, "you" held on F#4 from beat 4'),
    Line('SungLine3', 'Your signal keeps me close',
         ((0, 2, 64, 'your'), (2, 2, 66, 'signal'), (4, 2, 69, '~'), (7, 3, 73, 'keeps'), (10, 2, 71, 'me'),
          (12, 12, 69, 'close')), 'gb', 1,
         'E4 F#4 A4, "keeps" on the swung 16th on C#5, B4, "close" held on A4 from beat 4'),
)
LINE_BY_NAME = {ln.name: ln for ln in LINES}
PICK = 'SungLine1'           # the line the chops are cut from
GLOTTAL_DB = -15.0           # a glottal stop's frames against what Kokoro gives them. Spoken, SungLine1's stop before
                             # "on" is a bright noise burst: 7 dB over the vowel it starts and 18 dB over it from 6 kHz
                             # up, the loudest moment of the line, 26 ms ahead of the beat. 15 dB down it is a soft
                             # stop 3 dB under the vowel that still parts "me" from "on" (Whisper hears the line word
                             # for word)


def soften_stops(track):
    """The track with every glottal stop (the onset frames of a syllable that starts with one) GLOTTAL_DB down."""
    sp = track.sp.copy()
    for i, e in enumerate(track.events):
        if e['syms'].startswith('ʔ'):
            sp[(track.ev == i) & (track.kind == 1)] *= D.undb(GLOTTAL_DB) ** 2
    return replace(track, sp=sp)


@functools.lru_cache(maxsize=None)
def line_track(name):
    """One candidate line as one sung performance on the kit's swing: its frames before synthesis, glottal stops
    softened, the phrase ends not yet released (chops are cut from it)."""
    ln = LINE_BY_NAME[name]
    how = replace(LINE_HOW, glottal=ln.glottal)
    perf = V.performance(V.sing_text(ln.notes), K.VOICE, how.speed, how.glottal, ln.accent, ln.draw)
    return soften_stops(V.sing_track(perf, ln.notes, K.BPM, how, LINE_LEN, LINE_PRE, D.rng(K.KIT, name, 'sing'),
                                     swing=LINE_SWING))


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


# name, first and last syllable (event index: come 0, find 1, me 2, on 3, the 4, floor 5), the words, the chop's first
# note (its root), about. A chop of two words or more is a hook, so its words are title-checked like a line's
# (CHOP_CHECKS); single words are not (nearly every common word is the title of some release).
PICK_CHOPS = (
    ('ChopCome', 0, 0, 'come', 64, '"come", E4, on the beat'),
    ('ChopFind', 1, 1, 'find', 66, '"find", F#4'),
    ('ChopMe', 2, 2, 'me', 69, '"me", A4, held a dotted 8th'),
    ('ChopFloor', 5, 5, 'floor', 71, '"floor", its f whole, B4 falling to A4, held with vibrato into a breath tail'),
    ('ChopFindToFloor', 1, 5, 'find me on the floor', 66,
     '"find" to "floor": F#4 A4 G#4 F#4, then "floor" on B4 falling to A4, held into a breath tail'),
    ('ChopWholeLine', 0, 5, 'come find me on the floor', 64,
     'the whole line from its frames, no inhale: E4 F#4 A4 G#4 F#4, then "floor" on B4 falling to A4'),
)
PICK_CHOP_BY_NAME = {c[0]: c for c in PICK_CHOPS}
# The title checks of the pieces of the line the chops sing, or sang in round 2's first take (web search, the exact
# phrase with "song" and with "lyrics", 2026-10-29). The first take's set had the four "cut" pieces; no chop sings one
# whole or is named after one.
CHOP_CHECKS = {
    'come find me on the floor': 'kept: no release with that title or line (the line itself)',
    'find me on the floor': 'kept: no release with that title or line',
    'come find me': 'cut: the title of released singles, a 2025 house single among them',
    'find me': 'cut: the title of released singles, among them a 2016 dance single whose chorus sings "come find me"',
    'on the floor': 'cut: the title of released singles, among them a 2011 dance-pop number one',
    'the floor': 'cut: the title of released singles, a 1993 one among them',
}
# Chops whose first word follows an open syllable: the aligner splits the consonant between the two words, so "floor"
# alone kept only the second half of its f (about 40 of 80 ms) and Whisper heard "Do"; with lead_in() it hears "Floor"
LEAD_IN = frozenset({'ChopFloor'})


def lead_in(track, first):
    """The track with the coda frames of the event before `first` handed to `first` as the start of its onset, and
    unvoiced. "the" has no coda of its own: its coda frames are the vowel's release into the f of "floor" and that f's
    first half. Unvoiced, they lead in as breath and frication, with no pitch of the note before."""
    ev, kind, f0 = track.ev.copy(), track.kind.copy(), track.f0.copy()
    j = int(np.flatnonzero((track.ev == first) & (track.kind > 0))[0]) - 1
    while j >= 0 and track.ev[j] == first - 1 and track.kind[j] == 3:
        ev[j], kind[j], f0[j] = first, 1, 0.0
        j -= 1
    return replace(track, ev=ev, kind=kind, f0=f0)


def pick_cut(name):
    """One chop of the picked line, cut in the frame domain like round 1's (vocals.chop): (track, junction)."""
    _, first, last, _, _, _ = PICK_CHOP_BY_NAME[name]
    track = lead_in(line_track(PICK), first) if name in LEAD_IN else line_track(PICK)
    return V.chop(track, first, last)


def pick_chop(take):
    """One chop of the picked line, polished with the whole line's levels; the junction where its breath tail starts
    is logged as a splice."""
    tr, junction = pick_cut(take.item.name)
    return Out(V.polish(V.synth(tr), hp=HP, levels=line_sung(PICK)[1]), splices=(int(np.floor(junction * SR + 0.5)),))


def chop_onset(name):
    """Seconds from the start of a chop's file to its note (where the beat falls, after the onset consonant), for any
    chop here: round 1's (CHOPS) or the pick's (PICK_CHOPS). Needs the vocal models."""
    if name in PICK_CHOP_BY_NAME:
        return float(pick_cut(name)[0].events[0]['t_on'])
    _, first, last, _, _ = CHOP_BY_NAME[name]
    return float(V.chop(sung_track(), first, last)[0].events[0]['t_on'])


# ------------------------------------------------------------------ the robot phrase
ROBOT = 'Boost the signal'
ROBOT_SYLLABLES = ('boost', 'the', 'sig', 'nal')
ROBOT_GRID = ((0, 3), (3, 1), (4, 2), (6, 3))  # (16th step, length) of each syllable, one bar
ROBOT_NOTE = K.ROOT + 12     # F#3, 185 Hz: monotone
ROBOT_LEAD = 0.06            # a slice starts this long before its step: room for the onset consonant
ROBOT_PRE = ROBOT_LEAD + 0.01
ROBOT_LEN = 9 * K.STEP + 0.3
ECHO_STEPS = 3               # dotted 8th


def slice_sample(step):
    return int(np.floor((ROBOT_PRE + step * K.STEP - ROBOT_LEAD) * SR + 0.5))


# (nominal 16th step, onset sample in the file, syllable): the slice table for index.json, the same in dry and wet
ROBOT_SLICES = tuple((step, slice_sample(step), syl) for (step, _), syl in zip(ROBOT_GRID, ROBOT_SYLLABLES))
SLICE_NOTE = 'slices (16th step @ onset sample): ' + ', '.join(f'{s}@{n} {w}' for s, n, w in ROBOT_SLICES)


@functools.lru_cache(maxsize=1)
def robot_voice():
    """The robot, before its level: Kokoro's take on the grid, WORLD on a flat buzzing F#3 with the formants 10% down,
    then the metal: a +9 Hz frequency-shifted copy beating against it and a ring modulator on alloy-ratio carriers from
    F#1, both mixed under the voice; then grit, 4x oversampled, which also keeps the peaks near the loudness."""
    perf = V.performance('boost the signal.', K.VOICE, 1.0, False)
    tr = V.robot_track(perf, ROBOT_GRID, K.BPM, float(D.midi_hz(ROBOT_NOTE)), ROBOT_LEN, ROBOT_PRE, ROBOT_LEAD,
                       buzz=0.05, formant=0.9)
    y = D.bfilt(V.synth(tr), HP, 'highpass', order=4)
    y = 0.65 * y + 0.35 * D.freq_shift(y, 9.0)
    y = D.ring_mod(y, float(D.midi_hz(K.KICK_NOTE)), K.ALLOY, mix=0.18, r=D.rng(K.KIT, 'RobotBoost', 'ring'))
    y = V.compress(V.deess(y), -18, 3.0, 0.004, 0.08)
    y = y / (np.max(np.abs(y)) + 1e-12)
    y = 0.4 * y + 0.6 * D.oversampled(lambda u: D.tanh_adaa(u, 3.0), y)
    return D.bfilt(y, HP, 'highpass', order=4)


def slices():
    return tuple(n for _, n, _ in ROBOT_SLICES)


SLICE_META = {'slices': [{'step': step, 'onset': n, 'syllable': syl} for step, n, syl in ROBOT_SLICES]}


def robot_dry(take):
    return Out(robot_voice(), splices=slices(), meta=SLICE_META)


def echo(x, taps=6, fb=0.45, shift_hz=7.0):
    """Ping-pong echo on a dotted 8th whose every repeat is frequency-shifted a little further up (a barber-pole echo),
    darker and thinner each time. Mono in, stereo out, the dry signal not included."""
    d = int(np.floor(ECHO_STEPS * K.STEP * SR + 0.5))
    out = np.zeros((len(x) + d * taps, 2))
    tap = x
    for k in range(1, taps + 1):
        tap = D.freq_shift(D.bfilt(D.bfilt(tap, 5000, 'lowpass'), 300, 'highpass'), shift_hz)
        tap = tap * (fb if k > 1 else 0.7)
        out[k * d:k * d + len(x), (k - 1) % 2] += tap
        out[k * d:k * d + len(x), k % 2] += 0.25 * tap
    return out


def robot_wet(take):
    """The dry robot in the kit's space (the FDN on the FUT01/space seed) with the barber-pole echo. The space and the
    echo are mono under 150 Hz."""
    y = robot_voice()
    n_tail = int(4.5 * SR)
    send = D.bfilt(y, 300, 'highpass')
    verb = D.fdn_reverb(send, K.space_rng(), t60=2.4, size=0.9, damp_hz=6000.0, n_tail=n_tail)
    dly = echo(y)
    out = np.zeros((len(y) + n_tail, 2))
    out[:len(y)] += D.st(y)
    out[:len(dly)] += 0.55 * dly
    out += 0.32 * verb[:len(out)]
    out = D.mono_below(D.bfilt(out, HP, 'highpass', order=4), 150.0)
    return Out(out, splices=slices(), meta=SLICE_META)


ITEMS = [
    Item('Vox', 'SungPhrase', 'vox', sung_phrase, level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'dry', 'sung'),
         sets=('s1a',), script=SUNG, about=f'the sung line the chops come from, dry: "{SUNG}", F# minor'),
] + [
    Item('Vox', name, 'vox', sung_chop, root=SUNG_NOTES[first][2], level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'chop', 'sung'), sets=('s1a',), script=word, about=f'sung chop {about}, cut from "{SUNG}"')
    for name, first, last, word, about in CHOPS
] + [
    Item('Vox', ln.name, 'vox', sung_whole, root=ln.notes[0][2], level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'sung'), sets=('r2',), script=ln.text,
         about=f'candidate line {ln.name[-1]}{" (the pick: the chops come from it)" if ln.name == PICK else ""}, '
               f'dry: "{ln.text}", F# minor, on the kit swing: {ln.about}')
    for ln in LINES
] + [
    Item('Vox', name, 'vox', pick_chop, root=root, level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'chop', 'sung'),
         sets=('r2',), script=words, about=f'sung chop {about}, cut from "{LINE_BY_NAME[PICK].text}"')
    for name, first, last, words, root, about in PICK_CHOPS
] + [
    Item('Vox', 'RobotBoost', 'vox', robot_dry, root=ROBOT_NOTE, level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'robot'), sets=('s1a', 'r2'), script=ROBOT,
         about=f'robot phrase "{ROBOT}" on the 16th grid at 134 BPM, dry; {SLICE_NOTE}'),
    Item('Vox', 'RobotBoostWet', 'vox', robot_wet, root=ROBOT_NOTE, level=-16.0, flags=VOX, qa=WHY,
         tags=('clean', 'wet', 'robot'), sets=('s1a', 'r2'), script=ROBOT,
         about=f'robot phrase "{ROBOT}", wet: the kit space and a barber-pole echo; {SLICE_NOTE}'),
]
