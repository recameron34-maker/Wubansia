#pragma once

// The building blocks of the metrics (tools/legacy_audit/README.md, "Building blocks"): rounding,
// medians, standard deviations, stacks and harmony slots, swing delays and residuals, bar
// fingerprints, lag-1 autocorrelation and DFA. Exposed so the tests can check each rule alone.
//
// Every decision is integer arithmetic. Doubles appear in exactly two places, both reported
// statistics with the rounding the README states: sdMilli () (the square root) and
// dfaAlphaMilli () (the DFA exponent). Both follow the audit's operation order, and core is built
// with floating-point contraction off (wubansia_build_defaults), so sdMilli () is exact and
// dfaAlphaMilli () can differ from the audit only through log (), by at most 1 (README,
// "Tolerances for the C++ port").

#include "metrics/model.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace wubansia::metrics
{
    // ---- rounding and statistics ----

    // a / b in thousandths, rounded half up: floorDiv (2000a + b, 2b). -1 ("no value") when b is 0.
    // For a >= 0 and b >= 0.
    std::int64_t permille (std::int64_t a, std::int64_t b) noexcept;

    // The element at index floor ((n - 1) / 2) once sorted; nothing for no values.
    std::optional<std::int64_t> lowerMedian (std::vector<std::int64_t> values);

    // Population standard deviation in thousandths: floor (1000 * sqrt (n*S2 - S1^2) / n + 0.5),
    // in doubles in exactly that order (as the audit computes it). Nothing for no values.
    std::optional<std::int64_t> sdMilli (std::span<const std::int64_t> values);

    // The number of distinct values.
    std::int64_t countDistinct (std::vector<std::int64_t> values);

    // Lag-1 autocorrelation in thousandths, exact and rounded half up toward +infinity:
    // [n^2 P - n S1 (A + B) + (n - 1) S1^2] / [n (n S2 - S1^2)] with P = sum x[i] x[i-1],
    // A = sum x[1..n-1], B = sum x[0..n-2]. Nothing for fewer than 2 values, a constant series,
    // or values the 128-bit arithmetic can't hold (more than 2^31 values or any |x| > 1024; the
    // residuals it is meant for are within 30 ticks).
    std::optional<std::int64_t> lag1Permille (std::span<const std::int64_t> x);

    // DFA-1 alpha in thousandths (README, "G7"): box sizes 4, 8, 16... up to n / 4, a least-squares
    // slope of log F(s) against log s over at least 3 sizes, floor (1000 alpha + 0.5). Nothing for
    // fewer than 64 values, a series with no fluctuation, or fewer than 3 box sizes.
    std::optional<std::int64_t> dfaAlphaMilli (std::span<const std::int64_t> x);

    // ---- pitch classes ----

    int popcount12 (int mask) noexcept; // bits 0-11
    int pitchClassMask (std::span<const Note> notes) noexcept; // bit (key mod 12) per note
    int rotateMask (int mask, int r) noexcept; // pitch class r becomes 0
    std::string maskHex (int mask); // three lower-case hex digits: 0x895 -> "895"

    // ---- notes in time ----

    // The notes of a list sorted by tick whose onset is in [t0, t1).
    std::span<const Note> between (std::span<const Note> notes, std::int64_t t0, std::int64_t t1) noexcept;

    // ---- stacks and harmony ----

    // A run of notes (of a list sorted by onset) whose onsets are each at most 10 ticks after the
    // run's first onset. The notes are a view into the list given to stacksOf ().
    struct Stack
    {
        std::int64_t tick = 0; // the first onset
        std::span<const Note> notes;
        std::int64_t spread() const noexcept { return notes.back().tick - tick; }
    };
    std::vector<Stack> stacksOf (std::span<const Note> notes);

    // A stack of all chords-role notes merged, with at least 2 distinct pitch classes.
    struct HarmonyStack
    {
        std::int64_t tick = 0;
        int mask = 0; // pitch classes
        int low = 0; // the lowest key
    };
    std::vector<HarmonyStack> harmonyStacks (const Song& song);

    // The harmony of nSlots half-bar slots from tick w0 (README, "Harmony slots"): per slot the
    // stack nearest its start among those with onset in [start - 250, start + 710), a tie to the
    // later; an empty slot carries the previous slot's, leading empty slots take the first filled
    // one. Nothing if no slot has a stack. The pointers point into stacks.
    std::optional<std::vector<const HarmonyStack*>> harmonySlots (std::span<const HarmonyStack> stacks, std::int64_t w0, int nSlots);

    // The latest harmony stack with onset <= t; before the first one, the last one (a loop).
    // nullptr if there are none.
    const HarmonyStack* harmonyAt (std::span<const HarmonyStack> stacks, std::int64_t t) noexcept;

    // ---- swing ----

    // (tick mod 240) - 120 for every onset with tick mod 240 in [60, 180), in list order.
    std::vector<std::int64_t> oddOffsets (std::span<const Note> notes);

    // The lower median of the odd-16th offsets, if there are at least minCount of them.
    std::optional<std::int64_t> swingDelay (std::span<const Note> notes, std::size_t minCount);

    // Delays sorted; each cluster takes every delay within 2 ticks of its smallest. The count.
    std::int64_t distinctSwings (std::vector<std::int64_t> delays);

    // An onset's residual from the swung grid with delay d: the smallest in absolute value of
    // t - g, t - (g + 120 + d) and t - (g + 240), g = 240 floor (t / 240), a tie to the earlier;
    // nothing if that is over 30 ticks (another subdivision).
    std::optional<std::int64_t> residual (std::int64_t tick, std::int64_t delay) noexcept;

    // The residuals of notes (sorted by tick), per section against that section's own delay (the
    // lower median of its odd-16th offsets, 0 if it has none), in onset order across sections.
    std::vector<std::int64_t> residuals (const Song& song, std::span<const Note> notes);

    // On the swung 16th grid with delay d, within tol ticks.
    bool onSwungGrid (std::int64_t tick, std::int64_t delay, std::int64_t tol) noexcept;
    // On a straight 16th, within tol ticks.
    bool onStraightGrid (std::int64_t tick, std::int64_t tol) noexcept;

    // The swing delay of a whole file: the lower median of every note's odd-16th offset, 0 if none.
    std::int64_t fileSwing (const Song& song);

    // ---- bar fingerprints ----

    // A bar's step fingerprint: the sorted distinct step * 128 + key of the notes attributed to it
    // (step attribution, no velocity).
    using StepPrint = std::vector<std::int32_t>;

    // The step fingerprints of bars [bar0, bar0 + nBars).
    std::vector<StepPrint> stepBars (std::span<const Note> notes, std::int64_t bar0, std::int64_t nBars);

    // Section-relative bars i >= 8 that differ from bar i - 8 while bar i - 1 did not (bar 7 never
    // differs): one macro change per run of differing bars, at its first bar.
    std::vector<std::int64_t> macroChanges (std::span<const StepPrint> prints);

    // ---- syncopation ----

    // Longuet-Higgins and Lee syncopation of one bar's onset steps (sorted, distinct, 0-15), with
    // the bar cyclic (README, "R11").
    std::int64_t lhl (std::span<const std::int32_t> onsets);
}
