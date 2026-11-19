#include "services/PatternView.h"

#include "editor/ChordName.h"
#include "kit/channel_map.h"
#include "midi/reader.h"

#include <algorithm>
#include <map>

namespace wubansia
{
    namespace
    {
        // A tick in 16ths, rounded to the nearest, in integers: (tick * 4 + ppq / 2) / ppq.
        std::int64_t sixteenth (std::int64_t tick, std::int64_t ppq) { return (tick * 4 + ppq / 2) / ppq; }

        const dragout::MidiFile* findFile (const KitCatalog::Entry& entry, const std::string& name)
        {
            for (const auto& file : entry.midiFiles)
            {
                auto base = file.name;
                if (const auto slash = base.find_last_of ("/\\"); slash != std::string_view::npos)
                    base = base.substr (slash + 1);
                if (base == name)
                    return &file;
            }
            return nullptr;
        }

        bool byStepThenKey (const PatternNote& a, const PatternNote& b) { return a.step != b.step ? a.step < b.step : a.key < b.key; }
    }

    std::vector<PatternNote> PatternView::notesIn (engine::Lane lane, std::int32_t from, std::int32_t to) const
    {
        std::vector<PatternNote> notes;
        for (const auto& n : lanes[(std::size_t) lane])
        {
            if (n.step >= to)
                break;
            if (n.end() > from)
                notes.push_back (n);
        }
        return notes;
    }

    juce::String PatternView::chordAt (std::int32_t step) const
    {
        const auto after = std::upper_bound (chords.begin(), chords.end(), step, [] (std::int32_t s, const ChordChange& c) { return s < c.step; });
        if (after == chords.begin())
            return {};
        const auto& change = *(after - 1);
        return step < std::max (change.end, change.step + 33) ? change.name : juce::String();
    }

    std::shared_ptr<const PatternView> patternOf (const KitCatalog::Entry& entry, engine::PlayMode mode, engine::Variant variant)
    {
        auto view = std::make_shared<PatternView>();
        view->kit = entry.id;
        view->mode = mode == engine::PlayMode::song ? engine::PlayMode::song : engine::PlayMode::loop;
        view->variant = variant;
        view->flats = editor::keyUsesFlats (entry.card.key);

        const auto name = view->mode == engine::PlayMode::song ? dragout::songFileName (entry.id, variant) : dragout::loopFileName (entry.id, variant);
        const auto* file = findFile (entry, name);
        if (file == nullptr)
            return view;
        const auto parsed = midi::readSmf (file->bytes);
        if (! parsed.ok() || parsed.value.ppq == 0)
            return view;

        const std::int64_t ppq = parsed.value.ppq;
        std::int64_t endTick = 0;
        for (const auto& track : parsed.value.tracks)
        {
            endTick = std::max (endTick, track.endTick);
            const auto trackName = midi::trackName (track);
            const auto voicing = trackName == "Chord voicing";
            const auto lane = voicing ? -1 : engine::trackLane (entry.info, trackName);
            if (! voicing && lane < 0)
                continue;
            auto& into = voicing ? view->voicing : view->lanes[(std::size_t) lane];
            for (const auto& note : midi::notesOf (track))
            {
                PatternNote n;
                n.step = (std::int32_t) sixteenth (note.tick, ppq);
                n.length = (std::int16_t) std::clamp<std::int64_t> (sixteenth (note.length, ppq), 1, 32767);
                n.key = note.key;
                n.velocity = note.velocity;
                into.push_back (n);
            }
        }
        // Whole bars: the 4-bar Loop is 64 16ths, the 224-bar Song 3584.
        view->steps = (std::int32_t) ((sixteenth (endTick, ppq) + 15) / 16 * 16);

        for (auto& lane : view->lanes)
            std::stable_sort (lane.begin(), lane.end(), byStepThenKey);
        std::stable_sort (view->voicing.begin(), view->voicing.end(), byStepThenKey);

        // The voicing's onsets, each group named from its keys.
        for (std::size_t i = 0; i < view->voicing.size();)
        {
            ChordChange change;
            change.step = view->voicing[i].step;
            std::vector<std::uint8_t> keys;
            for (; i < view->voicing.size() && view->voicing[i].step == change.step; ++i)
            {
                keys.push_back (view->voicing[i].key);
                change.end = std::max (change.end, view->voicing[i].end());
            }
            change.name = editor::chordName (keys, view->flats);
            view->chords.push_back (std::move (change));
        }
        return view;
    }
}
