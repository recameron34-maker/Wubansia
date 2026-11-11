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
- The extended mix: the sci-fi club form (midi.CLUB_FORM) until the kit engineer writes FUT02's own (TODO below).
Notes are (16th, MIDI note, length in 16ths, velocity 0-1, slide).
"""
import math

from tools.sound import channels as CH
from tools.sound import kits
from tools.sound.midi import Hit, Key, Note, hits, twice
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


PARTS = dict(
    drums=dict(
        map={36: 'KickSplit', 38: 'ElectroClap', 42: 'ClosedHat', 46: 'OpenHat', 47: 'Zap'},
        note='Put the closed and open hats in one choke group: the closed hat cuts the open one.',
        # TODO(FUT02 engineer): busy (the hats already fill 15 of 16 16ths: a busy pattern on another layer, a zap or
        # glass-ping figure, audit B's 16-bar percussion cycle) and turn
    ),
    bass=dict(
        items=('RollBassClean', 'RollBassWarm'),
        lines={k: [(s, n, ln, v, bool(sl)) for s, n, ln, v, sl in line] for k, line in B.LINES.items()},
        pick=B.PICK,
        note=('The drop glide (channel 11) follows the bass A/B switch: Drop Glide Clean (A) or Warm (B), rooted at D1 '
              '(26).'),
    ),
    lead=dict(item='ChromeArp', name='Chrome arp phrase', notes=lead_notes()),
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
    cues=dict(riser='LaserRiser', impact=('SubDrop', 'GlassPing'), swell='ReverseChord', uplift='NoiseUplifter',
              stab=STAB[58]),
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
        dict(name='Vox synth riff, the velocity picks the vowel (soft oh, medium ah, hard ee)', file='VoxSynthRiff',
             on='vox', item='VoxSynth', notes=Vs.riff_midi(), throws=(Vs.THROW_AT,), bars=2),
        dict(name='Laser stab riff, Dm9 and B-flat maj9', file='LaserStabRiff', on='Laser Chord Stab',
             item='LaserChordStab', notes=[(s, z, A.STAB_LEN, v, False) for s, z, v in A.STAB_RIFF], bars=2),
        dict(name='Glass melody', file='GlassMelody', on='Glass Ping', item='GlassPing',
             notes=[(s, m, 2, v, False) for s, m, v in A.GLASS_MELODY], bars=4),
        dict(name='Vox choir chords, the velocity picks the vowel', file='VoxChoirChords', on='vox', item='VoxSynth',
             notes=A.choir_notes(), bars=4),
    ],
    rides=[dict(on='Lead', cc=74, law='log', lo=round(C.ARP_RIDE['lo_hz']), hi=round(C.ARP_RIDE['hi_hz']), unit='Hz',
                what="the arp's filter ride: open from bar 1 to 16th 46, closed again by the loop's end",
                values=arp_ride())],
    recorded={'bass': 'the Orbit and Relay lines (`Loops/Orbit Line Clean.wav` and the rest), as the loop plays them',
              'lead': 'the Chrome Arp Phrase (`Loops/Chrome Arp Phrase.wav`); its filter ride follows CC74',
              'vox': "the vox synth riff, as the loop sings it (a stem derived from the recipe, if approved)"},
    # TODO(FUT02 engineer): form=(...) (audit B, FUT02): the arp's skeleton in the groove (lead=dict(skeleton=...)),
    # the full arp and the vox in the drop, the laser stab riff (phrases=('Laser stab riff, ...',)) or the pad in
    # its lines 3-4, the choir chords in both breaks (chords blocks), a switch-up on Orbit with a zap or glass-ping
    # figure, drop 2 on Relay with the glass melody; thin the arp where the vox plays; revoice two choir chords.
)
