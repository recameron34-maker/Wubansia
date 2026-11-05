#pragma once

// buildTimeline: one embedded MIDI file (sched/timeline.h) as the scheduler plays it, made on the
// service thread from the SMF and the kit's two manifests. It reads the file with core's readSmf,
// notesOf, controllersOf, markersOf and textsOf, then, in order:
//
// 1. Swing snap. The files write each odd 16th late by round ((swing - 0.5) x 480) ticks (43, 10, 29
//    and 19 for FUT01-FUT04). A note-on or note-off exactly there moves to the exact swing,
//    (swing_pm - 500) x 480 milliticks (43.2, 9.6, 28.8 and 19.2 ticks); every other tick stays. Note
//    ends snap with their starts, so a note that ends where a swung note starts still ends exactly
//    there: no gap, and no overlap a mono legato instrument would glide on.
// 2. Pre-roll. A note whose sample has preroll_frames starts that much earlier, at the kit's BPM, in
//    milliticks (frames x bpm / 3); its end stays. The pre-roll is the file's played at the note's
//    pitch: a key 12 semitones over its zone's root halves it. An A/B pair shifts by the smaller pre-roll
//    of its two instruments, and the engine skips the rest of the larger one's (startFrame).
// 3. The loop wrap (TimelineOptions::loopTicks > 0). Positions wrap into [0, length): a pre-rolled
//    note at tick 0 moves to the loop's end, and anything at the loop's end (15360) moves to 0. A
//    note may cross the end (pos + len > length); the scheduler ends it in the next cycle. Notes ends
//    at the loop end come before the notes at 0, because the scheduler sends note-offs first.
// 4. Round robins and pan indices: per instrument (the keymap's A side), the note's count among that
//    instrument's notes in its rr block (rr_block_16ths, from tick 0), in (tick, channel, key) order,
//    as phrase.hits_lane counts. Counting by position makes a render from any bar the same.
// 5. CC ramps (CC74, CC71: the cc_filters' rides). A point whose next point on its channel and
//    controller comes within a 16th (at most 240 ticks later) ramps to it; in a loop the last point
//    looks at the first one of the next cycle (rampTo may then pass length). Other controllers but the
//    throws are dropped: the engine has no use for them.
// 6. Throw latches: Note::throwSpace and throwEcho are CC91's and CC94's values on the note's channel
//    at its note-on (a controller on the note's tick counts, as the files write it before the note).
//    A song starts at 0; a loop starts where its end leaves them.
// 7. Duck keys: every note of an instrument with duck_key (either side of an A/B pair) is a key on
//    its lane at its onset, moved pre_us earlier when an onset duck keyed on that lane has pre_us,
//    then wrapped in a loop.
// 8. annotatePhrases (P6, sched/phrase_match.h).
// 9. The chase list: notes longer than a beat, notes pre-roll moved earlier, and phrase notes. After
//    a start or a jump the scheduler starts them mid-sample.
//
// Notes that no keymap entry plays (channel 3, the voicing; channel 9; unmapped keys) are left out:
// nothing would ever play them.

#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "sched/timeline.h"
#include "util/result.h"

#include <cmath>
#include <cstdint>
#include <span>

namespace wubansia::engine
{
    struct TimelineOptions
    {
        // > 0: the file is a loop of this many 960-PPQ ticks (15360: the 4-bar loops); positions
        // wrap and notes cross the end. 0: a song, as long as the file (its tracks' last end).
        std::int64_t loopTicks = 0;
    };

    // The file's ticks must be 960 per quarter, the content's PPQ. The error names what is wrong:
    // "smf: <the reader's error>" or "smf: 480 ticks per quarter; the content has 960".
    [[nodiscard]] util::Result<Timeline> buildTimeline (std::span<const std::uint8_t> smf, const KitManifest& kit,
        const MixConfig& mix, TimelineOptions options);

    // The steps' arithmetic, for the tests and the phrase matcher.

    // Step 1: a tick of the file as a position in milliticks, snapped to the kit's exact swing.
    constexpr std::int64_t swingSnap (std::int64_t tick, std::int32_t swingPm) noexcept
    {
        const std::int64_t exact = std::int64_t { swingPm - 500 } * 480; // milliticks past the odd 16th
        const std::int64_t written = (exact >= 0 ? exact + 500 : exact - 500) / 1000; // round half away from 0, in ticks
        const std::int64_t inEighth = ((tick % 480) + 480) % 480;
        if (inEighth == 240 + written)
            return (tick - written) * kMilliticksPerTick + exact;
        return tick * kMilliticksPerTick;
    }

    // Step 2: frames of 48 kHz audio as milliticks at bpm, rounded half up: frames x bpm / 3.
    constexpr std::int64_t framesToMilliticks (std::int64_t frames, std::int32_t bpm) noexcept
    {
        const std::int64_t x = frames * bpm;
        return x >= 0 ? (2 * x + 3) / 6 : -((-2 * x + 3) / 6);
    }

    // The same for a fractional number of frames, rounded half up.
    inline std::int64_t timeToMilliticks (double frames, std::int32_t bpm) noexcept
    {
        return static_cast<std::int64_t> (std::floor (frames * static_cast<double> (bpm) / 3.0 + 0.5));
    }

    // 2 ^ (semitones / 12), from the octave's 12 ratios and exact powers of 2: no libm, so every platform
    // gets the same bits.
    constexpr double semitoneRatio (int semitones) noexcept
    {
        constexpr double octave[12] { 1.0, 1.0594630943592953, 1.122462048309373, 1.189207115002721, 1.2599210498948732,
            1.3348398541700344, 1.4142135623730951, 1.4983070768766815, 1.5874010519681994, 1.681792830507429,
            1.7817974362806785, 1.8877486253633868 };
        const int octaves = semitones >= 0 ? semitones / 12 : -((11 - semitones) / 12);
        double ratio = octave[semitones - 12 * octaves];
        for (int i = 0; i < octaves; ++i)
            ratio *= 2.0;
        for (int i = 0; i > octaves; --i)
            ratio *= 0.5;
        return ratio;
    }

    // A file's pre-roll as time: played `semitones` above its root, prerollFrames of it last
    // prerollFrames / 2 ^ (semitones / 12) frames at 48 kHz.
    constexpr double prerollTime (std::int32_t prerollFrames, int semitones) noexcept
    {
        return prerollFrames > 0 ? static_cast<double> (prerollFrames) / semitoneRatio (semitones) : 0.0;
    }

    // Step 7: microseconds as milliticks at bpm, rounded half up: us x bpm x 2 / 125.
    constexpr std::int64_t microsToMilliticks (std::int64_t us, std::int32_t bpm) noexcept
    {
        const std::int64_t x = us * bpm * 2;
        return x >= 0 ? (2 * x + 125) / 250 : -((-2 * x + 125) / 250);
    }
}
