#pragma once

// Harmony (plan §2.3, §4.1): a mode on the kit's tonic (Aeolian or Dorian, Phrygian colour in the
// break), one progression of 2-3 chords per slot over 2 or 4 bars, a harmonic rhythm in units that
// sum to 4 ([2,2], [3,1], [1,1,2]: a unit is loopBars / 4 bars), and 40-60% of changes pushed early
// by a 16th or an 8th. Progressions are drawn from a grammar (start and move weights per degree)
// with an anti-cliche memory: shapes from the legacy pack, and no slot repeating another's shape.
// Voicings come from the section's texture palette, chosen by voice-leading cost, a top-line arc
// and the register; recurring chords are sometimes re-voiced.

#include "gen/content.h"
#include "gen/song.h"
#include "gen/theory.h"
#include "rng/rng.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wubansia::gen
{
    struct Progression
    {
        std::vector<std::int32_t> degrees;
        std::vector<std::int32_t> units;
        std::int32_t loopBars = 2;
        Mode mode = Mode::aeolian;
    };

    // True if b is a rotation of a (same length, same cyclic order).
    bool sameShape (std::span<const std::int32_t> a, std::span<const std::int32_t> b) noexcept;

    // True if run (2 or more chords) appears in shape read cyclically, as consecutive chords.
    bool containsRun (std::span<const std::int32_t> shape, std::span<const std::int32_t> run) noexcept;

    // The anti-cliche penalty of a progression, permille (0 none, 1000 never): penaltyWhole if it
    // is a memory shape (up to rotation), else penaltyThree if 3 of its chords in a row (cyclically)
    // are a run of a memory shape, else penaltyTwo for 2, else 0.
    std::int32_t clichePenalty (const HarmonySpec& spec, std::span<const std::int32_t> degrees) noexcept;

    struct Candidate
    {
        std::vector<std::int32_t> degrees;
        std::int64_t weight = 0;
    };

    // Every progression of `count` chords the slot may take in `mode`, in enumeration order (the
    // degrees as base-7 digits, first chord most significant), with its weight: start weight of the
    // first chord, times every move (the last back to the first too), times each chord's degree
    // weight in the mode, less the cliche penalty. Neighbouring chords differ. A candidate whose
    // shape equals one in `taken` (another slot's) gets weight 0, and so does one breaking the
    // slot's tonic rule (present: holds degree 0; emergent: holds it but doesn't start on it;
    // absent: doesn't hold it) or missing the mode's colour degrees. Zero-weight candidates stay in
    // the list. count 1 is a pedal (a harmonic rhythm of [4]): the 7 single chords, weighted by
    // start and degree weights alone, with no moves. count is 1 to 3 (the loader's limit, which
    // keeps the weights inside 64 bits); any other count gives an empty list.
    std::vector<Candidate> progressionCandidates (const HarmonySpec& spec, const ProgressionSlot& slot, Mode mode, std::size_t count, std::span<const std::vector<std::int32_t>> taken);

    struct HarmonyPlan
    {
        Mode mode = Mode::aeolian; // the song's
        std::vector<std::pair<std::string, Progression>> slots; // in the style's slot order
        const Progression* slot (std::string_view name) const noexcept;
    };

    // The song's mode and one progression per slot. Keys: root.with (tag ("harmony")).
    HarmonyPlan planHarmony (const Style& style, rng::Key root);

    // Fills song.harmony (one per section) and song.chords from the plan: each section loops its
    // slot's progression from its start bar. Pushes are laid out per 8-bar phrase, so every phrase
    // of a section pushes the same changes (variation stays on phrase lines): of a phrase's n
    // changes, k = n * the section's share (clamped to the style's band where n allows it) are
    // pushed early by the section's push ticks, spread evenly from a rotation drawn per section.
    // A section's first chord is never pushed: no chord crosses a section's start. A section cut
    // short keeps the pushes of its first phrases.
    void layOutHarmony (Song& song, const Style& style, const HarmonyPlan& plan, rng::Key root);

    // The chord sounding at a tick (pushed boundaries), or nullptr.
    const ChordSpan* chordAt (const Song& song, std::int64_t tick) noexcept;

    // A voicing: the keys from low to high, and each key's tags (noteTag).
    struct Voiced
    {
        std::vector<std::int32_t> keys;
        std::vector<std::uint8_t> tags;
        friend bool operator== (const Voiced&, const Voiced&) = default;
    };

    // Every placement of a voicing type on a chord within the register [range.lo, range.hi]: each
    // tone in some octave, all keys distinct, the span in spec.span, neighbours below mudBelow at
    // least mudGap apart, the first tone lowest unless spec.inversions; with `choose`, every subset of
    // that many tones. A scale-step tone a minor 9th or 2nd over the root takes the step two above
    // (the 11th or 4th): no b9 on the chord's own root. In a fixed order (by the tones' octaves,
    // lowest first).
    std::vector<Voiced> voicingCandidates (const VoicingSpec& spec, std::int32_t tonicPc, Mode mode, std::int32_t degree, Range range, std::int32_t mudBelow, std::int32_t mudGap);

    // The cost of a voicing after prev (nullptr for the first): voice-leading (each voice to the
    // previous voicing's voice at the same relative height) times 4, less 3 per common tone, plus
    // twice the top voice's distance from targetTop, plus the centroid's distance from center.
    std::int64_t voicingCost (const Voiced& v, const Voiced* prev, std::int32_t targetTop, std::int32_t center) noexcept;

    // The candidate to play: the cheapest by voicingCost among those whose top key lies in spec.top,
    // the voicing's top window, if any does (else the cheapest of all), so the voice leading can't
    // walk a stack out of its register; the first of equals. skip (another index) leaves one out:
    // the second best, for a re-voicing, which stays inside the window too, so where no other
    // candidate does, skip itself comes back (the chord isn't re-voiced). cands must not be empty.
    std::size_t bestVoicing (const std::vector<Voiced>& cands, const Voiced* prev, std::int32_t targetTop, const VoicingSpec& spec, std::size_t skip = std::size_t (-1));
}
