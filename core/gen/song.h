#pragma once

// A generated song: its arrangement (sections on 8-bar lines), its harmony, and per lane the score
// (quantized, as composed) and the performance (the score after the feel pass). Score and
// performance are kept apart (plan §4.1), so the quantized and the with-feel files come from one
// score, and tests can check both.
//
// Pipeline (generator.cpp), feel always last:
//   1. arrangement: the seed's form, drawn from the style's grammar (an extended mix), its 8-bar
//      blocks of active lanes and one-bar dropouts, the salient-change schedule and the turnaround
//      bars (arrange.h)
//   2. harmony: mode, one progression per slot, chord spans with pushed changes (harmony.h)
//   3. rhythm: each drum lane's patterns from its grids, in dependency order (lanes.h, rhythm.h)
//   4. pitches: the chords voiced; the hook (a style with a hook block's: hook.h) planned over the
//      form and realized clear of the voicings; the bass on chord degrees, following its kick and
//      agreeing with the voicings; the lead lane carrying the hook (lanes.h, harmony.h, lead.h)
//   5. feel: swing, offset and velocity maps, 1/f fluctuation, anchors, the low-end lock (feel.h)
//   6. with Options::bars, the finished song cut there (cut.h): a --bars cut is exactly the whole
//      song's head, since every stage runs on the whole form
// Every random choice is a pure function of a key path from the seed (rng.h): no draw depends on
// another lane's draws, so changing one lane's parameters changes only that lane and the lanes
// that depend on it (the bass on its kick, and on the chords' voicings for its third, seventh and
// fifth; the hook's lead on the chords, which its voicing guard reads).

#include "gen/content.h"
#include "gen/theory.h"
#include "rng/rng.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::gen
{
    inline constexpr std::int64_t ticksPerStep = 120; // a 16th at 480 PPQ
    inline constexpr std::int64_t stepsPerBar = 16;
    inline constexpr std::int64_t ticksPerBar = ticksPerStep * stepsPerBar; // 1920
    inline constexpr std::int32_t barsPerBlock = 8; // sections, lane entries and exits: on 8-bar lines

    namespace noteFlag
    {
        inline constexpr std::uint16_t anchor = 1 << 0; // a 100% cell of an anchor lane: kept within +-2 ticks
        inline constexpr std::uint16_t ornament = 1 << 1; // drawn per bar
        inline constexpr std::uint16_t turn = 1 << 2; // a turnaround hit
        inline constexpr std::uint16_t follow = 1 << 3; // bass on a kick of its follow lane
        inline constexpr std::uint16_t approach = 1 << 4; // bass approach note into a chord change
        inline constexpr std::uint16_t slide = 1 << 5; // slides (legato) into the next note: overlaps it
        inline constexpr std::uint16_t pushed = 1 << 6; // chord or bass on a pushed chord change
        inline constexpr std::uint16_t top = 1 << 7; // a chord's top voice
        inline constexpr std::uint16_t poly = 1 << 8; // a polymeter cell
        inline constexpr std::uint16_t euclid = 1 << 9; // a Euclidean cell
        inline constexpr std::uint16_t hook = 1 << 10; // a note of the hook's material (hook.h)
        inline constexpr std::uint16_t pedal = 1 << 11; // the hook's pedal under its top voice (FUT02's ChromeArp)
    }

    namespace noteTag
    {
        inline constexpr std::uint8_t borrowed = 1 << 0; // outside the kit's natural minor, from the mode (Dorian 6, Phrygian b2)
        inline constexpr std::uint8_t planed = 1 << 1; // outside it from a planed (parallel) voicing
    }

    struct Note
    {
        std::int64_t tick = 0; // onset
        std::int64_t length = 0; // ticks, at least 1
        std::int64_t step = 0; // the score's 16th (absolute): the score onset is step * 120
        std::uint8_t key = 0;
        std::uint8_t velocity = 0; // 1-127
        std::uint8_t voice = 0; // drums: the lane's voice index; chords: the stack position (0 lowest)
        std::uint8_t tags = 0; // noteTag
        std::uint16_t flags = 0; // noteFlag
        std::int16_t section = 0;

        friend bool operator== (const Note&, const Note&) = default;
    };

    // A one-bar dropout inside a section (arrange.h): lanes of the bar's block that sit that bar out.
    struct Dropout
    {
        std::int32_t bar = 0; // within the section
        std::uint64_t lanes = 0; // bit i: style lane i

        friend bool operator== (const Dropout&, const Dropout&) = default;
    };

    struct Section
    {
        std::size_t index = 0; // 0-based, in song order
        std::string kind;
        std::int32_t startBar = 0; // 0-based; a multiple of 8
        std::int32_t bars = 0; // the form's (a --bars cut shortens the last section it keeps: cut.h)
        std::vector<std::uint64_t> blocks; // per 8-bar block (of `bars`): bit i = style lane i plays
        std::vector<Dropout> dropouts; // in bar order, at most one per bar
        std::int32_t variant = -1; // the pattern variant every lane starts the section on; -1: its kind's (the style's "sections")

        std::int64_t startTick() const noexcept { return startBar * ticksPerBar; }
        std::int64_t endTick() const noexcept { return (startBar + bars) * ticksPerBar; }
        bool active (std::size_t lane, std::int32_t barInSection) const noexcept;
        std::uint64_t lanesAt (std::int32_t barInSection) const noexcept; // bit i: lane i plays that bar (its block's lanes, less a dropout's)
        std::uint64_t lanesAnywhere() const noexcept; // bit i: lane i plays in some block
    };

    // A salient change (plan §2.3, "8-bar line"): from this bar on, the lane plays its next variant.
    struct Change
    {
        std::size_t section = 0;
        std::int32_t bar = 0; // within the section
        std::size_t lane = 0;
    };

    struct ChordSpan
    {
        std::int64_t start = 0; // ticks (pushed changes start early)
        std::int64_t end = 0;
        std::int32_t degree = 0; // the root's scale degree in `mode`, 0-6
        Mode mode = Mode::aeolian;
        std::int32_t rootPc = 0;
        std::int32_t pushedBy = 0; // ticks, 0 if not pushed
        std::int16_t section = 0;
        std::int32_t index = 0; // its position in the progression
    };

    // The harmony a section plays: one progression looping from the section's start.
    struct SectionHarmony
    {
        std::string slot;
        Mode mode = Mode::aeolian;
        std::vector<std::int32_t> degrees; // the progression's chord roots
        std::vector<std::int32_t> units; // its harmonic rhythm: units summing to 4
        std::int32_t loopBars = 2; // a unit is loopBars / 4 bars
        std::int32_t pushPermille = 500; // the share of changes pushed early
        std::int32_t pushTicks = 120;
        std::string voicing; // the texture the section starts with
    };

    // A controller value of a lane's CC lane (cc.h). Automation, not performance: both versions,
    // with feel and quantized, write the same points.
    struct ControlPoint
    {
        std::int64_t tick = 0;
        std::uint8_t number = 0; // the controller, e.g. 74
        std::uint8_t value = 0; // 0-127
        std::int16_t section = 0;

        friend bool operator== (const ControlPoint&, const ControlPoint&) = default;
    };

    struct LaneOut
    {
        std::string id;
        std::string track;
        std::uint8_t channel = 0;
        LaneKind kind = LaneKind::drums;
        std::vector<Note> score; // sorted by (tick, key, voice, length, velocity)
        std::vector<Note> perf; // the same notes after the feel pass, index for index: perf[i] plays score[i]
        std::int32_t swing = 500; // the swing the feel drew for the lane's default row (permille)
        std::vector<ControlPoint> cc; // sorted by (tick, number); one value per controller and tick
    };

    // ---- the hook (hook.h): one per seed, drawn by planHook from the style's hook block (content.h)

    // A note of the hook's material.
    struct HookNote
    {
        std::int32_t step = 0; // 16ths from the phrase's start
        std::int32_t degree = 0; // scale steps from the tonic at SongHook::base, in SongHook::mode (7: an octave up)
        std::uint8_t len16 = 1; // its length in 16ths
        std::uint8_t velocity = 0; // the fixed accent shape's
        bool accent = false;
        bool peak = false; // the contour's peak (a twin top: both of its notes)

        friend bool operator== (const HookNote&, const HookNote&) = default;
    };

    struct HookPhrase
    {
        std::int32_t bars = 0; // 2: a riff; 4: a period
        std::vector<HookNote> notes; // in step order

        friend bool operator== (const HookPhrase&, const HookPhrase&) = default;
    };

    // What a line plays of the hook (the plan's operators, once applied): out rests; full plays the
    // phrase; fragment, the half-bar call, each bar's first half plus its last onset when its second
    // half holds two or more; skeleton, the accented onsets (FUT02: the top voice alone); answer, the
    // call and then the response in each bar's second half; augment, the hook at half speed.
    enum class HookForm : std::uint8_t {
        out,
        full,
        fragment,
        skeleton,
        answer,
        augment
    };

    // How a line's last bar ends: as the texture plays it; vary, its last note on varyDegree;
    // cadence, its last note falling to the 5th, held to the bar line; breath, the bar resting.
    enum class HookEnding : std::uint8_t {
        none,
        vary,
        cadence,
        breath
    };

    // One 8-bar line of a section the carrier's plans play it in.
    struct HookLine
    {
        std::size_t section = 0;
        std::int32_t bar = 0; // the line's first bar in the section, a multiple of 8
        HookForm form = HookForm::out;
        std::uint8_t motif = 0; // 0: A, the hook; 1: B, drop 2's new phrase (NEW plays it full)
        std::int32_t shift = 0; // SEQUENCE: scale steps added to every degree
        HookEnding ending = HookEnding::none;
        std::uint16_t mask = 0; // bit 2 * bar + half (bit 0: bar 0's first half): a half-bar the lane's final notes keep busy

        friend bool operator== (const HookLine&, const HookLine&) = default;
    };

    struct SongHook
    {
        std::size_t lane = 0; // the carrier's index in Song::lanes
        Mode mode = Mode::aeolian; // the material's mode
        std::int32_t base = 0; // the tonic's key the degrees hang from (the register's octave)
        HookContour contour = HookContour::late;
        HookPhrase a; // the hook, a 2-bar riff
        HookPhrase b; // drop 2's new phrase, in the style's idiom
        HookPhrase response; // ANSWER's response: A's second-half onsets, walked afresh
        std::int32_t varyDegree = 0; // VARY's end degree
        std::vector<HookLine> lines; // every 8-bar line of the sections the carrier's plans play it in, in song order
        std::vector<Note> notes; // the realized lines in time order, before scoreLead keeps those inside the lane's active ranges (lead.h)

        friend bool operator== (const SongHook&, const SongHook&) = default;
    };

    struct Song
    {
        std::string kitId;
        std::string kitName;
        std::string keyName;
        std::string camelot;
        std::int32_t bpm = 0;
        std::int32_t tonic = 0;
        bool minor = true;
        std::uint64_t seed = 0;
        std::int32_t feelAmount = 1000;
        std::int32_t totalBars = 0;
        Mode mode = Mode::aeolian; // the song's mode
        std::vector<Section> sections;
        std::vector<Change> changes;
        std::vector<std::int32_t> turnBars; // absolute bars that carry a turnaround
        std::vector<SectionHarmony> harmony; // one per section
        std::vector<ChordSpan> chords; // every chord of the song, in order
        std::vector<LaneOut> lanes; // the style's lanes, in track order
        std::optional<SongHook> hook; // a style with a hook block's (hook.h); none otherwise

        std::int64_t endTick() const noexcept { return totalBars * ticksPerBar; }
    };

    // The root of a song's key tree: the seed, then the kit's id (rng::seedKey (seed).with (tag (kitId))),
    // so one seed gives unrelated songs in different kits. Every draw of the generator and the feel
    // hangs below it.
    inline rng::Key songRoot (std::string_view kitId, std::uint64_t seed) noexcept { return rng::seedKey (seed).with (rng::tag (kitId)); }

    struct Options
    {
        std::uint64_t seed = 0;
        std::int32_t feelAmount = -1; // permille; -1: the feel file's amount
        std::int32_t bars = 0; // > 0: the whole song cut at this bar, a multiple of 8 (cut.h); 0: the whole form
    };

    // Generates a song: the whole form, then with options.bars the cut. Fails only on options it
    // can't honour (bars not a multiple of 8 up to the form's length, a feel amount out of 0-2000);
    // a bundle that passed makeBundle always generates.
    [[nodiscard]] util::Result<Song> generate (const Bundle& bundle, const Options& options);
}
