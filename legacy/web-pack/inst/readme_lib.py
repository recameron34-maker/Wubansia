"""README.txt for the instrument library."""
import os as _os  # noqa: E402
PACK_ROOT = _os.environ.get('PACK_ROOT') or _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))  # the legacy web-pack folder
import json

W = PACK_ROOT
R = json.load(open(f'{W}/inst/build_report.json'))
ITEMS = {}
for f in ['manifest_fx', 'manifest_more', 'manifest_vox', 'manifest_vox2', 'manifest_brat', 'manifest_wave3']:
    for it in json.load(open(f'{W}/out/{f}.json')):
        ITEMS[it['id']] = it
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']


def nl(m):
    """Note name as Ableton, Logic and Cubase show it (middle C = C3 = 60)."""
    return NAMES[m % 12] + str(m // 12 - 2)


L = []
P = L.append
P('UKG & MINIMAL PACK INSTRUMENTS  (Decent Sampler + SFZ)')
P('=======================================================')
P('')
P('27 playable instruments made from the pack: drum kits, a break slicer, a tempo-synced loop player, basses, keys, vocals and FX,')
P('plus a garage-house set in the style of Disclosure. Everything is 44.1 kHz / 16-bit WAV.')
P('')
P('NEW IN THIS VERSION')
P('  Vocal Chops    all 22 syllables now come from the new voice engine (the first ten are re-sung), each trimmed to start on its sound')
P('  Vocal Mosaic   four hooks re-sung with clearer words: "All the girls say ooh", "Lift me higher, higher",')
P('                 "Missed the last train home, so we walk it slow" and "Hands up higher, everybody, one desire"')
P('  Shouts         the MC shouts and ad-libs carry a brighter top end (a synthesised 12-18 kHz octave), so they sit with the sung vocals')
P('')
P('IN THE LAST VERSION')
P('  Vocal Mosaic   16 hooks, all from the pack\'s new recordings (clearer words, sung phrasing, more air on top), led by the')
P('                 four written for the garage-house kits GH01-GH04')
P('  Vocal Chops    12 new sung syllables (22 in all), male and female, from "Hey!" and "Whoa!" to a high "Ooh"')
P('')
P('WHAT YOU NEED')
P('  Decent Sampler, a free sampler plug-in (AU, VST3 and AAX for Mac and Windows; it runs in Logic, Ableton, FL Studio, Cubase,')
P('  Bitwig, Reaper, Pro Tools and others). Get it from decentsamples.com. Version 1.37 or later: the Loop Player and the')
P('  Break Slicer\'s loop key use its tempo-follow mode, and the presets ask for 1.37 when they load.')
P('  The SFZ folder has versions for SFZ players such as sfizz or sforzando (both free). See SFZ VERSIONS below.')
P('')
P('HOW TO LOAD')
P('  1. Unzip and move the "UKG Minimal Instruments" folder somewhere permanent (Music or Documents). Keep it in one piece:')
P('     the presets find their samples by path inside this folder.')
P('  2. Put Decent Sampler on an instrument track in your DAW and open its window.')
P('  3. Use its file menu to load a sample library or preset, and pick any .dspreset in the folder. DSLibraryInfo.xml groups')
P('     them into Drums, Loops, Bass, Keys, Garage House, Vocals and FX in Decent Sampler\'s browser.')
P('  4. Drop a MIDI clip from the pack on the track. The drum kits, Break Slicer, FX Hits and Shouts use the same notes as the pack\'s MIDI.')
P('')
P('Note names below are given as MIDI numbers, with the name Ableton, Logic and Cubase show in brackets (middle C = C3 = 60).')
P('Sample file names use scientific pitch instead (middle C = C4 = 60), as the rest of the pack does.')
P('')
P('MIDI CONTROLLERS (already written into the pack\'s MIDI)')
P('  CC74  filter / Tone knob on every melodic instrument, so the build-ups open the filter on their own')
P('  CC1   Wobble on the Reese Bass (the speed garage bass parts ramp it)')
P('  CC71  resonance on the Reese and Acid Bass (the acid sections ramp it)')
P('  CC91  Room (the dub stab throws)')
P('')
P('THE INSTRUMENTS')
desc = {
    'Drums - UK Garage Kit': 'UK garage kit. General MIDI map (below) with a menu of six kicks.',
    'Drums - Minimal Tech Kit': 'Minimal tech house kit, same map.',
    'Drums - Speed Garage Kit': 'Speed garage kit, same map.',
    'Drums - Classic House Kit': 'Classic house kit (909-style), same map.',
    'Garage House - Drum Kit': 'Disclosure-style garage-house kit: round tuned kick, plate clap, crisp hats, snap on 40, shaker, tambourine.',
    'Drums - Break Slicer': 'Three breaks cut into 16ths at zero crossings. Notes 36-67 (C1-G3) are the slices; 72 (C4) plays the whole break in time.',
    'Loops - Loop Player (Tempo Sync)': '54 drum loops, tops, breaks, fills and grooves, one per key, re-timed to your song\'s tempo while held.',
    'FX - FX Hits': '36 impact, 38 reverse crash, 40 downlifter, 41 sub drop, 43 rewind, 44 tape stop (the FX Hits track), then 26 more FX from 48.',
    'Vocals - Shouts & Ad-libs': '28 shouts and ad-libs from note 48 (C2). The menu also has the exact Shouts layouts of MTH08, SG07 and SG08.',
    'Vocals - Vocal Chops': '22 sung syllables (dry), playable across the keyboard, with a synced echo. Starts on "Ooh", sung on note 74.',
    'Bass - Reese Bass': 'Detuned-saw Reese, 13 seamless loops C1-C4, a sine sub layer, mono with glide, synced filter wobble.',
    'Bass - 2-Step Sub': 'Garage sine sub (Decent Sampler\'s own oscillator) with pitch dip, drive and click; or the pack\'s 808 subs.',
    'Bass - Acid Bass': '303-style mono synth (oscillator, resonant filter with its own envelope, velocity accent, slides on overlapping notes).',
    'Bass - Organ Bass': 'M1-style house organ bass, 24 notes C1-B2, poly or mono with glide.',
    'Keys - House Piano': 'The classic-house piano, every minor third C2-C6.',
    'Keys - Garage Organ': 'M1-style drawbar organ for 2-step stabs, looped so held notes sustain.',
    'Keys - Garage Rhodes': 'FM electric piano, soft and bright velocity layers, synced suitcase auto-pan.',
    'Keys - Strings': 'Stereo string ensemble with vibrato, looped C2-C6.',
    'Keys - Dub Chords': 'Chord memory: every key plays a whole minor 7th / 9th / 11th or major 9th from that root, into a dark synced echo.',
    'Garage House - Chord Pluck': 'Disclosure-style detuned-saw chord pluck; Pump slows the attack for the sidechain swell.',
    'Garage House - Glide Bass': 'Disclosure-style round bass (sine plus filtered square), plucky filter, mono with glide.',
    'Garage House - Square Stab': 'Detuned square stab for riffs and hooks; poly, or mono with glide.',
    'Garage House - Deep Pad': 'Warm, dark saw pad for breakdowns, looped C2-C6.',
    'Keys - House Stabs': 'Chord memory: 4 brass stabs (one falls off), 3 orchestra hits, 3 choir chords; each key plays the whole chord from that root.',
    'Vocals - Vocal Mosaic': '16 sung hooks (the four garage-house hooks first) cut into syllables, one chop per key from 48 (C2); 72 (C4) plays the whole hook in time. A new chop cuts the last.',
    'Bass - Hoover': 'Rave hoover (detuned saws, a sweeping pulse, the octave below), 17 stereo loops C1-C5. Swoop scoops up into each note.',
    'Bass - Donk': 'The bassline donk: a sine that drops into pitch with a square bite and a woody knock, C1-C4.',
}
for cat, f, t in R['presets']:
    P(f'  {f}')
    P(f'      {desc.get(f, "")}')
P('')
P('DRUM MAP (all five kits; General MIDI, like every drum part in the pack)')
P('  36 Kick  37 Rim  38 Snare  39 Clap  40 Snap  42 Closed hat  46 Open hat (cut by the closed hat)  45/47/50 Toms  49 Crash  51 Ride')
P('  54 Tambourine  56 Cowbell  60/61 Bongos  63/64 Congas  70 Shaker  75 Clave  76 Click')
P('  The snap on 40 is the garage-house snap in every kit: the GH01-GH04 MIDI layers it with the clap.')
P('  Note 35 in the extended mixes is a silent sidechain trigger (velocity 1): it is left unmapped on purpose.')
P('')
P('LOOP PLAYER KEYS (hold a key; start notes on the beat; the loop follows the host tempo and keeps its pitch)')
for k, t, bpm, bars, rel in R['loops']:
    P(f'  {k:3d} ({nl(k):>4s})  {t}  ({bpm} BPM, {bars} bar{"s" if bars > 1 else ""})')
P('')
P('FX HITS KEYS FROM 48')
for k, iid in R['fx']:
    title = ITEMS[iid]['title'] if iid in ITEMS else iid
    P(f'  {k:3d} ({nl(k):>4s})  {title}')
P('')
P('HOUSE STABS (chord memory: the key that plays each stab at its own pitch)')
for lab, rootn, rel in R['stabs']:
    fl = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'][rootn % 12] + str(rootn // 12 - 2)
    P(f'  {lab:<20s} root {rootn} ({fl})')
P('')
P('VOCAL MOSAIC (hook, then the chop on each key from 48 = C2; 72 = C4 is the whole hook, tempo-synced)')
for hid, title, bpm, words in R['mosaic']:
    P(f'  {hid.upper()} "{title}" ({bpm} BPM)')
    row = '    '
    for j, w_ in enumerate(words):
        cell = f'{48 + j} {w_}  '
        if len(row) + len(cell) > 128:
            P(row.rstrip())
            row = '    '
        row += cell
    P(row.rstrip())
P('  Chops are cut at the sung notes (a melisma like "clo-ser" gives closer-1 and closer-2) and play at the pitch they were sung.')
P('')
P('GARAGE HOUSE (DISCLOSURE-STYLE)')
P('  Original sounds made from scratch in the style of Disclosure\'s early garage-house records: tight tuned kicks, plate claps,')
P('  swung hats and shakers, detuned-saw chord plucks, round glide basses, square stabs and dark pads. No audio, melodies or')
P('  lyrics from their music are used. The four grooves (Loop Player keys 24-27) are 128 BPM with 58% swing.')
P('  The pack\'s garage-house MIDI kits GH01-GH04 are written for this set: drums on the Garage House kit (snap on 40), Bass on the')
P('  Glide Bass (GH03 suits the Reese), Chords and Stabs on the Chord Pluck, Melody on the Square Stab, Pad on the Deep Pad, and the')
P('  pitched Vocal Chops track on Vocal Chops. The Vocal Mosaic is for writing your own chop riffs from the hooks those songs sing.')
P('')
P('SFZ VERSIONS')
P('  SFZ/<category>/*.sfz load the same samples (the folder must stay as it is). SFZ players have no tempo-follow, so the Loop')
P('  Player plays loops at the BPM shown, and the oscillator synths (2-Step Sub synth, Acid Bass) are Decent Sampler only; the')
P('  2-Step Sub SFZ uses the 808 samples. Knob and effect settings are Decent Sampler features; SFZ files carry the mapping,')
P('  envelopes, loops and (for the Reese) a CC74 filter.')
P('')
P('Made with the UKG & Minimal Pack. ' + str(len(R['presets'])) + ' Decent Sampler presets, ' + str(len(R['sfz'])) + ' SFZ files.')
open(f'{W}/inst/lib/README.txt', 'w').write('\r\n'.join(L) + '\r\n')
print('\n'.join(L[:40]))
print('...', len(L), 'lines')
