// The generator's pipeline (song.h): arrangement, harmony, rhythm, pitches, then the feel; a --bars
// cut last, on the finished song.

#include "gen/arp.h"
#include "gen/arrange.h"
#include "gen/cc.h"
#include "gen/cut.h"
#include "gen/duck.h"
#include "gen/feel.h"
#include "gen/harmony.h"
#include "gen/hold.h"
#include "gen/hook.h"
#include "gen/lanes.h"
#include "gen/song.h"

#include <string>
#include <vector>

namespace wubansia::gen
{
    util::Result<Song> generate (const Bundle& bundle, const Options& options)
    {
        using R = util::Result<Song>;
        const Kit& kit = bundle.kit;
        const Style& style = bundle.style;
        if (options.feelAmount < -1 || options.feelAmount > 2000)
            return R::failure ("the feel amount must be 0 to 2000 permille, not " + std::to_string (options.feelAmount));

        Song song;
        song.kitId = kit.id;
        song.kitName = kit.name;
        song.keyName = kit.keyName;
        song.camelot = kit.camelot;
        song.bpm = kit.bpm;
        song.tonic = kit.tonic;
        song.minor = kit.minor;
        song.seed = options.seed;
        song.feelAmount = options.feelAmount >= 0 ? options.feelAmount : bundle.feel.amount;

        // 1. the arrangement: the seed's whole form, whatever the cut (step 6)
        const rng::Key root = songRoot (kit.id, options.seed);
        song.sections = arrange (style, root);
        if (song.sections.empty())
            return R::failure ("the form drew no sections (a grammar the loader would refuse)");
        for (const auto& s : song.sections)
            song.totalBars += s.bars;
        for (const auto& lane : style.lanes)
        {
            const KitLane* kl = kit.lane (lane.id);
            LaneOut out;
            out.id = lane.id;
            out.track = kl->track;
            out.channel = kl->channel;
            out.kind = lane.kind;
            song.lanes.push_back (std::move (out));
        }
        planChanges (song, style, root);

        // 2. harmony
        const HarmonyPlan plan = planHarmony (style, root);
        song.mode = plan.mode;
        layOutHarmony (song, style, plan, root);

        // 3-4. the score: drum lanes first (a bass follows one), then the chords (the bass's third,
        // seventh and fifth agree with what they voice), then the bass and the arp
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
            if (style.lanes[i].kind == LaneKind::drums)
                scoreDrums (song, bundle, i, root);
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
            if (style.lanes[i].kind == LaneKind::chords)
            {
                scoreChords (song, bundle, i, root);
                holdPads (song, bundle, i); // nothing unless a pad grid holds
            }
        // the hook (hook.h): nothing without the style's hook block; its draws hang below their own tag
        planHook (song, bundle, root);
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
        {
            if (style.lanes[i].kind == LaneKind::bass)
            {
                scoreBass (song, bundle, i, root);
                duckScore (song, bundle, i); // nothing unless the lane has a duck
            }
            else if (style.lanes[i].kind == LaneKind::arp)
                scoreArp (song, bundle, i, root);
        }
        // the lanes' controllers: automation, the same in both versions (nothing without "cc")
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
            scoreControllers (song, bundle, i, root);

        // 5. the feel, always last; then the duck again, against the performed kicks
        perform (song, bundle);
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
            if (style.lanes[i].kind == LaneKind::bass)
                duckPerformance (song, bundle, i);

        // 6. a --bars cut: the finished song cut at that bar, so the cut is exactly its head
        if (options.bars != 0)
            return cutSong (std::move (song), options.bars);
        return R::success (std::move (song));
    }
}
