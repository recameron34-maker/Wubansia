# FUT04 "Furnace Code": round 5, the vox synth and the accents

Written for the kit's render folder (`build/fut04-r5`, from `render.py --kit FUT04 --set r5`), which holds the FLAC masters, `qa/`, `sheets/`, `qa_report.md`, `qa_report.json` and `listen.json` this page names; its copy in `content/samples/fut04/` sits beside the WAVs alone.

A dark industrial techno kit at 136 BPM in E minor (9A): a heavy split kick on E1, a gated iron snare and a 16th acid bass over a kick-fed rumble, with modular blips on top, every sound on one struck-iron tone (an anvil's modal ratios) and one dark concrete room. After round 3 you wrote: "sounds are very good, vocals suck, either ditch or rethink, need those cool accent synths". So round 5 does three things:
- it keeps every round 3 sound byte for byte;
- it drops the text-to-speech vocals and puts a wordless vox synth in their place;
- it adds the synth accents and phrases that FUT01 and FUT03 got in round 4, and features the accents in the loops.

**Nobody has listened to round 5.** Every number below is a measurement made in this session. None of it says how it sounds.

## Start here: the A/B

Play `Loops/Context Loop Club Vox.wav`, then `Loops/Context Loop Club No Vox.wav`. Both are round 3's loop, mix and club master, with the old vocals taken out:
- **Vox synth.** The vox riff answers the acid in bars 1-3 and throws a D4 into the fill bar.
- **No vox.** The same loop, instrumental. A zap and a metal chord hit take the vox's two throws.

Both loops have the same accents otherwise: a dark rave stab on the "and" of 4 in bar 1, and a reverse hit swelling into the downbeat. The vox loop adds a 1-bar noise sweep under the fill. The question is which one you'd keep, and whether the accents lift the loop without crowding the drums.

| Club master | Vox synth | No vox | Techno target |
|---|---|---|---|
| Loudness, LUFS-I (round the loop) | -8.61 | -8.10 | -8 ± 1 |
| True peak | -1.02 dBTP | -1.02 dBTP | ≤ -1.0 |
| PLR | 7.59 dB | 7.08 dB | ≥ 6.5 |
| Peak reduction (clipper + limiter) | 6.0 dB | 6.0 dB | the kit's declared cap: 6 (round 3's) |
| Kick crest in the loop, the lowest of 15 kicks | 6.95 dB | 6.30 dB | ≥ 4 |
| Crest the master takes (median) | kicks 2.01, claps 2.15 (the claps in 1000-5000 Hz: 0.38) | kicks 2.22, claps 2.21 (the claps in 1000-5000 Hz: 0.28) | ≤ 2.4, declared (round 3's) |
| Octaves re 1 kHz | sub +5.4, 125 -2.0, 250 -3.9, 500 +0.1, 2k -0.6, 4k -2.8, 8k -6.1, 16k -15.7 | sub +5.5, 125 -2.0, 250 -4.9, 500 +0.2, 2k -1.1, 4k -3.2, 8k -6.3, 16k -16.0 | 125 and 250 exempt, as round 3 |
| Side over mid, 125 / 250 Hz (the worse) | -12.21 dB | -12.89 dB | ≤ -10 |

Round 3's club loop read -8.04 LUFS-I with a PLR of 7.02 dB; its lowest kick crest was 6.03 dB (median 6.57), so both round 5 loops keep more crest than round 3's. Both round 5 loops still need the whole 6 dB cap. The no-vox loop lands closer to -8; the vox loop reads about 0.5 LU quieter at the same cap. As under round 3's vocals, the vox lane keys round 3's dips: the acid's top band and the fill's top band dip by up to 8.4 dB (median 7.4) while it sounds. Both loops' 125 and 250 Hz octaves read lower than round 3's (-1.1 and -1.6 dB re 1 kHz). They are still exempt, as round 3 declared.

## What changed

### 9. The vox synth (new): `Vox/`

This is a vocal synth built from synthesis alone: no speech engine, no models, no words (`tools/sound/recipes/fut04/voxsynth.py`). Each note is made like this:
- **Source.** A band-limited, glottal-like pulse with a strong fundamental, plus a detuned double on each side (±7 cents, 9 and 13 ms late).
- **Breath and onset.** Breath noise and an "h" go into the same tract. The tract starts closed (a "w"-"m") and opens into the vowel over 35 ms, with a soft 6 ms tick on the front.
- **Formants.** Five narrow band-passes per vowel, lifted at 2.3-4.2 kHz for the metallic ring, on a dark male "oh" or "oo". The tract closes towards "oo" as the note dies.
- **Pitch.** A 35-cent scoop into the note, a 1.5-cent jitter, and on held notes a vibrato from 0.55 s.
- **Drive and filtering.** A drive at 4x, then a high-pass at 210 Hz and a 4 dB dip at 300 Hz.

The files:
- `Vox Synth <note> v1 / v2.wav`: the playable multisample on E3 G3 B3 D4 E4 G4 B4. Velocity opens the vowel: v1 sings "oo", v2 sings "oh". It is held for 1.1 s and dry, so the sampler's sends place it.
- `Vox Stab Oh E3.wav` and `Vox Stab Oo B3.wav`: the single stabs, gated at 160 ms, driven harder, in the kit's dark hall (3.6 s, damped from 3 kHz).
- Under the "oh" layer, 150-400 Hz reads 3-30 dB under the whole file. An "oo" on E3 to E4 keeps its own fundamental there: that is what an "oo" is.

### 10. The vox synth riff (new): `Loops/Vox Synth Riff.wav`

Two bars on the multisample's files, played the way the sampler plays them:
- **Bar 1.** Stabs answer the acid line's leaps on the off-beat 8ths, on the 16th after each: E3 "oh", E3 "oo", G3 "oh".
- **Bar 2.** E3, E3 and B3, then D4 on the "and" of 4, thrown into the hall and a dotted-8th echo so it spills into the next bar.

In the context loop, the riff plays bars 1-2, plays its first bar again in bar 3, and throws its D4 on bar 3's "and" of 4 into the fill bar. It rests during the fill. The lane reads -21 LUFS, 4 LU under the kick lane. The MIDI is `FUT04_Phrase_VoxSynth_2bars.mid`, where velocity picks the vowel.

### 13. Synth accents (new)

Nine one-shots in E minor, each with its root in its name:

| File | Root | What |
|---|---|---|
| `Music/Accent Rave Stab Em.wav` | E4 | a dark rave stab: Em on detuned saws dropping a semitone into pitch, a filter blip, each note driven |
| `Music/Accent Metal Chord Em.wav` | E4 | each note of Em struck as the kit's iron (its seven modes) over a saw chord, a clank |
| `Music/Accent Acid Stab E4.wav` | E4 | the kit's own acid voice (the ladder at 4x) snapping open and shut, its octave on top |
| `Music/Accent Dist Chord Em.wav` | E4 | Em(add9), each note hard-driven and folded on its own (it grinds but stays in tune), gated |
| `Music/Accent Vox Chord Em.wav` | E3 | the vox synth on Em, "oh" closing to "oo", in the dark hall |
| `Drums/Accent Zap E5.wav` | E5 | a feedback zap: a sine falling from E8 into a comb tuned to E5 |
| `Bass/Accent Sub Drop E2.wav` | E2 | a clean sine dropping two octaves: the only accent in the low end |
| `FX/Accent Noise Sweep E.wav` | E (comb on E3) | 1 bar of noise through a tuned comb and a rising band-pass, crushed, into the downbeat |
| `FX/Accent Reverse Hit Em.wav` | E4 | the metal chord through the dark hall, reversed: 3 beats into the beat it ends on |

**In the loops**, each accent is set to its own loudness, like a record's ear candy (`context5.ACCENT_LUFS`). Over 500 ms at its hit, each reads 6.6-11.4 LU under the drum bus, with the accent lane's peak at least 4 dB under the kick's:
- rave stab 6.6 LU under;
- zap 7.1 LU under;
- metal chord 7.5 LU under;
- sweep and reverse together 8.3 LU under (the reverse alone, in the no-vox loop, 11.4).

The stabs sit over the low mids: 150-400 Hz holds 5-11 dB less than the rest of each. The acid stab is thinned under its root by a gentle 380 Hz high-pass.

### 14. Synth phrases (new): `Loops/`

Three riffs at 136 BPM on the kit's swing, each a loop and a MIDI phrase on channel 4 (`content/midi/fut04/FUT04_Phrase_*`):
- `Phrase Acid Stabs.wav` (2 bars): the acid stab, E4 G4 E4 D4 F#4, answered up to A4 and B4.
- `Phrase Metal Chords.wav` (4 bars): the metal chord on the off-beats, Em Em Am Bm (the Em sample a fourth and a fifth up), with a dub echo and the dark hall.
- `Phrase Rave Riff.wav` (2 bars): the rave stab on a 3-3-2 over Em, turning to Am and Bm at the end of bar 2.

The vox synth riff is the fourth phrase.

## What did not change

113 files of 27 round 3 instruments are byte-identical to `build/fut04-r3`:
- drums: the kick, the snare, the hats, the ride and the hat loop;
- the anvil;
- the acid bass, its lines and the 4-bar ride;
- the rumble, the drone and the blips;
- the stutter fill and the iron riser;
- the four previews and the two low-end loops.

The low-end loops (`Context Loop Low`, `Context Loop Low Warm`) are round 3's because round 5 leaves the kick, bass and rumble lanes alone. The sub drop is the only low accent, and no loop plays it. Sounds 1-8, 11 and 12 on the listening page are those files.

**Gone from the set:**
- the three sung lines;
- the seven chops;
- the robot phrase, dry and wet;
- round 3's four context loops that played them.

They still render with `--set r3` and need the vocal models. `--set r5` needs none.

## Declared exceptions (each with its reason in the recipe)

- **The club master.** Its cap, its two exempt octaves and its crest limit are round 3's (`context.MASTER`, `context5.CLUB_QA`). The cap is 6 dB against the standard's 4. 125 and 250 Hz are exempt. The crest loss is declared at 2.4 dB. They are still for your ear to settle.
- **The alias check is off for the vox synth and its stabs.** Its breath lies between the harmonics, and the vibrato and jitter move them. The same goes for the accents with inharmonic or driven parts by design: the struck iron, the folded chord, the acid's sweeping resonance and the rave stab's per-note drive.
- **Short tuning windows.** Every tonal item is tuned on its root to ±3 cents. The stabs are read over their gate (50-170 to 50-300 ms), not the default 0.1-0.6 s.

## What's here

| Folder or file | What |
|---|---|
| `Drums/`, `Bass/`, `Music/`, `Vox/`, `FX/`, `Loops/` | WAV copies to listen to: 147 files, 48 kHz / 24-bit, the same samples as the masters. `v1` is the softest velocity layer, `rr` the round robin |
| `FUT04_*.flac` | The 147 masters |
| `qa/` | The four-on-the-floor loop the kick's mud check ran on |
| `sheets/` | One spectrogram sheet per instrument |
| `qa_report.md`, `qa_report.json` | Every QA check with its number, every exception with its reason, the render environment and the club masters' reports |
| `listen.json` | The listening page's entry for this kit: 14 sounds, its first clips the two club masters |

QA: 0 failed checks over 147 files. Reproducibility: 49 of 49 instruments rendered byte-identical a second time, each in a fresh process.

**Not checked, by anyone:** how any of it sounds. That covers whether the vox synth reads as a cool dark-techno vox stab or a fake singer, whether the accents are too loud or too quiet, and how it plays in a DAW.
