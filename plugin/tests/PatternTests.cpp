// The play mode's notes for the looks' displays (ADR 0032): each kit's own Loop and Song, read from the embedded MIDI
// by lane in 16ths, and the chords its voicing plays. Expected values measured from the files (the plan's command 5).

#include "content/EmbeddedContent.h"
#include "services/KitCatalog.h"
#include "services/PatternView.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using namespace wubansia;
using engine::Lane;
using engine::PlayMode;
using engine::Variant;

namespace
{
    const KitCatalog& catalog()
    {
        static const KitCatalog c (EmbeddedContent::sources());
        return c;
    }

    const KitCatalog::Entry& entry (const char* id)
    {
        const auto* e = catalog().find (id);
        REQUIRE (e != nullptr);
        return *e;
    }

    std::size_t count (const PatternView& view, Lane lane) { return view.lanes[(std::size_t) lane].size(); }

    std::vector<std::int32_t> onsets (const PatternView& view, Lane lane, std::int32_t from, std::int32_t to)
    {
        std::vector<std::int32_t> steps;
        for (const auto& n : view.lanes[(std::size_t) lane])
            if (n.step >= from && n.step < to && (steps.empty() || steps.back() != n.step))
                steps.push_back (n.step);
        return steps;
    }
}

TEST_CASE ("patternOf reads each kit's Loop by lane in 16ths", "[content][looks]")
{
    const auto fut01 = patternOf (entry ("FUT01"), PlayMode::loop, Variant::vox);
    REQUIRE (fut01 != nullptr);
    CHECK (fut01->kit == "FUT01");
    CHECK (fut01->steps == 64);
    const std::map<Lane, std::size_t> expected { { Lane::kick, 10 }, { Lane::snare, 9 }, { Lane::hats, 45 }, { Lane::perc, 70 },
        { Lane::bass, 36 }, { Lane::lead, 24 }, { Lane::chords, 15 }, { Lane::vox, 16 }, { Lane::accents, 3 }, { Lane::fx, 1 } };
    for (const auto& [lane, n] : expected)
    {
        INFO ("lane " << (int) lane);
        CHECK (count (*fut01, lane) == n);
    }
    CHECK (fut01->voicing.size() == 75);
    CHECK (onsets (*fut01, Lane::kick, 0, 32) == std::vector<std::int32_t> { 0, 10, 16, 23, 26 });
    CHECK (onsets (*fut01, Lane::snare, 0, 32) == std::vector<std::int32_t> { 4, 12, 20, 28, 31 });
    CHECK_FALSE (fut01->flats); // F# minor

    // Sorted by step then key; every note at least a 16th long.
    for (const auto& lane : fut01->lanes)
        for (std::size_t i = 0; i < lane.size(); ++i)
        {
            CHECK (lane[i].length >= 1);
            CHECK (lane[i].velocity >= 1);
            if (i > 0)
                CHECK ((lane[i - 1].step < lane[i].step || (lane[i - 1].step == lane[i].step && lane[i - 1].key <= lane[i].key)));
        }

    const auto fut04 = patternOf (entry ("FUT04"), PlayMode::loop, Variant::vox);
    REQUIRE (fut04->steps == 64);
    // Four on the floor (key 36, a 16th each) over a held sub (key 35, two bars).
    std::vector<std::int32_t> hits;
    for (const auto& n : fut04->notesIn (Lane::kick, 0, 16))
        if (n.key == 36)
        {
            hits.push_back (n.step);
            CHECK (n.length == 1);
        }
        else
        {
            CHECK (n.key == 35);
            CHECK (n.length == 32);
        }
    CHECK (hits == std::vector<std::int32_t> { 0, 4, 8, 12 });

    const auto fut02 = patternOf (entry ("FUT02"), PlayMode::loop, Variant::vox);
    CHECK (fut02->notesIn (Lane::lead, 0, 16).size() == 15);
    CHECK (fut02->flats); // D minor

    const auto fut03 = patternOf (entry ("FUT03"), PlayMode::loop, Variant::vox);
    CHECK (fut03->steps == 64);
    CHECK (count (*fut03, Lane::lead) == 0);
    CHECK (count (*fut03, Lane::kick) == 16);
}

TEST_CASE ("patternOf reads each kit's Song", "[content][looks]")
{
    const auto fut01 = patternOf (entry ("FUT01"), PlayMode::song, Variant::vox);
    REQUIRE (fut01 != nullptr);
    CHECK (fut01->mode == PlayMode::song);
    CHECK (fut01->steps == 3584);
    const std::map<Lane, std::size_t> expected { { Lane::kick, 485 }, { Lane::snare, 431 }, { Lane::hats, 2430 }, { Lane::perc, 3430 },
        { Lane::bass, 924 }, { Lane::lead, 400 }, { Lane::chords, 506 }, { Lane::vox, 224 }, { Lane::accents, 60 }, { Lane::fx, 14 } };
    for (const auto& [lane, n] : expected)
    {
        INFO ("lane " << (int) lane);
        CHECK (count (*fut01, lane) == n);
    }
    CHECK (fut01->voicing.size() == 2410);

    const std::map<std::string, std::pair<std::size_t, std::size_t>> kicksAndHats { { "FUT02", { 727, 2678 } }, { "FUT03", { 758, 2783 } },
        { "FUT04", { 911, 3779 } } };
    for (const auto& [kit, n] : kicksAndHats)
    {
        INFO (kit);
        const auto view = patternOf (entry (kit.c_str()), PlayMode::song, Variant::vox);
        CHECK (view->steps == 3584);
        CHECK (count (*view, Lane::kick) == n.first);
        CHECK (count (*view, Lane::hats) == n.second);
    }
}

TEST_CASE ("Off reads the Loop, and the vox switch picks the file", "[content][looks]")
{
    const auto off = patternOf (entry ("FUT01"), PlayMode::off, Variant::vox);
    const auto loop = patternOf (entry ("FUT01"), PlayMode::loop, Variant::vox);
    CHECK (off->steps == 64);
    CHECK (off->mode == PlayMode::loop);
    for (std::size_t lane = 0; lane < loop->lanes.size(); ++lane)
        CHECK (off->lanes[lane].size() == loop->lanes[lane].size());

    const auto noVox = patternOf (entry ("FUT01"), PlayMode::loop, Variant::novox);
    CHECK (noVox->variant == Variant::novox);
    CHECK (noVox->steps == 64);
    CHECK (count (*noVox, Lane::vox) == 0);
    CHECK (count (*noVox, Lane::kick) == 10);
}

TEST_CASE ("the chord changes are the voicing's onsets, named", "[content][looks]")
{
    const auto fut01 = patternOf (entry ("FUT01"), PlayMode::loop, Variant::vox);
    REQUIRE_FALSE (fut01->chords.empty());
    for (const auto& change : fut01->chords)
    {
        CHECK ((change.name == "F#m9" || change.name == "Bm9"));
        CHECK (change.end > change.step);
    }
    // Bar by bar F#m9 and Bm9 from the first stab on the third 16th, each pushed a 16th ahead of the beat, and held
    // between the stabs.
    CHECK (fut01->chords.front().step == 2);
    for (int step = 0; step < 64; ++step)
    {
        INFO ("step " << step);
        CHECK (fut01->chordAt (step) == (step < 2 ? "" : ((step - 2) / 16) % 2 == 0 ? "F#m9" : "Bm9"));
    }

    const auto fut02 = patternOf (entry ("FUT02"), PlayMode::loop, Variant::vox);
    const auto fut04 = patternOf (entry ("FUT04"), PlayMode::loop, Variant::vox);
    for (int step = 0; step < 64; ++step)
    {
        INFO ("step " << step);
        CHECK (fut02->chordAt (step) == "Dm9"); // one chord held for the whole loop
        CHECK (fut04->chordAt (step) == "Em(add9)");
    }

    const auto fut03Song = patternOf (entry ("FUT03"), PlayMode::song, Variant::vox);
    std::set<juce::String> names;
    for (const auto& change : fut03Song->chords)
        names.insert (change.name);
    CHECK (names.count ("Gm9") == 1);
    CHECK (names.count ("Cm9") == 1);
    CHECK (fut03Song->flats); // G minor

    // Nothing after the last chord's notes end and 32 16ths have passed since its onset.
    const auto fut02Song = patternOf (entry ("FUT02"), PlayMode::song, Variant::vox);
    CHECK (fut02Song->chordAt (0) == ""); // the intro has no voicing
    const auto& last = fut02Song->chords.back();
    const auto after = std::max (last.end, last.step + 33);
    CHECK (fut02Song->chordAt (after - 1) == last.name);
    if (after < fut02Song->steps)
        CHECK (fut02Song->chordAt (after) == "");
}

TEST_CASE ("a kit without the file gives an empty view", "[content][looks]")
{
    auto copy = entry ("FUT01");
    std::erase_if (copy.midiFiles, [] (const dragout::MidiFile& f) { return f.name.find ("_Loop_") != std::string_view::npos; });
    const auto view = patternOf (copy, PlayMode::loop, Variant::vox);
    REQUIRE (view != nullptr);
    CHECK (view->steps == 0);
    CHECK (view->chordAt (0) == "");
    CHECK (view->notesIn (Lane::kick, 0, 64).empty());
    CHECK (patternOf (copy, PlayMode::song, Variant::vox)->steps == 3584); // the Song is still there
}
