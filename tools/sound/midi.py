"""Each kit's written loop as Standard MIDI Files: the drums, basslines, lead phrase and chords its context loop plays,
at the kit's tempo with its swing written in, so they play the kit's samples in any DAW; and the same parts arranged as
a DJ-friendly extended mix. A kit opts in with a `midi` module in its recipe package that defines PARTS (see
recipes/fut01/midi.py). Usage, from the repo root:

    .venv/bin/python tools/sound/midi.py --kit FUT02 [--out content/midi/fut02]

Format 1, 960 ticks a quarter, a tempo and 4/4 track first. Every track ends on the clip's last bar line, so a DAW
loops it there. Drums are on channel 10 (C1 = 36 is the kick), short triggers that let the samples ring out. A note
marked as a slide is glided into from the note before, as the kits' recipes play it: that note is held 12 ticks into
it, so a synth set to legato glide slides there (timed()).

The extended mix (extended/, extended_from()) is the kit's genre's form (FORMS below, by its DIRECTION): 224 bars on
8-bar lines, made only from the kit's written parts. Every drop that follows a break comes through its build: a riser
cue whose 8 bars end on the drop's first beat, over a snare or clap roll (quarters, then 8ths, then 16ths, rising)
into the kit's fill, the kick out but in future house. Sci-fi club and future house (the default): intro 32, groove
32, break 16, build 16, drop 32, switch-up 16, break 2 16 (its riser into drop 2), drop 2 32, outro 32. Techno: one
break, long grooves: intro 32, groove 64, break 16, build 16, drop 32 and drop 2 32 back to back, outro 32. UK
garage: a 16-bar intro of drums (the kick under the swung hats and the shaker, then the 2-step beat), the bass and
harmony in a 48-bar groove, then the default's form, its outro ending on the kick, hats and shaker as it began.
It comes as one file with a track per lane, a marker at every section start and cue points for the kit's audio (riser,
fills, vocals), one region file per section, regions.json and a README. PARTS needs no new field for it; these are
optional, each with a default:
- drum_lanes: {note: 'kick', 'snare', 'hats' or 'perc'}, over the General MIDI default (drum_lane());
- fill_bar, fill_swap: the bar of the 4-bar drums that leaves room for the kit's fill (default 4, the context loops'
  last bar) and the bar that plays in its place where no fill is due (default 2);
- switch_line, drop2_line: the switch-up's and drop 2's basslines (default: the last and the first line that isn't
  the pick).
"""
import argparse
import importlib
import json
import re
import struct
import sys
from pathlib import Path

if __package__ in (None, ''):  # run as a script: import the tools.sound package from the repo root
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sound import kits  # noqa: E402

PPQ = 960
S16 = PPQ // 4
SLIDE_TICKS = 12
TRIGGER_NOTE = 60                   # C3: where Ableton's Simpler and most samplers play a sample at its own pitch
DRUM_CHANNEL = 9                    # channel 10
NOTE_NAMES = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
MARKER, CUE_POINT = 0x06, 0x07      # meta event types: a section's name, a cue for the kit's audio


def note_name(n):
    """MIDI note to name with C1 = 36 (the drum-rack convention: C3 = 60)."""
    return f'{NOTE_NAMES[n % 12]}{n // 12 - 2}'


def sample_name(n):
    """MIDI note to name with C4 = 60, as the kits' sample names and DNA write notes."""
    return f'{NOTE_NAMES[n % 12]}{n // 12 - 1}'


def zone_notes(text):
    """A zones text ('F#1, A1, C#2', 'C4 to C6, every 3 semitones') with the MIDI notes of its names added, so the
    sample names (C4 = 60) can't be read an octave off in a DAW that calls MIDI 60 C3."""
    pc = dict(C=0, D=2, E=4, F=5, G=7, A=9, B=11)
    got = [12 * (int(o) + 1) + pc[n] + (s == '#') for n, s, o in re.findall(r'\b([A-G])(#?)(-?\d)\b', text)]
    sep = ' to ' if ' to ' in text and len(got) == 2 else ', '
    return f'{text} (MIDI {sep.join(map(str, got))})' if got else text


# ------------------------------------------------------------------ parts, as recipes write them
def hits(pairs, note, length=0.5):
    """(16th, velocity) pairs as drum notes: short triggers."""
    return [(s, note, length, v, False) for s, v in pairs]


def per_bar(bar_hits, note, bars=4, extra=None, drop=None, length=0.5):
    """One bar's (16th, velocity) hits repeated over `bars`, with per-bar extra hits and dropped 16ths."""
    out = []
    for b in range(bars):
        for s, v in list(bar_hits) + list((extra or {}).get(b, ())):
            if s not in (drop or {}).get(b, ()):
                out.append((16 * b + s, note, length, v, False))
    return out


# ------------------------------------------------------------------ Standard MIDI File
def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def meta(kind, text):
    """A text meta event: a marker (MARKER) or a cue point (CUE_POINT)."""
    data = text.encode()
    return bytes([0xFF, kind]) + vlq(len(data)) + data


def track(name, events, end):
    """A track chunk: its name, then events (tick, order, message), note-offs before note-ons at the same tick, then
    the end of track on tick `end` (or the last event, if later)."""
    data = b'\x00\xff\x03' + vlq(len(name.encode())) + name.encode()
    t = 0
    for tick, _, msg in sorted(events, key=lambda e: (e[0], e[1], e[2])):
        data += vlq(tick - t) + msg
        t = tick
    data += vlq(max(0, end - t)) + b'\xff\x2f\x00'
    return b'MTrk' + struct.pack('>I', len(data)) + data


def timed(notes, swing, repeat=1, period=32):
    """notes (16th, MIDI note, length in 16ths, velocity 0-1, slide), repeated `repeat` times every `period` 16ths, on
    ticks: ([on, off, note, velocity, slide] in the given order, how many slides it joins). Odd 16ths land late by the
    kit's swing. A slide marks a note glided into from the note before (FUT01's "legato from the note before", FUT02's
    "slide from the note before", FUT04's "glides into the note from the one before"): where that note, on another
    pitch, runs up to it (stops at most a 16th before it), it is held SLIDE_TICKS into it. Any other slide plays as
    written: FUT03's glide inside one note, which a MIDI note can't carry."""
    late = round((swing - 0.5) * 2 * S16)
    seq = []
    for r in range(repeat):
        for step, note, length, vel, slide in notes:
            s = step + r * period
            on = s * S16 + (late if s % 2 else 0)
            seq.append([on, on + max(1, round(length * S16)), note, vel, slide])
    order = sorted(range(len(seq)), key=lambda i: (seq[i][0], i))
    joined = 0
    for a, b in zip(order, order[1:]):
        before, slid = seq[a], seq[b]
        if slid[4] and before[2] != slid[2] and before[1] >= slid[0] - S16:
            before[1] = max(before[1], slid[0] + SLIDE_TICKS)
            joined += 1
    return seq, joined


def note_events(notes, swing, channel, repeat=1, period=32):
    """notes: (16th, MIDI note, length in 16ths, velocity 0-1, slide), as track events on the ticks timed() gives."""
    out = []
    for on, off, note, vel, _ in timed(notes, swing, repeat, period)[0]:
        out.append((on, 1, bytes([0x90 | channel, note, max(1, min(127, round(vel * 127)))])))
        out.append((off, 0, bytes([0x80 | channel, note, 0])))
    return out


def smf_tracks(bpm, title, tracks, end, marks=()):
    """A format 1 file of finished tracks, (name, events). The first track holds the title, the tempo, 4/4 and
    `marks`, (tick, meta event) pairs, a tick's marker before its cue points; every track ends on tick `end`."""
    us = round(60_000_000 / bpm)
    head = [(0, 0, b'\xff\x51\x03' + us.to_bytes(3, 'big')), (0, 0, b'\xff\x58\x04\x04\x02\x18\x08')]
    head += [(t, 1 if m[1] == MARKER else 2, m) for t, m in marks]
    chunks = [track(title, head, end)] + [track(n, events, end) for n, events in tracks]
    return b'MThd' + struct.pack('>IHHH', 6, 1, len(chunks), PPQ) + b''.join(chunks)


def smf(bpm, title, tracks, swing):
    """tracks: (name, notes, channel, repeat, period in 16ths). The clip is as long as its longest track."""
    end = max(rep * per for _, _, _, rep, per in tracks) * S16
    return smf_tracks(bpm, title, [(n, note_events(notes, swing, ch, rep, per)) for n, notes, ch, rep, per in tracks],
                      end)


# ------------------------------------------------------------------ a kit's files
def parts(kit):
    """The kit's PARTS (its recipe package's midi module), or None if it has none."""
    K = kits.load(kit)
    try:
        return K, importlib.import_module(f'{K.RECIPES}.midi').PARTS
    except ModuleNotFoundError:
        return K, None


def kit_parts(kit):
    K, P = parts(kit)
    if P is None:
        raise kits.UnknownName(f'{kit} has no midi module in {K.RECIPES}')
    return K, P


PHRASE_CHANNEL = 3                  # channel 4: the synth phrases, apart from the bass (1), lead (2) and chords (3)


def files(kit):
    """{file name: bytes} for every clip of the kit, plus its README.md."""
    K, P = kit_parts(kit)
    return files_from(kit, K, P)


def phrase_file(kit, ph):
    """A synth phrase's file name: `<kit>_Phrase_<sample>_<bars>bars.mid`."""
    return f'{kit}_Phrase_{ph["sample"].replace(" ", "").replace("#", "s")}_{ph.get("bars", 2)}bars.mid'


def chord_trigger(P):
    """The chords' sample trigger notes. A chord sample with one file per chord, each the whole chord rooted at its
    note, gives its own (P['chords']['trigger'], one note per stab on its chord's file, with the 'zones' its files
    are rooted at); a one-file chord sample plays every stab on TRIGGER_NOTE, at the file's own pitch."""
    ch = P['chords']
    if 'trigger' not in ch:
        return sorted({(s, TRIGGER_NOTE, ln, v, False) for s, _, ln, v, _ in ch['notes']})
    if not ch.get('zones'):
        raise ValueError(f"{ch['name']}: a trigger per chord file needs the zones its files are rooted at")
    return sorted(tuple(n) for n in ch['trigger'])


def chords_text(P):
    """The README's paragraph on the chords: the voicing for a synth, and how the chord sample's trigger plays it."""
    ch = P['chords']
    if 'trigger' in ch:
        return (f"**Chords:** the voicing file is for your own synth. The {ch['sample']} sample is one file per chord, "
                'each the whole chord: load them into a sampler as a\nmultisample, each file rooted at its note: '
                f"{zone_notes(ch['zones'])}. The sample trigger file plays one note per stab, on its chord's file.")
    return (f"**Chords:** the voicing file is for your own synth. The {ch['sample']} sample is the whole chord, so play it\n"
            f'with the sample trigger file: one note on {note_name(TRIGGER_NOTE)} (MIDI {TRIGGER_NOTE}), where '
            "Ableton's Simpler and most samplers\nplay a sample at its own pitch.")


def files_from(kit, K, P):
    """files() for a kit module K and its PARTS P. P['phrases'] is optional: a list of synth phrases, each
    dict(name, sample, zones, notes, bars), every one written to its own file on channel 4. P['chords'] may carry a
    trigger of its own (chord_trigger)."""
    title = f'{kit} {K.TITLE}, {K.BPM:g} BPM, {K.KEY}'
    lead, chords = P['lead'], P['chords']
    trigger = chord_trigger(P)
    out = {f'{kit}_Drums_4bars.mid': smf(K.BPM, title, [('Drums', P['drums'], DRUM_CHANNEL, 1, 64)], K.SWING)}
    for name, line_ in P['lines'].items():
        tag = '_InTheLoop' if name == P['pick'] else ''
        out[f'{kit}_Bassline_{name}{tag}_2bars.mid'] = smf(K.BPM, title, [(f'Bass {name}', line_, 0, 1, 32)], K.SWING)
    out[f'{kit}_Lead_{lead["sample"].replace(" ", "")}_2bars.mid'] = smf(
        K.BPM, title, [(lead['name'], lead['notes'], 1, 1, 32)], K.SWING)
    chord_file = chords['sample'].replace(' ', '').replace('#', 's')
    out[f'{kit}_Chords_{chord_file}_Voicing_4bars.mid'] = smf(
        K.BPM, title, [(chords['name'], chords['notes'], 2, 1, 64)], K.SWING)
    out[f'{kit}_Chords_{chord_file}_SampleTrigger_4bars.mid'] = smf(
        K.BPM, title, [('Chord sample trigger', trigger, 2, 1, 64)], K.SWING)
    out[f'{kit}_FullLoop_4bars.mid'] = smf(K.BPM, title, [
        ('Drums', P['drums'], DRUM_CHANNEL, 1, 64), (f'Bass {P["pick"]}', P['lines'][P['pick']], 0, 2, 32),
        (lead['name'], lead['notes'], 1, 2, 32), (chords['name'], chords['notes'], 2, 1, 64)], K.SWING)
    for ph in P.get('phrases', ()):
        out[phrase_file(kit, ph)] = smf(K.BPM, title, [(ph['name'], ph['notes'], PHRASE_CHANNEL, 1,
                                                         16 * ph.get('bars', 2))], K.SWING)
    out['README.md'] = readme(kit, K, P, sorted(out)).encode()
    return out


def slide_text(P, swing):
    """What the README says of the basslines' slides: the ones the MIDI carries and the ones it can't."""
    written = sum(bool(n[4]) for line_ in P['lines'].values() for n in line_)
    joined = sum(timed(line_, swing)[1] for line_ in P['lines'].values())
    if not written:
        return 'The lines have no slides.'
    held = (f'Where a line glides into a note, the note before is held {SLIDE_TICKS} ticks into it, so a synth set to '
            'legato glide slides there.')
    inside = written - joined
    many = inside > 1
    who = f'{inside} more slide' if joined else "The lines' slide"
    within = (f'{who}{"s" if many else ""} glide{"" if many else "s"} inside one note, which a MIDI note can\'t carry: '
              f'{"they play" if many else "it plays"} as written.')
    return ' '.join(([held] if joined else []) + ([within] if inside else []))


def phrases_text(kit, P):
    """The README's paragraph on the synth phrases, or nothing for a kit without them."""
    if not P.get('phrases'):
        return ''
    rows = '\n'.join(f'- `{phrase_file(kit, ph)}`: {ph["name"]} ({ph.get("bars", 2)} bars), on {ph["sample"]}, rooted '
                     f'at {zone_notes(ph["zones"])}' for ph in P['phrases'])
    return (f'\n**Synth phrases** (channel 4): the kit\'s synth riffs as MIDI, each on its own sample:\n\n{rows}\n')


def readme(kit, K, P, names):
    drums = '\n'.join(f'| {n} ({note_name(n)}) | {name} |' for n, name in sorted(P['drum_map'].items()))
    clips = '\n'.join(f'- `{n}`' for n in names if n.endswith('.mid'))
    total = arrangement_bars(arrangement(K))
    return f"""# {kit} "{K.TITLE}": the written loop as MIDI

{K.BPM:g} BPM, {K.KEY} ({K.CAMELOT}), swing {round(K.SWING * 100)}% written in. Made by `tools/sound/midi.py` from the
kit's recipes (`{K.RECIPES.replace('.', '/')}/midi.py`); a test keeps these files equal to a fresh export.

{clips}

**Drums** (channel 10): a drum rack with the kit's samples on these notes (C1 = 36).

| Note | Sample |
|---|---|
{drums}

{P['drum_note']}

**Basslines** (2 bars; `InTheLoop` is the one the context loop plays): load {P['bass_samples']} into a sampler as a
multisample, each file rooted at its note: {zone_notes(P['bass_zones'])}. The sample names count C4 = 60, the note
Ableton and Logic call C3, so root each file by its MIDI number. A is clean, B is warm. {slide_text(P, K.SWING)}

**Lead** (2 bars): {P['lead']['sample']}, rooted at {zone_notes(P['lead']['zones'])}.

{chords_text(P)}

**Full loop** (4 bars): all of the above in one file, the bassline and lead played twice.
{phrases_text(kit, P)}
**Extended mix** (`{EXTENDED}/`): these parts arranged as a DJ-friendly extended mix, {total} bars
({length(total, K.BPM)} at {K.BPM:g} BPM), as one file and one region per section, with `regions.json` and a README on
laying the regions out on a timeline.
"""


# ------------------------------------------------------------------ the extended mix
EXTENDED = 'extended'               # its folder, in the kit's
BAR = 16                            # 16ths a bar
LINE = 8                            # bars a line: every section, and every change in one, starts on a line
BLOCK = 4                           # bars of the written drums and chords
DRUM_LANES = ('kick', 'snare', 'hats', 'perc')
LANES = DRUM_LANES + ('bass', 'lead', 'chords')
OFFBEAT = 2                         # the off-beat 8th's 16th in a beat: thin hats keep only these
TRIGGER_CHANNEL = 3                 # channel 4: the chord sample's trigger, off the voicing's channel 3
CUES = {                            # the kit's audio, which the MIDI leaves to the producer: what each cue asks for
    'Riser': "the kit's riser (`FX/`), 8 bars from this bar, ending on the next section's first beat",
    'Fill': "the kit's fill (`FX/`) in this bar, its end on the next downbeat (a half-bar fill starts on beat 3); the "
            "drums play their written fill bar here, which leaves it room",
    'Vox': "the kit's vox synth riff (`Loops/`) or its vox synth one-shots (`Vox/`) from this bar",
}
ENDS = {'fill': 'fill', 'drop': 'drum drop'}


def line(lanes, **opt):
    """One 8-bar line of a section: its lanes (from LANES, space-separated) and how they play:
    - hats: 'full' (as written, the default), 'thin' (the off-beat 8ths only) or 'busy' (as written, plus the kit's
      closed hat at its softest written velocity on every empty swung 16th of a beat the hats play);
    - bass: the line, 'main' (the pick, the default), 'switch' or 'drop2';
    - lead: 'phrase' (the default) or 'call' (the phrase's first bar, its answer held back);
    - rise: (lane, from, to), the lane's velocities scaled from `from` in the first bar to `to` in the last;
    - roll: a drum lane that rolls (rolled()) instead of playing as written: its own note at its loudest written
      velocity on every quarter for 4 bars, every 8th for 2, every 16th for the last 2 (the last 1 on a fill);
    - end: 'fill' (the last bar plays the drums' written fill bar, with a Fill cue) or 'drop' (the drums sit it out);
    - cue: 'Riser' or 'Vox', on the line's first bar."""
    return dict(lanes=tuple(lanes.split()), **opt)


ROLL = (4, 4, 4, 4, 2, 2, 1, 1)     # the roll's 16ths between hits, bar by bar: quarters, 8ths, then 16ths


def rolled(notes, fill):
    """A drum lane's roll over a line (16ths from its start), from its written notes over the line: its most written
    note (the lowest of a tie) at its loudest written velocity, every ROLL[bar] 16ths; on a fill, the line's last bar
    keeps the written notes there, the fill bar, which leaves the kit's fill its room."""
    if not notes:
        return []
    count = {}
    for n in notes:
        count[n[1]] = count.get(n[1], 0) + 1
    note = min(count, key=lambda k: (-count[k], k))
    vel = max(n[3] for n in notes)
    out = [(b * BAR + s, note, 0.5, vel, False) for b, step in enumerate(ROLL) for s in range(0, BAR, step)
           if not (fill and b == LINE - 1)]
    return sorted(out + ([n for n in notes if n[0] >= (LINE - 1) * BAR] if fill else []))


EVERYTHING = ' '.join(LANES)
DRUMS_BASS = 'kick snare hats perc bass'
GROOVE = DRUMS_BASS + ' chords'
OUTRO = ('Outro', 'outro', (      # the mix-out: the intro backwards, drums only for its last 16 bars
    line(GROOVE),
    line(DRUMS_BASS),
    line('kick hats perc', hats='thin'),
    line('kick')))
BREAK = ('Break', 'break', (      # no kick: the harmony and the whole lead phrase, the vocals; the fill into the build
    line('chords lead', cue='Vox'),
    line('hats chords lead', hats='thin', rise=('hats', 0.4, 1.0), cue='Vox', end='fill')))
BUILD = ('Build', 'build', (      # the drums back but thinned, then the snare or clap rolling up under the riser into
    line('kick snare hats bass chords', hats='thin'),             # the kit's fill, the kick out: the riser cue's 8
    line('snare chords', roll='snare', rise=('snare', 0.55, 1.0), cue='Riser', end='fill')))   # bars end on the drop
BUILD_HOUSE = ('Build', 'build', (   # future house: the same roll with the kick playing on under it
    BUILD[2][0],
    line('kick snare chords', roll='snare', rise=('snare', 0.55, 1.0), cue='Riser', end='fill')))
DROP = ('Drop', 'drop', (         # everything; the drums sit out its last bar before the next section
    line(EVERYTHING, cue='Vox'),
    line(EVERYTHING, end='fill'),
    line(EVERYTHING, cue='Vox'),
    line(EVERYTHING, end='drop')))
DROP_2 = ('Drop 2', 'drop2', (    # everything, on drop 2's bassline with the hats busier
    line(EVERYTHING, hats='busy', bass='drop2', cue='Vox'),
    line(EVERYTHING, hats='busy', bass='drop2', end='fill'),
    line(EVERYTHING, hats='busy', bass='drop2', cue='Vox'),
    line(EVERYTHING, hats='busy', bass='drop2', end='fill')))
CLUB_FORM = (                     # sci-fi club and future house: two breaks, each with its build or riser into a drop
    ('Intro', 'intro', (          # the mix-in: a kick to beatmatch on from bar 1, drums only for 16 bars
        line('kick'),
        line('kick hats perc', hats='thin'),
        line(DRUMS_BASS),
        line(GROOVE, end='fill'))),
    ('Groove', 'groove', (        # the loop's groove; the lead's call joins, its answer saved for the break
        line(GROOVE),
        line(GROOVE, end='fill'),
        line(EVERYTHING, lead='call'),
        line(EVERYTHING, lead='call', end='fill'))),
    BREAK,
    BUILD,
    DROP,
    ('Switch-up', 'switch', (     # the other bassline under the drums alone, the hats busier; the chords come back
        line(DRUMS_BASS, hats='busy', bass='switch'),
        line(GROOVE, hats='busy', bass='switch', end='fill'))),
    ('Break 2', 'break', (        # no kick: the harmony and the vocals, then the lead and the riser into drop 2
        line('chords', cue='Vox'),
        line('hats chords lead', hats='thin', rise=('hats', 0.4, 1.0), cue='Riser'))),
    DROP_2,
    OUTRO,
)
TECHNO_FORM = (                   # dark industrial / techno: long grooves and one break, its build into a 64-bar peak
    ('Intro', 'intro', CLUB_FORM[0][2]),
    ('Groove', 'groove', (        # 64 bars: the groove, the lead's call, then the same with the hats busier
        line(GROOVE),
        line(GROOVE, end='fill'),
        line(EVERYTHING, lead='call'),
        line(EVERYTHING, lead='call', end='fill'),
        line(GROOVE, hats='busy'),
        line(GROOVE, hats='busy', end='fill'),
        line(EVERYTHING, hats='busy', lead='call'),
        line(EVERYTHING, hats='busy', lead='call', end='fill'))),
    BREAK,
    BUILD,
    DROP,                         # the peak, 64 bars: the drop, a bar of no drums, then drop 2 on the other bassline
    DROP_2,
    OUTRO,
)
HOUSE_FORM = tuple(BUILD_HOUSE if s is BUILD else s for s in CLUB_FORM)   # future house: the club form, the kick
                                                                             # playing on under the build's roll
UKG_OUTRO = ('Outro', 'outro', (  # the mix-out: the intro backwards, ending on the kick under the hats and shaker
    line(GROOVE),
    line(DRUMS_BASS),
    line('kick snare hats perc'),
    line('kick hats perc')))
UKG_FORM = (                      # UK garage / 2-step: a 16-bar intro of drums, the bass and harmony in the groove
    ('Intro', 'intro', (          # the mix-in: the 2-step kick under the full swung hats and the shaker (perc), which
        line('kick hats perc'),   # carry the pulse a DJ beatmatches on; then the 2-step beat with its shuffle
        line('kick snare hats perc'))),
    ('Groove', 'groove', (        # 48 bars: the bassline comes in, then the harmony, then the lead's call
        line(DRUMS_BASS),
        line(GROOVE, end='fill'),
        line(GROOVE),
        line(GROOVE, end='fill'),
        line(EVERYTHING, lead='call'),
        line(EVERYTHING, lead='call', end='fill'))),
) + CLUB_FORM[2:-1] + (UKG_OUTRO,)
FORMS = {'default': CLUB_FORM, 'hyper-clean future house': HOUSE_FORM, 'dark industrial / techno': TECHNO_FORM,
         'future garage / 2-step': UKG_FORM, 'garage-house / deep tech house': HOUSE_FORM}
ARRANGEMENT = CLUB_FORM           # the default form


def arrangement(K):
    """The kit's form: its genre's (FORMS, by K.DIRECTION), else the default."""
    return FORMS.get(getattr(K, 'DIRECTION', None), FORMS['default'])


def arrangement_bars(form=ARRANGEMENT):
    return LINE * sum(len(lines) for _, _, lines in form)


def length(bars, bpm):
    """m:ss of `bars` in 4/4 at `bpm`, to the nearest second."""
    m, s = divmod(round(bars * 240 / bpm), 60)
    return f'{m}:{s:02d}'


def drum_lane(note):
    """A drum note's lane by its General MIDI number: 35-36 kick; 37-40 snare and clap (side stick, snare, clap,
    electric snare); 42, 44 and 46 the hats, with the rides (51, 59); anything else perc (toms, cymbals, the kits'
    own hits)."""
    if note in (35, 36):
        return 'kick'
    if 37 <= note <= 40:
        return 'snare'
    if note in (42, 44, 46, 51, 59):
        return 'hats'
    return 'perc'


def material(K, P):
    """What the mix plays, from the kit's PARTS: each drum lane's 4 bars (drums: {lane: notes}), the basslines and
    which one plays where (lines, bass), the lead's 2 bars, the chords' 4 bars and their sample trigger's, the busy
    hats' ghost (note, velocity), the fill bar and its swap, the snare lane's track name, and the tracks (name, key,
    lane, channel), one per lane and the chords' trigger."""
    of = {n: drum_lane(n) for n in P['drum_map']}
    of.update(P.get('drum_lanes', {}))
    drums = {k: [] for k in DRUM_LANES}
    for n in P['drums']:
        drums[of.get(n[1], drum_lane(n[1]))].append(tuple(n))
    drums = {k: sorted(v) for k, v in drums.items()}
    hat_notes = sorted({n for _, n, *_ in drums['hats']})
    closed = 42 if 42 in hat_notes else (hat_notes[0] if hat_notes else 42)
    ghost = (closed, min((v for _, n, _, v, _ in drums['hats'] if n == closed), default=0.2))
    others = [n for n in P['lines'] if n != P['pick']] or [P['pick']]
    bass = {'main': P['pick'], 'switch': P.get('switch_line', others[-1]), 'drop2': P.get('drop2_line', others[0])}
    names = [P['drum_map'].get(n, '').lower() for n in sorted({n for _, n, *_ in drums['snare']})]
    snare = ('Clap' if names and all('clap' in s for s in names) else
             'Snare' if names and all('snare' in s for s in names) else 'Snare/Clap')
    chords = sorted(tuple(n) for n in P['chords']['notes'])
    return dict(
        drums=drums, lines=P['lines'], bass=bass, lead=sorted(tuple(n) for n in P['lead']['notes']), chords=chords,
        trigger=chord_trigger(P), ghost=ghost,
        fill_bar=P.get('fill_bar', BLOCK), swap=P.get('fill_swap', 2), snare=snare,
        tracks=(('Kick', 'kick', 'kick', DRUM_CHANNEL), (snare, 'snare', 'snare', DRUM_CHANNEL),
                ('Hats', 'hats', 'hats', DRUM_CHANNEL), ('Perc', 'perc', 'perc', DRUM_CHANNEL),
                ('Bass', 'bass', 'bass', 0), ('Lead', 'lead', 'lead', 1), ('Chords', 'chords', 'chords', 2),
                ('Chord trigger', 'trigger', 'chords', TRIGGER_CHANNEL)))


def shifted(notes, by):
    return [(n[0] + by,) + tuple(n[1:]) for n in notes]


def block(pattern, fill, fill_bar=BLOCK, swap=2):
    """A 4-bar lane as it plays: as written where a fill is due (its fill bar leaves room for the kit's fill), else
    with bar `swap` again in place of bar `fill_bar`, so the groove runs on."""
    if fill:
        return list(pattern)
    lo, src = BAR * (fill_bar - 1), BAR * (swap - 1)
    keep = [n for n in pattern if not lo <= n[0] < lo + BAR]
    return sorted(keep + shifted([n for n in pattern if src <= n[0] < src + BAR], lo - src))


def blocks(pattern, M, fill):
    """A 4-bar lane over a line: its blocks one after another, the last as written when the line ends on a fill."""
    n = LINE // BLOCK
    return [x for i in range(n)
            for x in shifted(block(pattern, fill and i == n - 1, M['fill_bar'], M['swap']), i * BLOCK * BAR)]


def looped(pattern, call=False, period=2 * BAR):
    """A 2-bar lane over a line; with `call`, the first bar of each pass alone."""
    one = [n for n in pattern if not call or n[0] < BAR]
    return [x for r in range(LINE * BAR // period) for x in shifted(one, r * period)]


def hat_density(notes, mode, ghost):
    """The hats 'thin' (the off-beat 8ths), 'full' (as written) or 'busy' (plus a ghost on every empty swung 16th of
    each beat they play: the "e" and the "a")."""
    if mode == 'thin':
        return [n for n in notes if n[0] % 4 == OFFBEAT]
    if mode == 'busy':
        taken = {n[0] for n in notes}
        beats = sorted({n[0] // 4 for n in notes})
        return sorted(notes + [(s, ghost[0], 0.5, ghost[1], False)
                               for b in beats for s in (4 * b + 1, 4 * b + 3) if s not in taken])
    return notes


def keys_of(lane):
    """The track keys a lane plays on."""
    return ('chords', 'trigger') if lane == 'chords' else (lane,)


def line_notes(M, ln):
    """One line as {track key: notes}, 16ths from its start."""
    fill = ln.get('end') == 'fill'
    out = {}
    for lane in ln['lanes']:
        if lane in DRUM_LANES:
            out[lane] = blocks(M['drums'][lane], M, fill)
        elif lane == 'chords':
            out['chords'], out['trigger'] = blocks(M['chords'], M, fill), blocks(M['trigger'], M, fill)
        elif lane == 'bass':
            out['bass'] = looped(M['lines'][M['bass'][ln.get('bass', 'main')]])
        elif lane == 'lead':
            out['lead'] = looped(M['lead'], call=ln.get('lead') == 'call')
    if 'hats' in out:
        out['hats'] = hat_density(out['hats'], ln.get('hats', 'full'), M['ghost'])
    if ln.get('roll') in out:
        out[ln['roll']] = rolled(out[ln['roll']], fill)
    if 'rise' in ln:
        lane, a, b = ln['rise']
        for k in keys_of(lane):
            if k in out:
                out[k] = [(s, n, d, v * (a + (b - a) * (s // BAR) / (LINE - 1)), sl) for s, n, d, v, sl in out[k]]
    if ln.get('end') == 'drop':
        for k in DRUM_LANES:
            if k in out:
                out[k] = [n for n in out[k] if n[0] < (LINE - 1) * BAR]
    return out


def line_info(M, ln, got, start):
    """regions.json's entry for a line: where it starts, the lanes with notes in it and how they play."""
    lanes = [k for k in LANES if any(got.get(key) for key in keys_of(k))]
    info = dict(start_bar=start, bars=LINE, lanes=lanes)
    if 'hats' in lanes:
        info['hats'] = ln.get('hats', 'full')
    if 'bass' in lanes:
        info['bass'] = M['bass'][ln.get('bass', 'main')]
    if 'lead' in lanes:
        info['lead'] = ln.get('lead', 'phrase')
    if ln.get('roll') in lanes:
        info['roll'] = ln['roll']
    if 'rise' in ln:
        info['rise'] = {'lane': ln['rise'][0], 'from': ln['rise'][1], 'to': ln['rise'][2]}
    if ln.get('end'):
        info['end'] = ENDS[ln['end']]
    return info


def stem(name):
    """A section's name for its file: 'Switch-up' -> 'SwitchUp', 'Drop 2' -> 'Drop2'."""
    return ''.join(w[:1].upper() + w[1:] for w in re.split(r'[\s-]+', name))


def plan(K, P):
    """(material, sections): each section's index, name, kind, start bar, bars, region file, notes per track key (16ths
    from its start), lanes, lines (regions.json's entries) and cues ({bar, cue}, the whole mix's bars)."""
    M = material(K, P)
    out, at = [], 1
    for i, (name, kind, lines) in enumerate(arrangement(K), 1):
        notes = {key: [] for _, key, _, _ in M['tracks']}
        info, cues = [], []
        for j, ln in enumerate(lines):
            start = at + j * LINE
            got = line_notes(M, ln)
            for k, v in got.items():
                notes[k] += shifted(v, j * LINE * BAR)
            info.append(line_info(M, ln, got, start))
            if ln.get('cue'):
                cues.append(dict(bar=start, cue=ln['cue']))
            if ln.get('end') == 'fill':
                cues.append(dict(bar=start + LINE - 1, cue='Fill'))
        bars = LINE * len(lines)
        out.append(dict(index=i, name=name, kind=kind, start_bar=at, bars=bars,
                        file=f'{i:02d}_{stem(name)}_{bars}bars.mid', notes=notes,
                        lanes=[k for k in LANES if any(k in x['lanes'] for x in info)], lines=info, cues=cues))
        at += bars
    return M, out


def section_tracks(K, M, sec):
    """A section's tracks as (name, events), on ticks from its start; a note-off past its end is pulled to its end, so
    the region ends on its last bar line and the regions tile the whole mix."""
    end = sec['bars'] * 4 * PPQ
    out = []
    for name, key, _, channel in M['tracks']:
        events = note_events(sec['notes'][key], K.SWING, channel)
        assert all(t < end for t, order, _ in events if order == 1), (sec['name'], name)
        out.append((name, [(min(t, end), order, m) for t, order, m in events]))
    return out


def section_marks(sec):
    """The section's marker on its first tick, then its cue points."""
    bar = 4 * PPQ
    return [(0, meta(MARKER, sec['name']))] + [((c['bar'] - sec['start_bar']) * bar, meta(CUE_POINT, c['cue']))
                                               for c in sec['cues']]


def track_notes(M, P, secs, key, lane):
    """regions.json's {MIDI note: name} of what a track plays in the mix: the drums and the chord trigger by sample
    (a chord sample with one file per chord by its file, as the sample names write it), the rest by note (C4 = 60)."""
    got = sorted({n for s in secs for _, n, *_ in s['notes'][key]})
    if key == 'trigger':
        ch = P['chords']
        return {str(n): f"{ch['sample']} {sample_name(n)}" if 'trigger' in ch else ch['sample'] for n in got}
    return {str(n): P['drum_map'].get(n, note_name(n)) if lane in DRUM_LANES else sample_name(n) for n in got}


def extended_from(kit, K, P):
    """{file name: bytes} of the kit's extended mix, from its kit module K and PARTS P: the whole mix, one region per
    section, regions.json and README.md."""
    M, secs = plan(K, P)
    bar = 4 * PPQ
    title = f'{kit} {K.TITLE}, {K.BPM:g} BPM, {K.KEY}, extended mix'
    total = sum(s['bars'] for s in secs)
    whole_name = f'{kit}_ExtendedMix_{total}bars.mid'
    out, whole, marks = {}, {name: [] for name, *_ in M['tracks']}, []
    for s in secs:
        tracks, sm = section_tracks(K, M, s), section_marks(s)
        out[s['file']] = smf_tracks(K.BPM, f'{title}: {s["name"]}', tracks, s['bars'] * bar, sm)
        off = (s['start_bar'] - 1) * bar
        for name, events in tracks:
            whole[name] += [(t + off, order, m) for t, order, m in events]
        marks += [(t + off, m) for t, m in sm]
    out[whole_name] = smf_tracks(K.BPM, title, list(whole.items()), total * bar, marks)
    seconds = round(total * 240 / K.BPM, 2)
    R = dict(kit=kit, title=K.TITLE, bpm=K.BPM, key=K.KEY, camelot=K.CAMELOT, swing=K.SWING, ppq=PPQ, bars=total,
             seconds=seconds, length=length(total, K.BPM), file=whole_name, note_names='C4 = 60, as the sample names',
             basslines=M['bass'],
             tracks=[dict(track=name, lane=lane, channel=channel + 1, notes=track_notes(M, P, secs, key, lane))
                     for name, key, lane, channel in M['tracks']],
             sections=[{k: s[k] for k in ('index', 'name', 'kind', 'start_bar', 'bars', 'file', 'lanes', 'lines',
                                          'cues')} for s in secs])
    out['regions.json'] = (json.dumps(R, indent=2) + '\n').encode()
    out['README.md'] = extended_readme(kit, K, P, M, secs, R).encode()
    return out


def extended(kit):
    """{file name: bytes} of the kit's extended mix (the extended/ folder)."""
    K, P = kit_parts(kit)
    return extended_from(kit, K, P)


# ------------------------------------------------------------------ the extended mix's README
def lane_word(M, lane):
    return M['snare'].lower() if lane == 'snare' else lane


def label(M, lane, info):
    """A lane as a line plays it: 'off-beat hats', 'bass (Walk)', "the lead's call"."""
    if lane == 'hats':
        return {'thin': 'off-beat hats', 'busy': 'busy hats'}.get(info.get('hats'), 'hats')
    if lane == 'bass':
        return f'bass ({info["bass"]})'
    if lane == 'lead':
        return "the lead's call (its first bar)" if info.get('lead') == 'call' else 'lead'
    return lane_word(M, lane)


def describe(M, have, prev, info):
    """What a line plays, against the line before it in its section."""
    words = []
    if prev is None and info['lanes'] == have:
        extra = [label(M, k, info) for k in ('hats', 'bass', 'lead')
                 if k in info['lanes'] and (k == 'bass' or info[k] not in ('full', 'phrase'))]
        words.append(', '.join(['every lane'] + extra))
    elif prev is None:
        words.append(', '.join(label(M, k, info) for k in info['lanes']))
    else:
        added = [label(M, k, info) for k in info['lanes'] if k not in prev['lanes']]
        gone = [lane_word(M, k) for k in prev['lanes'] if k not in info['lanes']]
        now = {'hats': {'thin': 'off-beat hats', 'busy': 'busy hats'}.get(info.get('hats'), 'full hats'),
               'bass': f'bass: {info.get("bass")}',
               'lead': "the lead's call" if info.get('lead') == 'call' else 'the whole lead phrase'}
        changed = [now[k] for k in ('hats', 'bass', 'lead')
                   if k in info['lanes'] and k in prev['lanes'] and info[k] != prev[k]]
        words += (['+ ' + ', '.join(added)] if added else []) + (['\u2212 ' + ', '.join(gone)] if gone else [])
        words += changed or ([] if added or gone else ['as before'])
    if 'roll' in info:
        words.append(f'the {lane_word(M, info["roll"])} rolls: every quarter for 4 bars, every 8th for 2, then every '
                     '16th' + (', into its fill bar' if info.get('end') == 'fill' else ''))
    if 'rise' in info:
        r = info['rise']
        lane = lane_word(M, r['lane'])
        top = 'full' if r['to'] == 1.0 else f'{round(r["to"] * 100)}%'
        words.append(f'the {lane} rise{"" if lane == "hats" else "s"} from {round(r["from"] * 100)}% to {top} velocity')
    return words


def and_list(words):
    words = list(words)
    return ', '.join(words[:-1]) + (' and ' if len(words) > 1 else '') + words[-1] if words else ''


def bass_uses(secs):
    """[(line, [sections it plays in])], in the order the lines first play."""
    uses = {}
    for s in secs:
        for ln in s['lines']:
            if 'bass' in ln and s['name'] not in uses.setdefault(ln['bass'], []):
                uses[ln['bass']].append(s['name'])
    return list(uses.items())


def the(name):
    """A section's name in a sentence: 'the switch-up', but 'drop 2'."""
    return name.lower() if name[-1].isdigit() else f'the {name.lower()}'


def busy_where(sec):
    """Where in a section the hats play busy: 'the switch-up', 'drop 2', "the groove's second half"."""
    name = the(sec['name'])
    busy = [ln.get('hats') == 'busy' for ln in sec['lines']]
    if all(busy):
        return name
    half = len(busy) // 2
    return f"{name}'s second half" if busy == [False] * half + [True] * (len(busy) - half) else f'parts of {name}'


def extended_readme(kit, K, P, M, secs, R):
    have = [k for k in LANES if any(s['notes'][key] for s in secs for key in keys_of(k))]
    uses = bass_uses(secs)
    pick = " (the context loop's pick)"
    bass_text = '; '.join(f'{name}{pick if name == M["bass"]["main"] else ""}: {and_list(n.lower() for n in where)}'
                          for name, where in uses)
    rows, detail = [], []
    for s in secs:
        last = s['start_bar'] + s['bars'] - 1
        rows.append(f'| {s["start_bar"]}-{last} | {s["name"]} | {s["kind"]} | `{s["file"]}` | '
                    f'{", ".join(lane_word(M, k) for k in s["lanes"])} |')
        detail.append(f'**{s["index"]}. {s["name"]}** (bars {s["start_bar"]}-{last}, `{s["file"]}`)')
        prev = None
        for ln in s['lines']:
            words = describe(M, have, prev, ln)
            at = {c['bar']: c['cue'] for c in s['cues'] if ln['start_bar'] <= c['bar'] < ln['start_bar'] + LINE}
            for bar_, cue in sorted(at.items()):
                words.append(f'fill in bar {bar_}' if cue == 'Fill' else f'{cue.lower()} cue')
            if ln.get('end') == 'drum drop':
                words.append(f'drums out in bar {ln["start_bar"] + LINE - 1}')
            detail.append(f'- {ln["start_bar"]}-{ln["start_bar"] + LINE - 1}: {"; ".join(words)}')
            prev = ln
        detail.append('')
    cue_bars = {c: [x['bar'] for s in secs for x in s['cues'] if x['cue'] == c] for c in CUES}
    cues = '\n'.join(f'- **{c}** (bar{"s" if len(b) > 1 else ""} {", ".join(map(str, b))}): {CUES[c]}.'
                     for c, b in cue_bars.items() if b)
    def plays(name, key, lane):
        notes = R['tracks'][[t['track'] for t in R['tracks']].index(name)]['notes']
        if lane in DRUM_LANES:
            return ', '.join(f'{n} ({note_name(int(n))}) {s}' for n, s in notes.items()) or 'nothing in this kit'
        if key == 'bass':
            return f'{P["bass_samples"]}, rooted at {zone_notes(P["bass_zones"])}: {bass_text}'
        if key == 'lead':
            return f'{P["lead"]["sample"]}, rooted at {zone_notes(P["lead"]["zones"])}'
        if key == 'chords':
            return f'{P["chords"]["name"]}: the voicing, for your own synth'
        if 'trigger' in P['chords']:
            return (f'the {P["chords"]["sample"]} sample, one file per chord, each the whole chord, rooted at '
                    f'{zone_notes(P["chords"]["zones"])}: one note per stab, on its chord\'s file')
        return (f'the {P["chords"]["sample"]} sample, the whole chord: one note on {note_name(TRIGGER_NOTE)} '
                f'(MIDI {TRIGGER_NOTE}) per chord, where samplers play a sample at its own pitch')
    tracks = '\n'.join(f'| {name} | {channel + 1} | {plays(name, key, lane)} |'
                       for name, key, lane, channel in M['tracks'])
    snare = lane_word(M, 'snare')
    rolls = [ln for s in secs if s['kind'] == 'build' for ln in s['lines'] if ln.get('roll')]
    kick_on = bool(rolls) and all('kick' in ln['lanes'] for ln in rolls)
    roll_kick = ('The kick plays on under it, as future house builds keep theirs.' if kick_on else
                 "The kick sits it out, so the drop's first kick lands after 8 bars without one.")
    sample_dir = f'content/samples/{kit.lower()}/'
    busy = and_list(busy_where(s) for s in secs if any(ln.get('hats') == 'busy' for ln in s['lines']))
    after_drop = [the(secs[i + 1]['name']) for i, s in enumerate(secs[:-1]) if s['kind'] == 'drop'][0]
    return f"""# {kit} "{K.TITLE}": the extended mix

The kit's written loop arranged as a DJ-friendly extended mix: {R['bars']} bars, {R['length']} at {K.BPM:g} BPM, in
{K.KEY} ({K.CAMELOT}), swing {round(K.SWING * 100)}% written in, every section on an 8-bar line. Made by
`tools/sound/midi.py` from the kit's written parts only (the drums, basslines, lead and chords in `../`, whose README
has the drum map); a test keeps these files equal to a fresh export.

- `{R['file']}`: the whole mix, a track per lane, a marker at every section start, and cue points (MIDI cue point
  events) where the kit's riser, fills and vocals go.
- One region per section, `{secs[0]['file']}` to `{secs[-1]['file']}`: the whole mix cut at its section lines, with
  the same tracks, the section's marker and its cues.
- `regions.json`: every section's start bar, length, kind and lanes, line by line, with its cues and the tracks.

## The sections

| Bars | Section | Kind | Region | Lanes |
|---|---|---|---|---|
{chr(10).join(rows)}

Line by line (bars of the whole mix):

{chr(10).join(detail)}
## The cues

The MIDI plays the kit's drums, bass, lead and chords. Its riser, fills and vocals are audio, in `{sample_dir}`;
the cue points, and this list, say where they go:
{cues}

## Tracks

Drums are named as drum racks name them (C1 = 36, so C3 = 60, as Ableton and Logic count); the samples' roots are
named as the sample names count, C4 = 60. The two meet at MIDI 60: root each file by its MIDI number.

| Track | Channel | Load and play |
|---|---|---|
{tracks}

The four drum tracks share the kit's drum map on channel 10: give each the kit's drum rack, or move their notes onto
one drum track. The closed and open hats share the Hats track, so the rack's choke group still works.

## On a timeline

Set the project to {K.BPM:g} BPM in 4/4 first: every file carries the tempo, but a DAW doesn't always take it from a
dropped file.
- **The whole mix:** drop `{R['file']}` on bar 1. It comes in as a track per lane, in the order above. Its section
  markers come in where the DAW reads MIDI markers; the table above has every section's bars either way.
- **Your own order:** drop the region files on bar lines, each where you want that section. A region starts on its
  first downbeat and ends on its last bar line, so regions laid end to end play exactly like the whole mix. Every
  region has the same eight tracks in the same order; a lane that rests for a whole section is an empty track there.
  If a DAW leaves an empty track out, the lanes under it come in one track up: drag those clips onto their tracks.
- **Ableton Live:** drag the files into the Arrangement view: a file's tracks come in as clips on consecutive MIDI
  tracks, all starting where you drop it. Set the locators from the section table.
- **Logic Pro:** open the whole-mix file (File > Open) to bring its tempo and section markers with it: Logic reads
  those when it opens a MIDI file, not when it imports one. Into a project, drag a file into the Tracks area or use
  File > Import > MIDI File: a software instrument track per MIDI track, at the playhead.

## How it uses the loop

Every note is one of the kit's written notes, arranged; only the build's roll plays one (the {snare}'s) on a rhythm
the loop doesn't write:
- **The fill bar.** The written drums' bar {M['fill_bar']} leaves room for the kit's fill, as in the context loop.
  Where no fill is due, bar {M['swap']} plays in its place, so the groove runs on; at a Fill cue the four bars play as
  written.
- **Hats:** as written; off-beat (the off-beat 8ths alone); busy (as written, plus the kit's closed hat at its softest
  written velocity on every empty "e" and "a" of a beat the hats play), in {busy}.
- **Bass:** {bass_text}.
- **Lead:** in the groove only its call, the phrase's first bar; the whole phrase in the breaks and the drops.
- **The build's roll:** over the build's last 8 bars, under the riser, the {snare} rolls on its own note, every quarter
  for 4 bars, every 8th for 2, every 16th for 1, then the written fill bar for the kit's fill, whose end is the drop's
  first beat. {roll_kick}
- **Rises:** in the build's roll the {snare} climbs from 55% to full velocity, bar by bar; in the breaks the off-beat
  hats climb from 40%.
- **Drum drop:** the drums sit out the drop's last bar, before {after_drop}.
- **Slides:** {slide_text(P, K.SWING)}

Not checked yet: how the mix sounds, and how each DAW opens these files. Nothing here has been played in a DAW or
listened to.
"""


def write(kit, out_dir):
    """Write the kit's loop files to `out_dir`, its extended mix to `out_dir`/extended; return (the files written,
    the files there that it didn't make)."""
    made = {**files(kit), **{f'{EXTENDED}/{n}': d for n, d in extended(kit).items()}}
    (out_dir / EXTENDED).mkdir(parents=True, exist_ok=True)
    for name, data in made.items():
        (out_dir / name).write_bytes(data)
    stale = sorted(str(p.relative_to(out_dir)) for d in (out_dir, out_dir / EXTENDED) for p in d.iterdir()
                   if p.is_file() and str(p.relative_to(out_dir)) not in made)
    return sorted(made), stale


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
