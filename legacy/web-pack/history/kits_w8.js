    {
      id: 'GV01', fam: 'gv', song: { bd: [['Amaj9', 'A2 E3 G#3 B3 C#4'], ['G#m7', 'G#2 D#3 F#3 B3'], ['F#m9', 'F#2 C#3 E3 G#3 A3'], ['C#m9', 'C#3 G#3 B3 D#4 E4']] }, genre: 'ch', name: "Canal Lights", bpm: 130, root: 1, mode: 'minor', swing: 0.60,
      style: "Groove house", vibe: "A bouncy kick and a clap drowned in reverb, swung vintage hats and glitch ticks; an FM rubber bass that starts on the roots and turns into its own melody, glow-saw chords whose reverb swells in the gaps, and a Juno lead in fifths.",
      sound: {'kit': 'gv', 'bass': 'gvfm', 'chords': 'gvglow', 'lead': 'gvfifth'},
      prog: "C#m9 | Amaj9 | F#m9 | G#m11",
      chords: {'C#m9': ['E4', 'G#4', 'B4', 'D#5'], 'Amaj9': ['E4', 'G#4', 'B4', 'C#5'], 'F#m9': ['E4', 'G#4', 'A4', 'C#5'], 'G#m11': ['D#4', 'F#4', 'B4', 'C#5']},
      drums: {
        main: { K: ['9...9...9...9...', '9...9...9...9...', '9...9...9...9...', '9...9...9...9..5'], CL: ['....9.......9...'], CH: ['5764576457645764', '5764576457645766'], OH: ['..6...6...6...6.'], SH: ['.4.4.4.4.4.4.4.5'], RIM: ['......5...5..4..', '...5..5.....5...'], WB: ['...........4....', '......3.......4.'] },
        var: { K: ['9...9...9...9...'], CL: ['....9.......9...'], CH: ['5764576457645764'], OH: ['..6...6...6...6.'], SH: ['.4.5.4.5.4.5.4.5'], BH: ['..5.....5...5.5.', '...5.5......5...'], BL: ['5......5..5.....'], RIM: ['..4..4....4..4..'], WB: ['....3.......3...'] },
        fill: { K: ['9...9...9.......'], CL: ['....9.......9.79'], CH: ['5764576457645...'], OH: ['..6...6...6.....'], RIM: ['..........5.5.66'] }
      },
      bass: {
        A: ['. . C#2 C#3, . . C#2 - . . C#2 E2 . . B1 C#2', '. . A1 A2, . . A1 - . . A1 C#2 . . G#1 A1', '. . F#1 F#2, . . F#1 - . . F#1 A1 . . E1 F#1', '. . G#1 G#2, . . G#1 - . . B1 C#2 . . D#2~ E2'],
        B: ['. . C#2 - . E2 . C#2 . . G#2 - . B1~ C#2 .', '. . A1 - . C#2 . A1 . . E2 - . G#1~ A1 .', '. . F#1 - . A1 . F#1 . . C#2 - . E1~ F#1 .', '. . G#1 - . B1 . G#1 . . D#2 - . F#2~ G#2 .']
      },
      chordsPart: {
        A: ['. . . @C#m9 - . @C#m9 - . . . . . . . .', '. . . @Amaj9 - . @Amaj9 - . . . . . . @Amaj9 -', '. . . @F#m9 - . @F#m9 - . . . . . . . .', '. . . @G#m11 - . @G#m11 - . . . . @G#m11 - . .'],
        B: ['@C#m9 - - - - - . . . . . . @C#m9 - . .', '@Amaj9 - - - - - . . . . . . @Amaj9 - . .', '@F#m9 - - - - - . . . . . . @F#m9 - . .', '@G#m11 - - - - - . . . . @G#m11 - - - . .']
      },
      lead: {
        A: ['. . . G#4 - . . . . . E4 - . F#4 . .', '. . . G#4 - . . . . . C#5 - . B4 . .', '. . . F#4 - . . . . . E4 - . C#4 . .', '. . . G#4 - . . . B4 - . . . . . .'],
        B: ['. . C#5 . . . C#5 . . . E5 . . . . .', '. . C#5 . . . C#5 . . . E5 . . B4 . .', '. . A4 . . . A4 . . . C#5 . . . . .', '. . B4 . . . B4 . . . E5 . . G#4 . .']
      },
      ext: {"sidechain": true, "form": [["Intro (hats first)", "intro", 32, {}], ["Bass in", "groove", 16, {"energy": 2, "chords": false, "lead": false}], ["Chords in", "groove", 16, {"energy": 2, "lead": false, "verbswell": true}], ["Fifths lead", "groove", 16, {"energy": 3, "counter": "call", "verbswell": true}], ["Breakdown (chords)", "breakdown", 16, {"strings": "pad", "verbswell": true}], ["Build", "build", 8, {}], ["Drop", "drop", 32, {"energy": 3, "counter": "harmony", "verbswell": true}], ["Bass solo (B line)", "switch", 16, {"chords": false}], ["Drop 2", "drop", 32, {"energy": 4, "counter": "call", "breaks": "layer", "verbswell": true}], ["Outro (DJ)", "outro", 32, {}]]}
    },
    {
      id: 'GV02', fam: 'gv', song: { bd: [['Ebmaj9', 'Eb2 Bb2 D3 F3 G3'], ['Cm9', 'C3 G3 Bb3 D4 Eb4'], ['Bbmaj7', 'Bb2 F3 A3 D4'], ['Dm7', 'D3 A3 C4 F4']] }, genre: 'ch', name: "Brown Cafe", bpm: 129, root: 7, mode: 'minor', swing: 0.58,
      style: "Jazzy groove house", vibe: "A bumping kick, a big-room clap and a bongo groove; an FM bass that walks between the roots, Rhodes stabs with dub throws, a sweet piano line and jazzy strings in the breakdown.",
      sound: {'kit': 'gv', 'bass': 'gvfm', 'chords': 'rhodes', 'lead': 'piano'},
      prog: "Gm9 | Cm9 | Ebmaj9 | D7b9",
      chords: {'Gm9': ['Bb3', 'D4', 'F4', 'A4'], 'Cm9': ['Bb3', 'D4', 'Eb4', 'G4'], 'Ebmaj9': ['Bb3', 'D4', 'F4', 'G4'], 'D7b9': ['C4', 'Eb4', 'F#4', 'A4']},
      drums: {
        main: { K: ['9...9...9...9...'], CL: ['....9.......9...'], CH: ['4.6.4.6.4.6.4.65'], OH: ['..6.......6.....'], SH: ['4545454545454545'], BH: ['..5.....5...5.5.', '...5.5......5...'], BL: ['5......5..5.....', '5.....5...5..5..'], RIM: ['......4.........', '...4......4.....'] },
        var: { K: ['9...9...9...9...'], CL: ['....9.......9...'], CH: ['4.6.4.6.4.6.4.6.'], OH: ['..6...6...6...6.'], SH: ['4545454545454545'], COH: ['......5...5.....', '...5..5.....5...'], CLO: ['5.......5..5....'], WB: ['..........4.....'] },
        fill: { K: ['9...9...9.......'], CL: ['....9.......9...'], COH: ['........5.5.6.6.'], CLO: ['..........5.5.6.'], CH: ['4.6.4.6.4.6.....'] }
      },
      bass: {
        A: ['G1 - - . . . G1 . . . Bb1 - . . D2 .', 'C2 - - . . . C2 . . . Eb2 - . . G2 .', 'Eb1 - - . . . Eb1 . . . G1 - . . Bb1 .', 'D1 - - . . . D2 . . . C2 - . . A1 .'],
        B: ['G1 - . Bb1 . C2 . D2 . . F2 - . D2 . C2', 'C2 - . D2 . Eb2 . G2 . . Bb2 - . G2 . Eb2', 'Eb1 - . G1 . Bb1 . D2 . . F2 - . D2 . Bb1', 'D2 - . C2 . A1 . F#1 . . D1 - . F#1~ G1 .']
      },
      chordsPart: {
        A: ['. . . @Gm9 - - . . . . @Gm9 - . . . .', '. . . @Cm9 - - . . . . @Cm9 - . . . .', '. . . @Ebmaj9 - - . . . . @Ebmaj9 - . . . .', '. . . @D7b9 - - . . . . @D7b9 - . . @D7b9 -'],
        B: ['@Gm9 - - - - - - - . . @Gm9 - - - . .', '@Cm9 - - - - - - - . . @Cm9 - - - . .', '@Ebmaj9 - - - - - - - . . @Ebmaj9 - - - . .', '@D7b9 - - - - - - - . . @D7b9 - - - . .']
      },
      lead: {
        A: ['. . D5 . . Bb4 . . C5 - . A4 . . . .', '. . Eb5 . . C5 . . D5 - . Bb4 . . . .', '. . F5 . . D5 . . Eb5 - . C5 . . . .', '. . F#5 . . A4 . . C5 - . D5 - - - .'],
        B: ['. . . . . . . . . . G5 . F5 . D5 .', 'C5 - - . . . . . . . . . . . . .', '. . . . . . . . . . Bb5 . A5 . F5 .', 'F#5 - - . . . . . . . . . A5 . D5 .']
      },
      ext: {"sidechain": true, "form": [["Intro (bongos)", "intro", 32, {}], ["Groove", "groove", 16, {"energy": 2, "stabs": true, "stabsFrom": 8, "lead": false}], ["Piano", "groove", 16, {"energy": 2, "piano": "fills"}], ["Dub break", "dubBreak", 8, {"toGap": true}], ["Drop", "drop", 32, {"energy": 3, "counter": "call"}], ["Breakdown (strings)", "breakdown", 16, {"strings": "legato"}], ["Build", "build", 8, {"strings": "swell"}], ["Drop 2", "drop", 32, {"energy": 4, "counter": "harmony", "piano": "fills"}], ["Outro (DJ)", "outro", 32, {}]]}
    },
    {
      id: 'GV03', fam: 'gv', song: { bd: [['Cbmaj9', 'Cb3 Gb3 Bb3 Db4 Eb4'], ['Db6', 'Db3 Ab3 Bb3 F4'], ['Abm9', 'Ab2 Eb3 Gb3 Bb3 Cb4'], ['Ebm9', 'Eb3 Bb3 Db4 F4 Gb4']] }, genre: 'ch', name: "Late Checkout", bpm: 131, root: 3, mode: 'minor', swing: 0.60,
      style: "Disco groove house", vibe: "Offbeat open hats and tambourine over a four-four, an FM bass jumping octaves like a disco record, stuttering glow-saw chords, a string riff on top and a 90s lead in fifths answering it.",
      sound: {'kit': 'gv', 'bass': 'gvfm', 'chords': 'gvglow', 'lead': 'strings'},
      prog: "Ebm9 | Cbmaj9 | Abm9 | Bbm11",
      chords: {'Ebm9': ['Gb3', 'Bb3', 'Db4', 'F4'], 'Cbmaj9': ['Gb3', 'Bb3', 'Db4', 'Eb4'], 'Abm9': ['Gb3', 'Bb3', 'Cb4', 'Eb4'], 'Bbm11': ['F3', 'Ab3', 'Db4', 'Eb4']},
      drums: {
        main: { K: ['9...9...9...9...'], CL: ['....9.......9...'], OH: ['..7...7...7...7.'], CH: ['5.4.5.4.5.4.5.45'], SH: ['.3.3.3.3.3.3.3.3'], TB: ['....5.......5...'], RIM: ['...4......4.....'] },
        var: { K: ['9...9...9...9...'], CL: ['....9.......9...'], SN: ['....5.......5...'], OH: ['..7...7...7...7.'], CH: ['5454545454545454'], TB: ['..4...4...4...4.'], WB: ['.......3.......3'] },
        fill: { K: ['9...9...9...9...'], CL: ['....9...9.9.9999'], OH: ['..7...7...7.....'], CH: ['5454545454545...'] }
      },
      bass: {
        A: ['Eb1 . Eb2 . Eb1 . Eb2 . Eb1 . Eb2 . Eb1 . Eb2 Db2', 'Cb2 . Cb3 . Cb2 . Cb3 . Cb2 . Cb3 . Cb2 . Cb3 Bb2', 'Ab1 . Ab2 . Ab1 . Ab2 . Ab1 . Ab2 . Ab1 . Ab2 Gb2', 'Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 . Db2 . Db3~ Eb3'],
        B: ['Eb1 - . Eb2 . . Eb1 . Gb1 - . Ab1 . Bb1 . Db2', 'Cb2 - . Cb3 . . Cb2 . Db2 - . Eb2 . Gb2 . Ab2', 'Ab1 - . Ab2 . . Ab1 . Bb1 - . Cb2 . Eb2 . Gb2', 'Bb1 - . Bb2 . . Bb1 . Db2 - . F2 . Ab2 . Bb2']
      },
      chordsPart: {
        A: ['. . @Ebm9 @Ebm9 . . @Ebm9 . . . @Ebm9 @Ebm9 . . @Ebm9 .', '. . @Cbmaj9 @Cbmaj9 . . @Cbmaj9 . . . @Cbmaj9 @Cbmaj9 . . @Cbmaj9 .', '. . @Abm9 @Abm9 . . @Abm9 . . . @Abm9 @Abm9 . . @Abm9 .', '. . @Bbm11 @Bbm11 . . @Bbm11 . . . @Bbm11 - - - . .'],
        B: ['@Ebm9 - - - - - - - - - - - . . . .', '@Cbmaj9 - - - - - - - - - - - . . . .', '@Abm9 - - - - - - - - - - - . . . .', '@Bbm11 - - - - - - - - - - - . . . .']
      },
      lead: {
        A: ['Bb4 - - . Db5 - . Eb5 - - - . . . . .', 'Gb5 - - . F5 - . Eb5 - - - . . . . .', 'Db5 - - . Eb5 - . Cb5 - - - . . . . .', 'Bb4 - - - - - . Ab4 - - - - F4 - - -'],
        B: ['. . . . . . . . Bb5 - . Ab5 . Gb5 . .', 'F5 - - - . . . . . . . . . . . .', '. . . . . . . . Eb5 - . Db5 . Cb5 . .', 'Bb4 - - - . . . . F5 - - - . . . .']
      },
      ext: {"sidechain": true, "form": [["Intro (DJ)", "intro", 32, {}], ["Octave bass", "groove", 16, {"energy": 2, "chords": false, "lead": false}], ["Stutter chords", "groove", 16, {"energy": 3, "lead": false, "verbswell": true}], ["String hook", "drop", 32, {"energy": 3, "counter": "call", "verbswell": true}], ["Breakdown (strings)", "breakdown", 16, {"strings": "legato"}], ["Build", "build", 8, {"strings": "swell"}], ["Drop 2", "drop", 32, {"energy": 4, "counter": "harmony", "verbswell": true}], ["Disco switch", "switch", 16, {}], ["Outro (DJ)", "outro", 32, {}]]}
    },
    {
      id: 'TH01', fam: 'th', song: { bd: [['Dbmaj9', 'Db3 Ab3 C4 Eb4 F4'], ['Eb6', 'Eb3 Bb3 C4 G4'], ['Cm7', 'C3 G3 Bb3 Eb4'], ['Fm9', 'F2 C3 Eb3 G3 Ab3']] }, genre: 'mth', name: "Night Shift Bounce", bpm: 128, root: 5, mode: 'minor', swing: 0.53,
      style: "Tech house", vibe: "A fat punchy kick and a hard snare on the backbeat, driving hats and an offbeat tom; a deep, plucky bounce bass whose filter opens on the hard-hit notes, a dry stab every other bar, an FM pluck motif and vocal shots on the offbeats in the drop.",
      sound: {'kit': 'th', 'bass': 'thbounce', 'chords': 'stab', 'lead': 'fmpluck'},
      prog: "Fm11 | Fm11 | Dbmaj9 | Eb6",
      chords: {'Fm11': ['Ab3', 'C4', 'Eb4', 'Bb4'], 'Dbmaj9': ['Ab3', 'C4', 'Eb4', 'F4'], 'Eb6': ['G3', 'Bb3', 'C4', 'Eb4']},
      drums: {
        main: { K: ['9...9...9...9...'], SN: ['....9.......9...'], CH: ['7.6.7.6.7.6.7.6.', '7.6.7.6.7.6.7.66'], SH: ['.4.5.4.5.4.5.4.5'], MT: ['.......6........', '...........6....'], OH: ['................', '..............5.'] },
        var: { K: ['9...9...9...9...'], SN: ['....9.......9...'], CL: ['....6.......6...'], CH: ['7.6.7.6.7.6.7.6.'], RIM: ['..6...6...6...6.'], SH: ['.4.5.4.5.4.5.4.5'], MT: ['.......6........'], LT: ['...........5....'], HT: ['...5............'] },
        fill: { K: ['9...9...9.......'], SN: ['....9...7.7.8899'], CH: ['7.6.7.6.7.6.....'], LT: ['..........6.....'], HT: ['..............7.'] }
      },
      bass: {
        A: ['F1* . . F1 . . F2, . F1* . . F1 . Eb1 F1 .', 'F1* . . F1 . . F2, . F1* . . F1 . Ab1 C2 .', 'Db2* . . Db2 . . Db3, . Db2* . . Db2 . C2 Db2 .', 'Eb1* . . Eb1 . . Eb2, . Eb1* . . Eb1 . F1 G1 .'],
        B: ['F1* . F1 . Ab1 . . F1 C2* . . Ab1 . F1 Eb1 .', 'F1* . F1 . Ab1 . . F1 Eb2* . . C2 . Ab1 F1 .', 'Db2* . Db2 . F2 . . Db2 Ab2* . . F2 . Db2 C2 .', 'Eb1* . Eb1 . G1 . . Eb1 Bb1* . . G1 . F1 Eb1 .']
      },
      chordsPart: {
        A: ['. . . . . . @Fm11 . . . . . . . . .', '. . . . . . . . . . . . . . . .', '. . . . . . @Dbmaj9 . . . . . . . . .', '. . . . . . . . . . @Eb6 . . . . .'],
        B: ['. . @Fm11 . . . @Fm11 . . . . . . . . .', '. . . . . . @Fm11 . . . . . @Fm11 . . .', '. . @Dbmaj9 . . . @Dbmaj9 . . . . . . . . .', '. . . . . . @Eb6 . . . @Eb6 . . . . .']
      },
      lead: {
        A: ['. . C5 . . . . . . . Ab4 . . . . .', '. . C5 . . . . . . . Eb5 . . C5 . .', '. . Ab4 . . . . . . . F4 . . . . .', '. . Bb4 . . . . . . . G4 . . Bb4 . .'],
        B: ['. . C5 . . C5 . . . . Ab4 . . Bb4 . .', '. . C5 . . C5 . . . . Eb5 . . F5 . .', '. . Ab4 . . Ab4 . . . . F4 . . G4 . .', '. . Bb4 . . Bb4 . . . . G4 . . Eb4 . .']
      },
      ext: {"sidechain": true, "shouts": "TH_SHOTS", "form": [["Intro (filtered kick)", "intro", 16, {"filter": "open"}], ["Bass first", "groove", 16, {"energy": 2, "chords": false, "lead": false}], ["Groove", "groove", 16, {"energy": 2, "stabs": true, "stabsFrom": 8, "lead": false, "shouts": "sparse"}], ["Build", "build", 8, {}], ["Drop", "drop", 32, {"energy": 3, "shouts": "offbeat", "counter": "call"}], ["Tom switch", "switch", 16, {"shouts": "answer"}], ["Break", "breakdown", 8, {}], ["Build 2", "build", 8, {"tapestop": true}], ["Drop 2", "drop", 32, {"energy": 4, "shouts": "offbeat"}], ["Outro (DJ)", "outro", 32, {}]]}
    },
    {
      id: 'TH02', fam: 'th', song: { bd: [['Cmaj7', 'C3 G3 B3 E4'], ['Am9', 'A2 E3 G3 B3 C4'], ['D6', 'D3 A3 B3 F#4'], ['Em9', 'E2 B2 D3 F#3 G3']] }, genre: 'mth', name: "Black Marble", bpm: 128, root: 4, mode: 'minor', swing: 0.52,
      style: "Dark tech house", vibe: "A hard four-four with a distorted snare and rolling toms; a talking bass that opens and closes like a mouth, a dramatic minor string hook with a Phrygian turn, and pitched vocal shots answering it.",
      sound: {'kit': 'th', 'bass': 'thtalk', 'chords': 'darkstab', 'lead': 'strings'},
      prog: "Em9 | Em7 | Cmaj7 | B7sus4",
      chords: {'Em9': ['G3', 'B3', 'D4', 'F#4'], 'Em7': ['G3', 'B3', 'D4', 'G4'], 'Cmaj7': ['G3', 'B3', 'C4', 'E4'], 'B7sus4': ['A3', 'B3', 'E4', 'F#4']},
      drums: {
        main: { K: ['9...9...9...9...'], SN: ['....9.......9...'], CH: ['7.6.7.6.7.6.7.6.'], OH: ['..5.......5.....'], MT: ['.......6.......5', '...6.......6....'], LT: ['...........6....'], SH: ['.3.4.3.4.3.4.3.4'] },
        var: { K: ['9...9...9...9...'], SN: ['....9.......9..4'], CL: ['....7.......7...'], CH: ['7676767676767676'], RIM: ['..5...5...5...5.'], MT: ['...6...6...6...6'], LT: ['.5.......5......'] },
        fill: { K: ['9...9...9.......'], SN: ['....9.......9...'], LT: ['..........6.6...'], MT: ['........7.7.....'], HT: ['............8.8.'] }
      },
      bass: {
        A: ['E1* - . E1 . . G1~ A1 - . E1* . . D1~ E1 .', 'E1* - . E1 . . G1~ A1 - . B1* . . A1~ G1 .', 'C2* - . C2 . . E2~ D2 - . C2* . . B1~ C2 .', 'B1* - . B1 . . D2~ E2 - . F#2* . . E2~ D2 .'],
        B: ['E1* - - - . . E2, . E1* - . G1 . A1~ B1 .', 'E1* - - - . . E2, . E1* - . D2 . B1~ G1 .', 'C2* - - - . . C3, . C2* - . E2 . G2~ E2 .', 'B1* - - - . . B2, . B1* - . A1 . F#1~ G1 .']
      },
      chordsPart: {
        A: ['. . . . . . . . . . . . . . @Em9 -', '. . . . . . . . . . . . . . . .', '. . . . . . . . . . . . . . @Cmaj7 -', '. . . . . . . . . . @B7sus4 - - - . .'],
        B: ['@Em9 - - - - - - - - - - - - - - .', '@Em7 - - - - - - - - - - - - - - .', '@Cmaj7 - - - - - - - - - - - - - - .', '@B7sus4 - - - - - - - - - - - - - - .']
      },
      lead: {
        A: ['E4 - - - G4 - - - B4 - - - A4 - G4 -', 'F4 - - - E4 - - - D4 - - - E4 - - -', 'C5 - - - B4 - - - A4 - - - G4 - - -', 'F#4 - - - A4 - - - B4 - - - - - - -'],
        B: ['E5 . . E5 . . G5 . . E5 . . B4 . . .', 'F5 . . F5 . . E5 . . D5 . . E5 . . .', 'C5 . . C5 . . E5 . . C5 . . G4 . . .', 'B4 . . B4 . . D5 . . F#5 . . B4 - - -']
      },
      ext: {"sidechain": true, "shouts": "TH_SHOTS", "form": [["Intro (DJ)", "intro", 16, {}], ["Talking bass", "groove", 16, {"energy": 2, "chords": false, "lead": false}], ["Strings enter", "groove", 16, {"energy": 3, "shouts": "sparse"}], ["Build", "build", 8, {"strings": "swell"}], ["Drop", "drop", 32, {"energy": 4, "shouts": "offbeat", "counter": "call"}], ["Toms", "switch", 16, {"chords": false, "shouts": "answer"}], ["Breakdown (strings)", "breakdown", 8, {"strings": "legato"}], ["Build 2", "build", 8, {}], ["Drop 2", "drop", 32, {"energy": 4, "shouts": "offbeat", "counter": "harmony"}], ["Outro (DJ)", "outro", 32, {}]]}
    },
    {
      id: 'TH03', fam: 'th', song: { bd: [['Emaj7', 'E2 B2 D#3 G#3'], ['F#6', 'F#2 C#3 D#3 A#3'], ['D#m7', 'D#3 A#3 C#4 F#4'], ['G#m9', 'G#2 D#3 F#3 A#3 B3']] }, genre: 'ukg', name: "Rave Signal", bpm: 132, root: 8, mode: 'minor', swing: 0.58,
      style: "UK house", vibe: "Skippy, swung drums with a layered clap and breakbeat fills; a crushed Reese that just repeats, M1-style organ stabs, a rave stab riff that turns every note into a minor chord, and a rewind into the last drop.",
      sound: {'kit': 'th', 'bass': 'threese', 'chords': 'organ', 'lead': 'thrave'},
      prog: "G#m7 | G#m7 | Emaj7 | F#6",
      chords: {'G#m7': ['B3', 'D#4', 'F#4', 'G#4'], 'Emaj7': ['B3', 'D#4', 'E4', 'G#4'], 'F#6': ['A#3', 'C#4', 'D#4', 'F#4']},
      drums: {
        main: { K: ['9...9...9...9...', '9...9...9..69...'], CL: ['....9.......9...'], SN: ['.......3.....3..'], CH: ['7.57.57.57.57.5.'], RIM: ['...5..5....5..5.'], OH: ['..6.......6.....'], SH: ['.3.4.3.4.3.4.3.4'] },
        var: { K: ['9.........9.....', '9......6..9.....'], CL: ['....9.......9...'], SN: ['..........3...4.'], CH: ['7.57.57.57.57.5.'], RIM: ['...5..5....5..5.'], SH: ['.3.4.3.4.3.4.3.4'], OH: ['..6.......6.....'] },
        fill: { K: ['9...9...9.......'], CL: ['....9.......9...'], SN: ['........5.6.7.89'], CH: ['7.57.57.57......'] }
      },
      bass: {
        A: ['G#1 - - - . . G#1 - . . . . B1 - - .', 'G#1 - - - . . G#1 - . . . . D#2 - C#2 -', 'E1 - - - . . E1 - . . . . G#1 - - .', 'F#1 - - - . . F#1 - . . . . A#1 - C#2 -'],
        B: ['G#1 - - . . . . . . . G#1 - . F#1~ G#1 .', 'G#1 - - . . . . . . . B1 - . A#1~ G#1 .', 'E1 - - . . . . . . . E1 - . D#1~ E1 .', 'F#1 - - . . . . . . . F#1 - . C#2~ D#2 .']
      },
      chordsPart: {
        A: ['. . @G#m7 . . @G#m7 . . . . @G#m7 . . @G#m7 . .', '. . @G#m7 . . @G#m7 . . . . @G#m7 . . . . .', '. . @Emaj7 . . @Emaj7 . . . . @Emaj7 . . @Emaj7 . .', '. . @F#6 . . @F#6 . . . . @F#6 . . @F#6 . @F#6'],
        B: ['@G#m7 - - - - - - - . . @G#m7 - - - . .', '@G#m7 - - - - - - - . . @G#m7 - - - . .', '@Emaj7 - - - - - - - . . @Emaj7 - - - . .', '@F#6 - - - - - - - . . @F#6 - - - . .']
      },
      lead: {
        A: ['G#3* . . G#3 . . . . B3* . . B3 . . . .', 'G#3* . . G#3 . . . . E4* . . D#4 . . . .', 'E4* . . E4 . . . . G#3* . . G#3 . . . .', 'F#4* . . F#4 . . . . A#3* . C#4 . D#4* . . .'],
        B: ['G#3* . G#3 . . G#3 . . B3* . B3 . . . . .', 'G#3* . G#3 . . G#3 . . D#4* . C#4 . . . . .', 'E4* . E4 . . E4 . . G#3* . G#3 . . . . .', 'F#4* . F#4 . . F#4 . . D#4* . . . C#4 . A#3 .']
      },
      ext: {"sidechain": true, "shouts": "TH_SHOTS_REWIND", "form": [["Intro (breaks)", "intro", 16, {"breaks": "straight"}], ["Groove (organ)", "groove", 16, {"energy": 2, "lead": false}], ["Rave stabs", "groove", 16, {"energy": 3, "breaks": "layer"}], ["Build", "build", 8, {"breaks": "roll"}], ["Drop 1", "drop", 32, {"energy": 3, "counter": "call", "shouts": "sparse"}], ["2-step switch", "twostep", 16, {}], ["Breakdown (organ)", "breakdown", 8, {}], ["Build 2", "build", 8, {}], ["Drop 2 (rewind)", "drop", 32, {"energy": 4, "rewind": true, "counter": "harmony", "breaks": "layer", "shouts": "drop"}], ["Outro (DJ)", "outro", 32, {"breaks": "straight"}]]}
    },
    {
      id: 'TH04', fam: 'th', song: { bd: [['Gmaj7', 'G2 D3 F#3 B3'], ['A6', 'A2 E3 F#3 C#4'], ['F#m7', 'F#2 C#3 E3 A3'], ['Bm9', 'B2 F#3 A3 C#4 D4']] }, genre: 'mth', name: "Fifth Floor", bpm: 130, root: 11, mode: 'minor', swing: 0.56,
      style: "UK tech house", vibe: "A layered clap, offbeat rims and a shaker over a thumping kick, with breakbeat fills; a Moog-style fifth bass, FM pluck chords, a one-note lead that only changes its length, and bit-crushed builds.",
      sound: {'kit': 'th', 'bass': 'thfifth', 'chords': 'fmpluck', 'lead': 'crushlead'},
      prog: "Bm9 | Bm9 | Gmaj7 | A6",
      chords: {'Bm9': ['D4', 'F#4', 'A4', 'C#5'], 'Gmaj7': ['D4', 'F#4', 'G4', 'B4'], 'A6': ['C#4', 'E4', 'F#4', 'A4']},
      drums: {
        main: { K: ['9...9...9...9...'], CL: ['....9.......9...'], RIM: ['..6...6...6...6.'], SH: ['3434343434343434'], CH: ['....5.......5...'], OH: ['..............5.'], WB: ['.......4......4.'] },
        var: { K: ['9...9...9...9...'], CL: ['....9.......9...'], SN: ['....6.......6...'], RIM: ['..6...6...6...6.'], SH: ['3434343434343434'], CH: ['5.5.5.5.5.5.5.5.'], OH: ['..6...6...6...6.'] },
        fill: { K: ['9...9...9.......'], CL: ['....9.......9...'], SN: ['........6.7.8.99'], RIM: ['..6...6...6.....'] }
      },
      bass: {
        A: ['B1 . . B1 . . . B1 . . B1 . . . D2 .', 'B1 . . B1 . . . B1 . . B1 . . . F#1 .', 'G1 . . G1 . . . G1 . . G1 . . . B1 .', 'A1 . . A1 . . . A1 . . C#2 . . . E2 .'],
        B: ['B1 . B1 . . B1 . . B1 . . D2 . . B1 .', 'B1 . B1 . . B1 . . A1 . . F#1 . . A1 .', 'G1 . G1 . . G1 . . G1 . . B1 . . D2 .', 'A1 . A1 . . A1 . . C#2 . . E2 . . F#2 .']
      },
      chordsPart: {
        A: ['. . @Bm9 . . . @Bm9 . . . @Bm9 . . . . .', '. . @Bm9 . . . @Bm9 . . . . . @Bm9 . . .', '. . @Gmaj7 . . . @Gmaj7 . . . @Gmaj7 . . . . .', '. . @A6 . . . @A6 . . . . . @A6 . @A6 .'],
        B: ['@Bm9 . . @Bm9 . . @Bm9 . . . . . . . . .', '@Bm9 . . @Bm9 . . @Bm9 . . . @Bm9 . . . . .', '@Gmaj7 . . @Gmaj7 . . @Gmaj7 . . . . . . . . .', '@A6 . . @A6 . . @A6 . . . @A6 . . . . .']
      },
      lead: {
        A: ['F#5 - - . F#5 . . F#5 - . . . F#5 - - -', '. . . . F#5 - . . . . F#5 . . . . .', 'F#5 - - . F#5 . . F#5 - . . . F#5 - . .', '. . . . F#5 - - - - - . . . . . .'],
        B: ['F#5 . F#5 . . F#5 . . F#5 . . . . . . .', 'F#5 - - - . . . . . . . . F#4 - . .', 'F#5 . F#5 . . F#5 . . F#5 . . . . . . .', 'E5 - - - . . . . . . . . F#5 - - -']
      },
      ext: {"sidechain": true, "form": [["Intro (breaks)", "intro", 16, {"breaks": "straight"}], ["Fifth bass", "groove", 16, {"energy": 2, "chords": false, "lead": false}], ["Plucks", "groove", 16, {"energy": 2, "lead": false}], ["One-note lead", "groove", 16, {"energy": 3, "breaks": "layer"}], ["Build (crushed)", "build", 8, {"crush": true}], ["Drop", "drop", 32, {"energy": 4, "counter": "call"}], ["Break", "breakdown", 16, {}], ["Build 2", "build", 8, {"tapestop": true, "crush": true}], ["Drop 2", "drop", 32, {"energy": 4, "counter": "harmony", "breaks": "layer"}], ["Outro (DJ)", "outro", 32, {"breaks": "straight"}]]}
    }
