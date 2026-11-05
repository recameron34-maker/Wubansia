// annotatePhrases (P6, engine/sched/phrase_match.h): the timeline's notes on phrase lanes tied to the slices of their
// stems, against the synthetic kit's lead phrase and against tools/sound/phrase_tables.py's resolve () on the four
// kits' Loop files (vectors from tools/sound/export_phrase_vectors.py).

#include "phrase_fixtures.h"

#include "sched/phrase_match.h"
#include "testing/smf_builder.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

using namespace phrase_test;
namespace testing = wubansia::engine::testing;

namespace
{
    // SYN01's lead phrase as its files write it (engine/testing/synthetic_kit.cpp): 9 notes from step 2, the one on
    // step 13 a retrigger slice 3 holds, so 8 slices; the last sounds 6 16ths, through the cycle's end into the looped
    // stem's start (its slice wraps), and its MIDI note stops at the cycle's end; a CC74 ride every 60 ticks, CC71 none.
    struct LeadNote
    {
        int step;
        int key;
        int steps;
    };
    constexpr std::array<LeadNote, 9> kLead { { { 2, 67, 2 }, { 4, 70, 3 }, { 8, 72, 3 }, { 12, 74, 1 }, { 13, 74, 2 },
        { 16, 72, 3 }, { 20, 70, 3 }, { 24, 67, 3 }, { 27, 65, 6 } } };
    constexpr int kLeadChannel = 1; // MIDI channel 2
    constexpr int kSwingPm = testing::kSyntheticSwingPm;
    constexpr std::int64_t kBpm = testing::kSyntheticBpm;
    constexpr std::int64_t kCycleMt = 32 * kMilliticksPerSixteenth;

    int leadRide (int i) { return i < 64 ? 32 + i : 96 - (i - 64); }

    std::int64_t tick (int step) { return step * 240 + (step % 2 != 0 ? testing::kSyntheticSwingTicks : 0); }

    // A lead track: in each cycle of `cycles`, the ride (rideOf (cycle, i), -1 for none) and the notes keep (cycle,
    // index into kLead) keeps, at keyOf (cycle, key) and stepOf (cycle, step).
    struct LeadPlan
    {
        std::vector<int> cycles;
        std::function<bool (int, int)> keep = [] (int, int) { return true; };
        std::function<int (int, int)> keyOf = [] (int, int key) { return key; };
        std::function<int (int, int)> stepOf = [] (int, int step) { return step; };
        std::function<int (int, int)> rideOf = [] (int, int i) { return leadRide (i); };
        std::vector<std::pair<int, int>> extra; // more notes: (16th, key)
    };

    void writeLead (testing::SmfBuilder::Track& lead, const LeadPlan& plan)
    {
        for (const int c : plan.cycles)
        {
            for (int i = 0; i < 128; ++i)
                if (const int v = plan.rideOf (c, i); v >= 0)
                    lead.cc (tick (c * 32) + i * kRideTicks, 2, 74, v);
            for (std::size_t i = 0; i < kLead.size(); ++i)
                if (plan.keep (c, static_cast<int> (i)))
                {
                    const int step = plan.stepOf (c, kLead[i].step);
                    const auto length = std::min<std::int64_t> (kLead[i].steps * 240, tick (32) - tick (step));
                    lead.note (tick (c * 32 + step), length, 2, plan.keyOf (c, kLead[i].key), 100);
                }
        }
        for (const auto& [sixteenth, key] : plan.extra)
            lead.note (tick (sixteenth), 120, 2, key, 90);
    }

    std::vector<std::uint8_t> leadSmf (const LeadPlan& plan, std::int64_t lengthTicks = 15360)
    {
        testing::SmfBuilder b (960);
        b.tempo (0, wubansia::midi::microsPerQuarter (static_cast<int> (kBpm))).timeSignature (0, 4, 2);
        writeLead (b.track ("Lead"), plan);
        return b.bytes (lengthTicks);
    }

    Timeline loopOf (const std::vector<std::uint8_t>& smf) { return timelineOf (smf, kSwingPm, 15360); }

    // The slice notes of channel, by position.
    std::vector<const Note*> sliceNotes (const Timeline& t, int channel = kLeadChannel)
    {
        std::vector<const Note*> out;
        for (const auto& n : t.notes)
            if (n.channel == channel && n.slice >= 0)
                out.push_back (&n);
        return out;
    }

    // The note at (pos, channel, key) that isn't a slice note, or nullptr.
    const Note* noteAt (const Timeline& t, std::int64_t pos, int channel, int key)
    {
        for (const auto& n : t.notes)
            if (n.slice < 0 && n.pos == pos && n.channel == channel && n.key == key)
                return &n;
        return nullptr;
    }

    // Where the slice note of slice j (of phrase p) of cycle c goes: the cycle's start plus the slice's start frame at
    // the kit's BPM (round a 4-bar loop), and its preroll, to the slice's 16th on the swung grid.
    std::int64_t slicePos (const PhraseEntry& p, int c, int j, bool loop = true)
    {
        const auto& s = p.slices.at (static_cast<std::size_t> (j));
        const auto pos = c * p.cycle16ths * kMilliticksPerSixteenth + framesToMt (s.start, kBpm);
        return loop ? (pos % kLoopLength + kLoopLength) % kLoopLength : pos;
    }

    std::int64_t slicePreroll (const PhraseEntry& p, int c, int j)
    {
        const auto& s = p.slices.at (static_cast<std::size_t> (j));
        return gridMt (std::int64_t { c } * p.cycle16ths + s.step, kSwingPm) - slicePos (p, c, j, false);
    }

    bool continues (const Note& n) { return (n.flags & NoteFlag::continues) != 0; }
    bool held (const Note& n) { return (n.flags & NoteFlag::held) != 0; }

    // The cycle a note belongs to: its onset (pos + preroll), by the cycle's length.
    std::int64_t cycleOf (const Note& n) { return (n.pos + n.preroll) / kCycleMt; }
}

TEST_CASE ("phrase match: a position's 16th on the swung grid", "[phrase][match]")
{
    CHECK (sixteenthStart (0, kSwingPm) == 0);
    CHECK (sixteenthStart (1, kSwingPm) == 240000 + 28800);
    CHECK (sixteenthStart (2, kSwingPm) == 480000);
    CHECK (sixteenthOf (0, kSwingPm) == 0);
    CHECK (sixteenthOf (240000, kSwingPm) == 0); // the written 16th, before the swing lands it
    CHECK (sixteenthOf (268799, kSwingPm) == 0);
    CHECK (sixteenthOf (268800, kSwingPm) == 1);
    CHECK (sixteenthOf (479999, kSwingPm) == 1);
    CHECK (sixteenthOf (480000, kSwingPm) == 2);
    CHECK (sixteenthOf (63 * 240000 + 28800, kSwingPm) == 63);
    CHECK (sixteenthOf (64 * 240000, 500) == 64);
    CHECK (sixteenthOf (-1, kSwingPm) == -1);
}

TEST_CASE ("phrase match: SYN01's Loop plays every lead note's slice and holds the retrigger", "[phrase][match]")
{
    const auto m = testing::syntheticKitManifest();
    const auto& phrase = m.phrases.at (0);
    REQUIRE (phrase.ch == kLeadChannel + 1);
    REQUIRE (phrase.slices.size() == 8u);
    REQUIRE (phrase.ride74.size() == 128u);
    for (int i = 0; i < 128; ++i)
        REQUIRE (phrase.ride74[static_cast<std::size_t> (i)] == leadRide (i));
    REQUIRE (phrase.slices.back().end - m.files.at (static_cast<std::size_t> (phrase.stemA)).frames == phrase.slices.front().start); // it wraps

    auto t = loopOf (testing::syntheticSmf (PlayMode::loop, Variant::vox));
    const auto before = t.notes;
    annotatePhrases (t, m);

    CHECK (sorted (t));
    REQUIRE (t.notes.size() == before.size() + 16); // a slice note for each of 2 x 8 slices

    // Every note that was there still is, as it was, but for the lead's phrase index (its multisample twin) and the
    // held flag on the retrigger.
    for (const auto& b : before)
    {
        const Note* n = noteAt (t, b.pos, b.channel, b.key);
        REQUIRE (n != nullptr);
        auto expected = b;
        expected.phrase = static_cast<std::int16_t> (b.channel == kLeadChannel ? 0 : -1);
        if (b.channel == kLeadChannel && sixteenthOf (b.pos, kSwingPm) % 32 == 13)
            expected.flags = static_cast<std::uint8_t> (expected.flags | NoteFlag::held);
        CHECK (*n == expected);
    }

    // The slice notes: each slice of each cycle from its start frame, as long as the slice, all reading on round the
    // loop (the last wraps into slice 0).
    const auto slices = sliceNotes (t);
    REQUIRE (slices.size() == 16u);
    for (int c = 0; c < 2; ++c)
        for (int j = 0; j < 8; ++j)
        {
            CAPTURE (c, j);
            const auto& s = phrase.slices[static_cast<std::size_t> (j)];
            const auto it = std::find_if (slices.begin(), slices.end(), [&] (const Note* n) {
                return n->slice == j && n->pos == slicePos (phrase, c, j);
            });
            REQUIRE (it != slices.end());
            const Note& n = **it;
            CHECK (n.phrase == 0);
            CHECK (n.key == s.key);
            CHECK (n.velocity == 100);
            CHECK (n.preroll == slicePreroll (phrase, c, j));
            CHECK (n.preroll == framesToMt (s.lead, kBpm)); // the lead at 128 BPM, exactly: 2048 for 48 frames
            CHECK (cycleOf (n) == c);
            CHECK (n.len == framesToMt (s.end - s.start, kBpm));
            CHECK (continues (n));
            CHECK ((n.flags & ~NoteFlag::continues) == 0);
        }
}

TEST_CASE ("phrase match: a call cycle plays its slices, and its last doesn't read on", "[phrase][match]")
{
    const auto m = testing::syntheticKitManifest();
    LeadPlan plan;
    plan.cycles = { 0, 1 };
    plan.keep = [] (int c, int i) { return c == 0 || kLead[static_cast<std::size_t> (i)].step < 16; }; // cycle 1: bar 1
    auto t = loopOf (leadSmf (plan));
    annotatePhrases (t, m);

    const auto slices = sliceNotes (t);
    REQUIRE (slices.size() == 8u + 4u);
    for (const Note* n : slices)
    {
        CAPTURE (n->pos, n->slice);
        if (cycleOf (*n) == 1 && n->slice == 3)
            CHECK_FALSE (continues (*n)); // slice 4 doesn't play after it
        else
            CHECK (continues (*n)); // cycle 0's last reads on into cycle 1's slice 0, across the stem's wrap
    }
    // The retrigger on step 13 is held in both cycles: phrase 0, no slice of its own.
    for (const int c : { 0, 1 })
    {
        const Note* h = noteAt (t, snap (tick (c * 32 + 13), kSwingPm), kLeadChannel, 74);
        REQUIRE (h != nullptr);
        CHECK (h->phrase == 0);
        CHECK (held (*h));
    }
}

TEST_CASE ("phrase match: a transposed or rewritten cycle plays the multisample", "[phrase][match]")
{
    const auto m = testing::syntheticKitManifest();

    SECTION ("transposed")
    {
        LeadPlan plan;
        plan.cycles = { 0, 1 };
        plan.keyOf = [] (int c, int key) { return c == 1 ? key + 2 : key; };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        const auto slices = sliceNotes (t);
        REQUIRE (slices.size() == 8u); // cycle 0's
        for (const Note* n : slices)
            CHECK (cycleOf (*n) == 0);
        for (const Note* n : notesOn (t, kLeadChannel))
            if (n->slice < 0 && n->pos >= kCycleMt)
                CHECK (n->phrase == -1);
        // cycle 0's last slice: the next is cycle 0's first again (round the loop), not the next cycle's.
        const auto last = std::find_if (slices.begin(), slices.end(), [] (const Note* n) { return n->slice == 7; });
        REQUIRE (last != slices.end());
        CHECK_FALSE (continues (**last));
    }

    SECTION ("rewritten: every note two 16ths late")
    {
        LeadPlan plan;
        plan.cycles = { 0 };
        plan.stepOf = [] (int, int step) { return step + 2; };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        CHECK (sliceNotes (t).empty());
        for (const Note* n : notesOn (t, kLeadChannel))
            CHECK (n->phrase == -1);
    }
}

TEST_CASE ("phrase match: of two phrases on one channel each cycle plays the one with more matches; a tie, the first", "[phrase][match]")
{
    auto m = testing::syntheticKitManifest();
    auto other = m.phrases.at (0); // the same first bar, another second bar: keys 5 up from step 16
    for (auto& s : other.slices)
        if (s.step >= 16)
            s.key += 5;
    m.phrases.push_back (other);
    const int firstOther = static_cast<int> (m.phrases[0].slices.size());

    // cycle 0 plays the first phrase, cycle 1 the other's notes, cycle 2 the first bar only (a tie)
    LeadPlan plan;
    plan.cycles = { 0, 1, 2 };
    plan.keep = [] (int c, int i) { return c != 2 || kLead[static_cast<std::size_t> (i)].step < 16; };
    plan.keyOf = [] (int, int key) { return key; };
    testing::SmfBuilder b (960);
    b.tempo (0, wubansia::midi::microsPerQuarter (static_cast<int> (kBpm))).timeSignature (0, 4, 2);
    auto& lead = b.track ("Lead");
    for (const int c : plan.cycles)
    {
        for (int i = 0; i < 128; ++i)
            lead.cc (tick (c * 32) + i * kRideTicks, 2, 74, leadRide (i));
        for (std::size_t i = 0; i < kLead.size(); ++i)
            if (plan.keep (c, static_cast<int> (i)))
            {
                const auto& n = kLead[i];
                const int key = c == 1 && n.step >= 16 ? n.key + 5 : n.key;
                lead.note (tick (c * 32 + n.step), std::min<std::int64_t> (n.steps * 240, tick (32) - tick (n.step)), 2, key, 100);
            }
    }
    auto t = timelineOf (b.bytes (3 * 7680), kSwingPm, 0); // a song: no loop
    annotatePhrases (t, m);

    const auto slices = sliceNotes (t);
    REQUIRE (slices.size() == 8u + 8u + 4u);
    for (const Note* n : slices)
    {
        const auto cycle = cycleOf (*n);
        CAPTURE (cycle, n->slice);
        CHECK (n->phrase == (cycle == 1 ? 1 : 0));
        if (cycle == 1)
            CHECK (n->slice >= firstOther);
        else
            CHECK (n->slice < firstOther);
    }
    // the first phrase's cycle 0 doesn't read on into the other phrase's cycle 1
    const auto last0 = std::find_if (slices.begin(), slices.end(), [] (const Note* n) { return n->phrase == 0 && n->slice == 7; });
    REQUIRE (last0 != slices.end());
    CHECK_FALSE (continues (**last0));
}

TEST_CASE ("phrase match: a slice holds its own notes, by key and in order, before the next slice's note", "[phrase][match]")
{
    const auto m = testing::syntheticKitManifest();

    SECTION ("a second retrigger is one too many: it plays the multisample")
    {
        LeadPlan plan;
        plan.cycles = { 0 };
        plan.extra = { { 14, 74 } };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        const Note* h = noteAt (t, snap (tick (13), kSwingPm), kLeadChannel, 74);
        const Note* extra = noteAt (t, snap (tick (14), kSwingPm), kLeadChannel, 74);
        REQUIRE (h != nullptr);
        REQUIRE (extra != nullptr);
        CHECK (h->phrase == 0);
        CHECK (held (*h));
        CHECK (extra->phrase == -1);
        CHECK_FALSE (held (*extra));
        CHECK (sliceNotes (t).size() == 8u);
    }

    SECTION ("a hold doesn't take another slice's cell")
    {
        auto changed = m;
        auto& s2 = changed.phrases[0].slices[2]; // step 8, key 72: let it hold a 72
        s2.holds[0] = 72;
        s2.holdCount = 1;
        LeadPlan plan;
        plan.cycles = { 0 };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, changed);
        // step 16's 72 is slice 4's cell: it plays slice 4, though slice 2's hold waits for a 72
        const auto slices = sliceNotes (t);
        CHECK (std::any_of (slices.begin(), slices.end(), [] (const Note* n) { return n->slice == 4; }));
        CHECK (slices.size() == 8u);
    }

    SECTION ("the held retrigger without its slice's note is a note of its own")
    {
        LeadPlan plan;
        plan.cycles = { 0 };
        plan.keep = [] (int, int i) { return i != 3; }; // step 12's note is gone; step 13's stays
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        const Note* lone = noteAt (t, snap (tick (13), kSwingPm), kLeadChannel, 74);
        REQUIRE (lone != nullptr);
        CHECK (lone->phrase == -1);
        CHECK_FALSE (held (*lone));
        CHECK (sliceNotes (t).size() == 7u);
    }
}

TEST_CASE ("phrase match: a phrase with a ride plays only cycles whose controllers ride it within 1", "[phrase][match]")
{
    const auto m = testing::syntheticKitManifest();

    SECTION ("one point 2 off: that cycle plays the multisample")
    {
        LeadPlan plan;
        plan.cycles = { 0, 1 };
        plan.rideOf = [] (int c, int i) { return leadRide (i) + (c == 1 && i == 40 ? 2 : 0); };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        const auto slices = sliceNotes (t);
        REQUIRE (slices.size() == 8u);
        for (const Note* n : slices)
            CHECK (cycleOf (*n) == 0);
    }

    SECTION ("every point 1 off: it still plays")
    {
        LeadPlan plan;
        plan.cycles = { 0, 1 };
        plan.rideOf = [] (int, int i) { return leadRide (i) + 1; };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        CHECK (sliceNotes (t).size() == 16u);
    }

    SECTION ("the ride's points thinned to every other: the last value at or before each point counts")
    {
        LeadPlan plan;
        plan.cycles = { 0 };
        plan.rideOf = [] (int, int i) { return i % 2 == 0 ? leadRide (i) : -1; }; // 1 off on the odd points
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        CHECK (sliceNotes (t).size() == 8u);
    }

    SECTION ("no controllers: 0 before any, not the ride")
    {
        LeadPlan plan;
        plan.cycles = { 0 };
        plan.rideOf = [] (int, int) { return -1; };
        auto t = loopOf (leadSmf (plan));
        annotatePhrases (t, m);
        CHECK (sliceNotes (t).empty());

        auto free = m; // without a ride the controllers don't matter
        free.phrases[0].ride74.clear();
        auto u = loopOf (leadSmf (plan));
        annotatePhrases (u, free);
        CHECK (sliceNotes (u).size() == 8u);
    }

    SECTION ("CC71, which the ride leaves free, and another channel's CC74 don't matter")
    {
        testing::SmfBuilder b (960);
        b.tempo (0, wubansia::midi::microsPerQuarter (static_cast<int> (kBpm))).timeSignature (0, 4, 2);
        auto& lead = b.track ("Lead");
        lead.cc (0, 2, 71, 127).cc (0, 3, 74, 0).cc (120, 1, 74, 127);
        LeadPlan plan;
        plan.cycles = { 0 };
        writeLead (lead, plan);
        auto t = loopOf (b.bytes (15360));
        annotatePhrases (t, m);
        CHECK (sliceNotes (t).size() == 8u);
    }
}

TEST_CASE ("phrase match: notes the pre-roll step moved earlier match at their onsets, a loop's wrapping round", "[phrase][match]")
{
    // The vox-style shift: every lead note 3 16ths earlier (preroll 720000), so the first, on step 2, wraps to the
    // loop's end. The matcher reads each onset as floorMod (pos + preroll, length): the same slices as unshifted.
    const auto m = testing::syntheticKitManifest();
    auto plain = loopOf (testing::syntheticSmf (PlayMode::loop, Variant::vox));
    auto shifted = plain;
    constexpr std::int32_t shift = 720000;
    for (auto& n : shifted.notes)
        if (n.channel == kLeadChannel)
        {
            n.pos = ((n.pos - shift) % kLoopLength + kLoopLength) % kLoopLength;
            n.len += shift;
            n.preroll = shift;
        }
    std::stable_sort (shifted.notes.begin(), shifted.notes.end(), [] (const Note& a, const Note& b) {
        return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
    });
    annotatePhrases (plain, m);
    annotatePhrases (shifted, m);

    const auto a = sliceNotes (plain);
    const auto b = sliceNotes (shifted);
    REQUIRE (a.size() == 16u);
    REQUIRE (b.size() == a.size());
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        CAPTURE (i);
        CHECK (b[i]->pos == a[i]->pos);
        CHECK (b[i]->slice == a[i]->slice);
        CHECK (b[i]->preroll == a[i]->preroll);
        CHECK (b[i]->flags == a[i]->flags);
    }
    // the twins keep their pre-rolled places and preroll; the retrigger is still held
    for (const auto& n : shifted.notes)
        if (n.channel == kLeadChannel && n.slice < 0)
        {
            CHECK (n.preroll == shift);
            CHECK (n.phrase == 0);
            CHECK (held (n) == (sixteenthOf ((n.pos + n.preroll) % kLoopLength, kSwingPm) % 32 == 13));
        }
}

TEST_CASE ("phrase match: the Song's cycles start at song 16th 0, and its last slice reads on into nothing", "[phrase][match]")
{
    const auto m = testing::syntheticKitManifest();
    const auto& phrase = m.phrases.at (0);
    auto t = timelineOf (testing::syntheticSmf (PlayMode::song, Variant::novox), kSwingPm, 0);
    annotatePhrases (t, m);
    CHECK (sorted (t));
    const auto slices = sliceNotes (t);
    REQUIRE (slices.size() == 4u * 8u); // the Drop: cycles 2-5
    for (int c = 2; c < 6; ++c)
        for (int j = 0; j < 8; ++j)
        {
            CAPTURE (c, j);
            const auto it = std::find_if (slices.begin(), slices.end(), [&] (const Note* n) {
                return n->slice == j && n->pos == slicePos (phrase, c, j, false);
            });
            REQUIRE (it != slices.end());
            CHECK ((*it)->preroll == slicePreroll (phrase, c, j));
            CHECK (continues (**it) == ! (c == 5 && j == 7));
        }
}

TEST_CASE ("phrase match: a last slice that doesn't end where slice 0 starts doesn't read on across the wrap", "[phrase][match]")
{
    auto m = testing::syntheticKitManifest();
    m.phrases[0].slices[0].start += 1; // slice 0 no longer starts where the last slice wraps to
    m.phrases[0].slices[0].lead -= 1;
    auto t = loopOf (testing::syntheticSmf (PlayMode::loop, Variant::vox));
    annotatePhrases (t, m);
    for (const Note* n : sliceNotes (t))
        CHECK (continues (*n) == (n->slice != 7));
}

TEST_CASE ("phrase match: the four kits' Loop files resolve as the Python model does", "[phrase][match][vectors]")
{
    for (const char* kitName : { "fut01", "fut02", "fut03", "fut04" })
    {
        CAPTURE (kitName);
        const auto v = readJson (std::string ("match_") + kitName + ".json");
        KitManifest m;
        m.kit.bpm = static_cast<std::int32_t> (integer (v, "bpm"));
        m.kit.swingPm = static_cast<std::int32_t> (integer (v, "swing_pm"));
        std::vector<int> firstSlice;
        int slicesSoFar = 0;
        for (const auto& p : at (v, "phrases").items)
        {
            FileEntry f;
            f.frames = integer (p, "stem_frames");
            f.channels = 2;
            m.files.push_back (f);
            PhraseEntry e;
            e.lane = static_cast<Lane> (laneIndex (at (p, "lane").string));
            e.ch = static_cast<std::int32_t> (integer (p, "ch"));
            e.stemA = static_cast<std::int32_t> (m.files.size() - 1);
            e.cycle16ths = static_cast<std::int32_t> (integer (p, "cycle_16ths"));
            e.tolerancePm = static_cast<std::int32_t> (integer (p, "tolerance_pm"));
            e.fadeUs = static_cast<std::int32_t> (integer (p, "fade_us"));
            e.gainMdb = static_cast<std::int32_t> (integer (p, "gain_mdb"));
            for (const auto& x : at (p, "ride74").items)
                e.ride74.push_back (static_cast<std::uint8_t> (x.integer));
            for (const auto& x : at (p, "ride71").items)
                e.ride71.push_back (static_cast<std::uint8_t> (x.integer));
            for (const auto& s : at (p, "slices").items)
            {
                SliceEntry x;
                x.step = static_cast<std::int32_t> (s.items.at (0).integer);
                x.key = static_cast<std::int32_t> (s.items.at (1).integer);
                x.start = s.items.at (2).integer;
                x.end = s.items.at (3).integer;
                x.fade = s.items.at (4).integer;
                x.lead = static_cast<std::int32_t> (s.items.at (5).integer);
                for (const auto& h : s.items.at (6).items)
                    x.holds[static_cast<std::size_t> (x.holdCount++)] = static_cast<std::int32_t> (h.integer);
                e.slices.push_back (x);
            }
            firstSlice.push_back (slicesSoFar);
            slicesSoFar += static_cast<int> (e.slices.size());
            m.phrases.push_back (std::move (e));
        }

        for (const auto& file : at (v, "files").items)
        {
            CAPTURE (at (file, "variant").string);
            Timeline t;
            t.loops = true;
            t.length = kLoopLength;
            const auto& notes = at (file, "notes").items;
            for (const auto& n : notes)
            {
                Note note;
                note.pos = snap (n.items.at (0).integer, m.kit.swingPm);
                note.len = 240000;
                note.channel = static_cast<std::uint8_t> (n.items.at (1).integer - 1);
                note.key = static_cast<std::uint8_t> (n.items.at (2).integer);
                note.velocity = static_cast<std::uint8_t> (n.items.at (3).integer);
                t.notes.push_back (note);
            }
            for (const auto& c : at (file, "ccs").items)
            {
                Cc cc;
                cc.pos = c.items.at (0).integer * kMilliticksPerTick;
                cc.rampTo = cc.pos;
                cc.channel = static_cast<std::uint8_t> (c.items.at (1).integer - 1);
                cc.controller = static_cast<std::uint8_t> (c.items.at (2).integer);
                cc.value = static_cast<std::uint8_t> (c.items.at (3).integer);
                cc.nextValue = cc.value;
                t.ccs.push_back (cc);
            }
            std::stable_sort (t.notes.begin(), t.notes.end(), [] (const Note& a, const Note& b) {
                return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
            });
            std::stable_sort (t.ccs.begin(), t.ccs.end(), [] (const Cc& a, const Cc& b) { return a.pos < b.pos; });

            annotatePhrases (t, m);
            CHECK (sorted (t));

            std::size_t expectedSlices = 0;
            std::vector<bool> taken (t.notes.size(), false);
            auto take = [&] (auto&& match) -> const Note* {
                for (std::size_t i = 0; i < t.notes.size(); ++i)
                    if (! taken[i] && match (t.notes[i]))
                    {
                        taken[i] = true;
                        return &t.notes[i];
                    }
                return nullptr;
            };
            for (const auto& n : notes)
            {
                const auto tickOf = n.items.at (0).integer;
                const auto channel = static_cast<int> (n.items.at (1).integer - 1);
                const auto key = static_cast<int> (n.items.at (2).integer);
                const auto phrase = static_cast<int> (n.items.at (4).integer);
                const auto slice = static_cast<int> (n.items.at (5).integer);
                CAPTURE (tickOf, channel + 1, key, phrase, slice);
                const Note* original = take ([&] (const Note& x) {
                    return x.slice < 0 && x.pos == snap (tickOf, m.kit.swingPm) && x.channel == channel && x.key == key;
                });
                REQUIRE (original != nullptr);
                CHECK (original->phrase == phrase);
                CHECK (held (*original) == (phrase >= 0 && slice < 0));
                if (slice < 0)
                    continue;
                ++expectedSlices;
                const auto pos = n.items.at (7).integer;
                const Note* s = take ([&] (const Note& x) {
                    return x.slice >= 0 && x.pos == pos && x.channel == channel && x.key == key;
                });
                REQUIRE (s != nullptr);
                CHECK (s->phrase == phrase);
                CHECK (s->slice == firstSlice[static_cast<std::size_t> (phrase)] + slice);
                CHECK (s->len == n.items.at (8).integer);
                CHECK (s->preroll == n.items.at (9).integer);
                CHECK (continues (*s) == (n.items.at (6).integer != 0));
            }
            CHECK (t.notes.size() == notes.size() + expectedSlices);
        }
    }
}
