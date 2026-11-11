"""FUT03's written music for the MIDI export (tools/sound/midi.py, MIDI v2; the schema: tools/sound/README.md, "MIDI").

- The loop (Loop_Vox, Loop_NoVox): the round 5 context loop (context5.py on context.py) lane by lane: the deep kick on
  every beat, the clap and snare, the shuffled hats, the shaker's 16ths, the congas, bongos and rims; the Rubber line
  twice; the dub stab off the kicks on G3 (its root: Gm9), its low-pass ride (CC74); Pad Warm held under all 4 bars
  (channel 11); the vox synth riff in bars 3-4; the variant's accents. The loop has no lead and no fill.
- The fill bar (the extended mix's, at every Fill cue): the kit's FillPreview (context.FILL_*): kick, hats and shaker on
  beats 1-3, the clap on beat 2, no congas, bongos or rim, beat 4 empty for the Build Fill (audit A, defect 7).
- The parts: the two rubber basslines, the keys riff (the lead), the dub stab and its Gm9 voicing, and the synth
  phrases: the rubber riff, the dub chords (Gm9 on G3, Cm9 on C4: the same file a fourth up), the keys arp and the vox
  synth riff.
- The extended mix: FUT03's own form (FORM, below). Deep tech house lives on its groove and develops slowly: something
  new on every 8-bar line, something structural every 16, the bass and the dub stab carrying the tension. 32 bars of
  drums at each end; the Rubber line from bar 33; the stab coming in dark and opening across the groove (its CC74
  ride); the keys, then the vox's call answered by the keys. Break 1 is the vox riff over Pad Warm, Gm9 | Cm9 bar by
  bar (the riff's second bar is Cm9). The build rolls the clap over 3 bars with the kick under it, then breathes on
  beat 4 of the FillPreview bar under the Build Fill. The drop lands on its impact with Gm9 | Cm9 and the whole riff,
  then the keys' call answered by the vox. The switch-up rides the Rubber riff; break 2 is the keys arp over the pad;
  drop 2 is on the Deep line with the arp, stripping into the outro. The arrangement's own drums: the bongo D5 pair
  only in a section's even lines, a conga run at every turnaround, the drops' rim on the 'and's of 3 and 4, the open
  hat at full velocity in bars 4 and 8, 32nd flicks in the busy hats.
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide).
"""
import math

from tools.sound import channels as CH
from tools.sound.midi import Hit, Key, Note, hits, line, section, shifted
from tools.sound.recipes.fut03 import accents as A, context as C, context5 as C5, groove as GV, rubber as B
from tools.sound.recipes.fut03 import voxsynth as VS

SHAKER = 54                         # GM's tambourine: GM has no shaker inside 35-59, where the map keeps every drum and
                                    # perc note, as the generator's shaker lanes do
RIM = 37                            # GM's side stick
HAND = {('Conga', 55): 41, ('Conga', 60): 43, ('Conga', 62): 45, ('Bongo', 67): 47, ('Bongo', 74): 48}
KEYS_ZONES = 'G3, D4, G4, D5, G5'
CHORD_ROOT = {A.GM9: 55, A.CM9: 60}  # the Gm9 sample played on G3 is Gm9; on C4, a fourth up, it is the Cm9 voicing


def notes(rows):
    return [(s, m, ln, v, False) for s, m, ln, v in rows]


def glided(rows):
    """A rubber line, (16th, length, note, velocity, glide), as MIDI notes. A glide inside a note, (target, where,
    seconds), becomes the note up to `where` of its length, then its target slid into from it for the rest (as FUT01
    writes its dips): a mono synth set to legato glides there (rubber.py's own glide time is the synth's)."""
    out = []
    for s, ln, n, v, glide in rows:
        if glide is None:
            out.append((s, n, ln, v, False))
            continue
        target, where, _ = glide
        out += [(s, n, ln * where, v, False), (s + ln * where, target, ln * (1 - where), v, True)]
    return out


def drums(kick, clap, hats, shaker, perc):
    """Drum notes from context.py's parts: (16th, velocity) hits, the hats' (16th, kind, velocity), the perc's (16th,
    name, zone, velocity)."""
    return (hits(kick, 36) + hits(clap, 38) + [Note(s, 42 if kind == 'closed' else 46, 0.5, v) for s, kind, v in hats]
            + hits(shaker, SHAKER) + [Note(s, RIM if name == 'Rimshot' else HAND[(name, z)], 0.5, v)
                                      for s, name, z, v in perc])


LOOP_DRUMS = drums(C.KICK, C.CLAP, C.HATS, C.SHAKER, C.PERC)
PREVIEW = drums(C.FILL_KICK, C.FILL_CLAP, C.FILL_HATS, C.FILL_SHAKER, C.FILL_PERC)   # 2 bars: the groove, the fill bar
FILL_BLOCK = [n for n in LOOP_DRUMS if n.step < 48] + shifted([n for n in PREVIEW if n.step >= 16], 32)
PART = {36: 'kick', 38: 'snare', 42: 'hats', 46: 'hats'}


def stab_ride():
    """The dub stab's low-pass ride as CC74 every 60 ticks over the 4 bars, from context.stab_lane: closed at 900 Hz on
    the loop's downbeat, open to 5 kHz at its middle, a raised cosine in log frequency; on channels.cc_value's law over
    900-5000 Hz. Its Q is fixed (0.9): no CC71."""
    lo, hi = C.STAB_RIDE
    return [CH.cc_value(lo * (hi / lo) ** (0.5 - 0.5 * math.cos(2 * math.pi * i / 256)), lo, hi) for i in range(256)]


def loop_accents(variant):
    """context5's accents for a variant as hits ('end': the file ends on the loop's downbeat)."""
    return [Hit(64, name, 1, 'end') if step == 'end' else Hit(step, name, 1) for step, name in C5.ACCENTS[variant]]


# ------------------------------------------------------------------ the extended mix's own material (audit B, FUT03)
PAIR = ((23, 'Bongo', 74), (27, 'Bongo', 74))   # the bongo D5 pair: the perc's second bar, 16ths 7 and 11, ghosts
DROP_RIM = (10, 14)                             # the drops' rim: the 'and's of 3 and 4, where the loop's is on 13


def perc(pair=True, rim=None):
    """The perc lane over 4 bars: the shaker's 16ths and groove.PERC twice (the congas, bongos and rim), the bongo D5
    pair in or out, the rim on its 16th 13 or on each bar's `rim` 16ths (at the loop's 0.6)."""
    rows = [p for p in GV.PERC if (pair or p[:3] not in PAIR) and (rim is None or p[1] != 'Rimshot')]
    rows = sorted(rows + [(16 * b + s, 'Rimshot', None, 0.6) for b in range(2) for s in (rim or ())],
                  key=lambda p: p[0])
    return drums((), (), (), C.SHAKER, [(s + 32 * r, n, z, v) for r in range(2) for s, n, z, v in rows])


def turn_bar(congas=True, rim=()):
    """A turnaround, the last bar of a line that doesn't end on a fill: the shaker as written and the perc's second
    bar without the bongo pair (congas=False: the shaker alone, where the line has no congas yet), then a conga run
    into the downbeat, D4 C4 G3 G3 on 16ths 12-15 in place of the rim and the ghosts: the hits on the beat and its
    'and' full, the swung 16ths lighter, as the loop's congas play; `rim`: the drops' rim kept on its 16ths."""
    bar2 = [(s - 16, n, z, v) for s, n, z, v in GV.PERC if 16 <= s < 28 and (s, n, z) not in PAIR] if congas else []
    run = [(12, 'Conga', 62, 1.0), (13, 'Conga', 60, 0.6), (14, 'Conga', 55, 1.0), (15, 'Conga', 55, 0.6)]
    rims = [(s, 'Rimshot', None, 0.6) for s in rim]
    return drums((), (), (), C.SHAKER[:16], sorted(bar2 + rims + run, key=lambda p: p[0]))


# the groove's hats: the loop's, the open hat on the 'and' of 4 of the block's last bar (bars 4 and 8 of a line) at
# full velocity, where it rings out (context.LET_RING): the 4-bar phrase marked in the hats
GROOVE_HATS = [Note(s, 42 if kind == 'closed' else 46, 0.5, 1.0 if (s, kind) == (62, 'open') else v)
               for s, kind, v in C.HATS]
# the busy hats: a closed-hat flick a 32nd before beats 2 and 4 and the next downbeat, the softest layer but one; every
# 16th of the hats is written but the 'a' of 4, where the open hat rings, so a busier line adds 32nds there
BUSY_HATS = [Note(3.5, 42, 0.5, 0.4), Note(11.5, 42, 0.5, 0.4), Note(15.5, 42, 0.5, 0.4)]


def chord_block(rows):
    """A chord block from (16th, velocity, chord) rows: the trigger, the chord sample at the chord's root (CHORD_ROOT),
    and its voicing, the chord's tones."""
    return dict(trigger=[(s, CHORD_ROOT[ch], 1.5, v, False) for s, v, ch in rows],
                voicing=[(s, n, 1.5, v, False) for s, v, ch in rows for n in ch])


GM_CM = 'Gm9 Cm9'                   # the loop's stab rhythm, Gm9 | Cm9 bar by bar: the push on a Gm9 bar's last 'a'
GM_CM_ROWS = [(s, v, A.CM9 if s // 16 % 2 or s % 16 == 15 else A.GM9) for s, v in C.STAB]   # anticipates the Cm9
DUB = 'Gm9 Gm9 Cm9 Gm9'             # the dub chords phrase's rhythm and harmony, as a block with its voicing
DUB_ROWS = [(16 * b + s, v, ch) for b, ch in enumerate(A.DUB_BARS) for s, v in A.DUB_RHYTHM]

# ------------------------------------------------------------------ the form
DR = 'kick snare hats perc'
STAB = 'Chords'                     # the ride's track: the dub stab's low-pass, CC74
ROLL = dict(lane='snare', bars=3, every=((1, 4), (1, 2), (1, 1)), ramp=(0.7, 1.0), rest='lane')
RIFF = ('Rubber riff',)
ARP = ('Keys arp',)
ORGAN = 'AccentOrganHitGm7'         # the switch-up's own ear candy: a Gm7 organ stab on the 'and' of 4 every 4 bars

FORM = (
    # 32 bars to mix in on, an element on every line, each line turned by the conga run (alone, until the congas come
    # in): the kick and the shaker; the open hats, fading in; the clap and the whole hats; the congas, bongos and rim
    section('Intro', 'intro', (
        line('kick perc', drums='shaker', end='turn', turn='run'),
        line('kick hats perc', hats='thin', rise=('hats', 0.5, 1.0), drums='shaker', end='turn', turn='run'),
        line(DR, drums='shaker', end='turn', turn='run'),
        line(DR, end='turn'))),
    # the Rubber line from 33, the first harmony a Cm9 dub chord on 40's 'and' of 4; the stab at 41, dark (CC74 from
    # 900 Hz) and opening line by line; the keys' riff at 49; the vox's call at 57 answered by the keys' second bar, the
    # drums out in bar 64 under the reverse swell, the keys' answer left hanging on its A4; the harmony holds Gm9, the
    # Cm9 only a hint (the dub chord accents) until the break
    section('Groove', 'groove', (
        line(DR + ' bass', drums='lite', end='turn', stab=True),
        line(DR + ' bass chords', ride={STAB: ('ramp', 0, 42)}, accents='alternate', end='turn', into='uplift'),
        line(DR + ' bass chords lead', drums='lite', ride={STAB: ('ramp', 42, 85)}, accents='alternate', end='turn',
             stab=True),
        line(DR + ' bass chords lead vox', lead='answer', vox='call', cue='Vox', ride={STAB: ('ramp', 85, 127)},
             accents='alternate', end='drop', into='swell'))),
    # no kick: the vox riff over Pad Warm from the break's downbeat, Gm9 | Cm9; then the stab, the keys' call and the
    # off-beat hats rising into the fill
    section('Break', 'break', (
        line('pad vox', chords=GM_CM, vox='riff', cue='Vox', accents='loop'),
        line('hats pad chords lead vox', chords=GM_CM, hats='thin', rise=('hats', 0.4, 1.0), lead='call', vox='riff',
             accents='lead-in', end='fill', into='swell'))),
    # the kick, the shaker (swelling) and the Rubber line back under the stab opening; then the bass out and the clap
    # rolling over the 3 bars before the fill bar with the kick under it, the hats to 16ths, the riser; the fill bar is
    # the FillPreview's, the kick out on beat 4 alone under the Build Fill
    section('Build', 'build', (
        line('kick hats perc bass chords pad', chords=GM_CM, hats='thin', drums='shaker', rise=('perc', 0.6, 1.0),
             ride={STAB: ('ramp', 16, 96)}, end='turn', turn='run', into='uplift'),
        line('kick snare hats perc chords pad', chords=GM_CM, hats=('thin', '16ths'), drums='shaker',
             ride={STAB: ('ramp', 96, 127)}, cue='Riser', end='fill', roll=ROLL))),
    # the impact; Gm9 | Cm9 under the whole riff, the keys out; the keys' call answered by the vox, its 16 bars marked
    # by the sweep (no tom fill inside a drop); the vox out, the dub chords' harmony under the keys' whole riff, the
    # hats busy; the riff and the call together, the drums out in the last bar under the swell
    section('Drop', 'drop', (
        line(DR + ' bass chords pad vox', chords=GM_CM, vox='riff', drums='drop lite', accents='loop',
             downbeat='impact', cue='Vox', end='turn', turn='drop', stab=True),
        line(DR + ' bass chords pad lead vox', chords=GM_CM, lead='call', vox='answer', drums='drop', accents='loop',
             end='turn', turn='drop', into='uplift'),
        line(DR + ' bass chords pad lead', chords=DUB, hats='busy', drums='drop lite', accents='loop', end='turn',
             turn='drop', stab=True),
        line(DR + ' bass chords pad lead vox', chords=GM_CM, lead='call', vox='riff', hats=('full', 'busy'),
             drums='drop', accents='loop', end='drop', into='swell'))),
    # the Rubber riff for a bassline, dry: no pad, the stab held dark under it, the hats busy, a Gm7 organ hit on the
    # 'and' of 4 every 4 bars; then the keys' riff and the stab opening, the riff resting under the fill
    section('Switch-up', 'switch', (
        line(DR + ' chords', phrases=RIFF, hats='busy', drums='lite', ride={STAB: ('hold', 24)},
             hits=(Hit(62, ORGAN, 1), Hit(126, ORGAN, 1)), end='turn'),
        line(DR + ' chords lead', phrases=RIFF, phrases_rest=True, hats='busy', ride={STAB: ('ramp', 24, 110)},
             hits=(Hit(62, ORGAN, 1),), end='fill', into='uplift'))),
    # no kick: the keys arp over Pad Warm from the break's downbeat; then the stab and the off-beat hats rising under
    # the riser, the arp resting under the fill
    section('Break 2', 'break', (
        line('pad', chords=GM_CM, phrases=ARP, accents='loop'),
        line('hats pad chords', chords=GM_CM, phrases=ARP, phrases_rest=True, hats='thin', rise=('hats', 0.4, 1.0),
             cue='Riser', accents='lead-in', end='fill'))),
    # the impact; the Deep line under the arp; the vox riff with the keys' call; the arp again, the stab wide open, the
    # hats to 16ths; then the strip: the pad, the keys and the harmony's movement out, the stab closing into the outro
    section('Drop 2', 'drop2', (
        line(DR + ' bass chords pad', bass='drop2', chords=GM_CM, phrases=ARP, hats='busy', drums='drop lite',
             accents='loop', downbeat='impact', end='turn', turn='drop', stab=True),
        line(DR + ' bass chords pad lead vox', bass='drop2', chords=GM_CM, lead='call', vox='riff', hats='busy',
             drums='drop', accents='loop', cue='Vox', end='turn', turn='drop', into='uplift'),
        line(DR + ' bass chords pad', bass='drop2', chords=GM_CM, phrases=ARP, hats=('busy', '16ths'),
             drums='drop lite', ride={STAB: ('hold', 127)}, accents='loop', end='turn', turn='drop', stab=True),
        line(DR + ' bass chords', bass='drop2', hats='busy', drums='drop', ride={STAB: ('ramp', 127, 0)},
             accents='alternate', end='turn', turn='drop'))),
    # 32 bars to mix out on, not the intro reversed: the busy beat; the clap out on 201's downbeat; the hats back to
    # their own pattern; the congas out, ending on the kick, the hats and the shaker
    section('Outro', 'outro', (
        line(DR, hats='busy', drums='lite', end='turn'),
        line('kick hats perc', hats='busy', end='turn'),
        line('kick hats perc', drums='lite', end='turn'),
        line('kick hats perc', drums='shaker', end='turn', turn='run'))),
)

PARTS = dict(
    drums=dict(
        map={36: 'KickDeep', RIM: 'Rimshot', 38: 'ClapSnare', 41: 'Conga', 42: 'ClosedHatDeep', 43: 'Conga',
             45: 'Conga', 46: 'OpenHatDeep', 47: 'Bongo', 48: 'Bongo', SHAKER: 'ShakerDeep'},
        zones={key: zone for (_, zone), key in HAND.items()},
        lanes={RIM: 'perc'},
        note=('Put the closed and open hats in one choke group: the closed hat cuts the open one. The congas and '
              'bongos are zoned samples (Conga G3 C4 D4, Bongo G4 D5): map each drum note to its zone.'),
        busy=BUSY_HATS,
        turn={'perc': turn_bar()},
        turns={'run': {'perc': turn_bar(congas=False)},                 # the run alone, the shaker under it
               'drop': {'perc': turn_bar(rim=DROP_RIM)}},               # the drops' rim kept through the run
        variants={'lite': {'perc': perc(pair=False)},              # a section's odd lines: the bongo pair out
                  'drop': {'perc': perc(rim=DROP_RIM)},            # the drops' even lines
                  'drop lite': {'perc': perc(pair=False, rim=DROP_RIM)},
                  'shaker': {'perc': drums((), (), (), C.SHAKER, ())}},   # the shaker alone: the ends, the build
    ),
    fill={part: [n for n in FILL_BLOCK if PART.get(n.key, 'perc') == part]
          for part in ('kick', 'snare', 'hats', 'perc')},
    groove={'hats': GROOVE_HATS},
    bass=dict(
        items=('RubberBassClean', 'RubberBassWarm'),
        lines={k: glided(rows) for k, rows in B.LINES.items()},
        pick=B.PICK,
        drop2='Deep',
        note=('Sub Deep layers under either bass (the manifest layers it, no notes). F1 (29) plays the G1 zone pitched '
              'down. A glide inside a note is written as the note, then its target slid into from it.'),
    ),
    lead=dict(item='KeysEP', name='Keys riff', notes=notes(A.KEYS_RIFF)),
    chords=dict(items=('DubStabGm9',), name='Dub stab Gm9, off the kicks',
                trigger=[(s, 55, 1.5, v, False) for s, v in C.STAB],
                voicing=[(s, n, 1.5, v, False) for s, v in C.STAB for n in A.GM9],
                blocks={GM_CM: chord_block(GM_CM_ROWS), DUB: chord_block(DUB_ROWS)}),
    pad=dict(on='Pad Warm', blocks={'main': [(0, 55, 64, 1.0, False)],
                                    GM_CM: [(16 * b, (55, 60)[b % 2], 16, 1.0, False) for b in range(4)],
                                    DUB: [(0, 55, 32, 1.0, False), (32, 60, 16, 1.0, False),
                                          (48, 55, 16, 1.0, False)]}),
    vox=dict(item='VoxSynth', riff=notes(VS.VOX_RIFF)),
    accents=(Key('AccentGlassPingD6'), Key('AccentDubChordCm9'), Key('AccentReverseChordGm9'), Key('AccentOrganHitGm7'),
             Key('AccentNoiseSweepUp'), Key('AccentPluckChordDm7'), Key('AccentZapG4'), Key('AccentVoxSynthGm'),
             Key('AccentRubberStabG1')),
    fx=(Key('BuildFill'), Key('Riser'), Key('Impact')),
    chops=(Key('VoxChopOohAhG4'), Key('VoxChopOohAhC5'), Key('VoxChopAhD5'), Key('VoxChopStackGm7')),
    extras={'Pad Warm': 'PadWarm'},
    frames={'AccentGlassPingD6': 76021, 'AccentDubChordCm9': 109332, 'AccentReverseChordGm9': 105600,
            'AccentOrganHitGm7': 21731, 'AccentNoiseSweepUp': 45000, 'AccentPluckChordDm7': 69172,
            'AccentZapG4': 17407, 'AccentVoxSynthGm': 28594, 'AccentRubberStabG1': 18656, 'BuildFill': 90000,
            'Riser': 720000, 'Impact': 177720, 'VoxChopOohAhG4': 93859, 'VoxChopOohAhC5': 84309,
            'VoxChopAhD5': 88282, 'VoxChopStackGm7': 93951},
    cues=dict(riser='Riser', fill='BuildFill', impact='Impact', swell='AccentReverseChordGm9',
              uplift='AccentNoiseSweepUp', stab='AccentDubChordCm9'),
    loop=dict(
        drums=LOOP_DRUMS,
        lead=[],
        vox=[Note(s + C5.VOX_AT, m, ln, v) for s, m, ln, v in VS.VOX_RIFF],
        accents={v: loop_accents(v) for v in ('vox', 'novox')},
        extras={'Pad Warm': [(0, 55, 64, 1.0, False)]},
    ),
    phrases=[
        dict(name='Rubber riff', file='RubberRiff', on='bass', item='RubberBassClean',
             notes=glided(B.PHRASES['Riff']), bars=2),
        dict(name='Dub chords, Gm9 Gm9 Cm9 Gm9', file='DubChords', on='chords', item='DubStabGm9',
             notes=[(16 * b + s, CHORD_ROOT[chord], 1.5, v, False) for b, chord in enumerate(A.DUB_BARS)
                    for s, v in A.DUB_RHYTHM], bars=4),
        dict(name='Keys arp', file='KeysArp', on='lead', item='KeysEP', notes=notes(A.ARP), bars=4),
        dict(name='Vox synth riff, Gm9 to Cm9', file='VoxSynthRiff', on='vox', item='VoxSynth',
             notes=notes(VS.VOX_RIFF), bars=2),
    ],
    rides=[dict(on='Chords', cc=74, law='log', lo=round(C.STAB_RIDE[0]), hi=round(C.STAB_RIDE[1]), unit='Hz',
                what="the dub stab's low-pass: closed on the loop's downbeat, open at its middle",
                values=stab_ride())],
    recorded={'bass': 'the Rubber and Deep lines (`Loops/Line Rubber Clean.wav` and the rest), as the loop plays them',
              'vox': 'the Phrase Vox Riff (`Loops/Phrase Vox Riff.wav`), its dub throw baked in'},
    form=FORM,
)
