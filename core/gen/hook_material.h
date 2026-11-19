#pragma once

// The hook's material (ADR 0026), shared by the hook (hook.cpp) and the phrase library (phrase.cpp, ADR 0031):
// A's rhythm, the fit table, the degree walk, the velocities, B (the period), the response, the redraws, and a
// line realized from it (the texture's notes in a mode, the avoid-note fit, the voicing guard, FUT02's pedal).
// Moved here from hook.cpp's anonymous namespace without a change: every draw and every byte of the songs is as
// it was (core/tests/snapshots/gen.sha256 and the hook tests).

#include "gen/content.h"
#include "gen/hook.h"
#include "gen/song.h"
#include "gen/theory.h"
#include "rng/rng.h"
#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace wubansia::gen::material
{
    std::int32_t semitones (Mode mode, std::int32_t degree);

    // A bar's prints sorted, each once.
    inline void tidy (BarPrint& p)
    {
        std::sort (p.begin(), p.end());
        p.erase (std::unique (p.begin(), p.end()), p.end());
    }

    using Notes = std::vector<HookNote>;

    struct Chord
    {
        std::int32_t rootPc = 0;
        Mode mode = Mode::aeolian;
        std::int32_t degree = 0;
    };

    // Per 16th of a 2-bar phrase, the chords it can sound over in the first drop's first line (each
    // repetition of the phrase there, push-robust). A degree's fit is the share, in percent, of those
    // chords that hold it as a chord tone of "pitch.tones" (1 3 5 7 9); 100 with no chords.
    struct Fit
    {
        std::array<std::vector<Chord>, 32> at;
        std::int32_t tonic = 0;
        Mode mode = Mode::aeolian;
        std::vector<std::int32_t> tones;

        std::int32_t operator() (std::int32_t step, std::int32_t degree) const
        {
            const auto& chords = at[static_cast<std::size_t> (util::floorMod (step, 32))];
            if (chords.empty())
                return 100;
            const int pc = pitchClass (tonic + semitones (mode, degree));
            std::int32_t hit = 0;
            for (const Chord& c : chords)
                for (const std::int32_t t : tones)
                    if (pitchClass (c.rootPc + chordInterval (c.mode, c.degree, t)) == pc)
                    {
                        ++hit;
                        break;
                    }
            return hit * 100 / static_cast<std::int32_t> (chords.size());
        }
    };

    // What the song gives the material.
    struct Setting
    {
        const HookSpec* spec = nullptr;
        std::size_t lane = 0;
        Mode mode = Mode::aeolian;
        std::int32_t tonic = 0;
        Fit fit;
        Range a; // A's and the response's register (keys): the register's floor, or the kit range's low end, to its high end
        Range b; // the new phrase's: the period's own register, else A's
        bool pedal = false;
        std::int32_t pedalTonic = 0; // the pedal's tonic: the tonic's key at A's register floor or above (FUT02: D5)
    };

    struct WalkOpts
    {
        std::int32_t base = 0; // the tonic's key the degrees hang from
        Range reg; // the register (keys)
        bool wrap = false; // the last note meets the first: a riff repeating
        bool twin = false; // the top struck on two onsets in a row
        bool fixedStart = false; // the first note keeps its degree (the new phrase starts on the tonic or the 5th)
        const std::array<std::int32_t, 8>* moves = nullptr; // the move weights, else the style's
        std::vector<std::pair<std::size_t, std::size_t>> joins; // other notes that meet when the phrase plays (the period's bar joins)
        std::vector<std::int32_t> endAlts; // the other end degrees, in order, for when no octave of the drawn one keeps the cap
    };

    // What a line plays of the material: HookLine's form, motif, shift and ending.
    struct LineState
    {
        HookForm form = HookForm::out;
        std::uint8_t motif = 0;
        std::int32_t shift = 0;
        HookEnding ending = HookEnding::none;
    };

    // A note of a realized line.
    struct LineNote
    {
        std::int32_t step = 0; // its 16th in the line, 0-127
        std::int32_t key = 0;
        std::int32_t degree = 0; // the top voice's, from SongHook::base; the pedal's, from its tonic
        std::int64_t length = 0; // ticks
        std::uint8_t velocity = 0;
        bool accent = false;
        bool pedal = false;
    };

    struct Realized
    {
        std::vector<LineNote> notes; // in step order
        LinePrints prints; // as G4 reads the lane: per bar, 16th * 128 + key, ascending, each once
        bool ok = true; // every bar holds a note (bar 7 aside for BREATH)
    };

    // What a line reads of its section (SectionCtx's part, hook.cpp): its own mode, and per 16th of its 8 bars
    // the pitch classes a semitone over a clash tone, and the keys the chords sound there.
    struct LineContext
    {
        Mode mode = Mode::aeolian; // the section's own: a Dorian 6 or a break's Phrygian b2 colours the hook
        std::int32_t reach = 1000; // the lane grid's length scale, 1000 at least: how far a note may sound
        std::array<std::uint16_t, 128> avoid {}; // per 16th: bit pc, a semitone over a clash tone (1 3 5 7)
        std::array<std::array<std::uint64_t, 2>, 128> chord {}; // per 16th: bit key, a chord-lane note sounds there
    };

    inline std::int32_t degreeClass (std::int32_t degree) { return util::floorMod (degree, 7); }

    inline bool inPool (const HookPitch& p, std::int32_t degree) { return std::find (p.pool.begin(), p.pool.end(), degreeClass (degree)) != p.pool.end(); }

    inline bool playing (const LineState& st) { return st.form != HookForm::out; }

    // A 16th of the pedal (FUT02's ChromeArp): a pluck shorter than the 16th, as the written pedal.
    inline constexpr std::int64_t pedalLength = ticksPerStep - 24;


    // A degree 0-6 by weight; the tonic when none can be drawn.
    std::int32_t pickDegree (rng::Key k, const std::array<std::int32_t, 7>& w);

    // The pool's next degree from d in direction dir (+1 or -1), within an octave; d + dir if none.
    std::int32_t nextPool (const HookPitch& p, std::int32_t d, std::int32_t dir);

    void bySteps (Notes& notes);

    // Lengths never run into the next onset nor past the bar line.
    void fitLengths (Notes& bar);

    bool occupied (const Notes& bar, std::int32_t step);

    // Bar 1 from a cell: per seed at most one non-accent onset out (never the first, its half's only
    // onset or the bar's only non-accent one, and never from a cell of three onsets) and at most one
    // extra onset in, on a 16th of "extra" (never 16th 15 in a pedal style).
    Notes barFromCell (const HookCell& c, const HookMutate& m, bool pedal, rng::Key k);

    // Bar 2 answers bar 1: its 16ths 0-7 exactly, and at most one edit among 8-15 by the style's
    // weights: the same, an onset moved by a 16th, one added (not an accent), one dropped (never an
    // accent, the half's only onset or the bar's only non-accent one).
    Notes answerBar (const Notes& bar1, const HookAnswer& a, bool pedal, rng::Key k);

    // A 2-bar riff's rhythm: bar 1 a cell by weight, mutated; bar 2 answering it.
    Notes riffRhythm (const std::vector<HookCell>& cells, const HookSpec& spec, bool pedal, rng::Key k);

    // Drop 2's period on the hook's own rhythm (the organ and the glass): bar a is A's first half plus
    // a second half of the period's own, its last onset an accent; bar b answers a (the glass: a
    // again); a' has a's rhythm; c keeps a's first half, then the organ's falls on 16ths 8, 10 and 12
    // (held to the bar line) and the glass's keeps a's rhythm.
    Notes periodRhythm (const Notes& aBar1, const HookSpec& spec, bool pedal, rng::Key k);

    // The degree walk: up to 32 attempts, each a peak drawn among the onsets in the contour's window,
    // then a move per onset by weight: the style's weights by size in scale steps, towards the peak
    // before it and the end after it (a leap of a 4th or more turns back), a strong onset on a chord
    // tone of every chord it sounds over (the fit table), only pool degrees inside the register and
    // within the leap cap; the peak raised over every other note (alone, or struck twice, the notes
    // after a twin top walked again from there); the end degree in the octave nearest the note before
    // it (and the first, where they meet), else the next end by weight. The first attempt that breaks
    // no rule, else the one that breaks the fewest. Sets the notes' degrees and peak flags.
    void walk (const Setting& s, Notes& ns, rng::Key k, std::int32_t startDeg, std::int32_t endDeg, Range window, const WalkOpts& wo);

    // The other end degrees by weight, heaviest first (the lower degree first among equals).
    std::vector<std::int32_t> endAlternatives (const std::array<std::int32_t, 7>& weights, std::int32_t drawn);

    // The fixed accent shape: one velocity per onset, drawn once, so bars i and i + 2 of a line agree:
    // the downbeat's accent, beat 3's, the other accents', the rest's; the peak raised; one offset per
    // onset in +-shape.
    void shapeVelocity (const HookVelocity& v, Notes& ns, rng::Key k);

    bool inside (const SongHook& h, Range reg, std::int32_t degree);

    // Bar 2's first half differs from bar 1's in a pitch (where "headDiffers"): the last note of bar
    // 2's first half that isn't the peak takes a pool step up, else down, where the register, the
    // cap with the notes around it and the contour (no new top) allow; else the note before it.
    void headDiffers (const Setting& s, SongHook& h);

    // A: the hook, a 2-bar riff. Sets h.a, h.base, h.contour and h.varyDegree.
    void drawRiff (const Setting& s, SongHook& h, rng::Key kh);

    // B's first note: the tonic, or the 5th when A starts on the tonic, in the octave inside B's
    // register that doesn't take A's key at that 16th (the nearest of the walk's order), so no bar of
    // B equals a bar that drop 1 plays.
    std::int32_t newPhraseStart (const Setting& s, const SongHook& h, std::int32_t firstStep, std::int32_t shift);

    // B, drop 2's new phrase, in the style's idiom, inside its own register, every join under the cap.
    void drawNewPhrase (const Setting& s, SongHook& h, rng::Key kh);

    // The response (ANSWER's): A's second-half onsets, walked afresh inside the register from below
    // the call and ending on varyDegree, 6 velocity steps softer; never A's own second half in a bar.
    void drawResponse (const Setting& s, SongHook& h, rng::Key kh);

    SongHook drawOnce (const Setting& s, rng::Key kh);

    // The rules a material keeps, counted where it breaks them: every move under the cap (A's and B's,
    // their wraps and B's joins included); A's contour (the late and early peak the highest note
    // alone in its window, the twin top struck on two onsets in a row); every note inside its
    // register; bar 2's first half differing from bar 1's in a pitch (with "headDiffers"); bar 2
    // never bar 1 moved; B's first note the tonic, or the 5th when A starts on it, never A's key at
    // that 16th; the organ's a' one note away from a; the response never A's second half in a bar.
    std::int32_t brokenRules (const Setting& s, const SongHook& h);

    // A material drawn below km, drawn again below km.with (tag ("redraw")).with (r), r = 1-8, while
    // it still breaks a rule after its walks; the first that breaks none, else the one breaking fewest.
    std::pair<SongHook, std::int32_t> materialBelow (const Setting& s, rng::Key km);

    // Bar `bar` (0-7) of a line of the form on the motif, before the fit and the guard: its notes,
    // each with its 16th in the bar (AUGMENT doubles 16ths and lengths, its lengths to the bar line at
    // most). FULL plays the phrase; FRAGMENT each bar's first half, and its last onset when its second
    // half holds two or more; SKELETON the accents; ANSWER the first half, then the response in the
    // second; AUGMENT the riff at half speed.
    Notes textureNotes (const SongHook& h, HookForm form, std::uint8_t motif, std::int32_t bar);

    LineState stateOf (HookForm form, std::uint8_t motif);

    // The written-reference mud rule against the chord-lane keys of a 16th: no minor 9th either way, and a
    // semitone only with the lower note at the kit's floor or above.
    bool rubs (const std::array<std::uint64_t, 2>& keys, std::int32_t key, std::int32_t floor);

    // The voicing guard's check over every 16th a note can sound in (its length under the lane grid's
    // length scale).
    bool clean (const LineContext& c, std::int32_t floor, std::int32_t key, std::int32_t step, std::int64_t length);

    // True if the pedal plays under the line (its "in": FULL, ANSWER, FRAGMENT, NEW).
    bool pedalUnder (const HookPedal& pedal, const LineState& st);

    // A line of the state in the section, as its lane plays it (plan, "Realization, per line"): the
    // texture's notes keyed in the section's own mode; a note the mode takes out of the register moves
    // to the nearest pool degree inside it within the cap, else rests (a SEQUENCE line fits whole or
    // not at all); VARY and CADENCE end the line (the end degree in the octave inside the register
    // nearest the note before, within the cap, else the next end by weight; a cadence held to the bar
    // line); the avoid-note fit; the voicing guard (a rubbing note moves to the nearest clean pool
    // degree within two steps that keeps the cap with the note before, as moved, and the one after, as
    // written, else rests, and the notes either side of a rest are checked like a move); FUT02's pedal
    // under the top voice, through the same guard. Lengths from the cell, never into the next note (10
    // ticks short) nor past the line.
    Realized realizeLine (const Setting& s, const LineContext& c, const SongHook& h, const LineState& st);
}
