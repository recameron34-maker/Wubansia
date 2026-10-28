"""Speed garage + classic house kits. Emits JS objects, checks token/step counts first."""
import json, re, sys

KITS = [
 dict(id='SG01', genre='sg', name='Dubplate Pressure', bpm=132, root=7, mode='minor', swing=0.56,
  style='Classic speed garage', vibe='Big detuned Reese with octave jumps and slides, offbeat organ, siren-whistle lead.',
  sound=dict(kit='sg', bass='reesebig', chords='organ', lead='saw'),
  chords={'Gm9': ['Bb3','D4','F4','A4'], 'Ebmaj9': ['Bb3','D4','F4','G4'], 'D7sus4': ['A3','C4','D4','G4']},
  bd=[['Ebmaj9','Eb3 Bb3 D4 F4 G4'],['F6','F3 C4 D4 A4'],['Dm7','D3 A3 C4 F4'],['Gm9','G3 D4 F4 A4 Bb4']],
  drums=dict(
   main=dict(K=['9...9...9...9...','9...9...9...9...','9...9...9...9...','9...9...9...9.6.'], CL=['....9.......9...'], OH=['..7...7...7...7.'], CH=['53.453.453.453.4'], RIM=['...5......5..5..']),
   var=dict(K=['9...9...9...9...','9...9...9...9...','9...9...9...9...','9...9...9...9.6.'], CL=['....9.......9...','....9.......9.5.','....9.......9...','....9.......9.67'], OH=['..7...7...7...7.'], CH=['53.553.553.553.5'], SH=['2423242324232423'], RIM=['...5..5...5..5..'], CLV=['.......4.......4']),
   fill=dict(K=['9...9...9.6.9696'], CL=['....9...........'], SN=['................4.5.5.6.67788999'], OH=['..7...7.........'])),
  bass=dict(
   A=['G1 - - - . . G1 - - . G2 - . F2 - .','G1 - - - . . G1 - - . Bb1 - . A1~ G1 -','Eb2 - - - . . Eb1 - - . Eb2 - . D2 - .','D2 - - - . . A1 - - . D2 - . C2~ Bb1~ A1'],
   B=['G1 - . G2 . G1 - . G1 - . G2 . F2~ G2 .','G1 - . G2 . G1 - . Bb1 - . Bb2 . A2~ G2 .','Eb2 - . Eb1 . Eb2 - . Eb2 - . Eb1 . D2~ Eb2 .','D2 - . A1 . D2 - . D2 - . C2 . Bb1~ A1 .']),
  chordsPart=dict(
   A=['. . @Gm9 . . . @Gm9 . . . @Gm9 . . . @Gm9 .','. . @Gm9 . . . @Gm9 . . . @Gm9 . . @Gm9 . .','. . @Ebmaj9 . . . @Ebmaj9 . . . @Ebmaj9 . . . @Ebmaj9 .','. . @D7sus4 . . . @D7sus4 . . . @D7sus4 . @D7sus4 . @D7sus4 .'],
   B=['@Gm9 - . . . . . . . . @Gm9 . . . . .','. . . . . . @Gm9 - . . . . @Gm9 . . .','@Ebmaj9 - . . . . . . . . @Ebmaj9 . . . . .','. . . . . . @D7sus4 - . . . . @D7sus4 - - -']),
  lead=dict(
   A=['D5 - - . . . D5 . F5 - . D5 . . C5 .','Bb4 - - - . . . . . . . . G4 . Bb4 .','D5 - - . . . D5 . F5 - . G5 . . F5 .','D5 - - - . . C5 - . . A4 - - - . .'],
   B=['. . . . . . . . . . G5 . . . F5 .','. . . . . . . . . . D5 . . . Bb4 .','. . . . . . . . . . G5 . . . Bb5 .','. . . . . . . . . . A5 . . . G5 .'])),

 dict(id='SG02', genre='sg', name='Bassline Rinse', bpm=134, root=6, mode='minor', swing=0.57,
  style='Bouncy speed garage', vibe='Syncopated Reese bounce with octave pops, stab chords and a pitched vocal-chop hook.',
  sound=dict(kit='sg', bass='reesebig', chords='stab', lead='vox'),
  chords={'F#m9': ['A3','C#4','E4','G#4'], 'Dmaj9': ['A3','C#4','E4','F#4'], 'Bm9': ['A3','C#4','D4','F#4'], 'C#7sus4': ['G#3','B3','C#4','F#4']},
  bd=[['Dmaj9','D3 A3 C#4 E4 F#4'],['E6','E3 B3 C#4 G#4'],['C#m7','C#3 G#3 B3 E4'],['F#m9','F#3 C#4 E4 G#4 A4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..6...6...6...6.'], CH=['53.353.353.353.3'], RIM=['..4...4....4..4.','..4...4....4.44.'], SN=['................','...............3']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..6...6...6...6.'], CH=['53.353.353.353.3'], SH=['2423242324232423'], TB=['....5.......5...'], RIM=['..45..4...45..4.']),
   fill=dict(K=['9...9...9...9.66'], SN=['........5.......5.5.6.6.77889999'], OH=['..6...6...6.....'], CH=['53.353.353.3....'])),
  bass=dict(
   A=['F#1 - . F#2 . . F#1 - . . A1 - . F#1 . E1','D2 - . A1 . . D2 - . . F#2 - . D2 . C#2','B1 - . B2 . . B1 - . . D2 - . B1 . A1','C#2 - . G#1 . . C#2 - . . E2 - . C#2~ B1~ A1'],
   B=['F#1 - - - - - - - F#2~ E2~ C#2 - - - . .','D2 - - - - - - - A1 - - - C#2~ D2 . .','B1 - - - - - - - B2~ A2~ F#2 - - - . .','C#2 - - - - - - - G#1 - - - B1~ A1~ G#1 .']),
  chordsPart=dict(
   A=['@F#m9 - . . . . . . . . @F#m9 . . . . .','@Dmaj9 - . . . . . . . . @Dmaj9 . . . . .','@Bm9 - . . . . . . . . @Bm9 . . . . .','@C#7sus4 - . . . . . . . . @C#7sus4 . . @C#7sus4 . .'],
   B=['. . @F#m9 . . . @F#m9 . . . @F#m9 . . . @F#m9 .','. . @Dmaj9 . . . @Dmaj9 . . . @Dmaj9 . . . @Dmaj9 .','. . @Bm9 . . . @Bm9 . . . @Bm9 . . . @Bm9 .','. . @C#7sus4 . . . @C#7sus4 . . . @C#7sus4 . . . @C#7sus4 .']),
  lead=dict(
   A=['C#5 - . A4 . . B4 - . . C#5 . . E5 - .','F#5 - . E5 . . C#5 - . . A4 . . B4 - .','D5 - . B4 . . C#5 - . . D5 . . F#5 - .','F#5 - . C#5 . . B4 - . . G#4 - - - . .'],
   B=['. . . . . . . . . . . . F#5 . A5 .','. . . . . . . . . . . . E5 . C#5 .','. . . . . . . . . . . . D5 . F#5 .','. . . . . . . . . . . . G#5 . F#5 .'])),

 dict(id='SG03', genre='sg', name='Ruff Neck Shuffle', bpm=130, root=5, mode='minor', swing=0.55,
  style='Dark speed garage', vibe='Half-note Reese with long glides, dark minor stabs and a harmonic-minor V chord.',
  sound=dict(kit='sg', bass='reesebig', chords='darkstab', lead='bell'),
  chords={'Fm9': ['Ab3','C4','Eb4','G4'], 'Dbmaj9': ['Ab3','C4','Eb4','F4'], 'Bbm9': ['Ab3','C4','Db4','F4'], 'C7b9': ['Bb3','Db4','E4','G4']},
  bd=[['Bbm9','Bb2 F3 Ab3 C4 Db4'],['Dbmaj9','Db3 Ab3 C4 Eb4 F4'],['Eb6','Eb3 Bb3 C4 G4'],['Fm9','F3 C4 Eb4 G4 Ab4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..6...6...6...6.'], CH=['43.343.343.343.3'], RIM=['...5.....5..5...'], CLV=['......4.......4.']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['....5.......5...'], RD=['..6...6...6...6.'], CH=['53.453.453.453.4'], RIM=['...5..5..5..5...'], CLV=['......4.......4.']),
   fill=dict(K=['9...9...9...9...'], SN=['......9.....4.45.56.6789'], RIM=['...5.....5.5.5.5'])),
  bass=dict(
   A=['F1 - - - - - - - F1~ Ab1~ C2 - - - - -','Db2 - - - - - - - C2~ Bb1~ Ab1 - - - - -','Bb1 - - - - - - - Bb1~ Db2~ F2 - - - - -','C2 - - - - - - - C2~ Bb1~ G1 - - - E1 -'],
   B=['F1 - . F1 - . F2 . F1 - . Eb2 - . C2 .','Db2 - . Db2 - . Ab1 . Db2 - . F2 - . Eb2 .','Bb1 - . Bb1 - . F1 . Bb1 - . Db2 - . C2 .','C2 - . C2 - . G1 . C2 - . Bb1 - . E1 .']),
  chordsPart=dict(
   A=['@Fm9 - - - . . . . . . @Fm9 - . . . .','@Dbmaj9 - - - . . . . . . @Dbmaj9 - . . . .','@Bbm9 - - - . . . . . . @Bbm9 - . . . .','@C7b9 - - - . . . . . . @C7b9 - . @C7b9 - .'],
   B=['. . @Fm9 . . . @Fm9 . . . @Fm9 . . . @Fm9 .','. . @Dbmaj9 . . . @Dbmaj9 . . . @Dbmaj9 . . . @Dbmaj9 .','. . @Bbm9 . . . @Bbm9 . . . @Bbm9 . . . @Bbm9 .','. . @C7b9 . . . @C7b9 . . . @C7b9 . . . @C7b9 .']),
  lead=dict(
   A=['C5 - - . . . . . Ab4 - - . . . . .','F5 - - . . . . . Eb5 - - . C5 - - .','Db5 - - . . . . . C5 - - . . . . .','E5 - - . . . . . Db5 - - . C5 - - .'],
   B=['F4 Ab4, C5 Ab4, F4 Ab4, C5 Eb5* F4 Ab4, C5 Ab4, F4 Ab4, Db5 C5*','F4 Ab4, Db5 Ab4, F4 Ab4, C5 Ab4, F4 Ab4, Db5 Ab4, F4 Ab4, Eb5 Db5*','F4 Bb4, Db5 Bb4, F4 Bb4, Db5 F5* F4 Bb4, Db5 Bb4, F4 Bb4, Db5 C5*','E4 G4, Bb4 G4, E4 G4, Db5 C5* E4 G4, Bb4 G4, E4 G4, Bb4 C5*'])),

 dict(id='SG04', genre='sg', name='Sound Boy Special', bpm=133, root=2, mode='minor', swing=0.56,
  style='Ragga speed garage', vibe='Offbeat organ skank, punchy Reese walking the roots, siren-style riff.',
  sound=dict(kit='sg', bass='reesebig', chords='organ', lead='saw'),
  chords={'Dm': ['D4','F4','A4'], 'Bb': ['D4','F4','Bb4'], 'C': ['C4','E4','G4'], 'A': ['C#4','E4','A4']},
  bd=[['Gm9','G2 D3 F3 A3 Bb3'],['Bbmaj7','Bb2 F3 A3 D4'],['C6','C3 G3 A3 E4'],['Dm9','D3 A3 C4 E4 F4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..7...7...7...7.'], CH=['53.353.353.353.3'], RIM=['...5..5....5..5.']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..7...7...7...7.'], CH=['53.353.353.353.3'], SH=['2423242324232423'], TB=['....5.......5...'], RIM=['.5.5..5....5.5.5']),
   fill=dict(K=['9...9...9...9...'], SN=['................4.4.5.5.66778899'], CL=['....9.......9...'])),
  bass=dict(
   A=['D2 - - - . . D2 - . . A1 - - - C2 -','Bb1 - - - . . Bb1 - . . F1 - - - A1 -','C2 - - - . . C2 - . . G1 - - - Bb1 -','A1 - - - . . A1 - . . E2 - - - C#2 -'],
   B=['D2 - . D3 . D2 - . D2 - . A1 . C2 - .','Bb1 - . Bb2 . Bb1 - . Bb1 - . F1 . A1 - .','C2 - . C3 . C2 - . C2 - . G1 . Bb1 - .','A1 - . A2 . A1 - . A1 - . E1 . C#2 - .']),
  chordsPart=dict(
   A=['. . @Dm . . . @Dm . . . @Dm . . . @Dm .','. . @Bb . . . @Bb . . . @Bb . . . @Bb .','. . @C . . . @C . . . @C . . . @C .','. . @A . . . @A . . . @A . . . @A .'],
   B=['. . @Dm - . . @Dm - . . @Dm - . . @Dm -','. . @Bb - . . @Bb - . . @Bb - . . @Bb -','. . @C - . . @C - . . @C - . . @C -','. . @A - . . @A - . . @A - . . @A -']),
  lead=dict(
   A=['A4 - - . A4 . . C5 - . A4 . G4 - F4 -','F4 - - - . . . . . . D4 - - - . .','E4 - - . E4 . . G4 - . E4 . C5 - Bb4 -','A4 - - - . . . . C#5 - - - E5 - - -'],
   B=['. . . . . . . . . . . . A5 - D6 -','. . . . . . . . . . . . Bb5 - F5 -','. . . . . . . . . . . . C6 - G5 -','. . . . . . . . . . . . C#6 - E6 -'])),

 dict(id='SG05', genre='sg', name='Pressure Drop', bpm=135, root=4, mode='minor', swing=0.55,
  style='Big-room speed garage', vibe='Sparse Reese drops with long slides, pumping octave B-section, wide pads and a pluck hook.',
  sound=dict(kit='sg', bass='reesebig', chords='pad', lead='pluck'),
  chords={'Em9': ['G3','B3','D4','F#4'], 'Cmaj9': ['G3','B3','D4','E4'], 'Am9': ['G3','B3','C4','E4'], 'B7sus4': ['A3','B3','E4','F#4']},
  bd=[['Am9','A2 E3 G3 B3 C4'],['Cmaj9','C3 G3 B3 D4 E4'],['D6','D3 A3 B3 F#4'],['Em9','E3 B3 D4 F#4 G4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..7...7...7...7.'], CH=['53.353.353.353.3'], RIM=['.......5.......5']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..7...7...7...7.'], CH=['53.553.553.553.5'], SH=['3243324332433243'], RIM=['...5...5...5...5']),
   fill=dict(K=['9...9...9.9.9999'], CL=['....9...........'], SN=['........6.......5.5.5.6.6.778899'], OH=['..7.............'])),
  bass=dict(
   A=['E1 - - - - - - - - - - . E2~ D2~ B1 -','C2 - - - - - - - - - - . C2~ B1~ G1 -','A1 - - - - - - - - - - . A1~ C2~ E2 -','B1 - - - - - - - - - - . B1~ A1~ F#1 -'],
   B=['E1 - E2 - E1 - E2 - E1 - E2 - D2 - B1 -','C2 - C3 - C2 - C3 - C2 - C3 - B1 - G1 -','A1 - A2 - A1 - A2 - A1 - A2 - C2 - E2 -','B1 - B2 - B1 - B2 - B1 - A1 - G1 - F#1 -']),
  chordsPart=dict(
   A=['@Em9 - - - - - - - . . @Em9 - - - . .','@Cmaj9 - - - - - - - . . @Cmaj9 - - - . .','@Am9 - - - - - - - . . @Am9 - - - . .','@B7sus4 - - - - - - - . . @B7sus4 - - - . .'],
   B=['. . @Em9 . . . @Em9 . . . @Em9 . . . @Em9 .','. . @Cmaj9 . . . @Cmaj9 . . . @Cmaj9 . . . @Cmaj9 .','. . @Am9 . . . @Am9 . . . @Am9 . . . @Am9 .','. . @B7sus4 . . . @B7sus4 . . . @B7sus4 . . . @B7sus4 .']),
  lead=dict(
   A=['B4 . . E5 . . G5 . . F#5 . . E5 . . .','B4 . . E5 . . G5 . . F#5 . . D5 . . .','A4 . . C5 . . E5 . . D5 . . C5 . . .','F#4 . . B4 . . E5 . . A4 . . B4 . . .'],
   B=['G5 - - - - - - - F#5 - - - E5 - - -','E5 - - - - - - - D5 - - - B4 - - -','C5 - - - - - - - E5 - - - A5 - - -','F#5 - - - - - - - E5 - - - F#5 - - -'])),

 dict(id='SG06', genre='sg', name='Champagne Riddim', bpm=136, root=0, mode='minor', swing=0.58,
  style='Shuffled speed garage', vibe='Heavy shuffle, skippy organ chords, Reese octave bounce with slide-downs.',
  sound=dict(kit='sg', bass='reesebig', chords='organ', lead='vox'),
  chords={'Cm9': ['Bb3','D4','Eb4','G4'], 'Fm9': ['Ab3','C4','Eb4','G4'], 'Abmaj9': ['Bb3','C4','Eb4','G4'], 'G7sus4': ['C4','D4','F4','G4']},
  bd=[['Fm9','F2 C3 Eb3 G3 Ab3'],['Abmaj9','Ab2 Eb3 G3 Bb3 C4'],['Bb6','Bb2 F3 G3 D4'],['Cm9','C3 G3 Bb3 D4 Eb4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], OH=['..7...7...7...7.'], CH=['53.453.453.453.4'], RIM=['..4...4....4..4.'], SH=['2423242324232423']),
   var=dict(K=['9...9...9...9...','9...9...9...9...','9...9...9...9...','9...9...9...9.6.'], CL=['....9.......9..5'], OH=['..7...7...7...7.'], CH=['53.453.453.453.4'], RIM=['..45..4...45..4.'], SH=['2423242324232423'], TB=['....5.......5...']),
   fill=dict(K=['9..5..9...9.9.66'], CL=['....9...........'], SN=['................4.5.5.6.67788999'], CH=['53.453.453......'])),
  bass=dict(
   A=['C2 - . C3 . . C2 - . . C2 - . Bb1 . G1','F1 - . F2 . . F1 - . . F1 - . Ab1 . C2','Ab1 - . Ab2 . . Ab1 - . . Ab1 - . G1 . Eb1','G1 - . G2 . . G1 - . . G1 - . F2~ D2~ B1'],
   B=['C2 - - - - - . C2 - - - . Eb2~ D2~ C2 -','F1 - - - - - . F1 - - - . Ab1~ G1~ F1 -','Ab1 - - - - - . Ab1 - - - . C2~ Bb1~ Ab1 -','G1 - - - - - . G1 - - - . G2~ F2~ D2 -']),
  chordsPart=dict(
   A=['@Cm9 - . @Cm9 . . @Cm9 - . . @Cm9 . . @Cm9 - .','@Fm9 - . @Fm9 . . @Fm9 - . . @Fm9 . . @Fm9 - .','@Abmaj9 - . @Abmaj9 . . @Abmaj9 - . . @Abmaj9 . . @Abmaj9 - .','@G7sus4 - . @G7sus4 . . @G7sus4 - . . @G7sus4 . @G7sus4 - - .'],
   B=['. . @Cm9 . . . @Cm9 . . . @Cm9 . . . @Cm9 .','. . @Fm9 . . . @Fm9 . . . @Fm9 . . . @Fm9 .','. . @Abmaj9 . . . @Abmaj9 . . . @Abmaj9 . . . @Abmaj9 .','. . @G7sus4 . . . @G7sus4 . . . @G7sus4 . . . @G7sus4 .']),
  lead=dict(
   A=['G5 - . Eb5 . . C5 - . . Eb5 . . F5 - .','Ab5 - . G5 . . F5 - . . Eb5 . . C5 - .','Eb5 - . C5 . . Bb4 - . . C5 . . Eb5 - .','D5 - . F5 . . G5 - . . F5 . D5 - - -'],
   B=['. . . . . . . . . . . . C6 . G5 .','. . . . . . . . . . . . Ab5 . F5 .','. . . . . . . . . . . . Eb6 . C6 .','. . . . . . . . . . . . D6 . G5 .'])),

 # ------------------------------------------------------------------ classic house
 dict(id='CH01', genre='ch', name='Piano Anthem', bpm=128, root=9, mode='minor', swing=0.54,
  style='Piano house', vibe='Classic syncopated house-piano stabs over a ii-V-I lift, octave organ bass, soaring strings.',
  sound=dict(kit='house', bass='organbass', chords='piano', lead='strings'),
  chords={'Am7': ['A3','C4','E4','G4'], 'Dm7': ['A3','C4','D4','F4'], 'G7': ['G3','B3','D4','F4'], 'Cmaj7': ['G3','B3','C4','E4']},
  bd=[['Dm9','D3 A3 C4 E4 F4'],['G6','G3 D4 E4 B4'],['Cmaj7','C3 G3 B3 E4'],['Fmaj7','F3 C4 E4 A4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....8.......8...'], OH=['..7...7...7...7.'], CH=['43.343.343.343.3'], TB=['....5.......5...']),
   var=dict(K=['9...9...9...9...'], CL=['....8.......8...'], OH=['..7...7...7...7.'], CH=['43.343.343.343.3'], TB=['....5.......5...'], RD=['5.4.5.4.5.4.5.4.'], COH=['......4.......4.'], CLO=['...5.......5....']),
   fill=dict(K=['9...9...9...9...'], CL=['....8...........'], HT=['........6.6.....'], MT=['............6.6.'], LT=['.............6.7'])),
  bass=dict(
   A=['. . A1 - . . A2 . . . A1 - . . A2 .','. . D2 - . . D3 . . . D2 - . . D3 .','. . G1 - . . G2 . . . G1 - . . G2 .','. . C2 - . . C3 . . . C2 - . . E2 .'],
   B=['A1 - . A1 . . C2 - . . E2 - . G2 . E2','D2 - . D2 . . F2 - . . A2 - . C3 . A2','G1 - . G1 . . B1 - . . D2 - . F2 . D2','C2 - . C2 . . E2 - . . G2 - . E2 . B1']),
  chordsPart=dict(
   A=['@Am7 - . @Am7 . . @Am7 - . . @Am7 . @Am7 - . .','@Dm7 - . @Dm7 . . @Dm7 - . . @Dm7 . @Dm7 - . .','@G7 - . @G7 . . @G7 - . . @G7 . @G7 - . .','@Cmaj7 - . @Cmaj7 . . @Cmaj7 - . . @Cmaj7 . @Cmaj7 - . .'],
   B=['@Am7 - - - . . @Am7 - . . @Am7 - - - . .','@Dm7 - - - . . @Dm7 - . . @Dm7 - - - . .','@G7 - - - . . @G7 - . . @G7 - - - . .','@Cmaj7 - - - . . @Cmaj7 - . . @Cmaj7 - - - . .']),
  lead=dict(
   A=['E5 - - - . . . . C5 - - - . . . .','D5 - - - . . . . F5 - - - . . . .','D5 - - - . . . . B4 - - - . . . .','C5 - - - . . . . E5 - - - G5 - - -'],
   B=['A5 . G5 . E5 . . . A5 . G5 . E5 . C5 .','A5 . F5 . D5 . . . A5 . F5 . D5 . C5 .','G5 . F5 . D5 . . . G5 . F5 . D5 . B4 .','G5 . E5 . C5 . . . G5 . E5 . C5 . B4 .'])),

 dict(id='CH02', genre='ch', name='Deep Hours', bpm=128, root=2, mode='minor', swing=0.55,
  style='Deep house', vibe='Warm Rhodes minor-nine changes, round sub, shaker-led groove, airy flute topline.',
  sound=dict(kit='house', bass='sub', chords='rhodes', lead='flute'),
  chords={'Dm9': ['F3','A3','C4','E4'], 'Gm9': ['F3','A3','Bb3','D4'], 'Bbmaj9': ['F3','A3','C4','D4'], 'A7sus4': ['G3','A3','D4','E4']},
  bd=[['Gm9','G2 D3 F3 A3 Bb3'],['C6','C3 G3 A3 E4'],['Fmaj7','F3 C4 E4 A4'],['A7sus4','A2 E3 G3 D4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....7.......7...'], SH=['3243324332433243'], OH=['..5...5...5...5.'], RIM=['...4......4.....']),
   var=dict(K=['9...9...9...9...'], CL=['....7.......7...'], SH=['3243324332433243'], OH=['..5...5...5...5.'], RIM=['...4..4...4..4..'], COH=['.......4......4.'], CLO=['...5.......5....'], CLV=['.....4.....4....']),
   fill=dict(K=['9...9...9...9...'], CL=['....7.......7.5.'], COH=['........5.5.5.5.'], CLO=['.........5.5.5.5'], SH=['32433243........'])),
  bass=dict(
   A=['D2 - - . . . . D2 . . A1 - . . C2 -','G1 - - . . . . G1 . . D2 - . . F2 -','Bb1 - - . . . . Bb1 . . F1 - . . A1 -','A1 - - . . . . A1 . . E2 - . . C#2 -'],
   B=['. . D2 . . . D2 . . . D2 . . . C2 .','. . G1 . . . G1 . . . G1 . . . F1 .','. . Bb1 . . . Bb1 . . . Bb1 . . . A1 .','. . A1 . . . A1 . . . A1 . . . C#2 .']),
  chordsPart=dict(
   A=['@Dm9 - - - - - . @Dm9 - - . . . . . .','@Gm9 - - - - - . @Gm9 - - . . . . . .','@Bbmaj9 - - - - - . @Bbmaj9 - - . . . . . .','@A7sus4 - - - - - . @A7sus4 - - . . @A7sus4 - . .'],
   B=['. . . @Dm9 . . . . . . @Dm9 . . . . .','. . . @Gm9 . . . . . . @Gm9 . . . . .','. . . @Bbmaj9 . . . . . . @Bbmaj9 . . . . .','. . . @A7sus4 . . . . . . @A7sus4 . . @A7sus4 . .']),
  lead=dict(
   A=['A4 - - - - - - - . . . . F5 - - -','D5 - - - - - - - . . . . A4 - - -','C5 - - - - - - - . . . . D5 - - -','E5 - - - - - - - . . . . C#5 - - -'],
   B=['. . . . . . D5 . . F5 . . A5 . . .','. . . . . . D5 . . G5 . . Bb5 . . .','. . . . . . C5 . . F5 . . A5 . . .','. . . . . . D5 . . E5 . . G5 . . .'])),

 dict(id='CH03', genre='ch', name='Jack Tracks', bpm=130, root=7, mode='minor', swing=0.56,
  style='Chicago jack', vibe='Busy 909 hats and cowbell, stabbing organ riff, a bassline that sits right on the kick.',
  sound=dict(kit='house', bass='organbass', chords='organ', lead='vox'),
  chords={'Gm7': ['Bb3','D4','F4','G4'], 'Cm7': ['Bb3','C4','Eb4','G4']},
  bd=[['Ebmaj9','Eb3 Bb3 D4 F4 G4'],['F6','F3 C4 D4 A4'],['Cm9','C3 G3 Bb3 D4 Eb4'],['Dm7','D3 A3 C4 F4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], CH=['6464646464.46464'], OH=['..........7.....'], CB=['...5...5...5..5.'], RIM=['.......5.......5']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9.5.'], CH=['6464646464.46464'], OH=['..........7.....'], CB=['...5...5..5..55.'], TB=['..5...5...5...5.']),
   fill=dict(K=['9...9...9.9.9999'], CL=['....9...........'], SN=['................4.4.5.5.66778899'], CH=['6464646464646...'])),
  bass=dict(
   A=['G1 . G1 . . G2 . G1 . . G1 . Bb1 . C2 .','G1 . G1 . . G2 . G1 . . G1 . F1 . G1 .','G1 . G1 . . G2 . G1 . . G1 . Bb1 . C2 .','C2 . C2 . . C3 . C2 . . C2 . Bb1 . G1 .'],
   B=['. . G1 G1 . . G2 . . . G1 G1 . . F1 .','. . G1 G1 . . G2 . . . Bb1 Bb1 . . A1 .','. . G1 G1 . . G2 . . . G1 G1 . . F1 .','. . C2 C2 . . C3 . . . Bb1 Bb1 . . A1 .']),
  chordsPart=dict(
   A=['@Gm7 . . @Gm7 . . @Gm7 . . . @Gm7 . @Gm7 . . .','@Gm7 . . @Gm7 . . @Gm7 . . . @Gm7 . @Gm7 . . .','@Gm7 . . @Gm7 . . @Gm7 . . . @Gm7 . @Gm7 . . .','@Cm7 . . @Cm7 . . @Cm7 . . . @Cm7 . @Cm7 . @Cm7 .'],
   B=['. . @Gm7 . . . @Gm7 . . . @Gm7 . . . @Gm7 .','. . @Gm7 . . . @Gm7 . . . @Gm7 . . . @Gm7 .','. . @Gm7 . . . @Gm7 . . . @Gm7 . . . @Gm7 .','. . @Cm7 . . . @Cm7 . . . @Cm7 . . . @Cm7 .']),
  lead=dict(
   A=['D5 . . D5 . . F5 . . D5 . . C5 . Bb4 .','G4 - - - . . . . . . . . Bb4 . C5 .','D5 . . D5 . . F5 . . D5 . . G5 . F5 .','Eb5 - - - . . . . . . . . D5 . C5 .'],
   B=['. . . . G5 . . . . . . . G5 . . .','. . . . F5 . . . . . . . D5 . . .','. . . . G5 . . . . . . . Bb5 . . .','. . . . G5 . . . . . . . Eb5 . . .'])),

 dict(id='CH04', genre='ch', name='Organ Shuffle', bpm=129, root=5, mode='minor', swing=0.58,
  style='NY garage house', vibe='Bouncing M1-style organ bass under minor-nine piano pushes, swung hats and tambourine.',
  sound=dict(kit='house', bass='organbass', chords='piano', lead='organlead'),
  chords={'Fm9': ['Ab3','C4','Eb4','G4'], 'Bbm9': ['Ab3','C4','Db4','F4'], 'Eb9sus4': ['Ab3','Bb3','Db4','F4'], 'Dbmaj9': ['Ab3','C4','Eb4','F4']},
  bd=[['Bbm9','Bb2 F3 Ab3 C4 Db4'],['Eb9sus4','Eb3 Ab3 Bb3 Db4 F4'],['Abmaj9','Ab2 Eb3 G3 Bb3 C4'],['Dbmaj9','Db3 Ab3 C4 Eb4 F4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....8.......8...'], OH=['..7...7...7...7.'], CH=['43.343.343.343.3'], SH=['2423242324232423'], TB=['....5.......5...']),
   var=dict(K=['9...9...9...9...'], CL=['....8.......8...'], OH=['..7...7...7...7.'], CH=['43.343.343.343.3'], SH=['2423242324232423'], TB=['....5.......5...'], RIM=['...4..4....4..4.']),
   fill=dict(K=['9...9...9...9.66'], CL=['....8...........'], SN=['................4.5.5.6.67788999'], OH=['..7...7.........'])),
  bass=dict(
   A=['F1 - . F2 . . F1 - . . Ab1 - . C2 . F1','Bb1 - . Bb2 . . Bb1 - . . Db2 - . F2 . Bb1','Eb2 - . Eb2 . . Bb1 - . . Eb2 - . Db2 . Bb1','Db2 - . Db2 . . Ab1 - . . Db2 - . C2 . E1'],
   B=['. . F1 - . F2 . F1 . . Ab1 - . C2 - .','. . Bb1 - . Bb2 . Bb1 . . Db2 - . F2 - .','. . Eb2 - . Eb2 . Bb1 . . Eb2 - . Db2 - .','. . Db2 - . Db2 . Ab1 . . Db2 - . C2 - .']),
  chordsPart=dict(
   A=['@Fm9 - . . . . @Fm9 - . . . . @Fm9 - . .','@Bbm9 - . . . . @Bbm9 - . . . . @Bbm9 - . .','@Eb9sus4 - . . . . @Eb9sus4 - . . . . @Eb9sus4 - . .','@Dbmaj9 - . . . . @Dbmaj9 - . . . . @Dbmaj9 - . .'],
   B=['. . @Fm9 . . . @Fm9 - . . @Fm9 . . . @Fm9 -','. . @Bbm9 . . . @Bbm9 - . . @Bbm9 . . . @Bbm9 -','. . @Eb9sus4 . . . @Eb9sus4 - . . @Eb9sus4 . . . @Eb9sus4 -','. . @Dbmaj9 . . . @Dbmaj9 - . . @Dbmaj9 . . . @Dbmaj9 -']),
  lead=dict(
   A=['C5 - . C5 . . Ab4 - . . F4 . . Ab4 - .','Bb4 - . Bb4 . . Db5 - . . F5 . . Db5 - .','Bb4 - . Bb4 . . Ab4 - . . F4 . . Ab4 - .','Ab4 - . Ab4 . . C5 - . . Eb5 . . F5 - .'],
   B=['. . . . . . . . Eb5 - . C5 . . . .','. . . . . . . . F5 - . Db5 . . . .','. . . . . . . . Eb5 - . Bb4 . . . .','. . . . . . . . F5 - . C5 . . . .'])),

 dict(id='CH05', genre='ch', name='Filter Disco', bpm=128, root=10, mode='minor', swing=0.52,
  style='Filter house', vibe='Disco octave bass, chopped funk chords, a string riff on top. Built for a sweeping filter.',
  sound=dict(kit='house', bass='pluck', chords='stab', lead='strings'),
  chords={'Bbm9': ['Db4','F4','Ab4','C5'], 'Gbmaj7': ['Db4','F4','Gb4','Bb4'], 'Ebm7': ['Db4','Eb4','Gb4','Bb4'], 'F7sus4': ['Eb4','F4','Bb4','C5']},
  bd=[['Gbmaj9','Gb2 Db3 F3 Ab3 Bb3'],['Ab6','Ab2 Eb3 F3 C4'],['Fm7','F2 C3 Eb3 Ab3'],['Bbm9','Bb2 F3 Ab3 C4 Db4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....8.......8...'], SN=['....6.......6...'], OH=['..7...7...7...7.'], CH=['53.353.353.353.3'], TB=['..5...5...5...5.']),
   var=dict(K=['9...9...9...9...'], CL=['....8.......8...'], SN=['....6.......6...'], OH=['..7...7...7...7.'], CH=['53.353.353.353.3'], TB=['..5...5...5...5.'], COH=['......5.......5.'], CLO=['...5.......5....']),
   fill=dict(K=['9...9...9...9...'], SN=['....6...6.6.6666'], OH=['..7...7.........'], COH=['........5.5.5.5.'])),
  bass=dict(
   A=['Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 .','Gb1 . Gb2 . Gb1 . Gb2 . Gb1 . Gb2 . Gb1 . Gb2 .','Eb2 . Eb3 . Eb2 . Eb3 . Eb2 . Eb3 . Eb2 . Eb3 .','F1 . F2 . F1 . F2 . F1 . F2 . F1 . A1 .'],
   B=['Bb1 - . Bb2 . Bb1 - . . Bb1 . Db2 . Eb2 . F2','Gb1 - . Gb2 . Gb1 - . . Gb1 . Bb1 . Db2 . Eb2','Eb2 - . Eb3 . Eb2 - . . Eb2 . Gb2 . F2 . Db2','F1 - . F2 . F1 - . . F1 . Ab1 . C2 . A1']),
  chordsPart=dict(
   A=['. . @Bbm9 . . @Bbm9 . . . . @Bbm9 . . @Bbm9 . .','. . @Gbmaj7 . . @Gbmaj7 . . . . @Gbmaj7 . . @Gbmaj7 . .','. . @Ebm7 . . @Ebm7 . . . . @Ebm7 . . @Ebm7 . .','. . @F7sus4 . . @F7sus4 . . . . @F7sus4 . . @F7sus4 . .'],
   B=['@Bbm9 - - - - - - - - - - - - - - -','@Gbmaj7 - - - - - - - - - - - - - - -','@Ebm7 - - - - - - - - - - - - - - -','@F7sus4 - - - - - - - - - - - - - - -']),
  lead=dict(
   A=['F5 - . F5 . . Db5 - . . Eb5 . . F5 - .','Gb5 - . F5 . . Db5 - . . Bb4 . . Db5 - .','Eb5 - . Eb5 . . Bb4 - . . Db5 . . Eb5 - .','F5 - - - . . Eb5 - . . C5 - - - . .'],
   B=['Bb5 - - - - - - - Ab5 - - - F5 - - -','Gb5 - - - - - - - F5 - - - Db5 - - -','Gb5 - - - - - - - Eb5 - - - Db5 - - -','C5 - - - - - - - Eb5 - - - F5 - - -'])),

 dict(id='CH06', genre='ch', name='Warehouse Gospel', bpm=132, root=8, mode='major', swing=0.55,
  style='Gospel house', vibe='IV-iii-ii-V gospel changes on piano, organ bass, strings carrying a soulful topline.',
  sound=dict(kit='house', bass='organbass', chords='piano', lead='strings'),
  chords={'Dbmaj9': ['Ab3','C4','Eb4','F4'], 'Cm7': ['Bb3','C4','Eb4','G4'], 'Bbm7': ['Ab3','Bb3','Db4','F4'], 'Eb9sus4': ['Db4','F4','Ab4','Bb4']},
  bd=[['Fm9','F2 C3 Eb3 G3 Ab3'],['Dbmaj9','Db3 Ab3 C4 Eb4 F4'],['Ab6','Ab2 Eb3 F3 C4'],['Eb9sus4','Eb3 Ab3 Bb3 Db4 F4']],
  drums=dict(
   main=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['....6.......6...'], OH=['..7...7...7...7.'], SH=['3243324332433243'], TB=['....6.......6...']),
   var=dict(K=['9...9...9...9...'], CL=['....9.......9...'], SN=['....6.......6...'], OH=['..7...7...7...7.'], SH=['3243324332433243'], TB=['....6.......6...'], RD=['5.5.5.5.5.5.5.5.'], CH=['43.343.343.343.3']),
   fill=dict(K=['9...9...9...9...'], CL=['....9...........'], SN=['................4.4.5.5.66778899'], TB=['....6...........'])),
  bass=dict(
   A=['Db2 - . Db2 . . Ab1 - . . Db2 - . F2 . Eb2','C2 - . C2 . . G1 - . . C2 - . Eb2 . Db2','Bb1 - . Bb1 . . F1 - . . Bb1 - . Db2 . C2','Eb2 - . Eb2 . . Bb1 - . . Eb2 - . Bb1 . C2'],
   B=['Db2 - - - . . Db2 . Eb2 - - . F2 - Ab2 -','C2 - - - . . C2 . Eb2 - - . G2 - Eb2 -','Bb1 - - - . . Bb1 . Db2 - - . F2 - Db2 -','Eb2 - - - . . Eb2 . Bb1 - - . Db2 - C2 -']),
  chordsPart=dict(
   A=['@Dbmaj9 - - . . . @Dbmaj9 - - . @Dbmaj9 - - . . .','@Cm7 - - . . . @Cm7 - - . @Cm7 - - . . .','@Bbm7 - - . . . @Bbm7 - - . @Bbm7 - - . . .','@Eb9sus4 - - . . . @Eb9sus4 - - . @Eb9sus4 - - . . .'],
   B=['. . @Dbmaj9 . . . @Dbmaj9 . . . @Dbmaj9 . . . @Dbmaj9 .','. . @Cm7 . . . @Cm7 . . . @Cm7 . . . @Cm7 .','. . @Bbm7 . . . @Bbm7 . . . @Bbm7 . . . @Bbm7 .','. . @Eb9sus4 . . . @Eb9sus4 . . . @Eb9sus4 . . . @Eb9sus4 .']),
  lead=dict(
   A=['C5 - - - . . Eb5 - . . F5 - - - . .','Eb5 - - - . . G5 - . . Eb5 - - - . .','Db5 - - - . . F5 - . . Ab5 - - - . .','Bb4 - - - . . Db5 - . . Eb5 - - - F5 -'],
   B=['. . . . . . . . . . . . Ab5 . Eb5 .','. . . . . . . . . . . . G5 . Eb5 .','. . . . . . . . . . . . F5 . Db5 .','. . . . . . . . . . . . Bb5 . F5 .'])),
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
    return bad


def to_js(k):
    q = lambda x: json.dumps(x).replace('"', "'")
    out = "    {\n"
    out += f"      id: '{k['id']}', song: {{ bd: {q(k['bd'])} }}, genre: '{k['genre']}', name: {q(k['name'])}, bpm: {k['bpm']}, root: {k['root']}, mode: '{k['mode']}', swing: {k['swing']},\n"
    out += f"      style: {q(k['style'])}, vibe: {q(k['vibe'])},\n"
    out += f"      sound: {q(k['sound'])},\n"
    out += f"      prog: '',\n      chords: {q(k['chords'])},\n"
    out += "      drums: {\n" + ",\n".join(f"        {v}: {{ " + ", ".join(f"{lane}: {q(b)}" for lane, b in lanes.items()) + " }" for v, lanes in k['drums'].items()) + "\n      },\n"
    for part in ('bass', 'chordsPart', 'lead'):
        out += f"      {part}: {{\n        A: {q(k[part]['A'])},\n        B: {q(k[part]['B'])}\n      }},\n"
    out = out.rstrip(',\n') + "\n    }"
    return out


if __name__ == '__main__':
    if check():
        sys.exit('fix token counts first')
    js = ",\n".join(to_js(k) for k in KITS)
    p = '/home/claude/work/core.js'
    s = open(p).read()
    if "id: 'SG01'" in s:
        sys.exit('already inserted')
    marker = "\n  ];\n\n  /* ---------- compile ---------- */"
    assert marker in s
    s = s.replace(marker, ",\n" + js + marker)
    open(p, 'w').write(s)
    print('inserted', len(KITS), 'kits')
