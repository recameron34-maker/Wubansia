#include "gen/duck.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace wubansia::gen
{
    namespace
    {
        // Shortens notes so each ends `gap` ticks before the follow lane's next onset in its own section
        // and before the section's end. notes and follow are one lane's score or performance.
        void duck (std::vector<Note>& notes, const std::vector<Note>& follow, const Song& song, std::int32_t gap, bool scoreSlides)
        {
            std::vector<std::vector<std::int64_t>> onsets (song.sections.size());
            for (const Note& k : follow)
                onsets[static_cast<std::size_t> (k.section)].push_back (k.tick);
            for (auto& v : onsets)
                std::stable_sort (v.begin(), v.end());
            for (std::size_t i = 0; i < notes.size(); ++i)
            {
                Note& n = notes[i];
                const auto si = static_cast<std::size_t> (n.section);
                const auto& on = onsets[si];
                std::int64_t limit = song.sections[si].endTick() - gap;
                const auto next = std::upper_bound (on.begin(), on.end(), n.tick); // strictly after: a note on a kick ducks the next one
                if (next != on.end())
                    limit = std::min (limit, *next - gap);
                if (n.tick + n.length <= limit)
                    continue;
                n.length = std::max<std::int64_t> (1, limit - n.tick);
                // a slide that no longer reaches its next note isn't one (the score is in time order)
                if (scoreSlides && (n.flags & noteFlag::slide) && (i + 1 >= notes.size() || n.tick + n.length <= notes[i + 1].tick))
                    n.flags = static_cast<std::uint16_t> (n.flags & ~noteFlag::slide);
            }
        }

        const std::vector<Note>* followOf (const Song& song, const Bundle& bundle, std::size_t lane, bool performed)
        {
            const LaneSpec& spec = bundle.style.lanes[lane];
            if (spec.kind != LaneKind::bass || spec.bass.duck <= 0)
                return nullptr;
            const auto follow = bundle.style.laneIndex (spec.bass.follow);
            if (!follow)
                return nullptr;
            return performed ? &song.lanes[*follow].perf : &song.lanes[*follow].score;
        }
    }

    void duckScore (Song& song, const Bundle& bundle, std::size_t lane)
    {
        if (const auto* follow = followOf (song, bundle, lane, false))
            duck (song.lanes[lane].score, *follow, song, bundle.style.lanes[lane].bass.duck, true);
    }

    void duckPerformance (Song& song, const Bundle& bundle, std::size_t lane)
    {
        if (const auto* follow = followOf (song, bundle, lane, true))
            duck (song.lanes[lane].perf, *follow, song, bundle.style.lanes[lane].bass.duck, false);
    }
}
