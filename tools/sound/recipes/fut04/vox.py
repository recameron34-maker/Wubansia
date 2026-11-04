"""FUT04 vocals for listening round 2: three sung candidate lines, the chops cut from the pick, and a distorted robot
phrase on the 16th grid, dry and wet.

Round 1's sung chops scored 4 but the line was a "weird phrase", so these are short, plain lines a club vocalist would
sing, darker for this kit, sung by its low male blend (K.VOICE: am_onyx 0.6, am_fenrir 0.4) in E minor, 2 bars at
136 BPM on the kit's 0.54 swing (K.SWING, as every lane):
- SungLine1, the pick: "Come down to the furnace" (E3 G3 F#3 E3, then "furnace" on beat 3, B3 falling to G3 over the
  bar line). An invitation down into the kit's own room; "fur" lands with the kick on beat 3.
- SungLine2: "Move like a machine" (G3 G3 F#3, "machine" A3 up to B3, held from beat 3).
- SungLine3: "Nothing stops the night" (G3 F#3 E3 G3, "night" held on B3 from beat 3, falling to A3).
Each was title-checked on 2026-10-29 (the exact phrase with "song" and with "lyrics"), and checked again the same way
in the fix round: no release uses it as a title and no lyric line matched (the nearest, "Nothing's Gonna Stop Us
Tonight", is another title). Three earlier candidates were dropped: "Keep me in the heat" (Whisper never heard "heat" in
12 takes), "Make the iron sing" (heard as "make the tire sing"), and "Dance inside the fire", one word from a recent
dance single's title. Of Kokoro draws 0-3 (US lexicon, the blend's own), each line's take is one Whisper hears word for
word as rendered and best under band noise 6 dB down: SungLine1 every draw (draw 0); SungLine2 draw 2; SungLine3 draw
0. The pick is SungLine1: the only line Whisper heard exactly on all twelve takes, under noise too, and its words
carry the kit.

The seven chops (ChopCome ... ChopDownToTheFurnace) are cut from SungLine1 in the WORLD frame domain (vocals.chop): each
keeps its onset consonant whole and ends in a breath tail, so no cut lands in the audio; each is polished with the
whole line's levels.

The robot: "Fill the furnace" (ROBOT), one syllable per slice on straight 8ths, "fill" on the beat, "the" on its
off-beat, "fur" on beat 2, "nace" on the off-beat 8th, held: a march. A monotone buzz on E3, the tonic. Then the
furnace: a +7 Hz frequency-shifted copy beating against it, a ring modulator on carriers on the kit's iron ratios from
E1 (the kick's note), a drive at 4x, and a much harder one on the band above 3 kHz alone, so it is distorted but every
slice still reads. A drive of 5-6 over the whole voice lost the words under band noise 18 dB down, and so did B2's
ring-modulator difference tones, which also put it over the low cut; the hard top band keeps the grit and the words.
The first line, "Feed the furnace", is the title of released tracks (a 2011 rock album track, a 2004 EP), so the fix
round replaced it. Whisper picked the line, the take and the note (robot_chain, the item's own chain; Kokoro draws 0-5
on B2, D3 and E3, heard as rendered and under 300-6000 Hz band noise 18 and 12 dB down; the first word and its coda are
the hard part): "Stoke the furnace" came back as "Stokely furnace", "Stoke the iron" as "so be I", "Strike the iron" as
"right in the car", "Light the furnace" as "like the furnace", "Work the furnace" as "what the furnace", "Load the
furnace" as "low need furnace", "Heat the furnace" as "heat deep furnace" and "Fan the furnace" as "fan furnace";
"Fuel" and "fire" split into two syllables. "Fill the furnace", draw 5 on E3, reads word for word in all three, and on
four more noise seeds word for word with noise 18 dB under the voice, 12 dB under on three of them and 9 dB on two. Its
title check, 2026-10-29: the exact phrase with "song", with "lyrics" and with "track OR album OR EP OR single" finds no
release that uses it as a title and no lyric line; run again the same three ways on 2026-11-04, with the same result.
It is still the hardest of the four kits' robots to read: with silence added before or after the file (0-0.5 s before,
0-1 s after, at 0 and -6 dB: 40 versions), Whisper hears it word for word on 13-14 of them, against 35-39 for the
other kits' robots and vocoders (FUT01's, kept in round 1, 39). A re-pick on 2026-11-04 found nothing steadier: every
other draw on B2, D3 and E3 reads 0-9 of 40 ("Philly furnace", "village furnace"; B2's draw 5 22, but 5 of 8 noise
versions against this take's 7), a lighter drive or grit at most 24, no top band 0, and the words' own stress (a short
"the" on the 16th before beat 2) 0 on every draw. So this take stays, and whether the words read is the listener's
call. The slice points are fixed by the grid (ROBOT_SLICES), the same in dry and wet, and go into the
item's metadata: the sampler fires slice k at its step, so the phrase stays on the grid at 128 or 140. The wet version
adds the kit's dark concrete space and a dotted-8th echo that darkens each repeat.

Every file is a WORLD resynthesis at 48 kHz from the features of a 24 kHz Kokoro take (ADR 0005, D4: hf_synthetic),
high-passed at 100 Hz; the wet robot is mono under 150 Hz. Rendering needs requirements-vocals.txt and the models
(WUBANSIA_MODELS_DIR); importing this module does not.

Provenance: the chain, the chops and the robot follow FUT01's (recipes/fut01/vox.py); the lines, notes and
processing are this kit's.
"""
import functools
from dataclasses import dataclass, replace

import numpy as np

from tools.sound import dsp48 as D
from tools.sound import vocals as V
from tools.sound.item import Item, Out
from tools.sound.kits import fut04 as K

SR = D.SR
HP = 100.0                   # nothing under ~100 Hz competes with the kick and sub
VOX = frozenset({'hf_synthetic'})
WHY = {'why': 'WORLD resynthesis at 48 kHz of a 24 kHz Kokoro take: the top band is synthetic (extend_hf, D4)'}
ACCENT = 'us'                # the blend's own lexicon (its heavier voice is am_)


# ------------------------------------------------------------------ the three candidate lines
@dataclass(frozen=True)
class Line:
    """One candidate line: its words, its notes ((16th step, length, MIDI, word); '~' carries the word on) and the
    Kokoro draw it is sung from."""
    name: str
    text: str
    notes: tuple
    draw: int
    about: str


LINE_HOW = V.Sing(speed=0.8, consonants=1.3)
LINE_PRE = 0.5               # seconds before step 0: the inhale and the first onset
LINE_LEN = 30 * K.STEP + 0.4
LINES = (
    Line('SungLine1', 'Come down to the furnace',
         ((0, 2, 52, 'come'), (2, 3, 55, 'down'), (5, 1, 54, 'to'), (6, 2, 52, 'the'), (8, 4, 59, 'furnace'),
          (12, 8, 55, '~')), 0,
         'E3 G3, "to" on the swung 16th on F#3, E3, "furnace" on beat 3 with the kick, B3 falling to G3 over the bar '
         'line'),
    Line('SungLine2', 'Move like a machine',
         ((0, 3, 55, 'move'), (3, 1, 55, 'like'), (4, 2, 54, 'a'), (6, 2, 57, 'machine'), (8, 8, 59, '~')), 2,
         'G3, "like" on the swung 16th, F#3, "machine" A3 up to B3, held from beat 3'),
    Line('SungLine3', 'Nothing stops the night',
         ((0, 2, 55, 'nothing'), (2, 1, 54, '~'), (3, 2, 52, 'stops'), (6, 2, 55, 'the'), (8, 8, 59, 'night'),
          (16, 4, 57, '~')), 0,
         'G3 F#3, "stops" on the swung 16th on E3, G3, "night" held on B3 from beat 3, falling to A3'),
)
LINE_BY_NAME = {ln.name: ln for ln in LINES}
PICK = 'SungLine1'           # the line the chops are cut from


@functools.lru_cache(maxsize=None)
def line_track(name):
    """One candidate line as one sung performance on the kit's swing: its frames before synthesis, the phrase ends not
    yet released (chops are cut from it)."""
    ln = LINE_BY_NAME[name]
    perf = V.performance(V.sing_text(ln.notes), K.VOICE, LINE_HOW.speed, LINE_HOW.glottal, ACCENT, ln.draw)
    return V.sing_track(perf, ln.notes, K.BPM, LINE_HOW, LINE_LEN, LINE_PRE, D.rng(K.KIT, name, 'sing'),
                        swing=K.SWING)


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
# name, first and last syllable (event index: come 0, down 1, to 2, the 3, fur 4, nace 5), the words, the chop's first
# note (its root), about
PICK_CHOPS = (
    ('ChopCome', 0, 0, 'come', 52, '"come", E3, on the beat'),
    ('ChopDown', 1, 1, 'down', 55, '"down", G3, held a dotted 8th'),
    ('ChopComeDown', 0, 1, 'come down', 52, '"come down", E3 up to G3'),
    ('ChopFur', 4, 4, 'fur', 59, '"fur" of furnace, B3: a syllable to play on the grid'),
    ('ChopFurnace', 4, 5, 'furnace', 59, '"furnace", B3 falling to G3, held into a breath tail'),
    ('ChopTheFurnace', 3, 5, 'the furnace', 52, '"the furnace", E3, then B3 falling to G3'),
    ('ChopDownToTheFurnace', 1, 5, 'down to the furnace', 55, '"down to the furnace", G3 F#3 E3, then B3 to G3'),
)
PICK_CHOP_BY_NAME = {c[0]: c for c in PICK_CHOPS}


def pick_chop(take):
    """One chop of the picked line, cut in the frame domain and polished with the whole line's levels; the junction
    where its breath tail starts is logged as a splice."""
    _, first, last, _, _, _ = PICK_CHOP_BY_NAME[take.item.name]
    tr, junction = V.chop(line_track(PICK), first, last)
    return Out(V.polish(V.synth(tr), hp=HP, levels=line_sung(PICK)[1]), splices=(int(np.floor(junction * SR + 0.5)),))


def chop_onset(name):
    """Seconds from the start of a chop's file to its note (where the beat falls, after the onset consonant). Needs the
    vocal models."""
    _, first, last, _, _, _ = PICK_CHOP_BY_NAME[name]
    return float(V.chop(line_track(PICK), first, last)[0].events[0]['t_on'])


# ------------------------------------------------------------------ the robot phrase
TITLE_CLASHES = ('Feed the furnace',)   # lines dropped because a release uses them as a title (the docstring has the
                                         # checks): no line, chop or robot here may come back to them
ROBOT = 'Fill the furnace'
ROBOT_ITEM = 'RobotFillTheFurnace'   # the dry item's name, after its words as the chops' are (the wet one adds Wet);
                                     # it also seeds the ring modulator
ROBOT_SYLLABLES = ('fill', 'the', 'fur', 'nace')
ROBOT_GRID = ((0, 2), (2, 2), (4, 2), (6, 4))   # (16th step, length) of each syllable, one bar: straight 8ths
ROBOT_NOTE = K.ROOT + 12     # E3, 164.8 Hz: monotone, on the tonic
ROBOT_DRAW = 5               # the take: of draws 0-5 on B2, D3 and E3, the one Whisper hears word for word through the
                             # whole chain, as rendered and under band noise (see the module docstring)
ROBOT_RING = 0.15            # the ring modulator's mix
ROBOT_DRIVE = (3.0, 0.6)     # the drive over the whole voice: tanh drive, its share of the mix
ROBOT_GRIT = (3000.0, 12.0, 0.4)   # the top band: from this frequency, tanh drive, mixed in at this level
ROBOT_LEAD = 0.06            # a slice starts this long before its step: room for the onset consonant
ROBOT_PRE = ROBOT_LEAD + 0.01
ROBOT_LEN = 10 * K.STEP + 0.3
ECHO_STEPS = 3               # dotted 8th


def swung(step):
    """A 16th on the kit's swing, in 16ths: odd 16ths land late, as on every lane."""
    return step + (2 * K.SWING - 1) * (step % 2)


def slice_sample(step):
    return int(np.floor((ROBOT_PRE + swung(step) * K.STEP - ROBOT_LEAD) * SR + 0.5))


# (nominal 16th step, onset sample in the file, syllable): the slice table for index.json, the same in dry and wet
ROBOT_SLICES = tuple((step, slice_sample(step), syl) for (step, _), syl in zip(ROBOT_GRID, ROBOT_SYLLABLES))
SLICE_NOTE = 'slices (16th step @ onset sample): ' + ', '.join(f'{s}@{n} {w}' for s, n, w in ROBOT_SLICES)
SLICE_META = {'slices': [{'step': step, 'onset': n, 'syllable': syl} for step, n, syl in ROBOT_SLICES],
              'swing': K.SWING, 'bpm': K.BPM}


def robot_chain(text, draw, note):
    """The robot on any line, Kokoro draw and MIDI note, before its level: Kokoro's take on the grid, WORLD on a flat
    buzzing note; then the furnace: a +7 Hz frequency-shifted copy beating against it, a ring modulator on iron-ratio
    carriers from E1 mixed under the voice, a drive at 4x (blended, so the words stay) and a much harder one on the band
    above 3 kHz; and a steep high-pass at 120 Hz for the ring modulator's difference tones. The Whisper search that
    picked ROBOT_DRAW and ROBOT_NOTE ran this same function."""
    perf = V.performance(f'{text.lower()}.', K.VOICE, 1.0, False, ACCENT, draw)
    grid = tuple((swung(s), ln) for s, ln in ROBOT_GRID)
    tr = V.robot_track(perf, grid, K.BPM, float(D.midi_hz(note)), ROBOT_LEN, ROBOT_PRE, ROBOT_LEAD, buzz=0.05)
    y = D.bfilt(V.synth(tr), HP, 'highpass', order=4)
    y = 0.65 * y + 0.35 * D.freq_shift(y, 7.0)
    y = D.ring_mod(y, float(D.midi_hz(K.KICK_NOTE)), K.RATIOS, mix=ROBOT_RING, r=D.rng(K.KIT, ROBOT_ITEM, 'ring'))
    y = V.compress(V.deess(y), -18, 3.0, 0.004, 0.08)
    y = y / (np.max(np.abs(y)) + 1e-12)
    drive, share = ROBOT_DRIVE
    y = (1 - share) * y + share * D.oversampled(lambda u: D.tanh_adaa(u, drive), y)
    hz, drive, mix = ROBOT_GRIT
    top = D.bfilt(y, hz, 'highpass', order=4)
    top = top / (np.max(np.abs(top)) + 1e-12)
    y = y + mix * D.bfilt(D.oversampled(lambda u: D.tanh_adaa(u, drive), top), hz, 'highpass', order=2)
    return D.bfilt(D.bfilt(y, 120.0, 'highpass', order=8), HP, 'highpass', order=2)


@functools.lru_cache(maxsize=1)
def robot_voice():
    """The kit's robot line through robot_chain, before its level."""
    return robot_chain(ROBOT, ROBOT_DRAW, ROBOT_NOTE)


def slices():
    return tuple(n for _, n, _ in ROBOT_SLICES)


def robot_dry(take):
    return Out(robot_voice(), splices=slices(), meta=SLICE_META)


def echo(x, taps=5, fb=0.42):
    """Ping-pong echo on a dotted 8th, each repeat darker and thinner. Mono in, stereo out, the dry signal not
    included."""
    d = int(np.floor(ECHO_STEPS * K.STEP * SR + 0.5))
    out = np.zeros((len(x) + d * taps, 2))
    tap = x
    for k in range(1, taps + 1):
        tap = D.bfilt(D.bfilt(tap, 3800, 'lowpass'), 350, 'highpass') * (fb if k > 1 else 0.6)
        out[k * d:k * d + len(x), (k - 1) % 2] += tap
        out[k * d:k * d + len(x), k % 2] += 0.25 * tap
    return out


def robot_wet(take):
    """The dry robot in the kit's dark concrete space (the FDN on its space seed) with the dotted-8th echo. The space
    and the echo are mono under 150 Hz."""
    y = robot_voice()
    n_tail = int(4.0 * SR)
    send = D.bfilt(y, 300, 'highpass')
    verb = D.fdn_reverb(send, K.space_rng(), t60=2.2, size=1.1, damp_hz=4000.0, n_tail=n_tail)
    dly = echo(y)
    out = np.zeros((len(y) + n_tail, 2))
    out[:len(y)] += D.st(y)
    out[:len(dly)] += 0.5 * dly
    out += 0.35 * verb[:len(out)]
    out = D.mono_below(D.bfilt(out, HP, 'highpass', order=4), 150.0)
    return Out(out, splices=slices(), meta=SLICE_META)


ITEMS = [
    Item('Vox', ln.name, 'vox', sung_whole, root=ln.notes[0][2], level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'sung'), sets=('r2',), script=ln.text,
         about=f'candidate line {ln.name[-1]}{" (the pick: the chops come from it)" if ln.name == PICK else ""}, '
               f'dry: "{ln.text}", E minor, the low male blend on the kit swing: {ln.about}')
    for ln in LINES
] + [
    Item('Vox', name, 'vox', pick_chop, root=root, level=-18.0, flags=VOX, qa=WHY, tags=('clean', 'chop', 'sung'),
         sets=('r2',), script=words, about=f'sung chop {about}, cut from "{LINE_BY_NAME[PICK].text}"')
    for name, first, last, words, root, about in PICK_CHOPS
] + [
    Item('Vox', ROBOT_ITEM, 'vox', robot_dry, root=ROBOT_NOTE, level=-18.0, flags=VOX, qa=WHY,
         tags=('clean', 'dry', 'robot'), sets=('r2',), script=ROBOT,
         about=f'robot phrase "{ROBOT}" on straight 8ths at 136 BPM, distorted, monotone '
               f'{D.note_name(ROBOT_NOTE)}, dry; {SLICE_NOTE}'),
    Item('Vox', f'{ROBOT_ITEM}Wet', 'vox', robot_wet, root=ROBOT_NOTE, level=-16.0, flags=VOX, qa=WHY,
         tags=('clean', 'wet', 'robot'), sets=('r2',), script=ROBOT,
         about=f'robot phrase "{ROBOT}", wet: the kit\'s dark concrete space and a dotted-8th echo; {SLICE_NOTE}'),
]
