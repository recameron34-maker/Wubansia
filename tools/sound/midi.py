"""MIDI v2: every kit's written music as Standard MIDI Files, the score Wubansia plays and drags out, right on its own
in any sampler or DAW (ADR 0019, docs/midi-map.md).

    .venv/bin/python tools/sound/midi.py --kit FUT02 [--out content/midi/fut02]

A kit opts in with a `midi` module in its recipe package that defines PARTS (tools/sound/README.md, "MIDI", has the
whole schema). This module is the only writer: it reads PARTS and the kit's Items, and writes on the one channel map,
tools/sound/channels.py.

What it writes (content/midi/<kit>/):
- <KIT>_Loop_Vox_4bars.mid and <KIT>_Loop_NoVox_4bars.mid: the kit's round 5 context loop as its club master plays it,
  lane by lane (PARTS['loop']): drums, bass, lead, chords and their voicing, the vox synth (vox file only), the
  variant's accents, the loop's FX, the kit's extras, the filter rides and the throws;
- the parts: <KIT>_Drums_4bars, <KIT>_Bassline_<Name>[_InTheLoop]_2bars, <KIT>_Lead_<Sample>_2bars,
  <KIT>_Chords_<Sample>_4bars (channel 4, at the roots) and <KIT>_Chords_<Sample>_Voicing_4bars (channel 3),
  <KIT>_Accents_Vox_4bars, <KIT>_Accents_NoVox_4bars, <KIT>_FX_4bars, and the synth phrases <KIT>_Phrase_<Name>_<n>bars
  on their instruments' channels;
- extended/vox/ and extended/novox/: the 224-bar extended mix in the kit's genre's form (FORMS, or the kit's own
  PARTS['form']), one region file per section, an integer-only regions.json and a README;
- README.md: what every file plays, on which channel, rooted where, and which lanes Wubansia plays from recorded
  phrases.

Conventions (docs/midi-map.md): format 1, 960 ticks a quarter, 4/4, the kit's tempo (the integer microseconds nearest
its BPM) and key (FF 59) on the conductor track. Odd 16ths land round((swing - 0.5) x 480) ticks late. On a tick,
note-offs come first, then controllers, then note-ons; no note overlaps its own key; tracks end on a bar line. Gates
are note lengths, and swing never runs a note into the next one on its track unless it was written to: where a note
slides (a written glide), the note before is held SLIDE_TICKS into it, so a mono synth set to legato glides there.
Velocities are round(v x 127), moved to the nearest integer that plays the recipe's layer where that would not
(written_velocity: the engine plays the layer whose round(layer x 127) is nearest, the louder on a tie). End-aligned
sounds end on their target, starting the sample's length (PARTS['frames']) before it. Filter rides are CC74/CC71 every
CC_STEP ticks while they move; a thrown note has CC91 and CC94 at 127 on its tick and back to 0 at its end.

The extended mix is built line by line (8 bars each) from the kit's material: each lane plays the loop's 4-bar block,
its fill bar where a line ends on a fill and its groove block (by default bar 2 again in place of bar 4) elsewhere;
line() lists what a line can do (lanes, hats, the bassline, the lead's mode, chord blocks, a pad, phrase lanes, vox
modes, accent and FX placement, rolls and rises, turnarounds, rides). The genres' forms (FORMS) are the defaults; a kit
declares its own arrangement as data, PARTS['form'], built from section() and line().
"""
import argparse
import bisect
import importlib
import json
import math
import re
import struct
import sys
from pathlib import Path
from typing import NamedTuple

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sound import channels as CH  # noqa: E402
from tools.sound import kits  # noqa: E402
from tools.sound import listen  # noqa: E402

PPQ = 960
S16 = PPQ // 4                      # ticks a 16th
BAR = 16                            # 16ths a bar
BLOCK = 4                           # bars of the loop: every lane's block
LINE = 8                            # bars of a line: every section, and every change in one, starts on a line
SLIDE_TICKS = 12                    # a note glided from is held this far into the note it slides to
CC_STEP = 60                        # ticks between a ride's points
DRUM_TICKS = S16 // 2               # a drum note: a short trigger, the sample rings out
VARIANTS = ('vox', 'novox')
VARIANT_NAME = {'vox': 'Vox', 'novox': 'NoVox'}
EXTENDED = 'extended'
MARKER, CUE_POINT = 0x06, 0x07      # meta events: a section's name, a cue (a label for the reader: the notes play)
CUTOFF, RESONANCE, SPACE, ECHO = 74, 71, 91, 94
NOTE_NAMES = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
DRUM_PARTS = ('kick', 'rumble', 'snare', 'hats', 'perc')   # drum lanes as the arrangement plays them
OPEN_HAT, CLOSED_HAT = 46, 42


def note_name(n):
    """MIDI note to name with C1 = 36 (the drum-rack convention: C3 = 60)."""
    return f'{NOTE_NAMES[n % 12]}{n // 12 - 2}'


def sample_name(n):
    """MIDI note to name with C4 = 60, as the kits' sample names and DNA write notes."""
    return f'{NOTE_NAMES[n % 12]}{n // 12 - 1}'


def words(name):
    """An Item's CamelCase name in words, as its WAV is named: 'DubStabGm9' -> 'Dub Stab Gm9'."""
    return ' '.join(listen.note(w) for w in listen.WORD.findall(name))


def nearest_layer(layers, v):
    """The layer a velocity picks (0-based): the nearest, the louder on a tie, as phrase.hits_lane picks it."""
    return min(range(len(layers)), key=lambda i: (abs(layers[i] - v), -i))


def velocity_layer(layers, c):
    """The layer Wubansia's sampler plays for a MIDI velocity c (the engine's integer rule, 0-based): the one whose
    round(layer x 127), half up, is nearest c, the louder on a tie."""
    return min(range(len(layers)), key=lambda i: (abs(c - math.floor(layers[i] * 127 + 0.5)), -i))


def written_velocity(layers, v, where=''):
    """A recipe's velocity 0-1 as MIDI's 1-127: round(v x 127), unless the engine's integer rule (velocity_layer) or
    the float rule on velocity / 127 would play another of the layers than the recipe picks from v; then the integer
    nearest v x 127 (the lower on a tie) that both play on the recipe's layer. FUT01's 0.9 kicks, halfway between the
    0.8 and 1.0 layers (the recipe plays the louder): 115, where round() gives 114, the 0.8 layer's; FUT01's and
    FUT03's 0.3 ghost claps (the recipe plays 0.2): 37, where 38 is as near 25 as 51 and the engine takes 51's."""
    vel = max(1, min(127, round(v * 127)))
    if not layers:
        return vel
    want = nearest_layer(layers, v)
    ok = [c for c in range(1, 128) if velocity_layer(layers, c) == want and nearest_layer(layers, c / 127) == want]
    if not ok:
        raise ValueError(f'{where}: no velocity picks layer {want + 1} of {layers}')
    return vel if vel in ok else min(ok, key=lambda c: (abs(c - v * 127), c))


# ------------------------------------------------------------------ what a kit writes
class Note(NamedTuple):
    """A note as a kit writes it on its 16th grid. step: the 16th it starts on (fractions for ratchets), or where it
    ends when `end`; key; length in 16ths; velocity 0-1; slide: glided into from the note before. ticks: a length in
    ticks instead (a sample's own length); end: end-aligned on `step`; throw: CC91/CC94 around it. PARTS may write the
    first five as a plain tuple, (16th, key, length, velocity, slide)."""
    step: float
    key: int
    length: float
    vel: float
    slide: bool = False
    ticks: int = 0
    end: bool = False
    throw: bool = False


def as_notes(rows):
    return [r if isinstance(r, Note) else Note(*r) for r in rows]


def shifted(notes, by):
    return [n._replace(step=n.step + by) for n in notes]


def window(notes, lo, hi):
    """The notes starting in [lo, hi) 16ths, shifted to start from lo."""
    return [n._replace(step=n.step - lo) for n in notes if lo <= n.step < hi]


class Key(NamedTuple):
    """A sound on a one-key channel (accents, FX, chops): its Item, the zone it plays (a zoned Item's note, else None),
    the 16th of its file it starts from (FUT01's glitch fill from its beat 2: 4) and its name (default: the Item's;
    hits and cues name it)."""
    item: str
    zone: int = None
    start: float = 0
    name: str = None

    @property
    def label(self):
        return self.name or self.item


class Hit(NamedTuple):
    """A one-shot placed on a one-key channel: the 16th it starts on (align 'on') or ends on ('end'); the sound (a
    Key's name); the layer the recipe plays (1-based: the velocity is round(layer value x 127)) or a velocity 0-1; a
    gate in 16ths (None: the sample's length); thrown (CC91/CC94)."""
    step: float
    sound: str
    layer: int = None
    align: str = 'on'
    gate: float = None
    throw: bool = False
    vel: float = None


def hits(pairs, key, length=0.5):
    """(16th, velocity) pairs as drum notes: short triggers."""
    return [Note(s, key, length, v) for s, v in pairs]


def per_bar(bar_hits, key, bars=4, extra=None, drop=None, length=0.5):
    """One bar's (16th, velocity) hits repeated over `bars`, with per-bar extra hits and dropped 16ths."""
    out = []
    for b in range(bars):
        for s, v in list(bar_hits) + list((extra or {}).get(b, ())):
            if s not in (drop or {}).get(b, ()):
                out.append(Note(16 * b + s, key, length, v))
    return out


def twice(notes, period=2 * BAR):
    """A 2-bar part twice: the loop's 4 bars."""
    notes = as_notes(notes)
    return notes + shifted(notes, period)


def swapped(pattern, fill_bar=BLOCK, swap=2):
    """A 4-bar block where no fill is due: bar `swap` again in place of bar `fill_bar`, so the groove runs on."""
    pattern = as_notes(pattern)
    lo, src = BAR * (fill_bar - 1), BAR * (swap - 1)
    keep = [n for n in pattern if not lo <= n.step < lo + BAR]
    return sorted(keep + shifted([n for n in pattern if src <= n.step < src + BAR], lo - src))


def drum_lane(note):
    """A drum note's lane by its General MIDI number: 35-36 kick; 37-40 snare and clap; 42, 44 and 46 the hats, with the
    rides (51, 59); anything else perc."""
    if note in (35, 36):
        return 'kick'
    if 37 <= note <= 40:
        return 'snare'
    if note in (42, 44, 46, 51, 59):
        return 'hats'
    return 'perc'


# ------------------------------------------------------------------ the extended mix's vocabulary
class Line(dict):
    """One 8-bar line (a dict, for regions.json): see line()."""


def line(lanes, **opt):
    """One 8-bar line of a section: its parts, space-separated, and how they play. Parts: the drum lanes (kick,
    rumble, snare, hats, perc), bass, lead, chords (the chord sample and its voicing), pad, vox. Options, each with a
    default:
    - hats: 'full' (as written), 'thin' (the off-beat 8ths), 'busy' (the kit's own busy pattern, PARTS['drums']['busy'],
      else a closed hat of at least 0.35 on every empty 16th) or '16ths' (the closed hat on every 16th); a pair gives
      each 4-bar block its own;
    - drums: a drum variant, PARTS['drums']['variants'][name] ({lane: 1- or 4-bar notes}, and optionally 'fill':
      {lane: a bar of notes}), in place of the loop's lanes it names. On a line that ends on a fill, a variant lane
      plays its bars 1-3, then the variant's own fill bar for that lane if it gives one, else the kit's fill bar for
      that lane (which leaves room for the kit's fill); nothing is cut, except under the line's own fill (fill=);
    - bass: a line of PARTS['bass']['lines'], or 'main' (the pick, the default), 'switch' or 'drop2' (the kit's picks
      for those sections, PARTS['bass']['switch'] and ['drop2']);
    - lead: 'phrase' (the default), 'call' (its first bar), 'answer' (its second bar), 'skeleton'
      (PARTS['lead']['skeleton']); on a fill line the call, the answer and the skeleton rest from the fill bar where
      the loop's own lead rests there (FUT02's arp);
    - chords: a chord block, PARTS['chords']['blocks'] (the default 'main', the loop's): harmony that moves; the pad
      plays its own block of that name (PARTS['pad']['blocks']), else its main one;
    - vox: 'off' (the default), 'loop' (the loop's 4-bar vox pattern), 'riff' (the riff every 2 bars), 'call' (its
      first bar), 'answer' (its second bar); throw=True adds the riff's last note on the line's last 'and' of 4,
      thrown (CC91/CC94);
    - phrases: names of PARTS['phrases'] that play over the line, each on its instrument's channel in place of that
      channel's part (a phrase on the vox synth's channel plays in the vox mix only, in place of the line's vox mode);
      phrases_rest=True: on a line that ends on a fill, its phrases rest from the kit's fill start, a note held over
      it cut there (else they play on under the fill, as FUT01's lead and bass play under its glitch fill);
    - accents: 'loop' (the variant's loop accents in every 4-bar block), 'alternate' (in the line's second block),
      'lead-in' (only the loop's end-aligned accents that end on a block's end, at the line's end, in a section's last
      line) or None (the default); stab: the 'stab' cue's sound on the 'and' of 4 of the line's last bar; into: a cue
      ('swell', 'uplift') end-aligned on the line's end; downbeat: a cue ('impact') on the line's first beat;
    - roll: a build roll, dict(lane, bars, every=((bars, 16ths), ...), last=16ths, ramp=(from, to)): the lane's own
      note every so many 16ths over the `bars` bars before the line's last bar, then every `last` 16ths in the last
      bar up to the fill's start, its velocity rising hit by hit from `from` to `to` times its loudest written
      velocity; rest='all' (the default): every drum rests from the fill's start; rest='lane': the rolled lane alone,
      the others playing the kit's fill bar under the fill (FUT03's one-bar Build Fill);
    - rise: (lane, from, to): the lane's velocities scaled bar by bar from `from` to `to`;
    - fill: on a line that ends on a fill, a sound (channel 6 or 7) end-aligned on the line's end in place of the
      kit's fill: every drum lane (busy hats and a roll included) rests from its start (FUT01's build: the whole glitch
      fill from beat 3, where the loop plays its beat 4 only);
    - end: 'fill' (the last bar is the loop's fill bar, with the kit's fill: a Fill cue), 'turn' (a turnaround in the
      last bar: PARTS['drums']['turn'], else a pickup roll on the last beat of the snare, the perc or the hats, the
      first with room for it) or 'drop' (the drums sit out the last bar); turn: a named turnaround,
      PARTS['drums']['turns'][name], in place of PARTS['drums']['turn'];
    - hits: one-shots placed by hand on channels 6 and 7, Hits on whole 16ths from the line's start (or end-aligned
      on them), a tuple for both variants or {'vox': hits, 'novox': hits};
    - cue: 'Riser' (the kit's riser, end-aligned on the line's end: put it on a section's last line) or 'Vox' (a
      label: the vox notes play);
    - ride: {track: 'loop' (the default) | ('hold', value) | ('ramp', from, to) | ('points', ((bar, value), ...))}:
      the filter rides on the track (every one of its controllers): the loop's curve, held, ramped over the line, or
      breakpoints on whole bars 0-8 joined linearly, held before the first and after the last."""
    return Line(lanes=tuple(lanes.split()), **opt)


def section(name, kind, lines):
    """A section: its name, its kind (intro, groove, break, build, drop, switch, drop2, outro) and its lines."""
    return (name, kind, tuple(lines))


DRUMS = 'kick snare hats perc'
DRUMS_BASS = DRUMS + ' bass'
GROOVE = DRUMS_BASS + ' chords'
EVERYTHING = GROOVE + ' pad lead vox'


def intro(first='kick', more='kick hats perc'):
    """32 bars of drums to mix in on (no fills: the kits' fills are tonal): the kick (or the given lanes) alone, then
    more, then the whole beat, a turnaround every 16 bars."""
    return section('Intro', 'intro', (
        line(first),
        line(more, hats='thin', end='turn'),
        line(DRUMS),
        line(DRUMS, end='turn')))


def outro(last='kick hats'):
    """32 bars of drums to mix out on (no fills), ending on the kick and the hats."""
    return section('Outro', 'outro', (
        line(DRUMS),
        line(DRUMS, end='turn'),
        line('kick hats perc', hats='thin'),
        line(last)))


def groove(bass='main'):
    """32 bars: the bass at 33, the chords at 41, the lead's call at 49, the vox's call at 57 with a throw into the
    break; accents in every other block once the harmony is in."""
    return section('Groove', 'groove', (
        line(DRUMS_BASS, bass=bass, end='turn'),
        line(GROOVE, bass=bass, accents='alternate', end='fill', into='uplift'),
        line(GROOVE + ' lead', bass=bass, lead='call', accents='alternate', end='turn', stab=True),
        line(GROOVE + ' lead vox', bass=bass, lead='call', vox='call', throw=True, accents='alternate',
             cue='Vox', end='fill', into='swell')))


def breakdown(name='Break', last_cue='Vox'):
    """16 bars, no kick: the pad and the chords with the vox's riff, the lead's call; the hats rise into the fill (or,
    before drop 2, the riser)."""
    return section(name, 'break', (
        line('pad chords lead', lead='call', vox='riff', cue='Vox'),
        line('hats pad chords lead', hats='thin', rise=('hats', 0.4, 1.0), vox='riff', accents='lead-in',
             cue=last_cue, end='fill')))


def drop(name='Drop', kind='drop', bass='main', hats='full'):
    """32 bars, everything: the vox's loop pattern, the accents in every block, an impact on the first beat; line 3
    trades the vox for busier hats; the drums sit out the last bar."""
    return section(name, kind, (
        line(EVERYTHING, bass=bass, hats=hats, vox='loop', accents='loop', downbeat='impact', cue='Vox', end='turn',
             stab=True),
        line(EVERYTHING, bass=bass, hats=hats, vox='loop', accents='loop', end='fill', into='uplift'),
        line(EVERYTHING, bass=bass, hats='busy', vox='off', accents='loop', end='turn', stab=True),
        line(EVERYTHING, bass=bass, hats=hats, vox='loop', accents='loop', cue='Vox', end='drop', into='swell')))


def switch_up():
    """16 bars on the switch-up's bassline, the hats busy; the chords back for the second line."""
    return section('Switch-up', 'switch', (
        line(DRUMS_BASS, bass='switch', hats='busy', accents='loop', end='turn', stab=True),
        line(GROOVE, bass='switch', hats='busy', accents='loop', end='fill')))


def drop_2():
    """32 bars on drop 2's bassline, the hats busy, the vox's riff, its answer; the last line strips into the outro."""
    return section('Drop 2', 'drop2', (
        line(EVERYTHING, bass='drop2', hats='busy', vox='loop', accents='loop', downbeat='impact', cue='Vox',
             end='turn', stab=True),
        line(EVERYTHING, bass='drop2', hats='busy', vox='loop', accents='loop', end='fill', into='uplift'),
        line(EVERYTHING, bass='drop2', hats='busy', vox='answer', accents='loop', end='turn', stab=True),
        line(GROOVE, bass='drop2', accents='alternate', end='fill')))


def build_club():
    """Sci-fi club: the drums back but thinned, then the snare or clap rolling for 8 bars (quarters, 8ths, then 16ths)
    up to the fill under the riser, the kick out."""
    return section('Build', 'build', (
        line('kick snare hats bass chords', hats='thin', end='turn'),
        line('snare hats chords', hats='thin', cue='Riser', end='fill',
             roll=dict(lane='snare', bars=7, every=((4, 4), (2, 2), (1, 1)), ramp=(0.55, 1.0)))))


def build_house():
    """House: the groove thinned, then a 4-bar roll with the kick playing on under it into the fill."""
    return section('Build', 'build', (
        line('kick snare hats perc bass chords', hats='thin', end='turn'),
        line('kick snare hats chords', hats='thin', cue='Riser', end='fill',
             roll=dict(lane='snare', bars=3, every=((1, 4), (1, 2), (1, 1)), ramp=(0.5, 1.0)))))


def build_techno():
    """Techno: the kick and the rumble on, the hats thin then full, then 16ths, a snare roll in the bar before the
    fill, the fill alone in its bar."""
    return section('Build', 'build', (
        line('kick rumble hats perc bass', hats=('thin', 'full'), end='turn'),
        line('kick rumble hats perc bass', hats='16ths', cue='Riser', end='fill',
             roll=dict(lane='snare', bars=1, every=((1, 1),), ramp=(0.35, 1.0)))))


def build_ukg():
    """UK garage: the 2-step beat on, the bass out, the harmony thinning; then a 2-bar clap roll over the shuffle (8ths,
    then 16ths) up to the fill, the kick out from its start."""
    return section('Build', 'build', (
        line('kick snare hats perc chords', hats='thin', end='turn'),
        line('kick hats perc', cue='Riser', end='fill',
             roll=dict(lane='snare', bars=1, every=((1, 2),), last=1, ramp=(0.45, 1.0)))))


CLUB_FORM = (intro(), groove(), breakdown(), build_club(), drop(), switch_up(), breakdown('Break 2', 'Riser'),
             drop_2(), outro())
HOUSE_FORM = (intro(), groove(), breakdown(), build_house(), drop(), switch_up(), breakdown('Break 2', 'Riser'),
              drop_2(), outro())
UKG_FORM = (intro('kick hats perc', 'kick hats perc'), groove(), breakdown(), build_ukg(), drop(), switch_up(),
            breakdown('Break 2', 'Riser'), drop_2(), outro('kick hats perc'))
TECHNO_FORM = (
    intro('kick', 'kick rumble hats perc'),
    section('Groove', 'groove', (
        line(DRUMS + ' rumble bass', end='turn'),
        line(DRUMS + ' rumble bass chords', accents='alternate', end='fill', into='uplift'),
        line(DRUMS + ' rumble bass chords lead', lead='call', accents='alternate', end='turn', stab=True),
        line(DRUMS + ' rumble bass chords lead vox', lead='call', vox='call', cue='Vox', accents='alternate',
             end='fill'),
        line(DRUMS + ' rumble bass chords lead', hats='busy', accents='alternate', end='turn', stab=True),
        line(DRUMS + ' rumble bass chords lead', hats='busy', accents='alternate', end='fill', into='uplift'),
        line(DRUMS + ' rumble bass chords lead vox', lead='call', vox='riff', cue='Vox', accents='alternate',
             end='turn', stab=True),
        line(DRUMS + ' rumble bass chords lead', accents='alternate', end='fill', into='swell'))),
    breakdown(),
    build_techno(),
    drop('Drop', 'drop'),
    drop_2(),
    outro())
for _sec in TECHNO_FORM[4:6]:     # techno's drops keep the rumble under the kick
    for _ln in _sec[2]:
        if 'kick' in _ln['lanes']:
            _ln['lanes'] += ('rumble',)
FORMS = {'default': CLUB_FORM, 'hyper-clean future house': HOUSE_FORM, 'dark industrial / techno': TECHNO_FORM,
         'future garage / 2-step': UKG_FORM, 'garage-house / deep tech house': HOUSE_FORM}
ARRANGEMENT = CLUB_FORM


def arrangement(K, P=None):
    """The kit's form: its own (PARTS['form']), else its genre's (FORMS, by K.DIRECTION), else the default."""
    if P is not None and P.get('form'):
        return P['form']
    return FORMS.get(getattr(K, 'DIRECTION', None), FORMS['default'])


def arrangement_bars(form=ARRANGEMENT):
    return LINE * sum(len(lines) for _, _, lines in form)


def length(bars, bpm):
    """m:ss of `bars` in 4/4 at `bpm`, to the nearest second."""
    m, s = divmod(round(bars * 240 / bpm), 60)
    return f'{m}:{s:02d}'


# ------------------------------------------------------------------ Standard MIDI File
def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def meta(kind, data):
    """A meta event: its type and its body (bytes, or text)."""
    data = data.encode() if isinstance(data, str) else bytes(data)
    return bytes([0xFF, kind]) + vlq(len(data)) + data


def tempo_us(bpm):
    """The tempo meta's microseconds a quarter: the integer nearest the kit's BPM (docs/midi-map.md: the residual)."""
    return round(60_000_000 / bpm)


MAJOR_SF = {'C': 0, 'G': 1, 'D': 2, 'A': 3, 'E': 4, 'B': 5, 'F#': 6, 'C#': 7, 'F': -1, 'Bb': -2, 'Eb': -3, 'Ab': -4,
            'Db': -5, 'Gb': -6, 'Cb': -7}
MINOR_SF = {'A': 0, 'E': 1, 'B': 2, 'F#': 3, 'C#': 4, 'G#': 5, 'D#': 6, 'A#': 7, 'D': -1, 'G': -2, 'C': -3, 'F': -4,
            'Bb': -5, 'Eb': -6, 'Ab': -7}


def key_signature(key):
    """A key ('F# minor') as the key signature meta's (sharps or -flats, 0 major or 1 minor)."""
    m = re.fullmatch(r'([A-G][#b]?) (minor|major)', key)
    if not m:
        raise ValueError(f'not a key: {key!r}')
    table = MINOR_SF if m[2] == 'minor' else MAJOR_SF
    return table[m[1]], int(m[2] == 'minor')


def track_chunk(name, events, end):
    """A track chunk: its name, then events (tick, order, message) sorted by tick, then order (0 note-offs, 1
    controllers, 2 note-ons; the conductor's metas in their own order), then message; the end of track on `end`."""
    data = b'\x00' + meta(0x03, name)
    t = 0
    for tick, _, msg in sorted(events):
        if tick > end:
            raise ValueError(f'{name}: an event at {tick}, past the end {end}')
        data += vlq(tick - t) + msg
        t = tick
    data += vlq(end - t) + b'\xff\x2f\x00'
    return b'MTrk' + struct.pack('>I', len(data)) + data


def smf(conductor, tracks, end):
    """A format 1 file: the conductor (name, events) and the tracks (name, events), every track ending on `end`."""
    chunks = [track_chunk(conductor[0], conductor[1], end)] + [track_chunk(n, ev, end) for n, ev in tracks]
    return b'MThd' + struct.pack('>IHHH', 6, 1, len(chunks), PPQ) + b''.join(chunks)


# ------------------------------------------------------------------ a kit
class Kit:
    """A kit's MIDI material: its PARTS checked and resolved against its Items, on the channel map. items: {name:
    Item} (default: every Item of the kit's recipe package)."""

    def __init__(self, kit, K, P, items=None):
        self.kit, self.K, self.P = kit, K, P
        self.items = items if items is not None else {it.name: it for it in kits.items(K)}
        self.late = round((K.SWING - 0.5) * 2 * S16)
        d = P['drums']
        self.drum_map = dict(d['map'])
        self.drum_zone = dict(d.get('zones', {}))
        self.drum_part = {n: drum_lane(n) for n in self.drum_map}
        self.drum_part.update(d.get('lanes', {}))
        self.keys = {role: {CH.KEY_BASE[role] + i: key for i, key in enumerate(P.get(role, ()))}
                     for role in ('accents', 'fx', 'chops')}
        self.sounds = {}
        for role, keys in self.keys.items():
            for k, key in keys.items():
                if key.label in self.sounds:
                    raise ValueError(f'{kit}: two sounds called {key.label!r}')
                self.sounds[key.label] = (role, k)
        self.extras = {name: (ch, lane) for ch, name, lane in CH.extras(kit)}
        for name in P.get('extras', {}):
            if name not in self.extras:
                raise ValueError(f'{kit}: PARTS names an extra {name!r} that channels.EXTRAS does not give it')
        self.phrases = [dict(ph) for ph in P.get('phrases', ())]
        self.rides = list(P.get('rides', ()))
        self._check()

    # ---------------------------------------------------------- the instruments
    def item(self, name):
        if name not in self.items:
            raise ValueError(f'{self.kit}: no Item called {name!r}')
        return self.items[name]

    def channel_items(self):
        """{channel: (item names)} of the pitched channels (A first where there are two)."""
        P = self.P
        out = {CH.CHANNELS['bass']: tuple(P['bass']['items']), CH.CHANNELS['lead']: (P['lead']['item'],),
               CH.CHANNELS['chords']: tuple(P['chords']['items'])}
        if P.get('vox'):
            out[CH.CHANNELS['vox']] = (P['vox']['item'],)
        for name, items in P.get('extras', {}).items():
            out[self.extras[name][0]] = (items,) if isinstance(items, str) else tuple(items)
        return out

    def pitched_channels(self):
        return sorted(self.channel_items())

    def items_on(self, ch):
        return self.channel_items()[ch]

    def zones(self, ch):
        """A pitched channel's zones: its Items' notes, or their roots."""
        out = set()
        for name in self.items_on(ch):
            it = self.item(name)
            out |= set(it.notes) if it.notes else {it.root}
        return sorted(out)

    def keymap(self, role):
        return self.keys[role]

    def key_of(self, role, item, zone=None, start=0):
        """The key a sound plays on, on a one-key channel."""
        for k, key in self.keys[role].items():
            if (key.item, key.zone, key.start) == (item, zone, start):
                return k
        raise KeyError(f'{self.kit}: no {role} key for {item} {zone} from {start}')

    def sound(self, name):
        """A sound by its name: (role, key, Key)."""
        if name not in self.sounds:
            raise ValueError(f'{self.kit}: no sound called {name!r} on channels 6-8')
        role, k = self.sounds[name]
        return role, k, self.keys[role][k]

    def instrument(self, ch, key):
        """The Item a note plays."""
        if ch == CH.CHANNELS['drums']:
            return self.item(self.drum_map[key])
        if ch in (6, 7, 8):
            role = {6: 'accents', 7: 'fx', 8: 'chops'}[ch]
            return self.item(self.keys[role][key].item)
        if ch == CH.CHANNELS['voicing']:
            return None
        return self.item(self.items_on(ch)[0])

    def layers(self, ch, key):
        it = self.instrument(ch, key)
        return None if it is None else it.layers

    def end_aligned(self, ch, key):
        it = self.instrument(ch, key)
        return it is not None and ch in (6, 7) and 'cut_end' in it.flags

    def velocity(self, ch, key, v):
        """The MIDI velocity of the recipe's velocity v on the note's instrument: written_velocity(), which plays the
        recipe's layer under the engine's integer rule and the float rule alike."""
        return written_velocity(self.layers(ch, key), v, f'{self.kit}: channel {ch} key {key}')

    def frames(self, item, zone=None):
        """A one-shot's length in samples at 48 kHz (PARTS['frames'], 'Item' or 'Item@zone')."""
        f = self.P.get('frames', {})
        name = f'{item}@{zone}' if zone is not None and f'{item}@{zone}' in f else item
        if name not in f:
            raise ValueError(f'{self.kit}: PARTS["frames"] has no length for {name}')
        return f[name]

    def sound_ticks(self, key):
        """A Key's sample in ticks at the kit's tempo, from its start."""
        return round(self.frames(key.item, key.zone) * self.K.BPM / 3000) - round(key.start * S16)

    def wav(self, name):
        """The WAV a sample length is read from, under content/samples/<kit>/: 'Item' (its loudest layer) or
        'Item@zone'."""
        item, _, zone = name.partition('@')
        it = self.item(item)
        parts = [words(item)] + ([sample_name(int(zone))] if zone else [])
        parts += [f'v{len(it.layers)}'] * (len(it.layers) > 1) + ['rr1'] * (it.rr > 1)
        return f'{listen.FOLDERS[it.group]}/{" ".join(parts)}.wav'

    def snare_name(self):
        names = [self.drum_map[n].lower() for n in sorted(self.drum_map) if self.drum_part[n] == 'snare']
        if names and all('clap' in s for s in names):
            return 'Clap'
        if names and all('snare' in s for s in names):
            return 'Snare'
        return 'Snare/Clap'

    def part_track(self, part):
        """The track a drum part plays on."""
        return {'kick': 'Kick', 'rumble': 'Rumble', 'snare': self.snare_name(), 'hats': 'Hats', 'perc': 'Perc'}[part]

    def tracks(self):
        """Every track the kit's files may carry, in file order: by lane (CH.LANES), each lane's own tracks, then the
        extras on it."""
        own = {'kick': ['Kick', 'Rumble'], 'snare': [self.snare_name()], 'hats': ['Hats'], 'perc': ['Perc'],
               'bass': ['Bass'], 'lead': ['Lead'], 'chords': ['Chords', 'Chord voicing'], 'vox': ['Vox', 'Vox chops'],
               'accents': ['Accents'], 'fx': ['FX']}
        out = []
        for lane in CH.LANES:
            out += own[lane] + [n for ch, n, ln in CH.extras(self.kit) if ln == lane]
        return out

    def channel(self, track):
        return CH.track_channel(self.kit, track)

    # ---------------------------------------------------------- checks
    def _check(self):
        P = self.P
        for n, name in self.drum_map.items():
            if not CH.DRUM_RANGE[0] <= n <= CH.DRUM_RANGE[1]:
                raise ValueError(f'{self.kit}: drum note {n} is outside {CH.DRUM_RANGE}')
            self.item(name)
        for ch, names in self.channel_items().items():
            layers = {self.item(n).layers for n in names}
            if len(layers) > 1:
                raise ValueError(f'{self.kit}: channel {ch} pairs Items with different layers: {names}')
        if P['bass']['pick'] not in P['bass']['lines']:
            raise ValueError(f'{self.kit}: the pick {P["bass"]["pick"]!r} is not one of the lines')
        for role, keys in self.keys.items():
            for key in keys.values():
                self.item(key.item)

    # ---------------------------------------------------------- timing
    def tick(self, step):
        """A 16th as a tick: odd 16ths late by the swing; a fraction of a 16th straight from its 16th."""
        base = math.floor(step)
        return base * S16 + (self.late if base % 2 else 0) + round((step - base) * S16)

    def rows(self, notes):
        """Notes as rows [on, off, key, vel, slide, written on, written off, throw] in ticks."""
        out = []
        for n in as_notes(notes):
            t = self.tick(n.step)
            dur = n.ticks or max(1, round(n.length * S16))
            if n.end:
                out.append([t - dur, t, n.key, n.vel, n.slide, t - dur, t, n.throw])
            else:
                w = round(n.step * S16)
                out.append([t, t + dur, n.key, n.vel, n.slide, w, w + dur, n.throw])
        return out

    def gated(self, notes, end, mono=False):
        """A track's notes as rows (see rows()) with its gates: a note never runs into a later note it didn't overlap
        as written (the swing's delay), nor on a mono line (a bass or vox track) into any later note; a note slid into
        from another pitch holds the note before SLIDE_TICKS into it where that note runs up to it (stops at most a
        16th before it); no note overlaps its own key; nothing runs past `end`."""
        rows = sorted(self.rows(notes), key=lambda r: (r[0], r[2]))
        starts = sorted({r[0] for r in rows})
        at = {}
        for r in rows:
            at.setdefault(r[0], []).append(r)
        for r in rows:
            later = next((s for s in starts if s > r[0]), None)
            if later is not None and r[1] > later and (mono or all(n[5] >= r[6] for n in at[later])):
                r[1] = later
        for before, slid in zip(rows, rows[1:]):
            if slid[4] and before[2] != slid[2] and before[0] < slid[0] and before[1] >= slid[0] - S16:
                before[1] = max(before[1], slid[0] + SLIDE_TICKS)
        out = []
        by_key = {}
        for r in rows:
            by_key.setdefault(r[2], []).append(r)
        for key_rows in by_key.values():
            for a, b in zip(key_rows, key_rows[1:]):
                if b[0] < a[1]:
                    a[1] = b[0]
        for r in rows:
            r[1] = min(r[1], end)
            if r[0] < end and r[1] > r[0]:
                out.append(r)
        return sorted(out, key=lambda r: (r[0], r[2]))

    def timed(self, notes, end, mono=False):
        """(on, off, key, velocity 0-1) of notes on one track, gated (gated())."""
        return [(r[0], r[1], r[2], r[3]) for r in self.gated(notes, end, mono)]

    def events(self, track, notes, ccs, end):
        """A track's MIDI events (tick, order, message): its gated notes, their throws, and its controllers ((tick,
        controller, value), on the 60-tick grid). A throw's CC91 and CC94 go to 127 on its note's tick and back to 0 at
        its note-off or at the track's next note-on that isn't thrown (on its tick, before it), whichever comes first:
        Wubansia latches the sends at each note-on, so a throw held over a later note would throw that note too (FUT04's
        no-vox metal chord over the accents after it). A note on a thrown note's tick is thrown with it."""
        ch = self.channel(track)
        out = []
        mono = CH.track_lane(self.kit, track) in ('bass', 'vox') and track != 'Vox chops'
        rows = self.gated(notes, end, mono)
        thrown = {r[0] for r in rows if r[7]}
        plain = sorted({r[0] for r in rows if not r[7]} - thrown)       # the ticks of the notes that aren't thrown
        for on, off, key, v, _, _, _, throw in rows:
            vel = self.velocity(ch, key, v)
            if ch == CH.CHANNELS['drums'] and key not in self.drum_map:
                raise ValueError(f'{self.kit}: drum note {key} on {track} is not on the drum map')
            out.append((on, 2, bytes([CH.status(0x90, ch), key, vel])))
            out.append((off, 0, bytes([CH.status(0x80, ch), key, 0])))
            if throw:
                i = bisect.bisect_right(plain, on)
                back = min(off, plain[i]) if i < len(plain) else off
                for cc in (SPACE, ECHO):
                    out.append((on, 1, bytes([CH.status(0xB0, ch), cc, 127])))
                    out.append((back, 1, bytes([CH.status(0xB0, ch), cc, 0])))
        for tick, cc, value in ccs:
            if not 0 <= value <= 127 or tick > end:
                raise ValueError(f'{self.kit}: controller {cc} = {value} at {tick} on {track}')
            out.append((tick, 1, bytes([CH.status(0xB0, ch), cc, value])))
        return out

    def title(self, what=''):
        K = self.K
        return f'{self.kit} {K.TITLE}, {K.BPM:g} BPM, {K.KEY}' + (f', {what}' if what else '')

    def conductor(self, title, marks=()):
        """The conductor track: (title, events): the tempo, 4/4 and the key at tick 0, then the marks ((tick, meta),
        a tick's marker before its cue points)."""
        sf, mi = key_signature(self.K.KEY)
        ev = [(0, 0, b'\xff\x51\x03' + tempo_us(self.K.BPM).to_bytes(3, 'big')),
              (0, 0, b'\xff\x58\x04\x04\x02\x18\x08'), (0, 0, b'\xff\x59\x02' + bytes([sf & 0xFF, mi]))]
        ev += [(t, 1 if m[1] == MARKER else 2, m) for t, m in marks]
        return title, ev

    def file(self, title, parts, end, marks=(), keep_empty=False, order=None):
        """A file from {track: (notes, ccs)}: the tracks in the kit's order, the empty ones left out unless
        keep_empty."""
        names = order or [t for t in self.tracks() if t in parts]
        tracks = []
        for name in names:
            notes, ccs = parts.get(name, ([], []))
            ev = self.events(name, notes, ccs, end)
            if ev or keep_empty:
                tracks.append((name, ev))
        return smf(self.conductor(title, marks), tracks, end)

    # ---------------------------------------------------------- the material
    def drums(self, part, kind='loop', variant='vox'):
        """A drum part's 4-bar block: 'loop' (the Loop file's), 'fill' (where a line ends on a fill: PARTS['fill']
        or the loop's) or 'groove' (elsewhere: PARTS['groove'] or the fill block with bar 2 again in bar 4)."""
        return self.block('drums', kind, part)

    def block(self, what, kind, part=None, variant='vox'):
        """A lane's 4-bar block (see drums()) as notes. what: 'drums' (part: a drum part), 'bass', 'lead', 'chords',
        'voicing', 'fx' or an extra's name."""
        P = self.P
        name = part if what == 'drums' else what
        if kind == 'groove' and name in P.get('groove', {}):
            return as_notes(P['groove'][name])
        if kind in ('fill', 'groove') and name in P.get('fill', {}):
            fill = as_notes(P['fill'][name])
        else:
            fill = self.loop_part(what, part, variant)
            if what == 'lead' and not fill and kind != 'loop':
                fill = twice(P['lead']['notes'])       # a loop without the lead (FUT03's): the phrase twice
        if kind == 'groove':
            d = P['drums']
            return swapped(fill, d.get('fill_bar', BLOCK), d.get('fill_swap', 2))
        return fill

    def loop_part(self, what, part=None, variant='vox'):
        """A lane of the loop (PARTS['loop'], with its defaults) as notes, 4 bars."""
        P, L = self.P, self.P.get('loop', {})
        if what == 'drums':
            notes = as_notes(L['drums'])
            off_map = sorted({n.key for n in notes} - set(self.drum_map))
            if off_map:
                raise ValueError(f'{self.kit}: drum notes {off_map} are not on the drum map')
            return [n for n in notes if self.drum_part[n.key] == part]
        if what == 'bass':
            if 'bass' in L:
                return as_notes(L['bass'])
            return twice(P['bass']['lines'][P['bass']['pick']])
        if what == 'lead':
            return as_notes(L['lead']) if 'lead' in L else twice(P['lead']['notes'])
        if what == 'chords':
            notes = as_notes(L.get('chords', P['chords']['trigger']))
            return [n for n in notes if n.step not in L.get('stab_drop', {}).get(variant, ())]
        if what == 'voicing':
            notes = as_notes(L.get('voicing', P['chords']['voicing']))
            return [n for n in notes if n.step not in L.get('stab_drop', {}).get(variant, ())]
        if what == 'fx':
            return self.hit_notes(L.get('fx', ()))
        if what in L.get('extras', {}):
            return as_notes(L['extras'][what])
        return []

    def hit_notes(self, hit_list):
        """Hits as notes on their one-key channel: {role: notes}, flattened to the notes (one role per list)."""
        return [self.hit_note(h)[1] for h in hit_list]

    def hit_note(self, h):
        """A Hit as (role, Note): the sound's key, its velocity (the layer's, or its own), its length (the gate, else
        the sample's from its start), end-aligned or not, thrown or not."""
        role, key, snd = self.sound(h.sound)
        it = self.item(snd.item)
        v = it.layers[h.layer - 1] if h.layer else (h.vel if h.vel is not None else it.layers[-1])
        dur = round(h.gate * S16) if h.gate else self.sound_ticks(snd)
        return role, Note(h.step, key, 0, v, False, dur, h.align == 'end', h.throw)

    def loop_accents(self, variant):
        return [self.hit_note(h)[1] for h in self.P.get('loop', {}).get('accents', {}).get(variant, ())]

    def loop_fx(self):
        return self.hit_notes(self.P.get('loop', {}).get('fx', ()))

    def loop_vox(self):
        """The loop's vox lane (vox file only): its notes, the thrown ones marked."""
        L = self.P.get('loop', {})
        throws = set(L.get('throws', ()))
        return [n._replace(throw=n.throw or n.step in throws) for n in as_notes(L.get('vox', ()))]

    def riff(self):
        """The vox synth's riff (2 bars), the thrown notes marked (PARTS['vox']['throws'], 16ths of the riff)."""
        v = self.P.get('vox') or {}
        throws = set(v.get('throws', ()))
        return [n._replace(throw=n.throw or n.step in throws) for n in as_notes(v.get('riff', ()))]

    def ride_ccs(self, track, block_values, lo16, hi16, base):
        """A ride's points over [lo16, hi16) 16ths of a block (its values every CC_STEP ticks over the 4 bars), as
        (tick from base, value)."""
        out = []
        for i in range(lo16 * S16 // CC_STEP, hi16 * S16 // CC_STEP):
            out.append((base + i * CC_STEP - lo16 * S16, block_values[i]))
        return out

    def ride_values(self, ride, line_name=None):
        """A ride's values over a 4-bar block (one every CC_STEP ticks): its curve, or the bass line's own."""
        v = ride['values']
        if isinstance(v, dict):
            return v.get(line_name) or v.get('*')
        return v

    # ---------------------------------------------------------- the loop
    def loop_tracks(self, variant):
        """{track: (notes, ride controllers)} of the Loop file in a variant."""
        P = self.P
        L = P.get('loop', {})
        out = {}
        for part in DRUM_PARTS:
            notes = self.loop_part('drums', part)
            if notes:
                out[self.part_track(part)] = notes
        out['Bass'] = self.loop_part('bass')
        out['Lead'] = self.loop_part('lead')
        out['Chords'] = self.loop_part('chords', variant=variant)
        out['Chord voicing'] = self.loop_part('voicing', variant=variant)
        for name in L.get('extras', {}):
            out[name] = self.loop_part(name)
        if variant == 'vox':
            out['Vox'] = self.loop_vox()
        out['Accents'] = self.loop_accents(variant)
        out['FX'] = self.loop_fx()
        rides = {}
        for ride in self.rides:
            if ride.get('loop', True) and out.get(ride['on']):
                vals = self.ride_values(ride, P['bass']['pick'])
                pts = dedupe([(i * CC_STEP, v) for i, v in enumerate(vals)])
                rides.setdefault(ride['on'], []).extend((t, ride['cc'], v) for t, v in pts)
        return {t: (out.get(t, []), rides.get(t, [])) for t in self.tracks() if out.get(t) or rides.get(t)}

    def allowed_keys(self):
        """{track: keys}: everything the kit's material can play on each track."""
        P = self.P
        out = {t: set() for t in self.tracks()}
        for n, part in self.drum_part.items():
            out[self.part_track(part)].add(n)
        on = P['bass'].get('on', {})
        for name, notes in P['bass']['lines'].items():
            out[on.get(name, 'Bass')] |= {n.key for n in as_notes(notes)}
        out['Lead'] |= {n.key for n in as_notes(P['lead']['notes'])}
        out['Lead'] |= {n.key for n in as_notes(P['lead'].get('skeleton', ()))}
        blocks = {'main': dict(trigger=P['chords']['trigger'], voicing=P['chords']['voicing']),
                  **P['chords'].get('blocks', {})}
        for b in blocks.values():
            out['Chords'] |= {n.key for n in as_notes(b['trigger'])}
            out['Chord voicing'] |= {n.key for n in as_notes(b['voicing'])}
        for what in ('bass', 'lead', 'chords', 'voicing'):
            name = {'bass': 'Bass', 'lead': 'Lead', 'chords': 'Chords', 'voicing': 'Chord voicing'}[what]
            for kind in ('loop', 'fill', 'groove'):
                out[name] |= {n.key for n in self.block(what, kind)}
        out['Vox'] |= {n.key for n in self.riff()} | {n.key for n in self.loop_vox()}
        for role, track in (('accents', 'Accents'), ('fx', 'FX'), ('chops', 'Vox chops')):
            out[track] |= set(self.keys[role])
        for name in self.extras:
            for kind in ('loop', 'fill', 'groove'):
                out[name] |= {n.key for n in self.block(name, kind)}
        pad = P.get('pad')
        if pad:
            for notes in pad['blocks'].values():
                out[pad['on']] |= {n.key for n in as_notes(notes)}
        for ph in self.phrases:
            out[phrase_track(self, ph)] |= {n.key for n in as_notes(ph['notes'])}
        return out


def dedupe(points):
    """A ride's (tick, value) points with the repeats left out: a point only where the value moves."""
    out, last = [], None
    for t, v in points:
        if v != last:
            out.append((t, v))
            last = v
    return out


def phrase_track(k, ph):
    """The track a phrase plays on: its instrument's (PARTS['phrases'][i]['on']: a role or an extra's name)."""
    on = ph['on']
    return {'bass': 'Bass', 'lead': 'Lead', 'chords': 'Chords', 'vox': 'Vox', 'accents': 'Accents',
            'chops': 'Vox chops'}.get(on, on)


def phrase_file(kit, ph):
    """A synth phrase's file: `<kit>_Phrase_<Name>_<bars>bars.mid`."""
    return f'{kit}_Phrase_{ph["file"]}_{ph.get("bars", 2)}bars.mid'


def stem(name):
    """A sample's or section's name for a file: 'Organ Stab' -> 'OrganStab', 'Switch-up' -> 'SwitchUp'."""
    return ''.join(w[:1].upper() + w[1:] for w in re.split(r'[\s#-]+', name) if w)


# ------------------------------------------------------------------ a kit's files
def parts(kit):
    """The kit's module and PARTS (its recipe package's midi module), or None if it has none."""
    K = kits.load(kit)
    try:
        return K, importlib.import_module(f'{K.RECIPES}.midi').PARTS
    except ModuleNotFoundError:
        return K, None


_KITS = {}


def kit_of(kit):
    """The kit's Kit (cached)."""
    if kit not in _KITS:
        K, P = parts(kit)
        if P is None:
            raise kits.UnknownName(f'{kit} has no midi module in {K.RECIPES}')
        _KITS[kit] = Kit(kit, K, P)
    return _KITS[kit]


def files(kit):
    """{file name: bytes} for the kit's folder: the loops, the parts, the phrases and README.md."""
    return files_of(kit_of(kit))


def extended(kit, variant):
    """{file name: bytes} of the kit's extended mix in a variant (its extended/<variant>/ folder)."""
    return extended_of(kit_of(kit), variant)


def chord_file(k):
    return stem(words(k.P['chords']['items'][0]))


def files_of(k):
    """files() for a Kit."""
    kit, P = k.kit, k.P
    out = {}
    bar4, bar2 = 4 * BAR * S16, 2 * BAR * S16
    for v in VARIANTS:
        out[f'{kit}_Loop_{VARIANT_NAME[v]}_4bars.mid'] = k.file(k.title(f'loop, {VARIANT_NAME[v]}'),
                                                                k.loop_tracks(v), bar4)
    drums = {}
    for part in DRUM_PARTS:
        notes = k.loop_part('drums', part)
        if notes:
            drums[k.part_track(part)] = (notes, [])
    out[f'{kit}_Drums_4bars.mid'] = k.file(k.title('drums'), drums, bar4)
    on = P['bass'].get('on', {})
    for name, line_ in P['bass']['lines'].items():
        tag = '_InTheLoop' if name == P['bass']['pick'] else ''
        track = on.get(name, 'Bass')
        out[f'{kit}_Bassline_{name}{tag}_2bars.mid'] = k.file(k.title(f'bassline {name}'), {track: (line_, [])}, bar2)
    lead = P['lead']
    out[f'{kit}_Lead_{stem(words(lead["item"]))}_2bars.mid'] = k.file(k.title('lead'), {'Lead': (lead['notes'], [])},
                                                                       bar2)
    out[f'{kit}_Chords_{chord_file(k)}_4bars.mid'] = k.file(
        k.title('chords'), {'Chords': (P['chords']['trigger'], [])}, bar4)
    out[f'{kit}_Chords_{chord_file(k)}_Voicing_4bars.mid'] = k.file(
        k.title('chord voicing'), {'Chord voicing': (P['chords']['voicing'], [])}, bar4)
    for v in VARIANTS:
        out[f'{kit}_Accents_{VARIANT_NAME[v]}_4bars.mid'] = k.file(k.title(f'accents, {VARIANT_NAME[v]}'),
                                                                   {'Accents': (k.loop_accents(v), [])}, bar4)
    if k.loop_fx():
        out[f'{kit}_FX_4bars.mid'] = k.file(k.title('FX'), {'FX': (k.loop_fx(), [])}, bar4)
    for ph in k.phrases:
        track = phrase_track(k, ph)
        thrown = set(ph.get('throws', ()))
        notes = [n._replace(throw=n.throw or n.step in thrown) for n in as_notes(ph['notes'])]
        out[phrase_file(kit, ph)] = k.file(k.title(ph['name']), {track: (notes, [])}, ph.get('bars', 2) * BAR * S16)
    out['README.md'] = readme(k, sorted(out)).encode()
    return out


# ------------------------------------------------------------------ the extended mix
class Context(NamedTuple):
    """Where a line sits: its section's kind, whether it's the section's last line, the section's index and the
    form."""
    kind: str
    last: bool
    first: bool
    variant: str


def roles(k):
    """The cue roles the kit maps to sounds (PARTS['cues']): riser, fill, impact, swell, uplift, stab."""
    return k.P.get('cues', {})


def cue_hits(k, role):
    """A cue role's sounds, as a tuple of sound names."""
    got = roles(k).get(role)
    return () if not got else ((got,) if isinstance(got, str) else tuple(got))


def fill_hits(k, ln=None):
    """The fill a line ends on, as hits on the fill bar (16ths of a 4-bar block): the line's own (ln['fill'], a sound
    end-aligned on the block's end), else the loop's FX in its last bar, else the 'fill' cue's sound end-aligned on the
    block's end."""
    if ln is not None and ln.get('fill'):
        return [Hit(BLOCK * BAR, ln['fill'], align='end')]
    loop_fx = [h for h in k.P.get('loop', {}).get('fx', ()) if h.step >= (BLOCK - 1) * BAR]
    if loop_fx:
        return list(loop_fx)
    return [Hit(BLOCK * BAR, s, align='end') for s in cue_hits(k, 'fill')]


def fill_start(k, ln=None):
    """The 16th of the fill bar (0-16) where the line's fill (fill_hits()) starts sounding."""
    starts = []
    for h in fill_hits(k, ln):
        _, note = k.hit_note(h)
        on = k.tick(note.step) - (note.ticks if note.end else 0)
        starts.append(on / S16 - (BLOCK - 1) * BAR)
    return max(0.0, min(starts)) if starts else BAR


def hats_mode(ln, block):
    mode = ln.get('hats', 'full')
    return mode[block] if isinstance(mode, tuple) else mode


def hat_density(k, notes, mode, until=None):
    """The hats lane in a mode: 'full' as written; 'thin' the off-beat 8ths; 'busy' the kit's busy pattern added
    (PARTS['drums']['busy'], a bar repeated), else the closed hat at the louder of 0.35 and its softest written velocity
    on every empty 16th; '16ths' the closed hat on every 16th, its written velocities kept. until: no hat is added from
    this 16th (a fill block's fill: its written bar leaves the fill its room)."""
    if mode == 'thin':
        return [n for n in notes if n.step % 4 == 2]
    if mode in ('busy', '16ths') and until is not None:
        dense = hat_density(k, [n for n in notes if n.step < until], mode)
        return sorted([n for n in dense if n.step < until] + [n for n in notes if n.step >= until])
    if mode in ('busy', '16ths'):
        bars_ = sorted({int(n.step // BAR) for n in notes}) or list(range(BLOCK))
        authored = k.P['drums'].get('busy')
        taken = {n.step for n in notes}
        closed = [n for n in notes if n.key == CLOSED_HAT] or notes
        if not closed:
            return notes
        key = closed[0].key
        if mode == 'busy' and authored:
            add = [n._replace(step=n.step + BAR * b) for b in bars_ for n in as_notes(authored)
                   if n.step + BAR * b not in taken]
            return sorted(notes + add)
        soft = max(0.35, min(n.vel for n in closed))
        add = [Note(BAR * b + s, key, 0.5, soft) for b in bars_ for s in range(BAR) if BAR * b + s not in taken]
        return sorted(notes + add)
    return notes


def rolled(k, lane_notes, roll, until, keep=True):
    """A build's roll (16ths from the line's start): the lane's most written note (the lowest of a tie) every so many
    16ths over the roll['bars'] bars before the line's last bar (roll['every']: (bars, 16ths) in turn), then in the
    last bar every roll['last'] 16ths (default: the last rate) up to `until` (the fill's start), its velocity rising
    hit by hit from ramp[0] to ramp[1] times the lane's loudest written velocity. keep: the lane's written notes before
    the roll stay (else the roll plays alone)."""
    if not lane_notes:
        return []
    key = main_key(lane_notes)
    top = max(n.vel for n in lane_notes)
    last_bar = (LINE - 1) * BAR
    start = last_bar - roll['bars'] * BAR
    steps, at = [], start
    for bars_, every in roll['every']:
        steps += range(at, at + bars_ * BAR, every)
        at += bars_ * BAR
    if at != last_bar:
        raise ValueError(f'a roll of {roll["bars"]} bars schedules {(at - start) // BAR} bars: {roll["every"]}')
    steps += range(last_bar, LINE * BAR, roll.get('last', roll['every'][-1][1]))
    steps = [s for s in steps if s < until]
    lo, hi = roll.get('ramp', (0.55, 1.0))
    n = len(steps)
    out = [Note(s, key, 0.5, top * (lo + (hi - lo) * i / max(1, n - 1))) for i, s in enumerate(steps)]
    return sorted([x for x in lane_notes if keep and x.step < start] + out)


PICKUP = ((13, 0.45), (14, 0.6), (15, 0.75))   # a default turnaround: the last beat's 'e', 'and' and 'a', rising


def main_key(notes):
    """A lane's most written note (the lowest of a tie)."""
    count = {}
    for n in notes:
        count[n.key] = count.get(n.key, 0) + 1
    return min(count, key=lambda c: (-count[c], c))


def pickup_lane(k, lanes, out, bar_lo):
    """The drum lane a default turnaround's pickup plays on: the first of snare, perc and hats the line plays whose
    most written note leaves room on the last beat's 'e', 'and' or 'a' of the bar (FUT01's tambourine, on every 16th,
    leaves none)."""
    for p in ('snare', 'perc', 'hats'):
        notes = out.get(k.part_track(p)) if p in lanes else None
        if notes:
            key = main_key(notes)
            have = {n.step - bar_lo for n in notes if n.key == key and bar_lo <= n.step < bar_lo + BAR}
            if any(s not in have for s, _ in PICKUP):
                return p
    return None


def turnaround(k, part, notes, bar_lo, pickup=None, name=None):
    """The line's last bar as a turnaround: PARTS['drums']['turn'][part] (a bar of notes; a named one,
    PARTS['drums']['turns'][name], where the line asks for it) in place of the bar, else on the pickup lane
    (pickup_lane()) a pickup roll on its most written note, the last beat's 'e', 'and' and 'a' at 0.45, 0.6 and 0.75 of
    the lane's loudest velocity, in place of that note's own there; every other note kept."""
    if name is not None:
        turns = k.P['drums'].get('turns', {})
        if name not in turns:
            raise ValueError(f"{k.kit}: no turnaround called {name!r} in PARTS['drums']['turns']")
        authored = turns[name]
    else:
        authored = k.P['drums'].get('turn')
    if authored is not None:
        if part not in authored:
            return notes
        keep = [n for n in notes if not bar_lo <= n.step < bar_lo + BAR]
        return sorted(keep + shifted(as_notes(authored[part]), bar_lo))
    if part != pickup or not notes:
        return notes
    key = main_key(notes)
    top = max(n.vel for n in notes)
    roll = [Note(bar_lo + s, key, 0.5, top * v) for s, v in PICKUP]
    keep = [n for n in notes if not (n.key == key and bar_lo + PICKUP[0][0] <= n.step < bar_lo + BAR)]
    return sorted(keep + roll)


def vox_notes(k, mode, variant):
    """The vox over a line (16ths from its start) in a mode."""
    if variant != 'vox' or mode in (None, 'off'):
        return []
    riff = k.riff()
    if mode == 'loop':
        pattern = k.loop_vox()
        return pattern + shifted(pattern, BLOCK * BAR)
    if mode == 'riff':
        return [n for r in range(LINE // 2) for n in shifted(riff, 2 * BAR * r)]
    if mode == 'call':
        one = [n for n in riff if n.step < BAR]
        return [n for r in range(LINE // 2) for n in shifted(one, 2 * BAR * r)]
    if mode == 'answer':
        one = [n for n in riff if n.step >= BAR]
        return [n for r in range(LINE // 2) for n in shifted(one, 2 * BAR * r)]
    raise ValueError(f'vox mode {mode!r}')


def throw_note(k):
    """The riff's last note alone on the line's last 'and' of 4, thrown."""
    riff = k.riff()
    last = max(riff, key=lambda n: n.step)
    return last._replace(step=(LINE - 1) * BAR + 14, throw=True)


def chord_block(k, name, kind, variant):
    """A chord block's (trigger, voicing) 4 bars: 'main' the loop's (its fill or groove block), else
    PARTS['chords']['blocks'][name]."""
    if name in (None, 'main'):
        return k.block('chords', kind), k.block('voicing', kind)
    b = k.P['chords']['blocks'][name]
    return as_notes(b['trigger']), as_notes(b['voicing'])


def rest_from(k, notes, step):
    """The notes that start before `step` (a 16th), a note held over it cut there, to the tick: where the drums rest
    (a roll's fill, a drum drop, a line's own fill) nothing sounds past the rest (FUT04's 2-bar rumble notes)."""
    out = []
    for n in notes:
        if n.step < step:
            dur = n.ticks or max(1, round(n.length * S16))
            out.append(n._replace(ticks=max(1, min(dur, k.tick(step) - k.tick(n.step)))))
    return out


def lead_rests_in_fill(k):
    """Whether the loop's lead rests in its fill bar (FUT02's arp, context.ARP_REST): then the lead's call and
    skeleton rest there too, where a line ends on a fill."""
    return not [n for n in k.block('lead', 'fill') if n.step >= (BLOCK - 1) * BAR]


def playing_phrases(k, ln, variant):
    """The phrases a line plays in a variant ({track: phrase}): a phrase on the vox synth's lane plays in the vox mix
    only."""
    out = {}
    for name in ln.get('phrases', ()):
        ph = next((p for p in k.phrases if p['name'] == name), None)
        if ph is None:
            raise ValueError(f'{k.kit}: a line plays a phrase PARTS does not have: {name!r}')
        track = phrase_track(k, ph)
        if CH.track_lane(k.kit, track) == 'vox' and variant != 'vox':
            continue
        out[track] = ph
    return out


def variant_block(k, variant, part, kind):
    """A drum variant's 4-bar block for a lane: its notes (a bar is played 4 times); in a fill block its bars 1-3,
    then its own fill bar for the lane (variant['fill'][lane]) if it gives one, else the kit's fill bar for the lane,
    which leaves room for the kit's fill."""
    src = as_notes(variant[part])
    if max((n.step for n in src), default=0) < BAR:
        src = [n for r in range(BLOCK) for n in shifted(src, BAR * r)]
    if kind != 'fill':
        return src
    last = (BLOCK - 1) * BAR
    own = variant.get('fill', {}).get(part)
    bar = shifted(as_notes(own), last) if own is not None else [
        n for n in k.block('drums', 'fill', part) if n.step >= last]
    return sorted([n for n in src if n.step < last] + bar)


def line_hits(k, ln, variant):
    """A line's own one-shots in a variant (ln['hits']: Hits, or {'vox': ..., 'novox': ...}), checked: accents or FX
    (channels 6 and 7) on whole 16ths."""
    got = ln.get('hits', ())
    hits_ = got.get(variant, ()) if isinstance(got, dict) else got
    for h in hits_:
        role = k.sound(h.sound)[0]
        if role not in ('accents', 'fx'):
            raise ValueError(f'{k.kit}: a line hit plays accents or FX (channels 6 and 7), not {h.sound!r} ({role})')
        if float(h.step) != int(h.step):
            raise ValueError(f'{k.kit}: a line hit sits on a 16th, not {h.step}')
    return tuple(hits_)


def line_notes(k, ln, ctx, variant):
    """One line as ({track: notes, 16ths from its start}, {track: [(tick from its start, controller, value)]})."""
    P = k.P
    fill = ln.get('end') == 'fill'
    lanes = set(ln['lanes'])
    out = {}
    add = lambda track, notes: out.setdefault(track, []).extend(notes)  # noqa: E731
    blocks = LINE // BLOCK
    stab_drop = set(P.get('loop', {}).get('stab_drop', {}).get(variant, ()))
    acc_blocks = {'loop': set(range(blocks)), 'alternate': {blocks - 1}}.get(ln.get('accents'), set())
    phrase_on = playing_phrases(k, ln, variant)
    variant_drums = P['drums']['variants'][ln['drums']] if ln.get('drums') else {}
    for b in range(blocks):
        kind = 'fill' if fill and b == blocks - 1 else 'groove'
        off = b * BLOCK * BAR
        # drums
        for part in DRUM_PARTS:
            if part not in lanes:
                continue
            if part in variant_drums:
                notes = variant_block(k, variant_drums, part, kind)
            else:
                notes = k.block('drums', kind, part)
            if part == 'hats':
                until = (BLOCK - 1) * BAR + fill_start(k, ln) if kind == 'fill' else None
                notes = hat_density(k, notes, hats_mode(ln, b), until)
            add(k.part_track(part), shifted(notes, off))
        # bass
        if 'bass' in lanes and 'Bass' not in phrase_on:
            name = bass_line(k, ln.get('bass', 'main'))
            track = P['bass'].get('on', {}).get(name, 'Bass')
            if name == P['bass']['pick']:
                add(track, shifted(k.block('bass', kind), off))
                for extra in P.get('loop', {}).get('extras', {}):
                    if k.extras.get(extra, (0, ''))[1] == 'bass':
                        add(extra, shifted(k.block(extra, kind), off))
            else:
                add(track, shifted(twice(P['bass']['lines'][name]), off))
        # lead
        if 'lead' in lanes and 'Lead' not in phrase_on:
            mode = ln.get('lead', 'phrase')
            if mode == 'phrase':
                notes = k.block('lead', kind)
            elif mode == 'call':
                notes = twice([n for n in as_notes(P['lead']['notes']) if n.step < BAR])
            elif mode == 'answer':
                notes = twice([n for n in as_notes(P['lead']['notes']) if BAR <= n.step < 2 * BAR])
            elif mode == 'skeleton':
                notes = twice(P['lead'].get('skeleton') or [n for n in as_notes(P['lead']['notes']) if n.step < BAR])
            else:
                raise ValueError(f'lead mode {mode!r}')
            if mode != 'phrase' and kind == 'fill' and lead_rests_in_fill(k):
                notes = rest_from(k, notes, (BLOCK - 1) * BAR)     # the fill bar is the fill's, as in the loop
            add('Lead', shifted(notes, off))
        # chords and the pad
        if 'chords' in lanes and 'Chords' not in phrase_on:
            trig, voic = chord_block(k, ln.get('chords'), kind, variant)
            if b in acc_blocks and stab_drop and ln.get('chords') in (None, 'main'):
                trig = [n for n in trig if n.step not in stab_drop]
                voic = [n for n in voic if n.step not in stab_drop]
            add('Chords', shifted(trig, off))
            add('Chord voicing', shifted(voic, off))
        if 'pad' in lanes and P.get('pad'):
            pad = P['pad']
            notes = as_notes(pad['blocks'].get(ln.get('chords') or 'main', pad['blocks']['main']))
            add(pad['on'], shifted(notes, off))
        # extras the loop plays on their own lanes (FUT02's glide rides with the bass above)
        for extra in P.get('loop', {}).get('extras', {}):
            ext_lane = k.extras.get(extra, (0, ''))[1]
            if ext_lane != 'bass' and extra in lanes:
                add(extra, shifted(k.block(extra, kind), off))
        # accents: the variant's loop pattern
        if b in acc_blocks:
            add('Accents', shifted(k.loop_accents(variant), off))
    # phrases (a vox phrase plays in the vox mix only, in place of the line's vox mode)
    for track, ph in phrase_on.items():
        period = ph.get('bars', 2) * BAR
        notes = as_notes(ph['notes'])
        notes = [n for r in range(LINE * BAR // period) for n in shifted(notes, r * period)]
        if fill and ln.get('phrases_rest'):
            notes = rest_from(k, notes, (LINE - 1) * BAR + fill_start(k, ln))   # the fill alone from its start
        add(track, notes)
    # vox
    if ('vox' in lanes or ln.get('vox', 'off') != 'off') and 'Vox' not in phrase_on:
        add('Vox', vox_notes(k, ln.get('vox', 'off'), variant))
    if ln.get('throw') and variant == 'vox' and k.riff():
        add('Vox', [throw_note(k)])
    # accents and FX placed by rule
    if ln.get('accents') == 'lead-in' and ctx.last:
        for n in k.loop_accents(variant):
            if n.end and n.step == BLOCK * BAR:
                add('Accents', [n._replace(step=LINE * BAR)])
    if ln.get('stab') and ln.get('end') != 'fill':
        for s in cue_hits(k, 'stab'):
            role, n = k.hit_note(Hit((LINE - 1) * BAR + 14, s))
            add(track_of_role(role), [n])
    if ln.get('into'):
        for s in cue_hits(k, ln['into']):
            role, n = k.hit_note(Hit(LINE * BAR, s, align='end'))
            add(track_of_role(role), [n])
    if ln.get('downbeat'):
        for s in cue_hits(k, ln['downbeat']):
            role, n = k.hit_note(Hit(0, s))
            add(track_of_role(role), [n])
    if fill:
        for h in fill_hits(k, ln):
            role, n = k.hit_note(h)
            add(track_of_role(role), [n._replace(step=n.step + (LINE - BLOCK) * BAR)])
    for h in line_hits(k, ln, variant):
        role, n = k.hit_note(h)
        add(track_of_role(role), [n])
    if ln.get('cue') == 'Riser':
        for s in cue_hits(k, 'riser'):
            role, n = k.hit_note(Hit(LINE * BAR, s, align='end'))
            add(track_of_role(role), [n])
    # the line's moves: rolls, rises, turnarounds, drum drops
    last_bar = (LINE - 1) * BAR
    if fill and ln.get('fill') and not ln.get('roll'):      # the line's own fill: the drums out from its start
        until = last_bar + fill_start(k, ln)
        for part in DRUM_PARTS:
            t = k.part_track(part)
            if t in out:
                out[t] = rest_from(k, out[t], until)
    if ln.get('roll'):
        roll = ln['roll']
        until = last_bar + fill_start(k, ln) if fill else LINE * BAR
        track = k.part_track(roll['lane'])
        src = out.get(track) or [n for b in range(blocks)
                                 for n in shifted(k.block('drums', 'groove', roll['lane']), b * BLOCK * BAR)]
        out[track] = rolled(k, src, roll, until, keep=roll['lane'] in lanes)
        if roll.get('rest', 'all') == 'all':                 # every drum rests from the fill's start
            for part in DRUM_PARTS:
                t = k.part_track(part)
                if t in out:
                    out[t] = rest_from(k, out[t], until)
    if ln.get('end') == 'turn':
        pickup = pickup_lane(k, lanes, out, last_bar)
        for part in DRUM_PARTS:
            t = k.part_track(part)
            if part in lanes and t in out:
                out[t] = turnaround(k, part, out[t], last_bar, pickup, ln.get('turn'))
    if 'rise' in ln:
        lane, a, z = ln['rise']
        for t in ([k.part_track(lane)] if lane in DRUM_PARTS else [lane]):
            if t in out:
                out[t] = [n._replace(vel=n.vel * (a + (z - a) * int(n.step // BAR) / (LINE - 1))) for n in out[t]]
    if ln.get('end') == 'drop':
        for part in DRUM_PARTS:
            t = k.part_track(part)
            if t in out:
                out[t] = rest_from(k, out[t], last_bar)
    # the rides
    ccs = {}
    for ride in k.rides:
        track = ride['on']
        if not out.get(track):
            continue
        mode = ln.get('ride', {}).get(track, 'loop')
        name = bass_line(k, ln.get('bass', 'main')) if track == 'Bass' else None
        vals = k.ride_values(ride, name)
        if vals is None:
            continue
        pts = []
        n_block = BLOCK * BAR * S16 // CC_STEP
        for i in range(LINE * BAR * S16 // CC_STEP):
            if mode == 'loop':
                v = vals[i % n_block]
            elif mode[0] == 'hold':
                v = mode[1]
            elif mode[0] == 'ramp':
                v = round(mode[1] + (mode[2] - mode[1]) * i / (LINE * BAR * S16 // CC_STEP - 1))
            elif mode[0] == 'points':
                v = ride_point(mode[1], i * CC_STEP / (BAR * S16))
            else:
                raise ValueError(f'ride mode {mode!r}')
            pts.append((i * CC_STEP, v))
        ccs.setdefault(track, []).extend((t, ride['cc'], v) for t, v in pts)
    return out, ccs


def ride_point(points, x):
    """A 'points' ride's value x bars into its line: (bar, value) breakpoints joined linearly, held before the first
    and after the last."""
    if x <= points[0][0]:
        return points[0][1]
    for (x0, v0), (x1, v1) in zip(points, points[1:]):
        if x <= x1:
            return round(v0 + (v1 - v0) * (x - x0) / (x1 - x0))
    return points[-1][1]


def ride_mode(k, mode):
    """A line's ride mode, checked, as regions.json records it: 'loop', or a list of integers and strings: ['hold',
    v], ['ramp', a, b], ['points', [[bar, v], ...]] (whole bars 0-8, ascending; values 0-127)."""
    if mode == 'loop':
        return mode
    kind, *rest = mode
    if kind == 'points':
        pts = [list(p) for p in rest[0]]
        bars_ = [b for b, _ in pts]
        ok = (pts and all(isinstance(b, int) and isinstance(v, int) and 0 <= b <= LINE and 0 <= v <= 127
                          for b, v in pts) and bars_ == sorted(set(bars_)))
        if not ok:
            raise ValueError(f'{k.kit}: a points ride is ((whole bar 0-8, value 0-127), ...), ascending: {rest[0]!r}')
        return [kind, pts]
    if kind not in ('hold', 'ramp') or not all(isinstance(v, int) and 0 <= v <= 127 for v in rest):
        raise ValueError(f'{k.kit}: ride mode {mode!r}')
    return [kind, *rest]


def track_of_role(role):
    return {'accents': 'Accents', 'fx': 'FX', 'chops': 'Vox chops'}[role]


def bass_line(k, choice):
    """The bassline a line plays: a line's name, or the kit's pick for 'main', 'switch' or 'drop2'."""
    B = k.P['bass']
    if choice in B['lines']:
        return choice
    others = [n for n in B['lines'] if n != B['pick']] or [B['pick']]
    defaults = {'main': B['pick'], 'switch': others[0], 'drop2': B['pick']}
    name = B.get(choice, defaults.get(choice))
    if name not in B['lines']:
        raise ValueError(f'{k.kit}: no bassline {choice!r}')
    return name


def plan(k, variant):
    """The extended mix's sections: dicts of index, name, kind, start bar, bars, region file, notes {track: notes,
    16ths from the section's start}, ccs {track: [(tick, controller, value)]}, lanes, lines (regions.json's), said
    (each line in words, for the README) and cues ({bar, cue})."""
    form = arrangement(k.K, k.P)
    out, at = [], 1
    for i, (name, kind, lines) in enumerate(form, 1):
        notes, ccs, info, said, cues = {}, {}, [], [], []
        for j, ln in enumerate(lines):
            ctx = Context(kind, j == len(lines) - 1, j == 0, variant)
            got, cc = line_notes(k, ln, ctx, variant)
            start = at + j * LINE
            for t, v in got.items():
                notes.setdefault(t, []).extend(shifted(v, j * LINE * BAR))
            for t, v in cc.items():
                ccs.setdefault(t, []).extend((tick + j * LINE * BAR * S16, c, val) for tick, c, val in v)
            info.append(line_info(k, ln, got, start, variant))
            said.append(describe_line(k, ln, info[-1], got, variant))
            if ln.get('cue') in ('Vox', 'Riser'):
                cues.append(dict(bar=start, cue=ln['cue']))
            if ln.get('end') == 'fill':
                cues.append(dict(bar=start + LINE - 1, cue='Fill'))
        for t, v in ccs.items():
            last, kept = {}, []
            for tick, c, val in sorted(v, key=lambda e: (e[1], e[0])):
                if last.get(c) != val:
                    kept.append((tick, c, val))
                    last[c] = val
            ccs[t] = sorted(kept)
        bars_ = LINE * len(lines)
        lanes = [lane for lane in CH.LANES if any(lane in x['lanes'] for x in info)]
        out.append(dict(index=i, name=name, kind=kind, start_bar=at, bars=bars_,
                        file=f'{i:02d}_{stem(name)}_{bars_}bars.mid', notes=notes, ccs=ccs, lanes=lanes, lines=info,
                        said=said,
                        cues=sorted(cues, key=lambda c: (c['bar'], c['cue'] != 'Fill' and c['cue'] != 'Riser'))))
        at += bars_
    return out


def own_parts(k, ln, variant):
    """The line's own parts that play in a variant, keyed on the line naming them, not on their lane having notes (a
    phrase or an extra on the lane is not the part: FUT01's organ riff on channel 12 is not the lead): 'bass', 'lead'
    and 'chords' where the line names them and no phrase plays in place of them; 'pad' where it names the pad and the
    kit has one; 'vox' where its vox mode plays (the vox mix only) and no phrase replaces it; 'throw' where it throws
    the riff's last note (the vox mix only)."""
    phrase_on = playing_phrases(k, ln, variant)
    named = set(ln['lanes'])
    out = {part for part, track in (('bass', 'Bass'), ('lead', 'Lead'), ('chords', 'Chords'))
           if part in named and track not in phrase_on}
    if 'pad' in named and k.P.get('pad'):
        out.add('pad')
    if variant == 'vox' and ln.get('vox', 'off') != 'off' and 'Vox' not in phrase_on:
        out.add('vox')
    if variant == 'vox' and ln.get('throw') and k.riff():
        out.add('throw')
    return out


def line_info(k, ln, got, start, variant='vox'):
    """regions.json's entry for a line: where it starts, the lanes with notes in it, and how the line's own parts play
    (own_parts(): only where the part plays, not where its lane plays a phrase or an extra): the bassline, the lead's
    mode, the vox's, a throw, a chord block and the pad's own; then only the phrases that play in the variant."""
    lanes = [lane for lane in CH.LANES if any(got.get(t) for t in got if CH.track_lane(k.kit, t) == lane)]
    info = dict(start_bar=start, bars=LINE, lanes=lanes)
    phrase_on = playing_phrases(k, ln, variant)
    parts_ = own_parts(k, ln, variant)
    if 'hats' in lanes:
        mode = ln.get('hats', 'full')
        info['hats'] = '/'.join(mode) if isinstance(mode, tuple) else mode
    if 'bass' in parts_:
        info['bass'] = bass_line(k, ln.get('bass', 'main'))
    if 'lead' in parts_:
        info['lead'] = ln.get('lead', 'phrase')
    if 'vox' in parts_:
        info['vox'] = ln['vox']
    if 'throw' in parts_:
        info['throw'] = 1                       # a flag, as the integer 1: regions.json is integers and strings only
    if 'chords' in parts_ and ln.get('chords'):
        info['chords'] = ln['chords']
    if 'pad' in parts_ and ln.get('chords') not in (None, 'main') and ln['chords'] in k.P['pad']['blocks']:
        info['pad'] = ln['chords']             # the pad's own block for the line's chords (else its main one)
    if phrase_on:
        info['phrases'] = [name for name in ln['phrases'] if name in {ph['name'] for ph in phrase_on.values()}]
    if ln.get('roll'):
        info['roll'] = ln['roll']['lane']
        info['roll_bars'] = ln['roll']['bars'] + 1
    if 'rise' in ln:
        info['rise'] = {'lane': ln['rise'][0], 'from_pm': round(ln['rise'][1] * 1000),
                        'to_pm': round(ln['rise'][2] * 1000)}
    if ln.get('accents'):
        info['accents'] = ln['accents']
    if ln.get('drums'):
        info['drums'] = ln['drums']
    if ln.get('end'):
        info['end'] = ln['end']
    if ln.get('fill') and ln.get('end') == 'fill':
        info['fill'] = ln['fill']
    if ln.get('turn') and ln.get('end') == 'turn':
        info['turn'] = ln['turn']
    if line_hits(k, ln, variant):
        info['hits'] = [[int(h.step), h.sound] for h in line_hits(k, ln, variant)]
    if ln.get('ride'):
        info['ride'] = {t: ride_mode(k, m) for t, m in ln['ride'].items()}
    return info


def section_tracks(k, sec, end, names):
    """A section's tracks as (name, events) in `names`' order, on ticks from its start: every region carries the same
    tracks, the mix's."""
    out = []
    for name in names:
        out.append((name, k.events(name, sec['notes'].get(name, []), sec['ccs'].get(name, []), end)))
    return out


def section_marks(sec):
    """The section's marker on its first tick, then its cue points."""
    bar = BAR * S16
    return [(0, meta(MARKER, sec['name']))] + [((c['bar'] - sec['start_bar']) * bar, meta(CUE_POINT, c['cue']))
                                               for c in sec['cues']]


def ride_meta(k):
    """regions.json's (and the README's) rides: each controller's channel and range (integers: Hz, or permille)."""
    out = []
    for r in k.rides:
        out.append(dict(track=r['on'], channel=k.channel(r['on']), cc=r['cc'], law=r['law'], lo=r['lo'], hi=r['hi'],
                        unit=r.get('unit', 'Hz'), what=r['what']))
    return out


def extended_of(k, variant):
    """extended() for a Kit."""
    kit, K = k.kit, k.K
    secs = plan(k, variant)
    total = sum(s['bars'] for s in secs)
    bar = BAR * S16
    used = [t for t in k.tracks() if any(s['notes'].get(t) or s['ccs'].get(t) for s in secs)]
    title = k.title(f'extended mix, {VARIANT_NAME[variant]}')
    whole_name = f'{kit}_ExtendedMix_{VARIANT_NAME[variant]}_{total}bars.mid'
    out, notes, ccs, marks = {}, {t: [] for t in used}, {t: [] for t in used}, []
    for s in secs:
        sm = section_marks(s)
        out[s['file']] = smf(k.conductor(f'{title}: {s["name"]}', sm), section_tracks(k, s, s['bars'] * bar, used),
                             s['bars'] * bar)            # a region: its notes stop on its last bar line
        off = (s['start_bar'] - 1) * bar
        for name in used:
            notes[name] += shifted(s['notes'].get(name, []), (s['start_bar'] - 1) * BAR)
            ccs[name] += [(t + off, c, v) for t, c, v in s['ccs'].get(name, [])]
        marks += [(t + off, m) for t, m in sm]
    # the whole mix from the plan's notes over all of it: a note held over a section line keeps its length
    whole = [(name, k.events(name, notes[name], ccs[name], total * bar)) for name in used]
    out[whole_name] = smf(k.conductor(title, marks), whole, total * bar)
    R = dict(kit=kit, title=K.TITLE, variant=variant, bpm=int(K.BPM), tempo_us=tempo_us(K.BPM), key=K.KEY,
             camelot=K.CAMELOT, swing_pm=round(K.SWING * 1000), ppq=PPQ, bars=total,
             length_ms=round(total * 240_000 / K.BPM), length=length(total, K.BPM), file=whole_name,
             note_names='C4 = 60, as the sample names',
             tracks=[dict(track=t, lane=CH.track_lane(kit, t), channel=k.channel(t),
                          notes=track_notes(k, secs, t)) for t in used],
             rides=ride_meta(k),
             sections=[{x: s[x] for x in ('index', 'name', 'kind', 'start_bar', 'bars', 'file', 'lanes', 'lines',
                                          'cues')} for s in secs])
    out['regions.json'] = (json.dumps(R, indent=2) + '\n').encode()
    out['README.md'] = extended_readme(k, variant, secs, R).encode()
    return out


def track_notes(k, secs, track):
    """regions.json's {MIDI note: name} of what a track plays in the mix."""
    got = sorted({n.key for s in secs for n in s['notes'].get(track, [])})
    return {str(n): key_name(k, track, n) for n in got}


def key_name(k, track, n):
    """What a key plays on a track: a drum or a sound by name, or a pitch (C4 = 60)."""
    ch = k.channel(track)
    if ch == CH.CHANNELS['drums']:
        return words(k.drum_map[n])
    if ch in (6, 7, 8):
        role = {6: 'accents', 7: 'fx', 8: 'chops'}[ch]
        return sound_words(k.keys[role][n])
    return sample_name(n)


def sound_words(key):
    """A one-key sound in words: its Item, its zone, where it starts."""
    out = words(key.item) + (f' {sample_name(key.zone)}' if key.zone is not None else '')
    return out + (f', from its 16th {key.start:g}' if key.start else '')


def write(kit, out_dir):
    """Write the kit's files to `out_dir` and its extended mixes to `out_dir`/extended/<variant>; return (the files
    written, the files there that it didn't make)."""
    made = dict(files(kit))
    for v in VARIANTS:
        made.update({f'{EXTENDED}/{v}/{n}': d for n, d in extended(kit, v).items()})
    for name, data in made.items():
        path = out_dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    stale = sorted(str(p.relative_to(out_dir)) for p in out_dir.rglob('*')
                   if p.is_file() and str(p.relative_to(out_dir)) not in made)
    return sorted(made), stale


# ------------------------------------------------------------------ the READMEs
def signature_words(sf, mi):
    """A key signature in words: '3 sharps, minor'."""
    kind = 'sharp' if sf > 0 else 'flat'
    count = 'no sharps or flats' if sf == 0 else f"{abs(sf)} {kind}{'s' if abs(sf) > 1 else ''}"
    return f"{count}, {'minor' if mi else 'major'}"


def chan_list(chs):
    return ', '.join(str(c) for c in sorted(chs))


def file_rows(k, names):
    """The kit README's table of files: what each plays and on which channels."""
    P, kit = k.P, k.kit
    rows = []
    phrase_of = {phrase_file(kit, ph): ph for ph in k.phrases}
    on = P['bass'].get('on', {})
    for n in names:
        if not n.endswith('.mid'):
            continue
        if '_Loop_' in n:
            v = 'vox' if '_Loop_Vox_' in n else 'novox'
            what = ("the context loop as its club master plays it, lane by lane: " +
                    ('with the vox synth' if v == 'vox' else 'no vox, the accents carry the top'))
            chans = {k.channel(t) for t in k.loop_tracks(v)}
        elif n.endswith('_Drums_4bars.mid'):
            what, chans = "the loop's drums and percussion, its fill bar included", {10}
        elif '_Bassline_' in n:
            name = re.search(r'_Bassline_([A-Za-z0-9]+)', n)[1]
            track = on.get(name, 'Bass')
            what = f'the {name} line' + (" (the loop's)" if name == P['bass']['pick'] else '') + f' on {track}'
            chans = {k.channel(track)}
        elif '_Lead_' in n:
            what, chans = f"the lead phrase, on {words(P['lead']['item'])}", {2}
        elif n.endswith('_Voicing_4bars.mid'):
            what, chans = 'the chord voicing, single notes for your own synth', {3}
        elif '_Chords_' in n:
            what, chans = (f"the chord sample, one note per chord at its root, on "
                           f"{' or '.join(words(i) for i in P['chords']['items'])}"), {4}
        elif '_Accents_' in n:
            what, chans = f"the loop's accents{', no vox' if 'NoVox' in n else ', with the vox synth'}", {6}
        elif n.endswith('_FX_4bars.mid'):
            what, chans = "the loop's FX: its fill", {7}
        elif n in phrase_of:
            ph = phrase_of[n]
            track = phrase_track(k, ph)
            what = f"{ph['name']}, on {words(ph['item'])}"
            chans = {k.channel(track)}
        else:
            raise ValueError(f'no README row for {n}')
        rows.append(f"| `{n}` | {what} | {'channels' if len(chans) > 1 else 'channel'} {chan_list(chans)} |")
    return '\n'.join(rows)


def zone_text(k, ch):
    """A pitched channel's zones as names and MIDI numbers."""
    return ', '.join(f'{sample_name(z)} ({z})' for z in k.zones(ch))


def layer_text(it, vowels=None):
    """An Item's layers as velocities (and the vowel each picks)."""
    if len(it.layers) == 1:
        return 'one layer'
    parts = []
    for i, v in enumerate(it.layers):
        parts.append(f'{math.floor(v * 127 + 0.5)}' + (f" '{vowels[i]}'" if vowels else ''))
    return 'layers at velocity ' + ', '.join(parts) + ' (a velocity plays the nearest, the louder on a tie)'


def instrument_rows(k):
    P = k.P
    vowels = P.get('vox', {}).get('vowels')
    rows = []
    names = {1: 'Bass', 2: 'Lead', 4: 'Chords', 5: 'Vox'}
    names.update({ch: n for n, (ch, _) in k.extras.items()})
    for ch in k.pitched_channels():
        items = k.items_on(ch)
        it = k.item(items[0])
        who = ' (A) or '.join(words(i) for i in items) + (' (B)' if len(items) > 1 else '')
        rows.append(f'| {ch} | {names[ch]} | {who} | {zone_text(k, ch)} | '
                    f'{layer_text(it, vowels if ch == 5 else None)} |')
    return '\n'.join(rows)


def sound_rows(k):
    rows = []
    for role, ch in (('accents', 6), ('fx', 7), ('chops', 8)):
        for key, snd in k.keymap(role).items():
            it = k.item(snd.item)
            how = 'end-aligned: ends on its beat' if 'cut_end' in it.flags else 'starts on its note'
            name = f'{snd.item}@{snd.zone}' if snd.zone is not None else snd.item
            rows.append(f'| {key} | {ch} | {sound_words(snd)} | `{k.wav(name)}` | {how}; {layer_text(it)} |')
    return '\n'.join(rows)


def ride_text(k):
    if not k.rides:
        return 'No filter rides.'
    rows = []
    for r in ride_meta(k):
        if r['law'] == 'log':
            rng = f"logarithmic, 0 = {r['lo']} {r['unit']} to 127 = {r['hi']} {r['unit']}"
        else:
            rng = f"linear, 0 = {r['lo'] / 1000:g} to 127 = {r['hi'] / 1000:g}"
        rows.append(f"- CC{r['cc']} on channel {r['channel']} ({r['track']}): {r['what']}; {rng}.")
    return '\n'.join(rows)


def kit_readme(k, names):
    K, P, kit = k.K, k.P, k.kit
    sf, mi = key_signature(K.KEY)
    drums = '\n'.join(f'| {n} ({note_name(n)}) | {words(name)} | {k.part_track(k.drum_part[n])} |'
                      for n, name in sorted(k.drum_map.items()))
    recorded = P.get('recorded', {})
    rec = ('\n'.join(f'- **{lane}**: {what}' for lane, what in recorded.items())
           if recorded else '- none: every lane plays its multisample.')
    throws = ('a thrown note has CC91 (Space) and CC94 (Echo) at 127 on its tick, before its note-on, and back to 0 '
              'at its note-off (the GS send controllers)')
    return f"""# {kit} "{K.TITLE}": the written music as MIDI

{K.BPM:g} BPM, {K.KEY} ({K.CAMELOT}), swing {round(K.SWING * 100)}% written in: every odd 16th lands {k.late} ticks
late. Made by `tools/sound/midi.py` from the kit's recipes (`{K.RECIPES.replace('.', '/')}/midi.py`) on the channel
map in `docs/midi-map.md`; a test keeps these files equal to a fresh export.

Every file is format 1 at 960 ticks a quarter, in 4/4, with the tempo ({tempo_us(K.BPM)} microseconds a quarter, the
integer nearest {K.BPM:g} BPM) and the key signature (FF 59: {signature_words(sf, mi)}) on its first track.

## The files

| File | What it plays | Channels |
|---|---|---|
{file_rows(k, names)}

The extended mix: `{EXTENDED}/vox/` (with the vox synth) and `{EXTENDED}/novox/` (without), each a \
{arrangement_bars(arrangement(K, P))}-bar mix in {"the kit's" + chr(10) + 'own form' if P.get('form') else
                                                  'the' + chr(10) + "genre's form"}, one region file per section, \
`regions.json` and a README.

## Drums (channel 10)

A drum rack with the kit's samples on these notes (C1 = 36): 120-tick triggers, so play them as one-shots.

| Note | Sample | Track |
|---|---|---|
{drums}

{P['drums'].get('note', '')}

## Pitched instruments

Root each sample file at its MIDI number: the sample names count C4 = 60, the note Ableton and Logic call C3. A note
plays its zone's file repitched by the distance to its root.

| Channel | Track | Instrument | Zones: name (MIDI number) | Velocity |
|---|---|---|---|---|
{instrument_rows(k)}

On channel 3, Chord voicing is the chords as single notes for your own synth; Wubansia ignores channel 3.
{P['bass'].get('note', '')}

## One-key sounds (channels 6, 7 and 8)

Each key plays one sound at its own pitch, from 48 up. A note lasts its sample's length (or the gate the loop gives
it), so a gated pad plays the whole file; an end-aligned sound's note ends on the beat it lands on.

| Key | Channel | Sound | File (in `content/samples/{kit.lower()}/`) | How it plays |
|---|---|---|---|---|
{sound_rows(k)}

## Controllers

Filter rides, every 60 ticks while they move (CC74 cutoff, CC71 resonance):
{ride_text(k)}

Throws: {throws}.

## Lanes Wubansia plays from recorded phrases

Near the kit's tempo (within 2 %), Wubansia plays these lanes from the kit's recorded lines and phrases, sliced at
these files' notes; away from it, and always from MIDI in, they play the multisample above:
{rec}
"""


def phrase_words(ph):
    """A phrase in a sentence: 'Vox choir chords, the velocity picks the vowel' -> 'the vox choir chords'."""
    name = ph['name'].split(',')[0]
    return 'the ' + name[:1].lower() + name[1:]


def describe_line(k, ln, info, got, variant):
    """A line in words (`ln` the line, `info` its regions.json entry, `got` its notes by track), lane by lane: the
    line's own parts on the lane (own_parts(): FUT01's organ riff on channel 12 is not the lead), then the phrases
    playing on it: '<lane> (<phrase>)' where a phrase plays alone on its lane (FUT03's 'lead (the keys arp)', FUT02's
    'chords (the laser stab riff)'), '<part> with <phrase>' beside the lane's part; then the line's moves."""
    parts_ = own_parts(k, ln, variant)
    on = playing_phrases(k, ln, variant)
    phrases = {}                                    # lane: the words of the phrases playing on it
    for track, ph in on.items():
        phrases.setdefault(CH.track_lane(k.kit, track), []).append(phrase_words(ph))
    played = {t for t, v in got.items() if v}
    said = []
    for lane in info['lanes']:
        own, phr = [], phrases.get(lane, [])
        if lane == 'hats':
            whole = {'thin': 'off-beat hats', 'busy': 'busy hats', '16ths': '16th hats'}
            first, _, second = (info.get('hats') or 'full').partition('/')
            own = [whole.get(first, 'hats')]
            if second and second != first:      # a mode per 4-bar block
                half = {'full': 'full', 'thin': 'off-beat', 'busy': 'busy', '16ths': '16ths'}[second]
                own[0] += f', {half} in its second half'
        elif lane == 'snare':
            own = [k.snare_name().lower()]
        elif lane in ('kick', 'perc'):
            own = [lane]
        elif lane == 'bass' and 'bass' in parts_:
            own = [f"bass ({info['bass']})"]
        elif lane == 'lead' and 'lead' in parts_:
            own = [{'call': "the lead's call", 'answer': "the lead's answer",
                    'skeleton': "the lead's skeleton"}.get(info['lead'], 'lead')]
        elif lane == 'chords':
            if 'chords' in parts_:
                own.append(f"chords ({info['chords']})" if info.get('chords') else 'chords')
            if 'pad' in parts_:
                own.append(f"pad ({info['pad']})" if info.get('pad') else 'pad')
        elif lane == 'vox':                     # a phrase on the vox synth plays in place of the vox mode
            vox = (f"vox ({info['vox']})" if 'vox' in parts_ else
                   f"vox ({phrase_words(on['Vox'])})" if 'Vox' in on else None)
            if 'throw' in parts_:
                vox = f'{vox}, thrown' if vox else "the vox's last note thrown"
            own = ([vox] if vox else []) + (['vox chops'] if 'Vox chops' in played and 'Vox chops' not in on else [])
            phr = [phrase_words(on['Vox chops'])] if 'Vox chops' in on else []
        elif lane in ('accents', 'fx'):
            track = track_of_role(lane)
            own = [lane] if track in played and track not in on else []
        if own and phr:
            own[-1] += ' with ' + ' and '.join(phr)
        elif phr:
            own = [f"{lane} ({' and '.join(phr)})"]
        said += own or [lane]
    words_ = [', '.join(said) or 'nothing']
    if info.get('drums'):
        words_.append(f"the drums' {info['drums']} variant")
    for track, m in info.get('ride', {}).items():
        if m == 'loop':
            how = "as the loop's"
        elif m[0] == 'hold':
            how = f'held at {m[1]}'
        elif m[0] == 'ramp':
            how = f'from {m[1]} to {m[2]}'
        else:
            how = ' then '.join(f'{v} at bar {b}' for b, v in m[1])
        words_.append(f'the {track} filter ride {how}')
    if ln.get('phrases_rest') and info.get('end') == 'fill' and on:
        words_.append('the phrases rest from the fill')
    if info.get('roll'):
        lane, bars_ = k.snare_name().lower() if info['roll'] == 'snare' else info['roll'], info['roll_bars']
        words_.append(f"the {lane} rolls over the line's last {bars_} bars up to the fill, rising hit by hit")
    if info.get('rise'):
        r = info['rise']
        words_.append(f"the {r['lane']} rise from {r['from_pm'] / 10:g}% to {r['to_pm'] / 10:g}% velocity")
    if info.get('end'):
        words_.append({'fill': 'the fill in its last bar', 'turn': 'a turnaround in its last bar',
                       'drop': 'the drums out in its last bar'}[info['end']])
    if info.get('fill'):
        words_[-1] = f"the fill ({words(info['fill'])}) in its last bar, the drums out from its start"
    if info.get('turn'):
        words_[-1] = f"a turnaround ({info['turn']}) in its last bar"
    return '; '.join(words_)


def extended_text(k, variant, secs, R):
    K, kit = k.K, k.kit
    other = 'novox' if variant == 'vox' else 'vox'
    rows, detail = [], []
    for s in secs:
        last = s['start_bar'] + s['bars'] - 1
        rows.append(f"| {s['start_bar']}-{last} | {s['name']} | {s['kind']} | `{s['file']}` | "
                    f"{', '.join(s['lanes'])} |")
        detail.append(f"**{s['index']}. {s['name']}** (bars {s['start_bar']}-{last})")
        for ln, said in zip(s['lines'], s['said']):
            detail.append(f"- {ln['start_bar']}-{ln['start_bar'] + LINE - 1}: {said}")
        detail.append('')
    cue_rows = '\n'.join(f"- bar {c['bar']}: {c['cue']}" for s in secs for c in s['cues'])
    tracks = '\n'.join(f"| {t['track']} | {t['channel']} | {t['lane']} | "
                       f"{', '.join(f'{n} {name}' for n, name in t['notes'].items())} |" for t in R['tracks'])
    return f"""# {kit} "{K.TITLE}": the extended mix, {'with the vox synth' if variant == 'vox' else 'no vox'}

The kit's written music arranged as a DJ-friendly extended mix: {R['bars']} bars, {R['length']} at {K.BPM:g} BPM, in
{K.KEY} ({K.CAMELOT}), swing {round(K.SWING * 100)}% written in, every section on an 8-bar line, 32 bars of drums at
each end. Made by `tools/sound/midi.py` from the kit's PARTS on the channel map in `docs/midi-map.md`; `../{other}/` is
the same mix {'without' if variant == 'vox' else 'with'} the vox synth. A test keeps these files equal to a fresh
export.

- `{R['file']}`: the whole mix, a track per instrument, a marker at every section start and cue points (labels: the
  notes play) at the risers, fills and vox entries.
- One region per section, `{secs[0]['file']}` to `{secs[-1]['file']}`: the mix cut at its section lines, with the same
  tracks in the same order, the section's marker and its cues.
- `regions.json`: integers and strings only: the tempo, swing (permille) and length (ms), every section's start bar
  (from 1), length, kind and lanes, line by line, its cues, the tracks and the filter rides.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
{chr(10).join(rows)}

Line by line (bars of the whole mix):

{chr(10).join(detail)}
## The cues

{cue_rows}

A riser's note ends on its section's first beat; a fill's on the next downbeat; the vox plays from a Vox cue.

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); pitched notes as the sample
names count them, C4 = 60. The two meet at MIDI 60: root each sample at its MIDI number. The kit's README (`../../`)
has every instrument's zones and every one-key sound's file.

| Track | Channel | Lane | Notes |
|---|---|---|---|
{tracks}

## On a timeline

Set the project to {K.BPM:g} BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `{R['file']}` on bar 1: a track per instrument, in the order above.
- **Your own order:** drop the region files on bar lines, each where you want its section. A region starts on its first
  downbeat and ends on its last bar line, so regions laid end to end play like the whole mix, except that a note held
  over a section line there (a vox throw into a break, a long one-shot) stops on the line. Every region has the same
  tracks; if a DAW leaves an empty one out, move the clips under it onto the tracks of the same name.
- **Ableton Live:** drag the files into the Arrangement view: a file's tracks come in as clips on consecutive MIDI
  tracks. Clips keep the notes, not their channels: route each track to its instrument (`docs/midi-map.md`).
- **Logic Pro:** File > Open on the whole-mix file brings its tempo, key and section markers with it (an import
  doesn't); File > Import > MIDI File puts a software instrument track per MIDI track at the playhead.

Not checked yet: how the mix sounds, and how each DAW opens these files. Nothing here has been played in a DAW or
listened to.
"""


def readme(k, names):
    return kit_readme(k, names)


def extended_readme(k, variant, secs, R):
    return extended_text(k, variant, secs, R)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--kit', required=True)
    ap.add_argument('--out', help='folder (default: content/midi/<kit>)')
    a = ap.parse_args(argv)
    out = Path(a.out) if a.out else Path(__file__).resolve().parents[2] / 'content' / 'midi' / a.kit.lower()
    try:
        names, stale = write(a.kit, out)
    except (kits.UnknownName, kits.KitNotReady) as e:
        print(f'error: {e}', file=sys.stderr)
        return 2
    print(f'{a.kit}: {len(names)} files -> {out}')
    for name in stale:
        print(f'warning: {out / name} is not part of the export (left as it is)', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
