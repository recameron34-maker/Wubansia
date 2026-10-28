"""Eight new songs, two per genre, each a full kit plus an extended-mix spec (custom form + extra layers).
Progressions are reharmonised so the pack's own sung hooks fit (checked separately). Emits JS into core.js."""
import json, sys

KITS = [
 # ---------------------------------------------------------------- UK garage
 dict(id='UKG07', genre='ukg', name='Halftime Pressure', bpm=132, root=2, mode='minor', swing=0.58,
  style='New-wave 2-step', vibe='Punchy kick, layered clap and ghost snares; a distorted Reese on the offbeats with a sine sub under it; chopped breaks and a half-time switch.',
  sound=dict(kit='ukg', bass='reese', chords='stab', lead='vox'),
  prog='F6 | Dm9 | Bbmaj7 | A7b9',
  chords={'F6': ['F3', 'A3', 'C4', 'D4'], 'Dm9': ['F3', 'A3', 'C4', 'E4'], 'Bbmaj7': ['F3', 'A3', 'Bb3', 'D4'], 'A7b9': ['G3', 'Bb3', 'C#4', 'E4']},
  bd=[['Bbmaj9', 'Bb2 F3 A3 C4 D4'], ['C6', 'C3 G3 A3 E4'], ['Dm9', 'D3 A3 C4 E4 F4'], ['A7sus4', 'A2 E3 G3 D4']],
  drums=dict(
   main=dict(K=['9.........9.....', '9......5..9.....', '9.........9..6..', '9.....6...9.....'], CL=['....9.......9...'],
             SN=['....8.......8...', '....8..3....8..3', '....8.......8...', '....8..3....8.45'],
             CH=['5364536453645364', '5364536453645.64', '5364536453645364', '536453645364536.'],
             OH=['..............7.', '..............7.', '..............7.', '......6.......7.'], RIM=['...4......4.....', '...4.....4....3.']),
   var=dict(K=['9..6......9.....', '9.........9...6.', '9..6......9.....', '9......6..9..6..'], CL=['....9.......9...'],
            SN=['....8..3....8...', '....8.......8.43', '....8..3....8...', '....8..4..3.8.56'],
            CH=['5364536453645364'], OH=['..7.......7.....'], SH=['3243324332433243']),
   fill=dict(K=['9.........9.....'], SN=['........6.6.6.7.7.8.8.9.9.9.9999'], CL=['....9...........'], CH=['5364536453......'])),
  bass=dict(
   A=['F1 - . . . . F1 - . . F2 . F1 - . .', 'D2 - . . . . D1 - . . D2 . D1 - . C2', 'Bb1 - . . . . Bb1 - . . Bb2 . Bb1 - . .', 'A1 - . . . . A1 - . . A2 . A1 - G1 E1'],
   B=['. . F1* - . F1 . . F2 - . . F1~ G1~ A1 .', '. . D2* - . D1 . . D2 - . . D1~ E1~ F1 .', '. . Bb1* - . Bb1 . . Bb2 - . . Bb1~ C2~ D2 .', '. . A1* - . A1 . . A2 - . . A1 . E2 C#2']),
  chordsPart=dict(
   A=['@F6 - . . . . @F6 . . . @F6 - . . . .', '@Dm9 - . . . . @Dm9 . . . @Dm9 - . . @Bbmaj7 -', '- - . . . . @Bbmaj7 . . . @Bbmaj7 - . . . .', '@A7b9 - . . . . @A7b9 . . . @A7b9 - . . @F6 -'],
   B=['@F6 - - - - - - - . . @F6 - - - - .', '@Dm9 - - - - - - - . . @Dm9 - - - - .', '@Bbmaj7 - - - - - - - . . @Bbmaj7 - - - - .', '@A7b9 - - - - - - - . . @A7b9 - - - - .']),
  lead=dict(
   A=['A4 . C5 . D5 - . . . . A4 . C5 . F5 -', '. . D5 . . . F5 - . . E5 . D5 - . .', 'A4 . C5 . D5 - . . . . F5 . A5 . F5 -', '. . E5 . . . C#5 - . . E5 . A5 - - .'],
   B=['. . . . . . . . . . A5 . F5 . D5 .', '. . . . . . . . . . F5 . D5 . C5 .', '. . . . . . . . . . D5 . F5 . A5 .', '. . . . . . . . . . E5 . C#5 . A4 .']),
  ext=dict(sub=True, sidechain=True,
   form=[['Intro (breaks)', 'intro', 16, dict(breaks='straight')],
         ['Verse (vocal)', 'groove', 16, dict(energy=2, vocal='h02', vocalBars=[0, 8], lead=False, stabs=True, stabsFrom=8, breaks='layer')],
         ['Build', 'build', 8, dict(breaks='roll')],
         ['Drop 1', 'drop', 32, dict(energy=3, vocal='h02', vocalBars=[0, 16], counter='call', breaks='layer')],
         ['Half-time switch', 'halftime', 16, dict(breaks='halftime', subdrop=True)],
         ['Breakdown', 'breakdown', 16, dict(vocal='h02', vocalBars=[8])],
         ['Build 2 (jungle)', 'build', 8, dict(breaks='jungle', tapestop=True)],
         ['Drop 2 (breaks)', 'drop', 32, dict(energy=4, rh=dict(turn=True), breaks='jungle', vocal='h02', vocalBars=[16], counter='call')],
         ['Outro (DJ)', 'outro', 32, dict(breaks='straight')]])),

 dict(id='UKG08', genre='ukg', name='Sunday Wub', bpm=133, root=3, mode='major', swing=0.59,
  style='R&B garage', vibe='Bubbling sub with pitch dips, pizzicato and legato strings, backing-vocal oohs, Rhodes pushes and two of the pack\'s sung hooks.',
  sound=dict(kit='ukg', bass='sub', chords='rhodes', lead='bell'),
  prog='Cm9 | Ebmaj9 | Fm11 | Bb6add11',
  chords={'Cm9': ['Bb3', 'D4', 'Eb4', 'G4'], 'Ebmaj9': ['G3', 'Bb3', 'D4', 'F4'], 'Fm11': ['Ab3', 'Bb3', 'Eb4', 'G4'], 'Bb6add11': ['F3', 'G3', 'Bb3', 'Eb4']},
  bd=[['Abmaj9', 'Ab2 Eb3 G3 Bb3 C4'], ['Gm7', 'G2 D3 F3 Bb3'], ['Fm9', 'F2 C3 Eb3 G3 Ab3'], ['Bb9sus4', 'Bb2 F3 Ab3 C4 Eb4']],
  drums=dict(
   main=dict(K=['9.........8.....', '9......5..8.....', '9.........8.....', '9.........8..6..'],
             SN=['....8.......8...', '....8..2....8..3', '....8.......8...', '....8..2....8.35'], CL=['............7...'],
             CH=['4363436343634363'], OH=['..............5.'], SH=['2423242324232423'], TB=['..4...4...4...4.']),
   var=dict(K=['9..6......8.....', '9.........8...5.', '9..6......8.....', '9......5..8..5..'],
            SN=['....8..2....8...', '....8.......8.32', '....8..2....8...', '....8..3....8.45'], CL=['....6.......7...'],
            CH=['4363436343634363'], OH=['..5.......5.....'], SH=['2423242324232423'], RIM=['...4..4..4...4..']),
   fill=dict(K=['9.........8.....'], SN=['......9.....4.45.56.6778'], CL=['....7...........'], CH=['4363436343......'])),
  bass=dict(
   A=['C2 - - - . . G1 . . . C2 - - . Bb1 .', 'Eb2 - - - . . Bb1 . . . Eb2 - - . D2 .', 'F1 - - - . . C2 . . . F1 - - . Ab1 .', 'Bb1 - - - . . F1 . . . Bb1 - - . D2 .'],
   B=['C2 . . C2 . . G1~ C2 . . C2 . Bb1 . G1 .', 'Eb2 . . Eb2 . . Bb1~ Eb2 . . Eb2 . D2 . Bb1 .', 'F1 . . F1 . . C2~ F2 . . F1 . Ab1 . C2 .', 'Bb1 . . Bb1 . . F1~ Bb1 . . Bb1 . D2 . F2 .']),
  chordsPart=dict(
   A=['@Cm9 - - - . . @Cm9 - . . @Cm9 - - . @Ebmaj9 -', '- - - - . . @Ebmaj9 - . . @Ebmaj9 - - . . .', '@Fm11 - - - . . @Fm11 - . . @Fm11 - - . @Bb6add11 -', '- - - - . . @Bb6add11 - . . @Bb6add11 - - . . .'],
   B=['@Cm9 . . @Cm9 . . @Cm9 - . . @Cm9 . . @Cm9 . .', '@Ebmaj9 . . @Ebmaj9 . . @Ebmaj9 - . . @Ebmaj9 . . @Ebmaj9 . .', '@Fm11 . . @Fm11 . . @Fm11 - . . @Fm11 . . @Fm11 . .', '@Bb6add11 . . @Bb6add11 . . @Bb6add11 - . . @Bb6add11 . . @Bb6add11 . .']),
  lead=dict(
   A=['G5 - . Eb5 . . Bb4 - . . C5 . . D5 - .', 'Eb5 - . D5 . . Bb4 - . . G4 . . Bb4 - .', 'C5 - . Ab4 . . F4 - . . Ab4 . . C5 - .', 'D5 - . Eb5 . . F5 - . . D5 . Bb4 - - .'],
   B=['Bb5 - - - . . G5 - . . . . . . . .', '. . . . . . F5 - . G5 - . Bb5 - - .', 'C6 - - - . . Ab5 - . . . . . . . .', '. . . . . . F5 - . G5 - . D5 - - .']),
  ext=dict(sidechain=True,
   form=[['Intro (strings)', 'intro', 16, dict(strings='pizz')],
         ['Verse (vocal)', 'groove', 16, dict(energy=2, vocal='h03', vocalBars=[0, 8], lead=False, strings='pizz', wub=True)],
         ['Pre-chorus', 'build', 8, dict(strings='swell', choir='oohs')],
         ['Chorus (vocal)', 'drop', 32, dict(energy=3, vocal='h03', vocalBars=[0, 16], choir='oohs', strings='legato', wub=True, counter='call')],
         ['Post-hook (brat)', 'switch', 16, dict(vocal='bh3', vocalBars=[0, 8], choir='oohs', wub=True, lead=False)],
         ['Breakdown', 'breakdown', 16, dict(choir='oohs', strings='legato')],
         ['Build', 'build', 8, dict(strings='runs')],
         ['Drop 2 (both hooks)', 'drop', 32, dict(energy=4, vocal='h03', vocalBars=[0, 16], vocal2='bh3', vocal2Bars=[8, 24], strings='runs', choir='oohs', wub=True)],
         ['Outro (DJ)', 'outro', 32, dict(strings='pizz')]])),

 # ---------------------------------------------------------------- minimal tech house
 dict(id='MTH07', genre='mth', name='Pattering Beads', bpm=128, root=9, mode='minor', swing=0.56,
  style='Rominimal roller', vibe='Muted kick and skipping first-half accents, a dubby bass that zooms and retracts, polymetric beads, micro clicks and a whispered hook.',
  sound=dict(kit='tech', bass='rubber', chords='dubstab', lead='bleep'),
  prog='Am11 | Am11 | Fmaj9 | G6',
  chords={'Am11': ['G3', 'C4', 'D4', 'E4'], 'Fmaj9': ['E3', 'G3', 'A3', 'C4'], 'G6': ['G3', 'B3', 'D4', 'E4']},
  bd=[['Dm9', 'D3 A3 C4 E4 F4'], ['Fmaj7', 'F2 C3 E3 A3'], ['Em7', 'E2 B2 D3 G3'], ['Am9', 'A2 E3 G3 B3 C4']],
  drums=dict(
   main=dict(K=['8...7...8...7...', '8...7...8...7..4', '8...7...8...7...', '8...7...8..47...'], CL=['....6.......6...'],
             RIM=['..4..3....4..3..', '..4..3...4..3...'], CH=['.5.4.5.4.5.4.5.4'], OH=['..5...5...5...5.'],
             SH=['4232.232.2324232', '42324232.232.232'], WB=['.....3.......3..']),
   var=dict(K=['8...7...8...7...', '8..47...8...7...', '8...7...8...7..4', '8...7..48...7...'], CL=['....6.......6...'],
            RIM=['..4..3.4..4..3..'], CH=['.5.4.5.4.5.4.5.4'], OH=['..5...5...5...5.'], SH=['4232423242324232'],
            COH=['......4...4.....', '...4..4.....4...'], CLV=['3..3..3...3.....']),
   fill=dict(K=['8...7...8...7...'], CL=['....6.......6...'], RIM=['..4..3..4.4.4444'], CH=['.5.4.5.4.5.4....'], WB=['........4.4.4.44'])),
  bass=dict(
   A=['A1 - . . . . A1 . . A2~ G2 . E2 . . .', '. . A1 - . . . . A1 . . C2~ D2 . E2 .', 'F1 - . . . . F1 . . F2~ E2 . C2 . . .', 'G1 - . . . . G1 . . G2~ D2 . E2 . G1 .'],
   B=['A1 . A1 A2 . A1 . . A1 . A2, . A1 . C2 .', 'A1 . A1 A2 . A1 . . A1 . E2, . A1 . G1 .', 'F1 . F1 F2 . F1 . . F1 . F2, . F1 . A1 .', 'G1 . G1 G2 . G1 . . G1 . G2, . G1 . B1 .']),
  chordsPart=dict(
   A=['. . . . . . @Am11 . . . . . . . . .', '. . . . . . . . . . @Am11 . . . . .', '. . . . . . @Fmaj9 . . . . . . . . .', '. . . . . . . . . . @G6 . . . . .'],
   B=['@Am11 - - - - - - - . . . . . . . .', '. . . . . . . . . . . . . . . .', '@Fmaj9 - - - - - - - . . . . . . . .', '@G6 - - - - - - - . . . . . . . .']),
  lead=dict(
   A=['E5 . . C5 . . A4 . . E5 . . G5 . . .', 'E5 . . C5 . . A4 . . E5 . . D5 . . .', 'E5 . . C5 . . A4 . . E5 . . G5 . . .', 'D5 . . B4 . . G4 . . D5 . . E5 . . .'],
   B=['. . . . . . . . A5 . . . . . . .', '. . . . . . . . G5 . . E5 . . . .', '. . . . . . . . A5 . . . . . . .', '. . . . . . . . G5 . . D5 . . B4 .']),
  ext=dict(sidechain=True,
   form=[['Intro (DJ)', 'intro', 32, dict(poly=True, micro=True)],
         ['Groove', 'groove', 32, dict(energy=2, chords=False, stabs=True, stabsFrom=16, poly=True, micro=True, zoom=True)],
         ['Whisper', 'groove', 16, dict(energy=2, chords=False, vocal='h06', vocalBars=[0, 8], lead=False, poly=True, micro=True, zoom=True)],
         ['Beads', 'groove', 32, dict(energy=3, lead=True, leadFrom=8, counter=True, poly='dense', micro=True, zoom=True)],
         ['Dip (no kick)', 'breakdown', 16, dict(poly=True, micro='glitch', vocal='h06', vocalBars=[8])],
         ['Build', 'build', 8, dict(poly=True)],
         ['Roller', 'drop', 32, dict(energy=4, counter='call', poly='dense', micro='glitch', zoom=True)],
         ['Micro-edits', 'micro', 16, dict(poly=True)],
         ['Peak', 'drop', 32, dict(energy=4, counter='harmony', poly='dense', micro=True, vocal='h06', vocalBars=[16, 24], zoom=True)],
         ['Outro (DJ)', 'outro', 32, dict(poly=True, micro=True)]])),

 dict(id='MTH08', genre='mth', name='Black Card Bounce', bpm=128, root=0, mode='minor', swing=0.54,
  style='Bounce tech house', vibe='Rubber bass that drops in pitch on every hit, a cowbell groove, the pack\'s "Rich bitch energy" chant, ad-lib shouts, an acid line and a rave hoover at the peak.',
  sound=dict(kit='tech', bass='rubber', chords='stab', lead='acidhi'),
  prog='Cm9 | Cm9 | Abmaj9 | Bb69',
  chords={'Cm9': ['Eb3', 'G3', 'Bb3', 'D4'], 'Abmaj9': ['G3', 'Bb3', 'C4', 'Eb4'], 'Bb69': ['F3', 'G3', 'C4', 'D4']},
  bd=[['Fm9', 'F2 C3 Eb3 G3 Ab3'], ['Abmaj7', 'Ab2 Eb3 G3 C4'], ['Bb6', 'Bb2 F3 G3 D4'], ['Cm9', 'C3 G3 Bb3 D4 Eb4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], CH=['3535353535353535'], OH=['..8...8...8...8.'],
             CB=['......5.....5...', '...5..5.......5.'], SH=['4343434343434343'], RIM=['...5......5..5..']),
   var=dict(K=['9...9...9...9...', '9...9...9...9..6', '9...9...9...9...', '9...9...9..69...'], CL=['....9.......9...'],
            CH=['3535353535353535'], OH=['..8...8...8...8.'], CB=['..5...5..5..5...', '......5...5...5.'], SH=['4343434343434343'],
            TB=['....6.......6...'], COH=['...5..5.....5...']),
   fill=dict(K=['9...9...9...9.9.'], CL=['....9.......9.99'], CH=['353535353535....'], CB=['......5...5.5.5.'])),
  bass=dict(
   A=['. . C2* . . . C2 . . . C3 . . C2 . .', '. . C2* . . . C2 . . . Eb2 . . G1 . .', '. . Ab1* . . . Ab1 . . . Ab2 . . Ab1 . .', '. . Bb1* . . . Bb1 . . . Bb2 . . D2 . .'],
   B=['. . C2* . . C2 C2 . . . C3 . C2 . Bb1 .', '. . C2* . . C2 Eb2 . . . G2 . C2 . G1 .', '. . Ab1* . . Ab1 Ab1 . . . Ab2 . Ab1 . G1 .', '. . Bb1* . . Bb1 D2 . . . F2 . Bb1 . D2 .']),
  chordsPart=dict(
   A=['. . . . . . @Cm9 . . . . . . . . .', '. . . . . . @Cm9 . . . . . . @Cm9 . .', '. . . . . . @Abmaj9 . . . . . . . . .', '. . . . . . @Bb69 . . . . . . @Bb69 . .'],
   B=['. . @Cm9 . . . @Cm9 . . . @Cm9 . . . . .', '. . @Cm9 . . . @Cm9 . . . @Cm9 . . . . .', '. . @Abmaj9 . . . @Abmaj9 . . . @Abmaj9 . . . . .', '. . @Bb69 . . . @Bb69 . . . @Bb69 . . . . .']),
  lead=dict(
   A=['C4 . C5 . . Eb4~ G4 . C4 . . Bb4, . . G4 .', 'C4 . C5 . . Eb4~ G4 . C4 . . D5, . . C5 .', 'Ab3 . Ab4 . . C4~ Eb4 . Ab3 . . G4, . . Eb4 .', 'Bb3 . Bb4 . . D4~ F4 . Bb3 . . G4, . . F4 .'],
   B=['. . . . . . . . C5* . . C4 . . . .', '. . . . . . . . Eb5* . . C4 . . . .', '. . . . . . . . C5* . . Ab3 . . . .', '. . . . . . . . D5* . . Bb3 . . . .']),
  ext=dict(sidechain=True, shouts=['ba12', 'ba03', 'ba09', 'ba10', 'a08', 'a06', 'ba05', 'a05'],
   form=[['Intro (DJ)', 'intro', 32, dict(cowbell=True)],
         ['Bass groove', 'groove', 16, dict(energy=2, chords=False, stabs=True, stabsFrom=8, shouts='sparse', bounce=True)],
         ['Hook (brat)', 'groove', 16, dict(energy=3, chords=False, vocal='bh2', vocalBars=[0, 8], shouts='answer', lead=False, bounce=True)],
         ['Acid build', 'build', 16, dict(acid=True)],
         ['Drop', 'drop', 32, dict(energy=4, counter='call', acid=True, shouts='drop', bounce=True)],
         ['Ghetto switch', 'switch', 16, dict(cowbell=True, shouts='call', vocal='bh2', vocalBars=[8], bounce=True)],
         ['Break', 'breakdown', 16, dict()],
         ['Build 2', 'build', 8, dict(tapestop=True)],
         ['Peak (hoover)', 'drop', 32, dict(energy=4, hoover=True, acid=True, vocal='bh2', vocalBars=[0, 16], shouts='drop', bounce=True, lead=False)],
         ['Outro (DJ)', 'outro', 32, dict(cowbell=True)]])),

 # ---------------------------------------------------------------- speed garage
 dict(id='SG07', genre='sg', name='Siren Pressure', bpm=134, root=6, mode='minor', swing=0.57,
  style='Dub speed garage', vibe='Four-to-the-floor with garage swing, a wall of wobbling Reese with pitch dives, dub sirens, a rewind, MC shouts and two of the pack\'s vocals.',
  sound=dict(kit='sg', bass='reesebig', chords='darkstab', lead='saw'),
  prog='A6 | F#m11 | Dmaj7 | Bm11',
  chords={'A6': ['E3', 'F#3', 'A3', 'C#4'], 'F#m11': ['E3', 'A3', 'B3', 'C#4'], 'Dmaj7': ['F#3', 'A3', 'C#4', 'D4'], 'Bm11': ['A3', 'B3', 'D4', 'E4']},
  bd=[['Dmaj9', 'D3 A3 C#4 E4 F#4'], ['E6', 'E3 B3 C#4 G#4'], ['C#m7', 'C#3 G#3 B3 E4'], ['F#m9', 'F#2 C#3 E3 G#3 A3']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['............6...', '............6..4'], CH=['5464546454645464'],
             OH=['..8...8...8...8.'], RIM=['...5......5.....', '...5..4...5..4..']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], CH=['5464546454645464'], OH=['..8...8...8...8.'],
            SH=['3343334333433343'], TB=['....5.......5...'], RIM=['..4..4....4..4..']),
   fill=dict(K=['9...9...9...9...'], SN=['........6.6.7.7.8.8.9.9.99999999'], CL=['....9...........'], CH=['54645464........'])),
  bass=dict(
   A=['A1 - - - - - A1~ C#2 - - - . A1 - . .', 'F#1 - - - - - F#1~ A1 - - - . F#1 - . .', 'D2 - - - - - D2~ F#2 - - - . D2 - . .', 'B1 - - - - - B1~ D2 - - - . B1 - C#2 -'],
   B=['. . A1* - . . A1 - . . A2 - . . A1 -', '. . F#1* - . . F#1 - . . F#2 - . . F#1 -', '. . D2* - . . D2 - . . D1 - . . D2 -', '. . B1* - . . B1 - . . B2 - . . C#2 -']),
  chordsPart=dict(
   A=['. . @A6 . . . . . . . @A6 . . . . .', '. . @F#m11 . . . . . . . @F#m11 . . . . .', '. . @Dmaj7 . . . . . . . @Dmaj7 . . . . .', '. . @Bm11 . . . . . . . @Bm11 . . . . .'],
   B=['@A6 - - - - - - - - - - - - - - -', '@F#m11 - - - - - - - - - - - - - - -', '@Dmaj7 - - - - - - - - - - - - - - -', '@Bm11 - - - - - - - - - - - - - - -']),
  lead=dict(
   A=['C#5 - . E5 . . C#5 - . . A4 . . B4 - .', 'C#5 - . A4 . . F#4 - . . A4 . . E5 - .', 'F#5 - . D5 . . A4 - . . C#5 . . D5 - .', 'D5 - . B4 . . F#4 - . . A4 . B4 - - .'],
   B=['. . . . . . . . . . E5 . . C#5 . .', '. . . . . . . . . . C#5 . . A4 . .', '. . . . . . . . . . F#5 . . D5 . .', '. . . . . . . . . . D5 . . B4 . .']),
  ext=dict(sidechain=True, shouts=['a01', 'a02', 'a03', 'a09', 'a12', 'a10', 'a11'],
   form=[['Intro', 'intro', 16, dict(siren='intro', shouts='intro')],
         ['Build', 'build', 8, dict(siren='build')],
         ['Drop 1', 'drop', 8, dict(energy=3, vocal='h11', vocalBars=[0], rewind=True, dives=True, shouts='drop', lead=False)],
         ['Drop 1 (come again)', 'drop', 32, dict(energy=3, vocal='h11', vocalBars=[0, 16], counter='call', siren='drop', dives=True, shouts='drop')],
         ['Siren switch', 'switch', 16, dict(siren='call', shouts='call', dives=True)],
         ['Breakdown', 'breakdown', 16, dict(siren='dub', vocal='bh4', vocalBars=[8])],
         ['Bass solo', 'bassSolo', 8, dict(dives=True)],
         ['Drop 2 (big bass)', 'drop', 32, dict(energy=4, vocal='bh4', vocalBars=[0, 8, 16, 24], hoover=True, dives=True, shouts='drop', lead=False)],
         ['Dub break', 'dubBreak', 8, dict(siren='dub', toGap=True)],
         ['Final drop', 'drop', 32, dict(energy=4, vocal='h11', vocalBars=[0, 16], vocal2='bh4', vocal2Bars=[8, 24], hoover=True, siren='drop', shouts='drop')],
         ['Outro', 'outro', 32, dict(siren='outro')]])),

 dict(id='SG08', genre='sg', name='Organ Wub', bpm=136, root=0, mode='minor', swing=0.55,
  style='Bassline', vibe='Rolling M1 organ bassline with wub pitch dips, an offbeat donk, snappy four-to-the-floor, orchestral string stabs and the pack\'s "All the girls say ooh".',
  sound=dict(kit='sg', bass='organbass', chords='organ', lead='strings'),
  prog='Cm7 | Abmaj7 | Fm9 | G7sus4 G7',
  chords={'Cm7': ['G3', 'Bb3', 'C4', 'Eb4'], 'Abmaj7': ['Ab3', 'C4', 'Eb4', 'G4'], 'Fm9': ['Eb3', 'G3', 'Ab3', 'C4'], 'G7sus4': ['F3', 'G3', 'C4', 'D4'], 'G7': ['F3', 'G3', 'B3', 'D4']},
  bd=[['Fm9', 'F2 C3 Eb3 G3 Ab3'], ['Ebmaj7', 'Eb3 Bb3 D4 G4'], ['Abmaj9', 'Ab2 Eb3 G3 Bb3 C4'], ['G7sus4', 'G2 D3 F3 C4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['....7.......7...'], CH=['..8...8...8...8.'], OH=['................', '..............6.'],
             SH=['5353535353535353'], TB=['....6.......6...']),
   var=dict(K=['9...9...9...9...', '9...9...9...9..6'], CL=['....9.......9...'], SN=['....7.......7..4'], CH=['..8...8...8...8.'],
            OH=['..6.......6.....'], SH=['5353535353535353'], RIM=['...4..4...4..4..']),
   fill=dict(K=['9...9...9...9...'], SN=['......6.6.6.7.7.8.8.9.99'], CL=['....9...........'], CH=['..8...8...8.....'])),
  bass=dict(
   A=['C2 . C2 C3 . C2 . Bb1 . C2 . C2 Eb2 . G1 .', 'Ab1 . Ab1 Ab2 . Ab1 . G1 . Ab1 . Ab1 C2 . Eb2 .', 'F1 . F1 F2 . F1 . Eb2 . F1 . F1 Ab1 . C2 .', 'G1 . G1 G2 . G1 . F2 . G1 . G1 B1 . D2 .'],
   B=['C2 - - . . . C2~ Eb2 - - . . G1 - . .', 'Ab1 - - . . . Ab1~ C2 - - . . Eb2 - . .', 'F1 - - . . . F1~ Ab1 - - . . C2 - . .', 'G1 - - . . . G1~ B1 - - . . D2 - . .']),
  chordsPart=dict(
   A=['. . @Cm7 . . . @Cm7 . . . @Cm7 . . . @Cm7 .', '. . @Abmaj7 . . . @Abmaj7 . . . @Abmaj7 . . . @Abmaj7 .', '. . @Fm9 . . . @Fm9 . . . @Fm9 . . . @Fm9 .', '. . @G7sus4 . . . @G7sus4 . @G7 . @G7 . . . @G7 .'],
   B=['@Cm7 - - - - - - - @Cm7 - - - - - - -', '@Abmaj7 - - - - - - - @Abmaj7 - - - - - - -', '@Fm9 - - - - - - - @Fm9 - - - - - - -', '@G7sus4 - - - - - - - @G7 - - - - - - -']),
  lead=dict(
   A=['G5 - . . Eb5 - . . C5 - . . Eb5 - G5 -', 'Ab5 - . . Eb5 - . . C5 - . . Eb5 - G5 -', 'Ab5 - . . F5 - . . C5 - . . Eb5 - F5 -', 'G5 - . . D5 - . . B4 - . . D5 - F5 -'],
   B=['. . . . . . . . . . . . C6 - Bb5 -', '. . . . . . . . . . . . C6 - Ab5 -', '. . . . . . . . . . . . Ab5 - G5 -', '. . . . . . . . . . . . G5 - F5 -']),
  ext=dict(sidechain=True, shouts=['a09', 'a10', 'a11', 'a12'],
   form=[['Intro', 'intro', 16, dict(donk='intro')],
         ['Build', 'build', 8, dict()],
         ['Drop 1', 'drop', 32, dict(energy=3, vocal='h12', vocalBars=[0, 16], donk=True, wub=True, counter='call')],
         ['2-step switch', 'twostep', 16, dict(wub=True)],
         ['Breakdown', 'breakdown', 16, dict(vocal='h12', vocalBars=[8], strings='legato')],
         ['Build 2', 'build', 8, dict(tapestop=True)],
         ['Drop 2 (donk)', 'drop', 32, dict(energy=4, vocal='h12', vocalBars=[8, 24], donk=True, wub=True, strings='stabs', shouts='drop')],
         ['Wub switch', 'switch', 16, dict(wub=True, donk=True, shouts='call')],
         ['Outro', 'outro', 32, dict(donk='outro')]])),

 # ---------------------------------------------------------------- classic house
 dict(id='CH07', genre='ch', name='Disco Diamonds', bpm=128, root=10, mode='minor', swing=0.52,
  style='Filter disco', vibe='Octave disco bass, string runs and swells, brass stabs, a talkbox line, long filter sweeps and two of the pack\'s vocals.',
  sound=dict(kit='house', bass='pluck', chords='stab', lead='strings'),
  prog='Bbm11 | Gbmaj9 | Ebm9 | Ab6',
  chords={'Bbm11': ['Db4', 'Eb4', 'F4', 'Ab4'], 'Gbmaj9': ['F3', 'Ab3', 'Bb3', 'Db4'], 'Ebm9': ['Gb3', 'Bb3', 'Db4', 'F4'], 'Ab6': ['Ab3', 'C4', 'Eb4', 'F4']},
  bd=[['Gbmaj9', 'Gb2 Db3 F3 Ab3 Bb3'], ['Ab6', 'Ab2 Eb3 F3 C4'], ['Fm7', 'F2 C3 Eb3 Ab3'], ['Bbm9', 'Bb2 F3 Ab3 C4 Db4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..8...8...8...8.'], CH=['4.4.4.4.4.4.4.4.'], TB=['3434343434343434'], COH=['..........5.....']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['....6.......6...'], OH=['..8...8...8...8.'], CH=['4545454545454545'],
            TB=['3434343434343434'], COH=['......5...5.....'], CLO=['5.......5.......'], RD=['5.5.5.5.5.5.5.5.']),
   fill=dict(K=['9...9...9...9...'], CL=['....9.......9.99'], SN=['................6.6.7.7.8.8.9.9.'], OH=['..8...8...8.....'])),
  bass=dict(
   A=['Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Ab2 .', 'Gb1 . Gb2 . Gb1 . Gb2 . Gb1 . Gb2 . Gb1 . F2 .', 'Eb2 . Eb3 . Eb2 . Eb3 . Eb2 . Eb3 . Eb2 . Db3 .', 'Ab1 . Ab2 . Ab1 . Ab2 . Ab1 . Ab2 . C2 . Eb2 .'],
   B=['Bb1 - . Bb2 . . Bb1 . . Bb1 Db2 . F2 . Bb2 .', 'Gb1 - . Gb2 . . Gb1 . . Gb1 Bb1 . Db2 . Gb2 .', 'Eb2 - . Eb3 . . Eb2 . . Eb2 Gb2 . Bb2 . Eb3 .', 'Ab1 - . Ab2 . . Ab1 . . Ab1 C2 . Eb2 . F2 .']),
  chordsPart=dict(
   A=['. . @Bbm11 . . . @Bbm11 . . . @Bbm11 . . . @Bbm11 .', '. . @Gbmaj9 . . . @Gbmaj9 . . . @Gbmaj9 . . . @Gbmaj9 .', '. . @Ebm9 . . . @Ebm9 . . . @Ebm9 . . . @Ebm9 .', '. . @Ab6 . . . @Ab6 . . . @Ab6 . . . @Ab6 .'],
   B=['@Bbm11 - - - - - - - - - - - - - @Gbmaj9 -', '- - - - - - - - - - - - - - . .', '@Ebm9 - - - - - - - - - - - - - @Ab6 -', '- - - - - - - - - - - - - - . .']),
  lead=dict(
   A=['F5 - - - Db5 - - - Eb5 - - - F5 - - -', 'Db5 - - - Bb4 - - - Ab4 - - - F4 - - -', 'Gb5 - - - F5 - - - Eb5 - - - Db5 - - -', 'C5 - - - Eb5 - - - F5 - - - Ab5 - - -'],
   B=['. . Bb4 . Db5 . . Bb4 . . F5 - . Eb5 . .', '. . Bb4 . Db5 . . Bb4 . . F5 - . Db5 . .', '. . Bb4 . Gb5 . . F5 . . Eb5 - . Db5 . .', '. . C5 . Eb5 . . F5 . . Ab5 - . F5 . .']),
  ext=dict(sidechain=True,
   form=[['Intro (filtered)', 'intro', 32, dict(filter='open', strings='pad')],
         ['Groove', 'groove', 16, dict(energy=2, strings='stabs', brass='sparse')],
         ['Verse (vocal)', 'groove', 16, dict(energy=2, vocal='h16', vocalBars=[0, 8], lead=False, strings='pizz')],
         ['Chorus (vocal)', 'drop', 32, dict(energy=3, vocal='h16', vocalBars=[0, 16], strings='runs', brass=True, talkbox=True)],
         ['String break', 'breakdown', 16, dict(strings='legato')],
         ['Build', 'build', 8, dict(strings='runs')],
         ['Drop (diamonds)', 'drop', 32, dict(energy=4, vocal='bh5', vocalBars=[0, 8, 16, 24], brass=True, strings='runs', talkbox=True, lead=False)],
         ['Filter switch', 'switch', 16, dict(filter='sweep', brass=True)],
         ['Outro (DJ)', 'outro', 32, dict(filter='close')]])),

 dict(id='CH08', genre='ch', name='Hands Up Higher', bpm=128, root=9, mode='minor', swing=0.53,
  style='Gospel piano house', vibe='Stabbing mid-range piano vamps, rubbery organ bass, a four-part choir, organ swells, piano runs, orchestra hits and a semitone key change for the last chorus.',
  sound=dict(kit='house', bass='organbass', chords='piano', lead='organlead'),
  prog='Fmaj9 | Dm9 | Em7 | Am9',
  chords={'Fmaj9': ['E4', 'G4', 'A4', 'C5'], 'Dm9': ['F4', 'A4', 'C5', 'E5'], 'Em7': ['D4', 'E4', 'G4', 'B4'], 'Am9': ['C4', 'E4', 'G4', 'B4']},
  bd=[['Dm9', 'D3 A3 C4 E4 F4'], ['G6', 'G2 D3 E3 B3'], ['Cmaj7', 'C3 G3 B3 E4'], ['Fmaj7', 'F2 C3 E3 A3']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..8...8...8...8.'], CH=['5.5.5.5.5.5.5.5.'], SH=['3434343434343434']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['....7.......7...'], OH=['..8...8...8...8.'], CH=['5.5.5.5.5.5.5.5.'],
            SH=['3434343434343434'], TB=['....6.......6...'], RD=['6.6.6.6.6.6.6.6.']),
   fill=dict(K=['9...9...9...9...'], CL=['....9.......9999'], SN=['................7.7.7.8.8.9.9.9.'], OH=['..8...8...8.....'])),
  bass=dict(
   A=['F1 . . F2 . . F1 . . F1 . F2 . . C2 .', 'D2 . . D3 . . D2 . . D2 . D3 . . A1 .', 'E1 . . E2 . . E1 . . E1 . E2 . . B1 .', 'A1 . . A2 . . A1 . . A1 . A2 . . G1 .'],
   B=['F1 - . F2 . F1 . . F1 - . F2 . E2 . C2', 'D2 - . D3 . D2 . . D2 - . D3 . C3 . A1', 'E1 - . E2 . E1 . . E1 - . E2 . D2 . B1', 'A1 - . A2 . A1 . . A1 - . A2 . G2 . E2']),
  chordsPart=dict(
   A=['@Fmaj9 . . @Fmaj9 . . @Fmaj9 . . . @Fmaj9 . . @Fmaj9 . .', '@Dm9 . . @Dm9 . . @Dm9 . . . @Dm9 . . @Dm9 . .', '@Em7 . . @Em7 . . @Em7 . . . @Em7 . . @Em7 . .', '@Am9 . . @Am9 . . @Am9 . . . @Am9 . . @Am9 . .'],
   B=['@Fmaj9 - - - - - - - . . @Fmaj9 - - - @Dm9 -', '- - - - - - - - . . @Dm9 - - - . .', '@Em7 - - - - - - - . . @Em7 - - - @Am9 -', '- - - - - - - - . . @Am9 - - - . .']),
  lead=dict(
   A=['C5 - . A4 . . C5 - . E5 - . G5 - . .', 'F5 - . E5 . . D5 - . C5 - . A4 - . .', 'B4 - . D5 . . E5 - . G5 - . E5 - . .', 'C5 - . B4 . . A4 - . G4 - . A4 - - .'],
   B=['. . . . . . . . . . . . E5 . G5 .', '. . . . . . . . . . . . F5 . E5 .', '. . . . . . . . . . . . G5 . B5 .', '. . . . . . . . . . . . A5 . E5 .']),
  ext=dict(sidechain=True,
   form=[['Intro (DJ)', 'intro', 16, dict()],
         ['Piano intro', 'groove', 16, dict(energy=2, piano='solo', organ='swell')],
         ['Verse (vocal)', 'groove', 16, dict(energy=2, vocal='h14', vocalBars=[0, 8], lead=False, choir='oohs')],
         ['Chorus (vocal)', 'drop', 32, dict(energy=3, vocal='h14', vocalBars=[0, 16], choir='full', piano='fills', orch=True)],
         ['Gospel breakdown', 'breakdown', 16, dict(choir='oohs', piano='solo', organ='swell')],
         ['Build', 'build', 8, dict(orch=True)],
         ['Chorus 2', 'drop', 32, dict(energy=4, vocal='h14', vocalBars=[0, 8, 16, 24], choir='full', piano='fills', organ=True, lead=False)],
         ['Key change chorus (+1)', 'drop', 16, dict(energy=4, lift=1, choir='full', piano='fills', orch=True, organ=True)],
         ['Outro (DJ)', 'outro', 32, dict()]])),
]


def check():
    bad = 0
    for k in KITS:
        for part in ('bass', 'chordsPart', 'lead'):
            for v, bars in k[part].items():
                for i, b in enumerate(bars):
                    n = len(b.split())
                    if n != 16:
                        bad += 1; print('TOKENS', k['id'], part, v, 'bar', i + 1, n)
        for v, lanes in k['drums'].items():
            for lane, bars in lanes.items():
                for i, b in enumerate(bars):
                    if len(b) not in (8, 12, 16, 24, 32):
                        bad += 1; print('DRUM LEN', k['id'], v, lane, i + 1, len(b))
        tot = sum(s[2] for s in k['ext']['form'])
        if tot % 8:
            bad += 1; print('FORM LENGTH', k['id'], tot)
        print(k['id'], k['name'], k['bpm'], 'BPM,', tot, 'bars,', round(tot * 4 * 60 / k['bpm'] / 60, 2), 'min')
    return bad


def to_js(k):
    q = lambda x: json.dumps(x).replace('"', "'").replace("\\'", "\\'")
    qs = lambda x: json.dumps(x)
    out = "    {\n"
    out += f"      id: '{k['id']}', song: {{ bd: {q(k['bd'])} }}, genre: '{k['genre']}', name: {qs(k['name'])}, bpm: {k['bpm']}, root: {k['root']}, mode: '{k['mode']}', swing: {k['swing']},\n"
    out += f"      style: {qs(k['style'])}, vibe: {qs(k['vibe'])},\n"
    out += f"      sound: {q(k['sound'])},\n"
    out += f"      prog: {qs(k['prog'])},\n      chords: {q(k['chords'])},\n"
    out += "      drums: {\n" + ",\n".join(f"        {v}: {{ " + ", ".join(f"{lane}: {q(b)}" for lane, b in lanes.items()) + " }" for v, lanes in k['drums'].items()) + "\n      },\n"
    for part in ('bass', 'chordsPart', 'lead'):
        out += f"      {part}: {{\n        A: {q(k[part]['A'])},\n        B: {q(k[part]['B'])}\n      }},\n"
    out += f"      ext: {json.dumps(k['ext'])}\n"
    out += "    }"
    return out


if __name__ == '__main__':
    if check():
        sys.exit('fix the kits first')
    js = ",\n".join(to_js(k) for k in KITS)
    p = '/home/claude/work/core.js'
    s = open(p).read()
    if "id: 'UKG07'" in s:
        sys.exit('already inserted')
    marker = "\n  ];\n\n  /* ---------- compile ---------- */"
    assert marker in s
    s = s.replace(marker, ",\n" + js + marker)
    open(p, 'w').write(s)
    print('inserted', len(KITS), 'kits')
