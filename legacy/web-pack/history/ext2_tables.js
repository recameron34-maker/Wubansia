  // Layer tracks used only by kits with an ext spec. ch: null = a free channel is assigned when the file is written.
  const EXT_LAYERS = [
    { key: 'breaks', name: 'Break Slices (C1 = slice 1, one 16th each)', ch: null, program: 0, after: 'fills' },
    { key: 'poly', name: 'Poly Percussion (3/5/7-step cycles)', ch: 9, program: 0, drum: true, after: 'breaks' },
    { key: 'micro', name: 'Micro Clicks & Glitch', ch: 9, program: 0, drum: true, after: 'poly' },
    { key: 'acid', name: 'Acid 303', ch: null, program: 87, after: 'sub' },
    { key: 'donk', name: 'Donk', ch: null, program: 12, after: 'acid' },
    { key: 'piano', name: 'Piano Fills & Runs', ch: null, program: 0, after: 'stabs' },
    { key: 'organ', name: 'Organ', ch: null, program: 16, after: 'piano' },
    { key: 'strings', name: 'Strings', ch: null, program: 48, after: 'organ' },
    { key: 'brass', name: 'Brass Stabs (falls by pitch bend)', ch: null, program: 61, after: 'strings' },
    { key: 'orch', name: 'Orchestra Hits', ch: null, program: 55, after: 'brass' },
    { key: 'choir', name: 'Choir', ch: null, program: 52, after: 'orch' },
    { key: 'talkbox', name: 'Talkbox Lead', ch: null, program: 85, after: 'counter' },
    { key: 'hoover', name: 'Hoover (swoops by pitch bend)', ch: null, program: 62, after: 'talkbox' },
    { key: 'siren', name: 'Dub Siren (pitch-bend LFO)', ch: null, program: 80, after: 'hoover' },
    { key: 'voxguide', name: 'Vocal Guide (melody, lyrics, audio cues)', ch: 11, program: 54, after: 'vox' },
    { key: 'shouts', name: 'Shouts', ch: null, program: 54, after: 'voxguide' },
    { key: 'sidechain', name: 'Sidechain Trigger (ghost kick, note 35)', ch: 9, program: 0, drum: true, after: 'fx' }
  ];
  EXT_LAYERS.forEach(l => { const i = EXT_TRACKS.findIndex(t => t.key === l.after); EXT_TRACKS.splice(i + 1, 0, Object.assign({ layer: true }, l)); });
  FX_NOTE.rewind = 43; FX_NOTE.tapestop = 44;

  // The pack's sung and chanted hooks as 4-bar loops (16th steps, note, word; '~' carries the word on), the octave they were
  // actually sung in, and the kit they were recorded on (for its tempo and swing). Used for the vocal guide tracks.
  const VOCALS = {
    h02: { kit: 'UKG02', root: 2, oct: -12, title: "Hold me closer, say you'll stay", mel: [[0, 4, 'A4', 'Hold'], [4, 2, 'C5', 'me'], [6, 3, 'D5', 'closer'], [9, 3, 'C5', '~'], [18, 2, 'D5', 'say'], [20, 2, 'F5', "you'll"], [22, 8, 'D5', 'stay'], [32, 4, 'Bb4', 'Hold'], [36, 2, 'D5', 'me'], [38, 3, 'F5', 'closer'], [41, 3, 'D5', '~'], [50, 2, 'C#5', 'say'], [52, 2, 'E5', "you'll"], [54, 8, 'A4', 'stay']] },
    h03: { kit: 'UKG05', root: 3, oct: 0, title: 'Sunday morning, feeling your love', mel: [[0, 3, 'Bb4', 'Sunday'], [3, 3, 'G4', '~'], [6, 4, 'Bb4', 'morning'], [10, 4, 'C5', '~'], [16, 3, 'Eb5', 'feeling'], [19, 3, 'D5', '~'], [22, 2, 'C5', 'your'], [24, 8, 'Bb4', 'love'], [32, 3, 'C5', 'Sunday'], [35, 3, 'Ab4', '~'], [38, 4, 'C5', 'morning'], [42, 4, 'Eb5', '~'], [48, 3, 'F5', 'feeling'], [51, 3, 'Eb5', '~'], [54, 2, 'D5', 'your'], [56, 8, 'Bb4', 'love']] },
    h06: { kit: 'MTH01', root: 9, oct: 0, title: 'Deeper, deeper, into the night', mel: [[2, 2, 'A4', 'Deeper'], [4, 4, 'E4', '~'], [10, 2, 'A4', 'deeper'], [12, 4, 'E4', '~'], [18, 2, 'G4', 'into'], [20, 2, 'A4', '~'], [22, 2, 'C5', 'the'], [24, 8, 'A4', 'night'], [34, 2, 'A4', 'Deeper'], [36, 4, 'E4', '~'], [42, 2, 'A4', 'deeper'], [44, 4, 'E4', '~'], [50, 2, 'G4', 'into'], [52, 2, 'E4', '~'], [54, 2, 'D4', 'the'], [56, 8, 'E4', 'night']] },
    h11: { kit: 'SG02', root: 6, oct: 0, title: 'Feel the pressure when the bass drop', mel: [[0, 2, 'C#3', 'Feel'], [2, 2, 'C#3', 'the'], [4, 2, 'E3', 'pressure'], [6, 4, 'C#3', '~'], [16, 2, 'A2', 'when'], [18, 2, 'B2', 'the'], [20, 4, 'C#3', 'bass'], [24, 6, 'F#2', 'drop'], [32, 2, 'D3', 'Feel'], [34, 2, 'D3', 'the'], [36, 2, 'F#3', 'pressure'], [38, 4, 'D3', '~'], [48, 2, 'B2', 'when'], [50, 2, 'C#3', 'the'], [52, 4, 'F#3', 'bass'], [56, 6, 'C#3', 'drop']] },
    h12: { kit: 'SG06', root: 0, oct: -12, title: 'All the girls say ooh', mel: [[0, 2, 'G4', 'All'], [2, 2, 'Bb4', 'the'], [4, 4, 'C5', 'girls'], [8, 4, 'Eb5', 'say'], [12, 12, 'Eb5', 'ooh'], [32, 2, 'Ab4', 'All'], [34, 2, 'C5', 'the'], [36, 4, 'Eb5', 'girls'], [40, 4, 'G5', 'say'], [44, 12, 'F5', 'ooh']] },
    h14: { kit: 'CH01', root: 9, oct: -12, title: 'Lift me higher, higher', mel: [[0, 4, 'E5', 'Lift'], [4, 2, 'C5', 'me'], [6, 4, 'E5', 'higher'], [10, 6, 'A4', '~'], [16, 6, 'F5', 'higher'], [22, 10, 'D5', '~'], [32, 4, 'D5', 'Lift'], [36, 2, 'B4', 'me'], [38, 4, 'D5', 'higher'], [42, 6, 'G4', '~'], [48, 6, 'E5', 'higher'], [54, 10, 'C5', '~']] },
    h16: { kit: 'CH05', root: 10, oct: -12, title: 'You got me dancing all night long', mel: [[0, 2, 'F4', 'You'], [2, 2, 'Ab4', 'got'], [4, 2, 'Bb4', 'me'], [6, 4, 'Db5', 'dancing'], [10, 6, 'C5', '~'], [18, 2, 'Bb4', 'all'], [20, 4, 'Db5', 'night'], [24, 8, 'F5', 'long'], [32, 2, 'Gb4', 'You'], [34, 2, 'Bb4', 'got'], [36, 2, 'Db5', 'me'], [38, 4, 'Eb5', 'dancing'], [42, 6, 'Db5', '~'], [50, 2, 'C5', 'all'], [52, 4, 'Eb5', 'night'], [56, 8, 'F5', 'long']] },
    bh2: { kit: 'MTH04', root: 0, oct: 0, talk: true, title: 'Rich bitch energy', mel: [[0, 'C4', 'Rich bitch.'], [8, 'Eb4', 'Energy.'], [16, 'C4', 'Rich bitch.'], [24, 'Bb3', 'Energy.'], [32, 'C4', 'Rich bitch.'], [40, 'Eb4', 'Energy.'], [48, 'C4', 'Rich bitch.'], [56, 'G3', 'Energy.']] },
    bh3: { kit: 'UKG05', root: 3, oct: -12, title: 'Expensive taste, pretty face', mel: [[0, 2, 'Bb4', 'Expensive'], [2, 2, 'C5', '~'], [4, 2, 'Bb4', '~'], [6, 6, 'G4', 'taste'], [16, 2, 'Eb5', 'pretty'], [18, 2, 'D5', '~'], [20, 8, 'Bb4', 'face'], [32, 2, 'Bb4', 'Expensive'], [34, 2, 'C5', '~'], [36, 2, 'Eb5', '~'], [38, 6, 'F5', 'taste'], [48, 2, 'G5', 'pretty'], [50, 2, 'F5', '~'], [52, 10, 'Eb5', 'face']] },
    bh4: { kit: 'SG02', root: 6, oct: 0, talk: true, title: 'Big bass, big bag', mel: [[0, 'A3', 'Big bass,'], [8, 'C#4', 'big bag.'], [16, 'A3', 'Big bass,'], [24, 'E4', 'big bag.'], [32, 'A3', 'Big bass,'], [40, 'C#4', 'big bag.'], [48, 'A3', 'Big bass,'], [56, 'E4', 'big bag.']] },
    bh5: { kit: 'CH05', root: 10, oct: -12, title: 'Buy me diamonds, baby', mel: [[0, 3, 'F5', 'Buy'], [3, 3, 'Db5', 'me'], [6, 2, 'Eb5', 'diamonds'], [8, 6, 'Db5', '~'], [16, 2, 'Bb4', 'baby'], [18, 10, 'F4', '~'], [32, 3, 'F5', 'Buy'], [35, 3, 'Db5', 'me'], [38, 2, 'Eb5', 'diamonds'], [40, 6, 'F5', '~'], [48, 2, 'Ab5', 'baby'], [50, 10, 'F5', '~']] }
  };
  const ADLIBS = {
    a01: 'Wheel it up!', a02: 'Come again!', a03: 'Oi oi!', a05: 'Mmm.', a06: 'Yes.', a08: 'Hey.', a09: 'Bo!', a10: 'Wicked!', a11: 'Big tune!', a12: 'Pull up!',
    ba03: 'Whatever.', ba05: 'Kiss kiss.', ba09: 'So hot.', ba10: 'Pay me.', ba12: 'Bitch, please.'
  };
  // break re-sequencing: which slice plays on each 16th (J5 rolls the snare slice)
  const SLICE = {
    J1: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
    J2: [0, 1, 2, 3, 4, 5, 4, 5, 8, 9, 10, 11, 12, 12, 14, 15],
    J3: [0, 1, 0, 1, 4, 5, 6, 7, 8, 9, 8, 9, 12, 13, 14, 15],
    J4: [0, 1, 2, 3, 12, 13, 6, 7, 8, 1, 2, 3, 12, 13, 14, 15],
    J5: [0, 1, 2, 3, 4, 5, 6, 7, 4, 4, 4, 4, 4, 4, 4, 4]
  };
  const SECTION_KIND = { intro: 'intro', groove: 'groove', build: 'build', drop: 'drop', switch: 'switch', breakdown: 'breakdown', dubBreak: 'switch', bassSolo: 'build', outro: 'outro', halftime: 'switch', micro: 'switch', twostep: 'switch' };
