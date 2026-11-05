"""FUT02's written music for the MIDI export (tools/sound/midi.py, MIDI v2; the schema: tools/sound/README.md, "MIDI").

- The loop (Loop_Vox, Loop_NoVox): the round 5 context loop (context5.py on context.py) lane by lane: four on the floor
  (out on beat 4 of bar 4), the clap on 2 and 4 with its ghosts (none in bar 4), the closed and open hats in bars 1-3
  (bar 4 is the laser fill's), the four zaps; the Orbit line twice, muted under the drop glide (channel 11), struck on
  bar 4's downbeat and lifted 60 ms before beat 3's kick; the chrome arp twice, resting in the fill bar; the starfield
  pad held on D3 (its root); the vox synth riff from bar 2, its throw (CC91/CC94) into the fill bar; the variant's
  accents; the laser fill, ending on the loop's downbeat; the arp's filter ride (CC74).
- The parts: the two rolling basslines, the arp phrase, the pad on its root and its Dm9 voicing, and the synth phrases:
  the vox synth riff and the vox choir chords (channel 5; the velocity picks the vowel: 64 'oh', 95 'ah', 127 'ee'),
  the laser stab riff (channel 12) and the glass melody (channel 13).
- The extended mix: FUT02's own form (FORM, below), 224 bars of sci-fi club in the club form's sections, built from the
  loop and the phrases as recorded (subsets of them: the arp's skeleton and call, the riff's call and answer) and a
  little written drum material (BUSY, TURN, the zap figures PINGPONG and TRESILLO). In short: 32 bars of drums; the
  groove on Orbit and the arp's skeleton under a closed filter, no pad; break 1 the pad with the choir chords (the vox
  mix) and the arp's call; the build opening the arp's filter under a clap roll and the riser; the drop the loop as
  recorded (the full arp, the vox's pattern, an impact), then the laser stab riff moving the harmony to B-flat maj9,
  the vox calling over its Dm9 bars; a switch-up on Orbit with the arp out and a 3-3-2 zap figure; break 2 the choir,
  then the stab riff, the vox's call and the riser; drop 2 on Relay, the vox calling and the glass melody answering,
  then the riff's answer to the arp's call, stripping to the stab riff; 32 bars of drums of its own. FORM's comments
  have every line.
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide).
"""
import math

from tools.sound import channels as CH
from tools.sound import kits
from tools.sound.midi import DRUMS, Hit, Key, Note, hits, line, section, twice
from tools.sound.recipes.fut02 import accents as A, bass as B, context as C, context5 as C5, lead as L, pad as Pd
from tools.sound.recipes.fut02 import voxsynth as Vs

K = kits.module('FUT02')
VOX_ZONES = 'D4, F4, A4, C5, F5, A5'
STAB = {62: 'LaserChordStabDm9', 58: 'LaserChordStabBbmaj9'}   # the laser chord stab's two files: D4 and B-flat 3
GLIDE_TICKS = 8 * 240 - round(C.GLIDE_EARLY * K.BPM / 60 * 960)   # struck on 16th 48, lifted 60 ms before 56's kick


def sound(item, zone):
    return STAB[zone] if item == 'LaserChordStab' else item


def loop_accents(version):
    """context5's accents for a version as hits: (16th, item, zone, layer, ..., ends on the 16th)."""
    return [Hit(row[0], sound(row[1], row[2]), row[3], 'end' if row[-1] else 'on') for row in C5.ACCENTS[version]]


def arp_ride():
    """The arp's filter ride as CC74 every 60 ticks over the 4 bars, from context.ride_hz: from 1.6 kHz on bar 1's
    downbeat up to 16 kHz at 16th 46, a raised cosine in log frequency, and back by the loop's end; on
    channels.cc_value's law over 1.6-16 kHz. Its Q is fixed (1.4): no CC71."""
    lo, hi = C.ARP_RIDE['lo_hz'], C.ARP_RIDE['hi_hz']
    top, end = C.ARP_RIDE['top_step'] * 240, 64 * 240
    out = []
    for i in range(256):
        t = i * 60
        if t < top:
            u = 0.5 - 0.5 * math.cos(math.pi * t / top)
        else:
            u = 0.5 + 0.5 * math.cos(math.pi * (t - top) / (end - top))
        out.append(CH.cc_value(lo * (hi / lo) ** u, lo, hi))
    return out


def orbit_loop():
    """The Orbit line twice, muted where the drop glide plays (from 16th 48 to the kick on 56; the line rests on both
    kicks)."""
    return [n for n in twice(B.LINES[B.PICK]) if not C.GLIDE[0] <= n.step < C.GLIDE[1]]


def lead_notes():
    return [(s, n, ln, v, False) for s, n, ln, v in L.phrase_notes()]


def arp_loop():
    """The arp twice, resting from the fill bar (context.ARP_REST): a note in bar 3 ends by its downbeat."""
    rest = C.ARP_REST[0]
    return [n._replace(length=min(n.length, rest - n.step)) for n in twice(lead_notes()) if n.step < rest]


# ------------------------------------------------------------------ the extended mix's own material
SKELETON_STEPS = (0, 3, 6, 8, 11, 14)     # the arp's 3-3-2 accents, 16ths of a bar


def skeleton():
    """The arp's skeleton: the phrase's notes on its 3-3-2 accents alone (F5 A5 G5 F5 E5 C5 | F5 A5 G5 C6 A5 E5), as
    recorded, without the D5 and A4 between them: the groove's arp (audit B), and the arp a third over the vox's call,
    whose register (A4-A5) the drone would crowd."""
    return [n for n in lead_notes() if n[0] % 16 in SKELETON_STEPS]


def zaps(bars):
    """A 4-bar zap figure (drum note 47) from each bar's (16th, velocity) hits."""
    return hits([(16 * b + s, v) for b, bar in enumerate(bars) for s, v in bar], 47)


# The drums the form adds to the loop's (the velocities pick the files' layers, 0.2 to 1.0 in fifths):
# - BUSY, a busy line's hats: a 32nd closed-hat grace before every off-beat open hat, the last before the loudest, so
#   the open hats still ring (the loop's hats leave only one 16th a bar empty, and a closed hat there would choke them);
# - TURN, a line's last bar where it turns over: a 32nd clap drag into the downbeat (the laser fill's own claps, quieter
#   and shorter), the zaps answering the backbeat on the 'and' of 2, the 'and' of 3 and the 'e' of 4;
# - two zap figures for a line's perc (drums='pingpong' or 'tresillo'), so the drops' percussion turns over every 16
#   bars against the loop's four zaps: PINGPONG, the zaps on the off-beat 8ths, bar against bar, a pickup at the end;
#   TRESILLO, the arp's 3-3-2 taken over by the zaps (the switch-up's, where the arp is out): its first half in the odd
#   bars, its second in the even ones
BUSY = hits(((1.5, 0.35), (5.5, 0.35), (9.5, 0.35), (13.5, 0.45)), 42)
TURN = {'snare': hits(((4, 1.0), (12, 1.0), (14.5, 0.4), (15, 0.6), (15.5, 0.8)), 38),
        'perc': hits(((6, 0.6), (10, 0.8), (13, 1.0)), 47)}
PINGPONG = zaps((((2, 0.6), (10, 0.6)), ((6, 0.8), (14, 0.6)), ((2, 0.6), (10, 0.6)), ((6, 0.8), (9, 0.6), (14, 1.0))))
TRESILLO = zaps((((0, 0.8), (3, 0.6), (6, 0.6)), ((8, 0.6), (11, 0.6), (14, 0.8)),
                 ((0, 0.8), (3, 0.6), (6, 0.6)), ((8, 0.6), (11, 0.8), (14, 1.0))))

# the phrases a line plays, by their names in PARTS['phrases']
RIFF = 'Vox synth riff, the velocity picks the vowel (soft oh, medium ah, hard ee)'
STABS = 'Laser stab riff, Dm9 and B-flat maj9'
GLASS = 'Glass melody'
CHOIR = 'Vox choir chords, the velocity picks the vowel'


def hold(v):
    """The arp's filter held at a CC74 value (1.6 kHz x 10^(v / 127))."""
    return {'Lead': ('hold', v)}


def ramp(a, b):
    """The arp's filter ridden from one CC74 value to another over the line."""
    return {'Lead': ('ramp', a, b)}


# FUT02's extended mix, both variants (the no-vox mix has no vox, no choir, and its own accents). Rules it keeps:
# - the hook is held back: one vox throw in the groove, the choir in the break, the riff itself only from the drop;
# - where the vox sings more than the loop's pattern, the arp plays its skeleton (a third over the vox), its call (in
#   the bars the vox leaves) or rests; the vox calls where the stab riff plays (its call sits on the stabs' Dm9 bars,
#   the riff's E5 would be a tritone against their B-flat maj9) and where the glass melody plays (the riff's E5 is a
#   minor ninth under the glass's F6 in the melody's second bar), and never sings over the pad (whose E5 rubs the F5);
# - the pad plays where nothing plays B-flat: the B-flat maj9 stabs' root B-flat 3 is a semitone over its A3;
# - the arp's filter ride (CC74): closed and held low in the groove (12, then 20: 2-2.3 kHz), opening slowly into the
#   break, from closed to fully open across the build, the loop's own ride in the drops;
# - the laser fill ends the lines into the breaks, the build and the drops; a turnaround (TURN) every other line;
#   the stab cue (B-flat maj9 on the 'and' of 4, the VI into the i) where no pad and no arp's E5 meet it.
GROOVE = DRUMS + ' bass'
FORM = (
    section('Intro', 'intro', (                       # 1-32: drums to mix in on, the kick alone first
        line('kick'),                                 # 1: the kick
        line('kick hats perc', hats='thin', end='turn'),                                  # 9: open hats, the zaps
        line(DRUMS, end='turn'),                                                          # 17: the clap, closed 16ths
        line(DRUMS, hats='busy', end='turn'))),                                           # 25: the hats' 32nds
    section('Groove', 'groove', (                     # 33-64: the Orbit line, the arp's skeleton, closed; no pad
        line(GROOVE, stab=True, end='turn'),                                              # 33: Orbit; a B-flat stab
        line(GROOVE + ' lead', lead='skeleton', ride=hold(12), accents='alternate', end='fill'),             # 41
        line(GROOVE + ' lead', hats='busy', lead='skeleton', ride=hold(20), accents='alternate', end='turn'),  # 49
        line(GROOVE + ' lead', lead='skeleton', ride=ramp(20, 40), accents='loop', throw=True, end='fill'))),  # 57
    section('Break', 'break', (                       # 65-80: the pad, the choir's chords (vox), the arp's call
        line('chords lead vox', lead='call', ride=hold(48), phrases=(CHOIR,), accents='loop', cue='Vox'),     # 65
        line('hats chords lead vox', hats='thin', rise=('hats', 0.4, 1.0), lead='call', ride=ramp(48, 96),
             phrases=(CHOIR,), phrases_rest=True, accents='lead-in', end='fill'))),                         # 73
    section('Build', 'build', (                       # 81-96: the full arp opening from closed, the clap rolling
        line('kick snare hats bass chords lead', hats='thin', ride=ramp(0, 64), end='turn'),                  # 81
        line('snare hats chords lead', hats=('thin', '16ths'), ride=ramp(64, 127), cue='Riser', end='fill',
             roll=dict(lane='snare', bars=7, every=((2, 8), (2, 4), (2, 2), (1, 1)), ramp=(0.45, 1.0))))),   # 89
    section('Drop', 'drop', (                         # 97-128: the loop as recorded, then the harmony moves
        line(GROOVE + ' lead vox', drums='pingpong', vox='loop', accents='loop', downbeat='impact', cue='Vox',
             end='turn'),                                                                                     # 97
        line(GROOVE + ' lead vox', hats='busy', vox='loop', accents='loop', end='fill'),                      # 105
        line(GROOVE + ' lead', drums='pingpong', phrases=(STABS,), end='turn'),                               # 113
        line(GROOVE + ' lead vox', hats='busy', lead='skeleton', vox='call', phrases=(STABS,), throw=True,
             cue='Vox', end='drop', into='swell'))),                                                          # 121
    section('Switch-up', 'switch', (                  # 129-144: Orbit, the arp out, the zaps' 3-3-2
        line(GROOVE, bass='switch', hats='busy', drums='tresillo', downbeat='zap', accents='alternate', stab=True,
             end='turn'),                                                                                     # 129
        line(GROOVE + ' lead', bass='switch', drums='tresillo', lead='skeleton', ride=hold(24),
             accents='alternate', end='turn'))),                                                              # 137
    section('Break 2', 'break', (                     # 145-160: the choir again, then the stabs and the vox's call
        line('hats chords lead vox', hats='thin', lead='call', ride=hold(64), phrases=(CHOIR,), accents='loop',
             cue='Vox'),                                                                                      # 145
        line('hats vox', hats='thin', rise=('hats', 0.4, 1.0), vox='call', phrases=(STABS,), phrases_rest=True,
             accents='lead-in', cue='Riser', end='fill'))),                                                   # 153
    section('Drop 2', 'drop2', (                      # 161-192: Relay, the vox calling, the glass answering
        line(GROOVE + ' vox', bass='drop2', drums='tresillo', vox='call', phrases=(GLASS,), downbeat='sub',
             cue='Vox', end='turn'),                                                                          # 161
        line(GROOVE + ' lead vox', bass='drop2', hats='busy', lead='skeleton', vox='call', phrases=(GLASS,),
             phrases_rest=True, end='fill', into='uplift'),                                                   # 169
        line(GROOVE + ' lead vox', bass='drop2', drums='pingpong', lead='call', vox='answer', accents='loop',
             cue='Vox', stab=True, end='turn'),                                                               # 177
        line(GROOVE, bass='drop2', hats='busy', phrases=(STABS,), phrases_rest=True, throw=True, end='fill'))),  # 185
    section('Outro', 'outro', (                       # 193-224: drums to mix out on, its own lines
        line(DRUMS, hats='busy', drums='pingpong', end='turn'),                           # 193: drop 2's drums
        line(DRUMS, drums='tresillo', end='turn'),                                        # 201: the zaps' 3-3-2
        line('kick hats perc', end='turn'),                                               # 209: the clap out
        line('kick hats', hats='thin'))),                                                 # 217: the kick, open hats
)


PARTS = dict(
    drums=dict(
        map={36: 'KickSplit', 38: 'ElectroClap', 42: 'ClosedHat', 46: 'OpenHat', 47: 'Zap'},
        note='Put the closed and open hats in one choke group: the closed hat cuts the open one.',
        busy=BUSY, turn=TURN, variants={'pingpong': {'perc': PINGPONG}, 'tresillo': {'perc': TRESILLO}},
    ),
    bass=dict(
        items=('RollBassClean', 'RollBassWarm'),
        lines={k: [(s, n, ln, v, bool(sl)) for s, n, ln, v, sl in notes] for k, notes in B.LINES.items()},
        pick=B.PICK, switch='Orbit', drop2='Relay',
        note=('The drop glide (channel 11) follows the bass A/B switch: Drop Glide Clean (A) or Warm (B), rooted at D1 '
              '(26).'),
    ),
    lead=dict(item='ChromeArp', name='Chrome arp phrase', notes=lead_notes(), skeleton=skeleton()),
    chords=dict(items=('StarfieldPadDm9',), name='Starfield pad Dm9, held',
                trigger=[(0, 50, 64, 1.0, False)], voicing=[(0, n, 64, 1.0, False) for n in Pd.CHORD]),
    vox=dict(item='VoxSynth', riff=Vs.riff_midi(), throws=(Vs.THROW_AT,), vowels=Vs.LAYER_VOWEL),
    accents=(Key('LaserChordStab', 62, name=STAB[62]), Key('LaserChordStab', 58, name=STAB[58]), Key('GlassPing'),
             Key('StarfieldSweep'), Key('ReverseChord'), Key('ArpStab'), Key('NoiseUplifter'), Key('ResonantZap'),
             Key('VoxSynthStab')),
    fx=(Key('LaserFill'), Key('LaserRiser'), Key('SubDrop')),
    chops=(Key('VoxChopAhD5'), Key('VoxChopEeF5'), Key('VoxChopOhA4'), Key('VoxChopEeAhC5'), Key('VoxChopAhOhA5')),
    extras={'Drop Glide': ('DropGlideClean', 'DropGlideWarm'), 'Laser Chord Stab': 'LaserChordStab',
            'Glass Ping': 'GlassPing'},
    frames={'LaserChordStab@62': 16805, 'LaserChordStab@58': 16983, 'GlassPing': 119991, 'StarfieldSweep': 88615,
            'ReverseChord': 44307, 'ArpStab': 37543, 'NoiseUplifter': 177231, 'ResonantZap': 59724,
            'VoxSynthStab': 25069, 'LaserFill': 88615, 'LaserRiser': 708923, 'SubDrop': 88490, 'VoxChopAhD5': 19536,
            'VoxChopEeF5': 19085, 'VoxChopOhA4': 26899, 'VoxChopEeAhC5': 26452, 'VoxChopAhOhA5': 45179},
    # the cues' sounds; a line's downbeat or into names one. zap: the switch-up's downbeat; sub: drop 2's, the sub drop
    # alone (the glass melody's own D6 is its ping: the impact's glass ping would double it)
    cues=dict(riser='LaserRiser', impact=('SubDrop', 'GlassPing'), swell='ReverseChord', uplift='NoiseUplifter',
              stab=STAB[58], zap='ResonantZap', sub='SubDrop'),
    loop=dict(
        drums=(hits(C.KICK, 36) + hits(C.CLAP, 38) + hits(C.HAT_CLOSED, 42) + hits(C.HAT_OPEN, 46) + hits(C.ZAP, 47)),
        bass=orbit_loop(),
        lead=arp_loop(),
        vox=[Note(s + C5.RIFF_AT, n, ln, v) for s, n, ln, v in Vs.RIFF],
        throws=(C5.RIFF_AT + Vs.THROW_AT,),
        accents={'vox': loop_accents(C5.VOX), 'novox': loop_accents(C5.NOVOX)},
        fx=[Hit(C.FILL_AT, 'LaserFill', 1)],
        extras={'Drop Glide': [Note(C.GLIDE[0], 26, 0, 1.0, ticks=GLIDE_TICKS)]},
    ),
    phrases=[
        dict(name=RIFF, file='VoxSynthRiff', on='vox', item='VoxSynth', notes=Vs.riff_midi(), throws=(Vs.THROW_AT,),
             bars=2),
        dict(name=STABS, file='LaserStabRiff', on='Laser Chord Stab', item='LaserChordStab',
             notes=[(s, z, A.STAB_LEN, v, False) for s, z, v in A.STAB_RIFF], bars=2),
        dict(name=GLASS, file='GlassMelody', on='Glass Ping', item='GlassPing',
             notes=[(s, m, 2, v, False) for s, m, v in A.GLASS_MELODY], bars=4),
        dict(name=CHOIR, file='VoxChoirChords', on='vox', item='VoxSynth', notes=A.choir_notes(), bars=4),
    ],
    rides=[dict(on='Lead', cc=74, law='log', lo=round(C.ARP_RIDE['lo_hz']), hi=round(C.ARP_RIDE['hi_hz']), unit='Hz',
                what="the arp's filter ride: open from bar 1 to 16th 46, closed again by the loop's end",
                values=arp_ride())],
    recorded={'bass': 'the Orbit and Relay lines (`Loops/Orbit Line Clean.wav` and the rest), as the loop plays them',
              'lead': 'the Chrome Arp Phrase (`Loops/Chrome Arp Phrase.wav`); its filter ride follows CC74',
              'vox': "the vox synth riff, as the loop sings it (a stem derived from the recipe, if approved)"},
    form=FORM,
)
