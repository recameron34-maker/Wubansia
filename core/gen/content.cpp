#include "gen/content.h"

#include "gen/arrange.h"
#include "gen/json.h"
#include "util/file_io.h"
#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace wubansia::gen
{
    namespace
    {
        using json::Node;

        bool knownKind (std::string_view kind)
        {
            return std::find (sectionKinds.begin(), sectionKinds.end(), kind) != sectionKinds.end();
        }

        // 16 or 32 values; 16 means both bars alike.
        Grid gridOf (const Node& n, std::int32_t lo, std::int32_t hi)
        {
            Grid g {};
            if (!n.exists())
                return g;
            const std::size_t count = n.size();
            if (n.isArray() && count != 16 && count != 32)
            {
                n.fail ("a grid must hold 16 or 32 values (one or two bars of 16ths), not " + std::to_string (count));
                return g;
            }
            const auto v = n.ints (count, count, lo, hi);
            for (std::size_t s = 0; s < gridSteps && !v.empty(); ++s)
                g[s] = v[s % v.size()];
            return g;
        }

        BarGrid barGridOf (const Node& n, std::int32_t lo, std::int32_t hi)
        {
            BarGrid g {};
            if (!n.exists())
                return g;
            const auto v = n.ints (16, 16, lo, hi);
            for (std::size_t s = 0; s < v.size(); ++s)
                g[s] = v[s];
            return g;
        }

        Range rangeOf (const Node& n, std::int32_t lo, std::int32_t hi)
        {
            const auto v = n.ints (2, 2, lo, hi);
            if (v.size() != 2)
                return { lo, lo };
            if (v[0] > v[1])
            {
                n.fail ("a range must be [low, high], not [" + std::to_string (v[0]) + ", " + std::to_string (v[1]) + "]");
                return { v[1], v[1] };
            }
            return { v[0], v[1] };
        }

        std::optional<Range> optionalRange (const Node& parent, std::string_view key, std::int32_t lo, std::int32_t hi)
        {
            const Node n = parent.optional (key);
            if (!n.exists())
                return std::nullopt;
            return rangeOf (n, lo, hi);
        }

        std::vector<Weighted> weightedOf (const Node& n, std::string_view valueKey, std::int32_t lo, std::int32_t hi)
        {
            std::vector<Weighted> out;
            const std::size_t count = n.size();
            if (n.isArray() && count == 0)
                n.fail ("at least one entry is needed");
            std::int64_t total = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                const Node e = n.at (i);
                e.only ({ valueKey, "w" });
                Weighted w;
                w.value = e[valueKey].int32 (lo, hi);
                w.weight = e["w"].int32 (0, 100);
                total += w.weight;
                out.push_back (w);
            }
            if (count > 0 && total == 0)
                n.fail ("every weight is 0: nothing could be picked");
            return out;
        }

        template <typename T>
        bool hasName (const std::vector<T>& v, std::string_view name)
        {
            return std::any_of (v.begin(), v.end(), [&] (const T& x) { return x.name == name; });
        }

        std::vector<std::pair<std::string, std::string>> kindMap (const Node& n)
        {
            std::vector<std::pair<std::string, std::string>> out;
            for (const auto& [kind, value] : n.members())
            {
                if (!knownKind (kind))
                {
                    value.fail ("unknown section kind \"" + kind + "\"");
                    continue;
                }
                out.emplace_back (kind, value.string (1, 64));
            }
            return out;
        }

        std::string lowered (std::string_view s)
        {
            std::string out (s);
            for (auto& c : out)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char> (c - 'A' + 'a');
            return out;
        }

        template <typename T>
        util::Result<T> finish (const json::Errors& errors, T value)
        {
            if (errors.any())
                return util::Result<T>::failure (errors.first());
            return util::Result<T>::success (std::move (value));
        }

        // ------------------------------------------------------------------ style lanes

        DrumVoiceGrid drumVoiceGrid (const Node& n, const std::string& voice, const DrumGrid& grid)
        {
            n.only ({ "p", "vel", "spread", "orn", "ornVel", "skip", "min", "turn", "turnVel", "mute", "euclid", "poly" });
            DrumVoiceGrid v;
            v.voice = voice;
            const Node p = n.optional ("p");
            const Node eu = n.optional ("euclid");
            const Node po = n.optional ("poly");
            const int kinds = (p.exists() ? 1 : 0) + (eu.exists() ? 1 : 0) + (po.exists() ? 1 : 0);
            if (kinds != 1)
                n.fail ("a voice needs exactly one of \"p\" (cells), \"euclid\" or \"poly\"");
            if (p.exists())
            {
                v.hasCells = true;
                v.p = gridOf (p, 0, 100);
                v.vel = gridOf (n["vel"], 0, 127);
                v.spread = n.int32Or ("spread", 0, 0, 30);
                v.orn = gridOf (n.optional ("orn"), 0, 100);
                v.skip = n.int32Or ("skip", 0, 0, 100);
                v.minPerBar = n.int32Or ("min", 0, 0, 16);
                v.turn = barGridOf (n.optional ("turn"), 0, 100);
                v.mute = barGridOf (n.optional ("mute"), 0, 100);
                for (std::size_t s = 0; s < gridSteps; ++s)
                    if (v.p[s] > 0 && v.vel[s] < 1)
                    {
                        n["vel"].fail ("p[" + std::to_string (s) + "] is " + std::to_string (v.p[s]) + " but vel[" + std::to_string (s)
                                       + "] is 0: a cell that can hit needs a velocity");
                        break;
                    }
                const bool anyOrn = std::any_of (v.orn.begin(), v.orn.end(), [] (std::int32_t x) { return x > 0; });
                const bool anyTurn = std::any_of (v.turn.begin(), v.turn.end(), [] (std::int32_t x) { return x > 0; });
                if (anyOrn)
                    v.ornVel = rangeOf (n["ornVel"], 1, 127);
                else if (n.optional ("ornVel").exists())
                    n.fail ("\"ornVel\" without \"orn\"");
                if (anyTurn)
                    v.turnVel = rangeOf (n["turnVel"], 1, 127);
                else if (n.optional ("turnVel").exists())
                    n.fail ("\"turnVel\" without \"turn\"");
                for (const auto& combo : grid.never)
                    for (const std::size_t bar : { 0u, 16u })
                    {
                        bool all = !combo.empty();
                        for (const auto s : combo)
                            all = all && v.p[bar + static_cast<std::size_t> (s)] >= 100;
                        if (all)
                            n["p"].fail ("its anchors hold a combination \"never\" forbids, so it can't be repaired");
                    }
            }
            else if (eu.exists())
            {
                eu.only ({ "k", "n", "rotation", "p", "vel", "accent" });
                EuclidCells e;
                e.n = eu["n"].int32 (8, 32);
                if (e.n != 8 && e.n != 16 && e.n != 32)
                    eu["n"].fail ("n must be 8, 16 or 32 (it tiles 2 bars)");
                e.k = eu["k"].int32 (0, 32);
                if (e.k > e.n)
                    eu["k"].fail ("k can't be more than n");
                e.rotation = rangeOf (eu["rotation"], 0, 31);
                if (e.rotation.hi >= e.n)
                    eu["rotation"].fail ("a rotation must be below n");
                e.p = eu.int32Or ("p", 100, 0, 100);
                e.vel = rangeOf (eu["vel"], 1, 127);
                e.accent = eu.int32Or ("accent", 0, 0, 30);
                v.euclid = e;
            }
            else if (po.exists())
            {
                po.only ({ "every", "reset", "phase", "p", "vel", "accent" });
                PolyCells c;
                c.every = po["every"].int32 (2, 16);
                c.resetBars = po["reset"].int32 (1, 16);
                c.phase = rangeOf (po["phase"], 0, 15);
                if (c.phase.hi >= c.every)
                    po["phase"].fail ("a phase must be below \"every\"");
                c.p = po.int32Or ("p", 100, 0, 100);
                c.vel = rangeOf (po["vel"], 1, 127);
                c.accent = po.int32Or ("accent", 0, 0, 30);
                v.poly = c;
            }
            return v;
        }

        void drumLane (const Node& n, LaneSpec& lane)
        {
            n.only ({ "id", "kind", "voices", "choke", "exclusive", "use", "grids", "cc" });
            DrumLane& d = lane.drums;
            const Node voices = n["voices"];
            for (std::size_t i = 0; i < voices.size(); ++i)
            {
                const Node v = voices.at (i);
                v.only ({ "id", "length" });
                DrumVoice dv;
                dv.id = v["id"].string (1, 32);
                dv.length = v["length"].int32 (1, 3840);
                if (std::any_of (d.voices.begin(), d.voices.end(), [&] (const DrumVoice& x) { return x.id == dv.id; }))
                    v["id"].fail ("the voice \"" + dv.id + "\" is listed twice");
                d.voices.push_back (dv);
            }
            if (voices.exists() && d.voices.empty())
                voices.fail ("a drum lane needs at least one voice");
            if (d.voices.size() > 8)
                voices.fail ("at most 8 voices");
            const auto isVoice = [&d] (std::string_view id) {
                return std::any_of (d.voices.begin(), d.voices.end(), [&] (const DrumVoice& x) { return x.id == id; });
            };
            const Node choke = n.optional ("choke");
            for (std::size_t i = 0; i < choke.size(); ++i)
            {
                const Node c = choke.at (i);
                c.only ({ "voice", "by" });
                ChokeRule r;
                r.voice = c["voice"].string (1, 32);
                r.by = c["by"].strings (1, 8);
                if (!isVoice (r.voice))
                    c["voice"].fail ("unknown voice \"" + r.voice + "\"");
                for (const auto& b : r.by)
                    if (!isVoice (b))
                        c["by"].fail ("unknown voice \"" + b + "\"");
                d.chokes.push_back (r);
            }
            const Node excl = n.optional ("exclusive");
            for (std::size_t i = 0; i < excl.size(); ++i)
            {
                const auto pair = excl.at (i).strings (2, 2);
                if (pair.size() == 2)
                {
                    for (const auto& vname : pair)
                        if (!isVoice (vname))
                            excl.at (i).fail ("unknown voice \"" + vname + "\"");
                    if (pair[0] == pair[1]) // it would find itself on its own step and silence every note
                        excl.at (i).fail ("a voice can't be exclusive with itself");
                    d.exclusive.emplace_back (pair[0], pair[1]);
                }
            }
            const Node grids = n["grids"];
            for (const auto& [name, g] : grids.members())
            {
                g.only ({ "sync", "never", "voices", "stages", "ramp" });
                DrumGrid grid;
                grid.name = name;
                if (g.optional ("ramp").exists())
                    grid.ramp = rangeOf (g["ramp"], 100, 2000); // permille
                const Node stages = g.optional ("stages");
                if (stages.exists())
                {
                    // a ladder of the lane's other grids: no voices, sync or never of its own
                    grid.stages = stages.strings (1, 8);
                    if (g.optional ("voices").exists() || g.optional ("sync").exists() || g.optional ("never").exists())
                        g.fail ("a grid with \"stages\" plays the stages' voices: it has no \"voices\", \"sync\" or \"never\" of its own");
                    d.grids.push_back (std::move (grid));
                    continue;
                }
                grid.sync = optionalRange (g, "sync", 0, 400);
                const Node never = g.optional ("never");
                for (std::size_t i = 0; i < never.size(); ++i)
                    grid.never.push_back (never.at (i).ints (2, 16, 0, 15));
                const Node gv = g["voices"];
                for (const auto& [voiceId, vn] : gv.members())
                    if (!isVoice (voiceId))
                        vn.fail ("unknown voice \"" + voiceId + "\"");
                for (const auto& dv : d.voices) // in the lane's voice order
                {
                    const Node vn = gv.optional (dv.id);
                    if (vn.exists())
                        grid.voices.push_back (drumVoiceGrid (vn, dv.id, grid));
                }
                if (gv.exists() && grid.voices.empty())
                    gv.fail ("a grid needs at least one voice");
                d.grids.push_back (std::move (grid));
            }
            // each stage is a grid of the lane with voices of its own, and no ramp: the staged grid's
            // ramp spans the whole section
            for (const auto& grid : d.grids)
                for (const auto& stage : grid.stages)
                {
                    const auto it = std::find_if (d.grids.begin(), d.grids.end(), [&] (const DrumGrid& x) { return x.name == stage; });
                    const Node at = grids[grid.name]["stages"];
                    if (it == d.grids.end())
                        at.fail ("the stage \"" + stage + "\" isn't a grid of this lane");
                    else if (!it->stages.empty())
                        at.fail ("the stage \"" + stage + "\" has stages of its own: a ladder is one level deep");
                    else if (it->ramp)
                        at.fail ("the stage \"" + stage + "\" has a \"ramp\": a stage plays under the ramp of the grid that stages it");
                }
        }

        void bassLane (const Node& n, LaneSpec& lane)
        {
            n.only ({ "id", "kind", "follow", "duck", "use", "degreeSets", "hold", "approach", "slide", "restrike", "grids", "cc" });
            BassLane& b = lane.bass;
            const Node follow = n.optional ("follow");
            b.follow = follow.exists() ? follow.string (1, 32) : std::string();
            const Node sets = n["degreeSets"];
            for (std::size_t i = 0; i < sets.size(); ++i)
            {
                const auto w = sets.at (i).ints (bassDegreeCount, bassDegreeCount, 0, 100);
                DegreeWeights dw {};
                std::int32_t total = 0;
                for (std::size_t k = 0; k < w.size(); ++k)
                {
                    dw[k] = w[k];
                    total += w[k];
                }
                if (w.size() == bassDegreeCount && total == 0)
                    sets.at (i).fail ("every weight is 0");
                b.degreeSets.push_back (dw);
            }
            if (sets.exists() && b.degreeSets.empty())
                sets.fail ("at least one degree set is needed");
            const Node hold = n["hold"];
            hold.only ({ "gap", "max" });
            b.holdGap = hold["gap"].int32 (0, 480);
            b.holdMax = hold["max"].int32 (120, 7680);
            const Node appr = n["approach"];
            appr.only ({ "p", "by", "below", "vel" });
            b.approach = appr["p"].int32 (0, 100);
            b.approachBy = weightedOf (appr["by"], "ticks", 120, 480);
            for (const auto& w : b.approachBy)
                if (w.value % 120 != 0)
                    appr["by"].fail ("approach ticks must be a multiple of 120 (a 16th)");
            b.approachBelow = appr["below"].int32 (0, 100);
            b.approachVel = rangeOf (appr["vel"], 1, 127);
            const Node slide = n["slide"];
            slide.only ({ "p", "overlap", "maxJump" });
            b.slide = slide["p"].int32 (0, 100);
            b.slideOverlap = slide["overlap"].int32 (1, 120);
            b.slideMaxJump = slide["maxJump"].int32 (1, 24);
            b.restrike = n.int32Or ("restrike", 480, 0, 7680);
            if (n.optional ("duck").exists())
            {
                b.duck = n["duck"].int32 (1, 90); // at most 90, so a note on the 16th grid keeps 30 ticks (duck.h)
                if (b.follow.empty())
                    n["duck"].fail ("a duck needs a lane to follow: it leaves room before that lane's onsets");
            }
            for (const auto& [name, g] : n["grids"].members())
            {
                g.only ({ "follow", "p", "orn", "ornBars", "deg", "len", "vel", "spread", "ornVel", "ornDeg", "slide", "mutate" });
                BassGrid grid;
                grid.name = name;
                grid.follow = gridOf (g.optional ("follow"), 0, 100);
                grid.p = gridOf (g.optional ("p"), 0, 100);
                grid.orn = gridOf (g.optional ("orn"), 0, 100);
                grid.ornBars = g.int32Or ("ornBars", 1, 1, 32);
                grid.deg = gridOf (g["deg"], 0, 15);
                grid.len = gridOf (g["len"], 0, 3840);
                grid.vel = gridOf (g["vel"], 0, 127);
                grid.spread = g.int32Or ("spread", 0, 0, 30);
                if (g.optional ("slide").exists())
                {
                    grid.hasSlide = true; // per-step slides, drawn with the cell (lanes.cpp)
                    grid.slide = gridOf (g["slide"], 0, 100);
                }
                grid.mutate = g.int32Or ("mutate", 0, 1, 8); // mutate.h
                const auto setCount = static_cast<std::int32_t> (b.degreeSets.size());
                for (std::size_t s = 0; s < gridSteps; ++s)
                {
                    if (grid.deg[s] >= setCount && setCount > 0)
                    {
                        g["deg"].fail ("deg[" + std::to_string (s) + "] names degree set " + std::to_string (grid.deg[s]) + ", but there are "
                                       + std::to_string (setCount));
                        break;
                    }
                    if ((grid.p[s] > 0 || grid.follow[s] > 0) && grid.vel[s] < 1)
                    {
                        g["vel"].fail ("step " + std::to_string (s) + " can play but vel[" + std::to_string (s) + "] is 0: it needs a velocity");
                        break;
                    }
                }
                if (std::any_of (grid.orn.begin(), grid.orn.end(), [] (std::int32_t x) { return x > 0; }))
                {
                    grid.ornVel = rangeOf (g["ornVel"], 1, 127);
                    grid.ornDeg = g["ornDeg"].int32 (0, std::max (0, setCount - 1));
                }
                b.grids.push_back (std::move (grid));
            }
        }

        void chordLane (const Node& n, LaneSpec& lane)
        {
            n.only ({ "id", "kind", "use", "top", "voice", "fresh", "grids", "cc" });
            ChordLane& c = lane.chords;
            if (n.optional ("top").exists())
                c.top = rangeOf (n["top"], 0, 20);
            if (n.optional ("voice").exists())
                c.voice = rangeOf (n["voice"], -10, 10);
            c.fresh = n.int32Or ("fresh", 0, 0, 1) == 1;
            for (const auto& [name, g] : n["grids"].members())
            {
                ChordGrid grid;
                grid.name = name;
                const Node pad = g.optional ("pad");
                if (pad.exists())
                {
                    g.only ({ "pad" });
                    pad.only ({ "vel", "gap", "hold" });
                    grid.pad = true;
                    grid.padVel = pad["vel"].int32 (1, 127);
                    grid.padGap = pad["gap"].int32 (0, 480);
                    grid.padHold = pad.int32Or ("hold", 0, 0, 1) == 1; // hold.h
                }
                else
                {
                    g.only ({ "p", "orn", "ornBars", "len", "vel", "spread", "min", "anticipate" });
                    grid.p = gridOf (g["p"], 0, 100);
                    grid.orn = gridOf (g.optional ("orn"), 0, 100);
                    grid.ornBars = g.int32Or ("ornBars", 1, 1, 32);
                    grid.len = gridOf (g["len"], 0, 3840);
                    grid.vel = gridOf (g["vel"], 0, 127);
                    grid.spread = g.int32Or ("spread", 0, 0, 30);
                    grid.minPerBar = g.int32Or ("min", 0, 0, 16);
                    grid.anticipate = g.int32Or ("anticipate", 1, 0, 1) == 1;
                    for (std::size_t s = 0; s < gridSteps; ++s)
                        if ((grid.p[s] > 0 || grid.orn[s] > 0) && (grid.vel[s] < 1 || grid.len[s] < 1))
                        {
                            g["vel"].fail ("step " + std::to_string (s) + " can play but has no velocity or length");
                            break;
                        }
                }
                c.grids.push_back (std::move (grid));
            }
        }

        // An arp lane (arp.h): a ladder of chord tones, a 16th grid and the cells it plays.
        void arpLane (const Node& n, LaneSpec& lane)
        {
            n.only ({ "id", "kind", "use", "center", "grids", "cc" });
            ArpLane& a = lane.arp;
            a.center = n["center"].int32 (0, 127);
            for (const auto& [name, g] : n["grids"].members())
            {
                g.only ({ "tones", "p", "vel", "len", "spread", "cells", "restart", "shift", "octave" });
                ArpGrid grid;
                grid.name = name;
                grid.tones = g["tones"].ints (1, 7, 0, 6);
                if (!grid.tones.empty() && grid.tones.front() != 0)
                    g["tones"].fail ("the ladder starts on the root: tones begins with 0");
                for (std::size_t i = 1; i < grid.tones.size(); ++i)
                    if (grid.tones[i] <= grid.tones[i - 1])
                    {
                        g["tones"].fail ("tones must be ascending: scale steps in one octave, 0 to 6");
                        break;
                    }
                grid.p = gridOf (g["p"], 0, 100);
                grid.vel = gridOf (g["vel"], 0, 127);
                grid.len = gridOf (g["len"], 0, 3840);
                grid.spread = g.int32Or ("spread", 0, 0, 30);
                for (std::size_t s = 0; s < gridSteps; ++s)
                    if (grid.p[s] > 0 && (grid.vel[s] < 1 || grid.len[s] < 1))
                    {
                        g["vel"].fail ("step " + std::to_string (s) + " can play but has no velocity or length");
                        break;
                    }
                const Node cells = g["cells"];
                std::int32_t total = 0;
                for (std::size_t i = 0; i < cells.size(); ++i)
                {
                    const Node c = cells.at (i);
                    c.only ({ "idx", "w" });
                    ArpCell cell;
                    cell.idx = c["idx"].ints (1, 16, 0, 15);
                    cell.weight = c["w"].int32 (0, 100);
                    total += cell.weight;
                    grid.cells.push_back (std::move (cell));
                }
                if (cells.exists() && grid.cells.empty())
                    cells.fail ("at least one cell is needed");
                else if (cells.exists() && total == 0)
                    cells.fail ("every weight is 0: no cell could be picked");
                const Node restart = g.optional ("restart");
                if (restart.exists())
                    grid.restart = restart.ints (0, 16, 0, 15);
                if (g.optional ("shift").exists())
                    grid.shift = rangeOf (g["shift"], 0, 15);
                grid.octave = g.int32Or ("octave", 0, -24, 24);
                if (grid.octave % 12 != 0)
                    g["octave"].fail ("the octave is a multiple of 12 semitones, not " + std::to_string (grid.octave));
                a.grids.push_back (std::move (grid));
            }
        }

        // A lead lane (lead.h): the hook's carrier. Its grids hold only the scales scoreLead puts on the
        // hook's notes: "grids": { "plant": { "vel": 920, "len": 850 }, ... }, permille.
        void leadLane (const Node& n, LaneSpec& lane)
        {
            n.only ({ "id", "kind", "use", "grids", "cc" });
            for (const auto& [name, g] : n["grids"].members())
            {
                g.only ({ "vel", "len" });
                LeadGrid grid;
                grid.name = name;
                grid.vel = g["vel"].int32 (1, 2000);
                grid.len = g["len"].int32 (1, 2000);
                lane.lead.grids.push_back (std::move (grid));
            }
        }

        // A lane's controller lanes (cc.h), any lane kind.
        std::vector<CcSpec> ccSpecs (const Node& n)
        {
            std::vector<CcSpec> out;
            if (!n.exists())
                return out;
            for (std::size_t i = 0; i < n.size(); ++i)
            {
                const Node c = n.at (i);
                c.only ({ "number", "step", "rows" });
                CcSpec spec;
                spec.number = c["number"].int32 (0, 119); // 120-127 are channel mode messages
                spec.step = c.int32Or ("step", 120, 1, 1920);
                if (spec.step != 60 && spec.step != 120 && spec.step != 240 && spec.step != 480)
                    c["step"].fail ("the step between points is 60, 120, 240 or 480 ticks, not " + std::to_string (spec.step));
                for (const auto& [kind, r] : c["rows"].members())
                {
                    if (!knownKind (kind))
                    {
                        r.fail ("unknown section kind \"" + kind + "\"");
                        continue;
                    }
                    r.only ({ "from", "to", "wave", "depth" });
                    CcRow row;
                    row.kind = kind;
                    row.from = rangeOf (r["from"], 0, 127);
                    row.to = rangeOf (r["to"], 0, 127);
                    const Node wave = r.optional ("wave");
                    if (wave.exists())
                    {
                        row.waveBars = wave.ints (1, 4, 1, 64);
                        for (const auto w : row.waveBars)
                            if (w != 4 && w != 8 && w != 16 && w != 32)
                            {
                                wave.fail ("an arch is 4, 8, 16 or 32 bars long, not " + std::to_string (w));
                                break;
                            }
                        row.depth = rangeOf (r["depth"], -127, 127);
                    }
                    else if (r.optional ("depth").exists())
                        r.fail ("\"depth\" without \"wave\"");
                    spec.rows.push_back (std::move (row));
                }
                if (std::any_of (out.begin(), out.end(), [&] (const CcSpec& x) { return x.number == spec.number; }))
                    c["number"].fail ("the controller " + std::to_string (spec.number) + " is listed twice");
                out.push_back (std::move (spec));
            }
            return out;
        }

        bool laneHasGrid (const LaneSpec& lane, std::string_view grid)
        {
            switch (lane.kind)
            {
                case LaneKind::drums:
                    return hasName (lane.drums.grids, grid);
                case LaneKind::bass:
                    return hasName (lane.bass.grids, grid);
                case LaneKind::chords:
                    return hasName (lane.chords.grids, grid);
                case LaneKind::arp:
                    return hasName (lane.arp.grids, grid);
                case LaneKind::lead:
                    return hasName (lane.lead.grids, grid);
            }
            return false;
        }

        // ------------------------------------------------------------------ harmony

        void harmony (const Node& n, HarmonySpec& h)
        {
            n.only ({ "modes", "colour", "loops", "rhythms", "push", "degrees", "move", "memory", "penalty", "slots", "use", "palette", "voicings", "mud", "revoice", "arcs" });
            const Node modes = n["modes"];
            for (std::size_t i = 0; i < modes.size(); ++i)
            {
                const Node m = modes.at (i);
                m.only ({ "mode", "w" });
                const std::string name = m["mode"].string();
                const auto mode = modeFromName (name);
                if (!mode)
                    m["mode"].fail ("unknown mode \"" + name + "\" (aeolian, dorian or phrygian)");
                h.modes.push_back ({ static_cast<std::int32_t> (mode.value_or (Mode::aeolian)), m["w"].int32 (0, 100) });
            }
            if (modes.exists() && h.modes.empty())
                modes.fail ("at least one mode is needed");
            for (const auto& [name, v] : n.optional ("colour").members())
            {
                const auto mode = modeFromName (name);
                if (!mode)
                {
                    v.fail ("unknown mode \"" + name + "\"");
                    continue;
                }
                h.colour[static_cast<std::size_t> (*mode)] = v.ints (1, 7, 0, 6);
            }
            h.loops = weightedOf (n["loops"], "bars", 2, 4);
            for (const auto& w : h.loops)
                if (w.value != 2 && w.value != 4)
                    n["loops"].fail ("a loop is 2 or 4 bars");
            const Node rhythms = n["rhythms"];
            for (std::size_t i = 0; i < rhythms.size(); ++i)
            {
                const Node r = rhythms.at (i);
                r.only ({ "units", "w" });
                // [4]: one chord over the whole loop, a pedal. At most 3 chords, the plan's grammar: it
                // also keeps the progression weights (harmony.cpp) far inside 64 bits.
                const Node unitsNode = r["units"];
                std::vector<std::int32_t> units;
                if (unitsNode.isArray() && unitsNode.size() > 3)
                    unitsNode.fail ("a harmonic rhythm holds at most 3 chords (2 or 3, or [4] for a one-chord pedal), not " + std::to_string (unitsNode.size()));
                else
                    units = unitsNode.ints (1, 3, 1, 4);
                std::int32_t sum = 0;
                for (const auto u : units)
                    sum += u;
                if (!units.empty() && sum != 4)
                    r["units"].fail ("harmonic rhythm units must add up to 4, not " + std::to_string (sum));
                h.rhythms.push_back (std::move (units));
                h.rhythmWeights.push_back (r["w"].int32 (0, 100));
            }
            const Node push = n["push"];
            push.only ({ "permille", "by", "alike" });
            h.pushPermille = rangeOf (push["permille"], 0, 1000);
            h.pushBy = weightedOf (push["by"], "ticks", 120, 480);
            h.pushAlike = push.int32Or ("alike", 0, 0, 1) == 1;
            for (const auto& w : h.pushBy)
                if (w.value % 120 != 0)
                    push["by"].fail ("push ticks must be a multiple of 120 (a 16th)");
            const Node degrees = n["degrees"];
            degrees.only ({ "aeolian", "dorian", "phrygian" });
            for (const Mode m : { Mode::aeolian, Mode::dorian, Mode::phrygian })
            {
                const auto w = degrees[modeName (m)].ints (7, 7, 0, 100);
                for (std::size_t i = 0; i < w.size(); ++i)
                    h.degrees[static_cast<std::size_t> (m)][i] = w[i];
            }
            const Node move = n["move"];
            if (move.exists() && move.size() != 7)
                move.fail ("the move table needs 7 rows, one per degree");
            for (std::size_t a = 0; a < 7 && a < move.size(); ++a)
            {
                const auto row = move.at (a).ints (7, 7, 0, 100);
                for (std::size_t b = 0; b < row.size(); ++b)
                    h.move[a][b] = row[b];
            }
            const Node memory = n["memory"];
            for (std::size_t i = 0; i < memory.size(); ++i)
            {
                const Node m = memory.at (i);
                m.only ({ "name", "shape" });
                h.memory.push_back ({ m["name"].string (1, 128), m["shape"].ints (2, 8, 0, 6) });
            }
            const Node penalty = n["penalty"];
            penalty.only ({ "whole", "three", "two" });
            h.penaltyWhole = penalty["whole"].int32 (0, 1000);
            h.penaltyThree = penalty["three"].int32 (0, 1000);
            h.penaltyTwo = penalty["two"].int32 (0, 1000);
            const Node slots = n["slots"];
            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                const Node s = slots.at (i);
                s.only ({ "name", "start", "tonic", "phrygian" });
                ProgressionSlot slot;
                slot.name = s["name"].string (1, 32);
                const auto start = s["start"].ints (7, 7, 0, 100);
                for (std::size_t k = 0; k < start.size(); ++k)
                    slot.start[k] = start[k];
                slot.tonic = s["tonic"].string();
                if (slot.tonic != "present" && slot.tonic != "emergent" && slot.tonic != "absent")
                    s["tonic"].fail ("the tonic strategy is present, emergent or absent, not \"" + slot.tonic + "\"");
                slot.phrygian = s.int32Or ("phrygian", 0, 0, 100);
                if (hasName (h.slots, slot.name))
                    s["name"].fail ("the slot \"" + slot.name + "\" is listed twice");
                h.slots.push_back (std::move (slot));
            }
            h.use = kindMap (n["use"]);
            for (const auto& [kind, slot] : h.use)
                if (!hasName (h.slots, slot))
                    n["use"].fail ("the section kind \"" + kind + "\" uses the slot \"" + slot + "\", which isn't defined");
            const Node voicings = n["voicings"];
            for (std::size_t i = 0; i < voicings.size(); ++i)
            {
                const Node v = voicings.at (i);
                v.only ({ "name", "degrees", "shape", "choose", "inversions", "span", "top", "center", "quality" });
                VoicingSpec spec;
                spec.name = v["name"].string (1, 32);
                const Node degs = v.optional ("degrees");
                const Node shape = v.optional ("shape");
                if (degs.exists() == shape.exists())
                    v.fail ("a voicing needs exactly one of \"degrees\" (scale steps) or \"shape\" (semitones, planed)");
                if (degs.exists())
                    spec.degrees = degs.ints (1, 6, 0, 14);
                if (shape.exists())
                    spec.shape = shape.ints (1, 6, 0, 24);
                const std::size_t tones = spec.degrees.empty() ? spec.shape.size() : spec.degrees.size();
                spec.choose = v.int32Or ("choose", 0, 0, static_cast<std::int32_t> (tones));
                spec.inversions = v.int32Or ("inversions", 0, 0, 1) == 1;
                if (v.optional ("span").exists())
                    spec.span = rangeOf (v["span"], 0, 48);
                spec.top = rangeOf (v["top"], 0, 127);
                spec.center = v["center"].int32 (0, 127);
                const Node quality = v.optional ("quality");
                if (quality.exists())
                {
                    const std::string q = quality.string();
                    if (q == "minor")
                        spec.quality = 1;
                    else if (q != "any")
                        quality.fail ("quality is \"minor\" or \"any\", not \"" + q + "\"");
                }
                if (hasName (h.voicings, spec.name))
                    v["name"].fail ("the voicing \"" + spec.name + "\" is listed twice");
                h.voicings.push_back (std::move (spec));
            }
            for (const auto& [kind, list] : n["palette"].members())
            {
                if (!knownKind (kind))
                {
                    list.fail ("unknown section kind \"" + kind + "\"");
                    continue;
                }
                auto names = list.strings (1, 12);
                for (const auto& name : names)
                    if (!hasName (h.voicings, name))
                        list.fail ("unknown voicing \"" + name + "\"");
                h.palette.emplace_back (kind, std::move (names));
            }
            const Node mud = n["mud"];
            mud.only ({ "below", "gap", "minorSecond", "minorNinth" });
            h.mud.below = mud["below"].int32 (0, 127);
            h.mud.gap = mud["gap"].int32 (0, 12);
            h.mud.minorSecond = mud["minorSecond"].int32 (0, 127);
            h.mud.minorNinth = mud["minorNinth"].int32 (0, 127);
            h.revoice = n["revoice"].int32 (0, 100);
            h.arcs = n["arcs"].strings (1, 4);
            for (const auto& a : h.arcs)
                if (a != "rise" && a != "fall" && a != "arch" && a != "flat")
                    n["arcs"].fail ("an arc is rise, fall, arch or flat, not \"" + a + "\"");
        }

        bool isDrumLane (const Style& style, std::string_view id)
        {
            const auto i = style.laneIndex (id);
            return i && style.lanes[*i].kind == LaneKind::drums;
        }

        // ------------------------------------------------------------------ the form grammar (arrange.h)

        bool contains (const std::vector<std::string>& ids, std::string_view id) { return std::find (ids.begin(), ids.end(), id) != ids.end(); }

        // Lane ids, each a lane of the style, none twice.
        std::vector<std::string> laneIds (const Node& n, const Style& style, std::size_t minCount)
        {
            auto ids = n.strings (minCount, 32);
            for (std::size_t a = 0; a < ids.size(); ++a)
            {
                if (!style.laneIndex (ids[a]))
                    n.fail ("unknown lane \"" + ids[a] + "\"");
                for (std::size_t b = a + 1; b < ids.size(); ++b)
                    if (ids[a] == ids[b])
                        n.fail ("the lane \"" + ids[a] + "\" is listed twice");
            }
            return ids;
        }

        // Lengths in bars by weight: multiples of 8 in [lo, 256].
        std::vector<Weighted> barsOf (const Node& n, std::int32_t lo)
        {
            auto out = weightedOf (n, "bars", lo, 256);
            for (std::size_t i = 0; i < out.size(); ++i)
                if (out[i].value % 8 != 0)
                    n.at (i)["bars"].fail ("a length is a multiple of 8 bars (sections and lane moves sit on 8-bar lines), not " + std::to_string (out[i].value));
            return out;
        }

        std::vector<LaneWeight> laneWeights (const Node& n, const Style& style)
        {
            std::vector<LaneWeight> out;
            std::int64_t total = 0;
            for (std::size_t i = 0; i < n.size(); ++i)
            {
                const Node e = n.at (i);
                e.only ({ "lane", "w" });
                LaneWeight lw { e["lane"].string (1, 32), e["w"].int32 (0, 100) };
                if (!style.laneIndex (lw.lane))
                    e["lane"].fail ("unknown lane \"" + lw.lane + "\"");
                total += lw.weight;
                out.push_back (std::move (lw));
            }
            if (n.isArray() && total == 0)
                n.fail ("every weight is 0: nothing could be picked");
            return out;
        }

        // The DJ rules of a 32-bar mix-in (in) or mix-out: drums only in its first (last) block; lanes
        // only joining (leaving), at least one every 8 or 16 bars (never three blocks in a row alike).
        void edgeBlocks (const Style& style, const std::vector<std::vector<std::string>>& blocks, const Node& at, bool in)
        {
            const std::string name = in ? "the mix-in" : "the mix-out";
            if (blocks.size() != 4)
            {
                at.fail (name + " is 32 bars long (4 blocks), not " + std::to_string (blocks.size() * 8));
                return;
            }
            const std::size_t edge = in ? 0 : blocks.size() - 1;
            if (blocks[edge].empty())
            {
                at.at (edge).fail (name + (in ? " starts" : " ends") + " on drums, but its " + (in ? "first" : "last") + " block is empty");
                return;
            }
            for (const auto& id : blocks[edge])
                if (!isDrumLane (style, id))
                {
                    at.at (edge).fail (name + (in ? " starts" : " ends") + " with drums only, but its " + (in ? "first" : "last") + " block has \"" + id + "\"");
                    return;
                }
            // a DJ mixes on the kick: every lane a bass follows plays the edge block
            for (const LaneSpec& lane : style.lanes)
                if (lane.kind == LaneKind::bass && !lane.bass.follow.empty() && !contains (blocks[edge], lane.bass.follow))
                {
                    at.at (edge).fail (name + (in ? " starts" : " ends") + " on the lane a DJ mixes on, \"" + lane.bass.follow + "\" (the " + lane.id + " follows it), but its "
                                       + (in ? "first" : "last") + " block doesn't have it");
                    return;
                }
            const auto same = [] (const std::vector<std::string>& a, const std::vector<std::string>& b) {
                return a.size() == b.size() && std::all_of (a.begin(), a.end(), [&] (const std::string& id) { return contains (b, id); });
            };
            for (std::size_t k = 1; k < blocks.size(); ++k)
            {
                const auto& before = blocks[k - 1];
                const auto& here = blocks[k];
                for (const auto& id : in ? before : here)
                    if (!contains (in ? here : before, id))
                    {
                        at.at (k).fail (in ? "lanes only join in " + name + ", but \"" + id + "\" leaves here" : "lanes only leave in " + name + ", but \"" + id + "\" joins here");
                        return;
                    }
                if (k >= 2 && same (here, before) && same (before, blocks[k - 2]))
                {
                    at.at (k).fail (name + (in ? " brings a lane in" : " takes a lane out") + " every 8 or 16 bars, but this is the third block in a row with the same lanes");
                    return;
                }
            }
        }

        KindPlan kindPlan (const Node& n, const std::string& kind, const Style& style)
        {
            KindPlan plan;
            plan.kind = kind;
            if (kind == "intro" || kind == "outro")
            {
                n.only ({ "choices" });
                const Node choices = n["choices"];
                if (choices.isArray() && choices.size() == 0)
                    choices.fail ("at least one choice is needed");
                std::int64_t total = 0;
                for (std::size_t i = 0; i < choices.size(); ++i)
                {
                    const Node c = choices.at (i);
                    c.only ({ "w", "blocks" });
                    BlockChoice choice;
                    choice.weight = c["w"].int32 (0, 100);
                    total += choice.weight;
                    const Node blocks = c["blocks"];
                    for (std::size_t k = 0; k < blocks.size(); ++k)
                        choice.blocks.push_back (laneIds (blocks.at (k), style, 0));
                    if (blocks.exists())
                        edgeBlocks (style, choice.blocks, blocks, kind == "intro");
                    plan.choices.push_back (std::move (choice));
                }
                if (choices.isArray() && choices.size() > 0 && total == 0)
                    choices.fail ("every weight is 0: nothing could be picked");
                return plan;
            }
            n.only ({ "lanes", "entries", "rest", "dropout", "change" });
            plan.lanes = laneIds (n["lanes"], style, 1);
            const Node entries = n.optional ("entries");
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                const Node e = entries.at (i);
                e.only ({ "lanes", "after", "last", "before" });
                LaneEntry entry;
                entry.lanes = laneIds (e["lanes"], style, 1);
                for (const auto& id : entry.lanes)
                {
                    if (!contains (plan.lanes, id))
                        e["lanes"].fail ("\"" + id + "\" isn't one of the " + kind + "'s lanes");
                    for (const auto& other : plan.entries)
                        if (contains (other.lanes, id))
                            e["lanes"].fail ("\"" + id + "\" is in another entry: a lane joins or leaves once");
                }
                const bool after = e.optional ("after").exists(), last = e.optional ("last").exists(), before = e.optional ("before").exists();
                if (int (after) + int (last) + int (before) != 1)
                    e.fail ("an entry has one of \"after\", \"last\" or \"before\"");
                std::string_view verb = "before";
                entry.entry = Entry::before;
                if (after)
                {
                    verb = "after";
                    entry.entry = Entry::after;
                }
                else if (last)
                {
                    verb = "last";
                    entry.entry = Entry::last;
                }
                if (after || last || before)
                    entry.bars = barsOf (e[verb], last ? 8 : 0); // "last" plays at least one block
                plan.entries.push_back (std::move (entry));
            }
            const auto entered = [&plan] (std::string_view id) {
                return std::any_of (plan.entries.begin(), plan.entries.end(), [&] (const LaneEntry& e) { return contains (e.lanes, id); });
            };
            const auto layered = [&style] (std::string_view id) {
                return std::any_of (style.form.layer.begin(), style.form.layer.end(), [&] (const LaneWeight& lw) { return lw.lane == id; });
            };
            if (const Node r = n.optional ("rest"); r.exists())
            {
                r.only ({ "p", "bars", "lanes" });
                RestSpec rest;
                rest.p = r["p"].int32 (0, 100);
                rest.bars = r["bars"].int32 (8, 16);
                if (rest.bars != 8 && rest.bars != 16)
                    r["bars"].fail ("a rest is 8 or 16 bars, not " + std::to_string (rest.bars));
                rest.lanes = laneWeights (r["lanes"], style);
                for (const auto& lw : rest.lanes)
                    if (!contains (plan.lanes, lw.lane) || entered (lw.lane) || layered (lw.lane))
                        r["lanes"].fail ("\"" + lw.lane + "\" can't rest: a rest takes one of the " + kind + "'s lanes that plays all through it (no entry, not drop 2's layer)");
                plan.rest = std::move (rest);
            }
            if (const Node d = n.optional ("dropout"); d.exists())
            {
                d.only ({ "p", "every", "lanes" });
                DropoutSpec dropout;
                dropout.p = d["p"].int32 (0, 100);
                dropout.every = d["every"].int32 (16, 32);
                if (dropout.every != 16 && dropout.every != 32)
                    d["every"].fail ("a dropout comes before every 16- or 32-bar line, not " + std::to_string (dropout.every));
                const Node groups = d["lanes"];
                std::int64_t total = 0;
                for (std::size_t i = 0; i < groups.size(); ++i)
                {
                    const Node g = groups.at (i);
                    g.only ({ "lanes", "w" });
                    LaneGroup group { laneIds (g["lanes"], style, 1), g["w"].int32 (0, 100) };
                    total += group.weight;
                    for (const auto& id : group.lanes)
                        if (!contains (plan.lanes, id))
                            g["lanes"].fail ("\"" + id + "\" isn't one of the " + kind + "'s lanes");
                    if (std::all_of (group.lanes.begin(), group.lanes.end(), [&] (const std::string& id) { return entered (id) || layered (id); }))
                        g["lanes"].fail ("a dropout group needs a lane that plays all through the " + kind + " (no entry, not drop 2's layer)");
                    dropout.lanes.push_back (std::move (group));
                }
                if (groups.isArray() && total == 0)
                    groups.fail ("every weight is 0: nothing could be picked");
                plan.dropout = std::move (dropout);
            }
            if (n.optional ("change").exists())
                plan.change = n["change"].boolean();
            return plan;
        }

        void formOf (const Node& n, Style& style)
        {
            n.only ({ "bars", "sixteen", "sections", "layer", "plans" });
            FormSpec& form = style.form;
            form.bars = rangeOf (n["bars"], 64, 512);
            if (form.bars.lo % 8 != 0 || form.bars.hi % 8 != 0)
                n["bars"].fail ("the form's length is a multiple of 8 bars at both ends");
            form.sixteen = n["sixteen"].int32 (0, 100);

            const Node sections = n["sections"];
            if (sections.isArray() && sections.size() < 2)
                sections.fail ("the form needs a mix-in and a mix-out at least");
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                const Node s = sections.at (i);
                s.only ({ "kind", "bars", "variant" });
                FormSlot slot;
                slot.kind = s["kind"].string();
                if (!knownKind (slot.kind))
                    s["kind"].fail ("unknown section kind \"" + slot.kind + "\"");
                else if (style.section (slot.kind) == nullptr)
                    s["kind"].fail ("the kind \"" + slot.kind + "\" has no entry in \"sections\"");
                slot.bars = barsOf (s["bars"], 0);
                slot.variant = s.int32Or ("variant", -1, 0, 15);
                form.slots.push_back (std::move (slot));
            }
            if (n.errors().any())
                return;
            // A DJ-friendly extended mix: mixed in on a 32-bar intro, mixed out on a 32-bar outro.
            for (std::size_t i = 0; i < form.slots.size(); ++i)
            {
                const FormSlot& slot = form.slots[i];
                const Node at = sections.at (i);
                const bool first = i == 0, last = i + 1 == form.slots.size();
                if (first && slot.kind != "intro")
                    at["kind"].fail ("the form starts with an intro, the mix-in");
                else if (last && slot.kind != "outro")
                    at["kind"].fail ("the form ends with an outro, the mix-out");
                else if (!first && slot.kind == "intro")
                    at["kind"].fail ("the intro, the mix-in, comes first and only there");
                else if (!last && slot.kind == "outro")
                    at["kind"].fail ("the outro, the mix-out, comes last and only there");
                else if (slot.kind == "intro" || slot.kind == "outro")
                {
                    for (std::size_t k = 0; k < slot.bars.size(); ++k)
                        if (slot.bars[k].value != 32)
                            at["bars"].at (k)["bars"].fail ((first ? "the mix-in" : "the mix-out") + std::string (" is 32 bars long, not ") + std::to_string (slot.bars[k].value));
                }
                // a build leads into a drop: the next slot is a drop or drop2 that always plays
                if (slot.kind == "build")
                {
                    const bool intoDrop = !last && (form.slots[i + 1].kind == "drop" || form.slots[i + 1].kind == "drop2")
                                          && std::none_of (form.slots[i + 1].bars.begin(), form.slots[i + 1].bars.end(), [] (const Weighted& w) { return w.value == 0 && w.weight > 0; });
                    if (!intoDrop)
                        at["kind"].fail ("a build leads into a drop: the next section is a drop or drop2 that is never left out");
                }
            }
            if (n.errors().any())
                return;

            // drop 2's layer: before the plans, which keep it out of their rests
            if (const Node layer = n.optional ("layer"); layer.exists())
                form.layer = laneWeights (layer, style);

            const Node plans = n["plans"];
            for (const auto& [kind, p] : plans.members())
            {
                if (!knownKind (kind))
                {
                    p.fail ("unknown section kind \"" + kind + "\"");
                    continue;
                }
                if (std::none_of (form.slots.begin(), form.slots.end(), [&] (const FormSlot& s) { return s.kind == kind; }))
                {
                    p.fail ("the form has no " + kind + " section");
                    continue;
                }
                form.plans.push_back (kindPlan (p, kind, style));
            }
            for (std::size_t i = 0; i < form.slots.size(); ++i)
                if (form.plan (form.slots[i].kind) == nullptr)
                {
                    plans.fail ("the form has a " + form.slots[i].kind + " section, but \"plans\" gives it no plan");
                    break;
                }
            if (n.errors().any())
                return;

            // the layer plays all through drop 2 and is held out of drop 1
            if (!form.layer.empty())
            {
                const KindPlan* drop = form.plan ("drop");
                const KindPlan* drop2 = form.plan ("drop2");
                if (drop == nullptr || drop2 == nullptr)
                    n["layer"].fail ("drop 2's layer needs a drop and a drop2 section");
                else
                    for (const auto& lw : form.layer)
                        if (!contains (drop2->lanes, lw.lane) || std::any_of (drop2->entries.begin(), drop2->entries.end(), [&] (const LaneEntry& e) { return contains (e.lanes, lw.lane); }))
                            n["layer"].fail ("\"" + lw.lane + "\" isn't one of the drop2's lanes that plays all through it");
            }

            const auto optionalSlot = [&form] (std::size_t i) {
                return std::any_of (form.slots[i].bars.begin(), form.slots[i].bars.end(), [] (const Weighted& w) { return w.value == 0 && w.weight > 0; });
            };
            const auto layered = [&form] (std::string_view id) {
                return std::any_of (form.layer.begin(), form.layer.end(), [&] (const LaneWeight& lw) { return lw.weight > 0 && lw.lane == id; });
            };

            // the mix-in's lanes carry on into the section after it (lanes only join from the mix-in on):
            // every lane a mix-in choice ends on is one of that kind's lanes from its start. Late: an
            // "after" or "last" entry, or a "before" entry long enough to keep the lane out of the
            // whole section; and drop 2's layer, which drop 1 never plays
            const KindPlan* intro = form.plan ("intro");
            for (std::size_t i = 1; intro != nullptr && i < form.slots.size(); ++i)
            {
                const FormSlot& slot = form.slots[i];
                const KindPlan& next = *form.plan (slot.kind);
                std::int32_t shortest = 0;
                for (const Weighted& w : slot.bars)
                    if (w.value > 0 && w.weight > 0 && (shortest == 0 || w.value < shortest))
                        shortest = w.value;
                const auto late = [&] (const LaneEntry& e) {
                    if (e.entry != Entry::before)
                        return true;
                    return std::any_of (e.bars.begin(), e.bars.end(), [&] (const Weighted& w) { return w.weight > 0 && w.value >= shortest; });
                };
                for (const BlockChoice& c : intro->choices)
                {
                    if (c.blocks.empty())
                        continue;
                    for (const auto& id : c.blocks.back())
                    {
                        if (!contains (next.lanes, id) || std::any_of (next.entries.begin(), next.entries.end(), [&] (const LaneEntry& e) { return contains (e.lanes, id) && late (e); }))
                        {
                            plans[slot.kind].fail ("the mix-in ends on \"" + id + "\", so the " + slot.kind + " after it plays it from its start");
                            return;
                        }
                        if (slot.kind == "drop" && layered (id))
                        {
                            n["layer"].fail ("the mix-in ends on \"" + id + "\", so the drop after it plays it from its start, but it's drop 2's layer, which drop 1 never plays");
                            return;
                        }
                    }
                }
                // the next slot that can follow the mix-in, if this one can be left out
                if (!optionalSlot (i))
                    break;
            }

            // lanes only leave from the section before the mix-out on (arrange.h draws only such a
            // choice): some mix-out choice starts on no non-drum lane that a section able to come
            // before it can end without (a lane it doesn't play, a "before" entry, an "after" entry
            // that can sit the section out, a rest that can take its last block, drop 2's layer in a
            // drop, a mix-in choice that ends without it)
            const KindPlan* outro = form.plan ("outro");
            if (outro != nullptr && form.slots.size() >= 2 && form.slots.back().kind == "outro")
            {
                std::set<std::string, std::less<>> endless; // non-drum lanes some section before the mix-out can end without
                for (std::size_t i = form.slots.size() - 1; i-- > 0;)
                {
                    const FormSlot& slot = form.slots[i];
                    const KindPlan& plan = *form.plan (slot.kind);
                    std::int32_t shortest = 0;
                    for (const Weighted& w : slot.bars)
                        if (w.value > 0 && w.weight > 0 && (shortest == 0 || w.value < shortest))
                            shortest = w.value;
                    for (const LaneSpec& lane : style.lanes)
                    {
                        if (lane.kind == LaneKind::drums)
                            continue;
                        bool without = false;
                        if (!plan.choices.empty())
                            without = std::any_of (plan.choices.begin(), plan.choices.end(), [&] (const BlockChoice& c) { return c.weight > 0 && (c.blocks.empty() || !contains (c.blocks.back(), lane.id)); });
                        else
                        {
                            without = !contains (plan.lanes, lane.id) || (slot.kind == "drop" && layered (lane.id));
                            for (const LaneEntry& e : plan.entries)
                                if (contains (e.lanes, lane.id))
                                    without = without || std::any_of (e.bars.begin(), e.bars.end(), [&] (const Weighted& w) {
                                        return w.weight > 0 && ((e.entry == Entry::before && w.value > 0) || (e.entry == Entry::after && w.value >= shortest));
                                    });
                            if (plan.rest)
                                without = without || std::any_of (plan.rest->lanes.begin(), plan.rest->lanes.end(), [&] (const LaneWeight& lw) { return lw.weight > 0 && lw.lane == lane.id; });
                        }
                        if (without)
                            endless.insert (lane.id);
                    }
                    if (!optionalSlot (i))
                        break;
                }
                const bool fits = std::any_of (outro->choices.begin(), outro->choices.end(), [&] (const BlockChoice& c) {
                    return c.weight > 0 && !c.blocks.empty() && std::none_of (c.blocks.front().begin(), c.blocks.front().end(), [&] (const std::string& id) { return endless.contains (id); });
                });
                if (!fits)
                {
                    std::string named;
                    for (const auto& id : endless)
                        named += std::string (named.empty() ? "" : ", ") + "\"" + id + "\"";
                    plans["outro"]["choices"].fail ("lanes only leave from the section before the mix-out on, but every mix-out choice starts on a lane that section can end without ("
                                                    + named + ")");
                    return;
                }
            }

            // a kind with "change" can always make one: a rest or a dropout fits every length it takes
            // (a drum lane rests inside the section, never through its last block: arrange.h)
            for (std::size_t i = 0; i < form.slots.size(); ++i)
            {
                const KindPlan& plan = *form.plan (form.slots[i].kind);
                if (!plan.change)
                    continue;
                for (std::size_t k = 0; k < form.slots[i].bars.size(); ++k)
                {
                    const std::int32_t bars = form.slots[i].bars[k].value;
                    if (bars == 0 || form.slots[i].bars[k].weight == 0)
                        continue;
                    const bool rest = plan.rest && std::any_of (plan.rest->lanes.begin(), plan.rest->lanes.end(), [&] (const LaneWeight& lw) {
                        return lw.weight > 0 && (isDrumLane (style, lw.lane) ? 2 * plan.rest->bars < bars : 2 * plan.rest->bars <= bars);
                    });
                    const bool dropout = plan.dropout && plan.dropout->every < bars;
                    if (!rest && !dropout)
                        sections.at (i)["bars"].at (k)["bars"].fail ("a " + plan.kind + " has a lane change inside it, but a " + std::to_string (bars)
                                                                     + "-bar one has no room for a rest or a dropout");
                }
            }

            // the lines: every combination of drop and break starts on and off 16-bar lines that the
            // form can draw must fit its length range
            std::vector<std::size_t> landmarks;
            for (std::size_t i = 0; i < form.slots.size(); ++i)
                if (startsOnLine (form.slots[i].kind))
                    landmarks.push_back (i);
            if (landmarks.size() > 12)
            {
                sections.fail ("at most 12 drop, drop2 and break sections");
                return;
            }
            const bool on = form.sixteen > 0, off = form.sixteen < 100;
            for (std::uint32_t combo = 0; combo < (std::uint32_t (1) << landmarks.size()); ++combo)
            {
                std::vector<std::int8_t> lines (form.slots.size(), -1);
                bool drawable = true;
                std::string named;
                for (std::size_t b = 0; b < landmarks.size(); ++b)
                {
                    const bool sixteen = ((combo >> b) & 1u) != 0;
                    drawable = drawable && (sixteen ? on : off);
                    lines[landmarks[b]] = sixteen ? 1 : 0;
                    named += std::string (named.empty() ? "" : ", ") + form.slots[landmarks[b]].kind + " (form.sections[" + std::to_string (landmarks[b]) + "]) "
                             + (sixteen ? "on a 16-bar line" : "on 8 mod 16");
                }
                if (drawable && !formFits (form, lines))
                {
                    sections.fail ("no lengths fit " + std::to_string (form.bars.lo) + " to " + std::to_string (form.bars.hi) + " bars with " + named);
                    return;
                }
            }
        }

        // ------------------------------------------------------------------ the hook (hook.h)

        bool containsInt (const std::vector<std::int32_t>& v, std::int32_t x) { return std::find (v.begin(), v.end(), x) != v.end(); }

        std::string_view laneKindName (LaneKind kind)
        {
            switch (kind)
            {
                case LaneKind::drums:
                    return "drums";
                case LaneKind::bass:
                    return "bass";
                case LaneKind::chords:
                    return "chords";
                case LaneKind::arp:
                    return "arp";
                case LaneKind::lead:
                    return "lead";
            }
            return "?";
        }

        std::optional<HookOp> hookOpNamed (std::string_view name)
        {
            static constexpr std::array<std::pair<std::string_view, HookOp>, 12> ops { {
                { "out", HookOp::out },
                { "repeat", HookOp::repeat },
                { "full", HookOp::full },
                { "fragment", HookOp::fragment },
                { "skeleton", HookOp::skeleton },
                { "answer", HookOp::answer },
                { "augment", HookOp::augment },
                { "vary", HookOp::vary },
                { "cadence", HookOp::cadence },
                { "breath", HookOp::breath },
                { "new", HookOp::newPhrase },
                { "sequence", HookOp::sequence },
            } };
            for (const auto& [n, op] : ops)
                if (n == name)
                    return op;
            return std::nullopt;
        }

        // A bar-1 rhythm ("cells", "period.cells"): onsets on 16ths 0-15 in ascending order, a length of
        // 1-16 16ths per onset, accents among the onsets, an onset in each half of the bar, and an onset
        // that isn't an accent, so the skeleton (the accents alone) differs from the riff in every bar.
        // A pedal style's cells start on 16th 0 instead and may be all accents: there FULL differs from
        // SKELETON by the pedal, which never plays before a bar's first top note.
        HookCell hookCell (const Node& c, bool pedal)
        {
            c.only ({ "on", "len", "acc", "w" });
            HookCell cell;
            cell.on = c["on"].ints (1, 16, 0, 15);
            for (std::size_t i = 1; i < cell.on.size(); ++i)
                if (cell.on[i] <= cell.on[i - 1])
                {
                    c["on"].fail ("the onsets are 16ths in ascending order, each once: " + std::to_string (cell.on[i]) + " comes after " + std::to_string (cell.on[i - 1]));
                    break;
                }
            const Node len = c["len"];
            if (len.isArray() && len.size() != cell.on.size())
                len.fail ("one length per onset: " + std::to_string (cell.on.size()) + " onsets, " + std::to_string (len.size()) + " lengths");
            else
                cell.len = len.ints (cell.on.size(), cell.on.size(), 1, 16);
            cell.acc = c["acc"].ints (0, 16, 0, 15);
            for (const auto a : cell.acc)
                if (!containsInt (cell.on, a))
                {
                    c["acc"].fail ("the accent on 16th " + std::to_string (a) + " isn't one of the onsets");
                    break;
                }
            cell.weight = c["w"].int32 (0, 100);
            if (cell.on.empty())
                return cell;
            const bool firstHalf = std::any_of (cell.on.begin(), cell.on.end(), [] (std::int32_t s) { return s < 8; });
            const bool secondHalf = std::any_of (cell.on.begin(), cell.on.end(), [] (std::int32_t s) { return s >= 8; });
            if (!firstHalf || !secondHalf)
                c.fail (std::string ("each half of the bar holds an onset, but 16ths ") + (firstHalf ? "8-15" : "0-7") + " hold none");
            else if (pedal && cell.on.front() != 0)
                c.fail ("a pedal style's cells start on 16th 0, as the written arp does (the pedal never plays before a bar's first top note), not on 16th "
                        + std::to_string (cell.on.front()));
            else if (!pedal && std::all_of (cell.on.begin(), cell.on.end(), [&] (std::int32_t s) { return containsInt (cell.acc, s); }))
                c.fail ("every onset is an accent: a cell needs one that isn't, so the skeleton (the accents alone) differs from the riff in every bar");
            return cell;
        }

        std::vector<HookCell> hookCells (const Node& n, bool pedal)
        {
            std::vector<HookCell> out;
            const std::size_t count = n.size();
            if (n.isArray() && count == 0)
                n.fail ("at least one cell is needed");
            std::int64_t total = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                out.push_back (hookCell (n.at (i), pedal));
                total += out.back().weight;
            }
            if (count > 0 && total == 0)
                n.fail ("every weight is 0: no cell could be picked");
            return out;
        }

        // A menu's ops by weight: [ { "op": name, "w": 0-100 } ]. [] is allowed (an empty "next" repeats,
        // an empty "last" plays "next"); a list that holds entries can draw one.
        std::vector<HookOpWeight> hookOps (const Node& n)
        {
            std::vector<HookOpWeight> out;
            const std::size_t count = n.size();
            std::int64_t total = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                const Node e = n.at (i);
                e.only ({ "op", "w" });
                const std::string name = e["op"].string();
                const auto op = hookOpNamed (name);
                if (!op)
                    e["op"].fail ("unknown op \"" + name + "\" (out, repeat, full, fragment, skeleton, answer, augment, vary, cadence, breath, new or sequence)");
                HookOpWeight w;
                w.op = op.value_or (HookOp::out);
                w.weight = e["w"].int32 (0, 100);
                total += w.weight;
                out.push_back (w);
            }
            if (count > 0 && total == 0)
                n.fail ("every weight is 0: no op could be picked");
            return out;
        }

        bool isDropKind (std::string_view kind) { return kind == "drop" || kind == "drop2"; }

        // One kind's line plan: { "enter" (a groove's), "first", "next", "last", "holdFull" (drop 1's),
        // "cover" (a drop's and drop 2's) }, or an after build's { "continue": 1 }.
        HookMenu hookMenu (const Node& m, const std::string& kind, bool after, const std::vector<std::string>& withhold)
        {
            m.only ({ "enter", "first", "next", "last", "holdFull", "cover", "continue" });
            HookMenu menu;
            menu.kind = kind;
            menu.after = after;
            if (const Node c = m.optional ("continue"); c.exists())
            {
                (void) c.int32 (1, 1);
                menu.carry = true;
                if (!after || kind != "build")
                    c.fail ("\"continue\" is an after build's: from the first drop on, a build keeps the section before's last state (\"plan\": { \"after\": { \"build\": { \"continue\": 1 } } }); a "
                            + kind + (after ? "" : " before it") + " draws its own lines");
                else if (m.optional ("enter").exists() || m.optional ("first").exists() || m.optional ("next").exists() || m.optional ("last").exists())
                    m.fail ("a build that continues the section before draws no ops: it has no \"enter\", \"first\", \"next\" or \"last\"");
                return menu;
            }
            if (const Node e = m.optional ("enter"); e.exists())
            {
                if (kind != "groove")
                    e.fail ("only a groove has leading OUT lines (\"enter\"), not a " + kind);
                menu.enter = weightedOf (e, "lines", 0, 32);
            }
            const Node first = m["first"];
            if (first.isArray() && first.size() == 0)
                first.fail ("a section's first playing line draws an op from it: at least one is needed");
            menu.first = hookOps (first);
            menu.next = hookOps (m.optional ("next"));
            menu.last = hookOps (m.optional ("last"));
            for (std::size_t i = 0; i < menu.first.size(); ++i)
            {
                const HookOp op = menu.first[i].op;
                if (op == HookOp::vary || op == HookOp::cadence || op == HookOp::breath)
                    first.at (i)["op"].fail ("VARY, CADENCE and BREATH end the line before, and a section's first playing line has none: not in a \"first\" menu");
                else if (kind == "drop2" && op != HookOp::newPhrase)
                    first.at (i)["op"].fail ("drop 2 opens on its new phrase: its \"first\" menu is \"new\" only");
            }
            if (!after && contains (withhold, kind))
                for (const auto& [name, list] : { std::pair { "first", &menu.first }, std::pair { "next", &menu.next }, std::pair { "last", &menu.last } })
                    for (std::size_t i = 0; i < list->size(); ++i)
                        if ((*list)[i].op == HookOp::full || (*list)[i].op == HookOp::newPhrase)
                            m[name].at (i)["op"].fail ("the " + kind + " is withheld (\"withhold\"): before the first drop it plays no FULL or NEW line, so not in its menu");
            if (const Node h = m.optional ("holdFull"); h.exists())
            {
                menu.holdFull = h.int32 (0, 1) == 1;
                if (kind != "drop")
                    h.fail ("drop 1 holds a FULL line (\"holdFull\"), not a " + kind);
            }
            if (isDropKind (kind))
            {
                const Node cover = m["cover"];
                menu.cover = cover.int32 (0, 1000);
                if (cover.exists() && menu.cover < 500)
                    cover.fail ("a " + kind + " plays the hook in half its lines or more: a cover of 500 permille at least, not " + std::to_string (menu.cover));
            }
            else if (const Node cover = m.optional ("cover"); cover.exists())
                cover.fail ("a drop's and drop 2's menus have a cover, not a " + kind + "'s");
            return menu;
        }

        // The line plan against the drawn forms (plan §5.1 at every length the form can draw): a menu
        // for each kind the carrier's plans play it in (a groove's, break's or build's before the first
        // drop, and its after menu from then on) and for no other; a cover every drawable drop length
        // can reach with a line to spare; a section before the first drop that always plants the hook.
        void hookAgainstForm (const Node& plan, const Style& style, const HookSpec& hook)
        {
            if (plan.errors().any() || hook.lane.empty())
                return;
            const FormSpec& form = style.form;
            std::size_t firstDrop = form.slots.size();
            for (std::size_t i = 0; i < form.slots.size(); ++i)
                if (form.slots[i].kind == "drop")
                {
                    firstDrop = i;
                    break;
                }
            for (const HookMenu& m : hook.menus)
            {
                const Node at = m.after ? plan["after"][m.kind] : plan[m.kind];
                if (!form.mayPlay (m.kind, hook.lane))
                {
                    at.fail ("the form's " + m.kind + " plan doesn't play the hook's carrier \"" + hook.lane + "\": no menu for it");
                    return;
                }
                bool used = false;
                for (std::size_t i = 0; i < form.slots.size(); ++i)
                    used = used || (form.slots[i].kind == m.kind && (isDropKind (m.kind) || (m.after ? i > firstDrop : i < firstDrop)));
                if (!used)
                {
                    at.fail ("no " + m.kind + " of the form comes " + (m.after ? "after" : "before") + " the first drop, so no section plays this menu");
                    return;
                }
            }
            for (std::size_t i = 0; i < form.slots.size(); ++i)
            {
                const std::string& kind = form.slots[i].kind;
                if (!form.mayPlay (kind, hook.lane))
                    continue;
                const bool after = !isDropKind (kind) && i > firstDrop;
                if (hook.menu (kind, after) == nullptr)
                {
                    plan.fail ("the form plays the hook's carrier \"" + hook.lane + "\" in its " + kind + " (form.sections[" + std::to_string (i) + "]"
                               + (isDropKind (kind) ? "" : after ? ", after the first drop"
                                                                 : ", before the first drop")
                               + "), but the plan has no menu for that "
                               + (after ? "\"after\" " : "") + kind);
                    return;
                }
            }
            // a cover each drawable length can reach with a line to spare (drop 1's first line may rest:
            // r2): cover x lines <= (lines - 1) x 1000
            for (const HookMenu& m : hook.menus)
                if (isDropKind (m.kind))
                    for (const FormSlot& slot : form.slots)
                        if (slot.kind == m.kind)
                            for (const Weighted& w : slot.bars)
                            {
                                const std::int64_t lines = w.value / 8;
                                if (w.weight > 0 && lines > 0 && m.cover * lines > (lines - 1) * 1000)
                                {
                                    plan[m.kind]["cover"].fail ("a " + std::to_string (w.value) + "-bar " + m.kind + " has " + std::to_string (lines) + " lines: a cover of "
                                                                + std::to_string (m.cover) + " permille needs " + std::to_string (util::ceilDiv (m.cover * lines, std::int64_t (1000)))
                                                                + " of them to play, and with one to spare (drop 1's first line may rest) at most " + std::to_string (lines - 1) + " can");
                                    return;
                                }
                            }
            // a section before the first drop always plants the hook: a groove that plays the carrier all
            // through (no entry or rest takes it out) and whose "first" menu holds no OUT (its "enter" is
            // clamped, so its first playing line draws from "first"), or such a build whose menus hold none
            const auto drawsOut = [] (const std::vector<HookOpWeight>& ops) {
                return std::any_of (ops.begin(), ops.end(), [] (const HookOpWeight& o) { return o.op == HookOp::out && o.weight > 0; });
            };
            bool plants = false;
            for (std::size_t i = 0; i < firstDrop && i < form.slots.size() && !plants; ++i)
            {
                const FormSlot& slot = form.slots[i];
                if (slot.kind != "groove" && slot.kind != "build")
                    continue;
                if (std::any_of (slot.bars.begin(), slot.bars.end(), [] (const Weighted& w) { return w.value == 0 && w.weight > 0; }))
                    continue; // it may be left out
                const KindPlan* kp = form.plan (slot.kind);
                if (kp == nullptr || !contains (kp->lanes, hook.lane))
                    continue;
                if (std::any_of (kp->entries.begin(), kp->entries.end(), [&] (const LaneEntry& e) { return contains (e.lanes, hook.lane); }))
                    continue;
                if (kp->rest && std::any_of (kp->rest->lanes.begin(), kp->rest->lanes.end(), [&] (const LaneWeight& lw) { return lw.weight > 0 && lw.lane == hook.lane; }))
                    continue;
                const HookMenu* m = hook.menu (slot.kind, false);
                if (m == nullptr || m->carry)
                    continue;
                plants = slot.kind == "groove" ? !drawsOut (m->first) : !drawsOut (m->first) && !drawsOut (m->next) && !drawsOut (m->last);
            }
            if (!plants)
                plan.fail ("no section before the first drop always plants the hook: a groove or build the form never leaves out, that plays \"" + hook.lane
                           + "\" all through (no entry or rest), whose menu draws no OUT for its first playing line (a groove's \"first\") or for any line (a build's)");
        }

        // The hook block (hook.h; content.h's HookSpec has the keys), read after the lanes, the form
        // and the harmony, which it names. makeBundle checks it against the carrier's kit range.
        std::optional<HookSpec> hookBlock (const Node& h, const Style& style)
        {
            if (!h.exists())
                return std::nullopt;
            h.only ({ "lane", "register", "contours", "cells", "mutate", "answer", "pitch", "vel", "guard", "period", "pedal", "withhold", "plan" });
            HookSpec hook;
            const bool pedal = h.optional ("pedal").exists();

            // the carrier: one lead lane of the style
            const Node lane = h.optional ("lane");
            if (!lane.exists()) // a missing key has no line of its own: the block's
                h.errors().add (lane.path(), "line " + std::to_string (h.value()->line) + ": missing \"lane\": the hook needs its carrier, a lead lane of the style");
            else
            {
                hook.lane = lane.string (1, 32);
                const auto li = style.laneIndex (hook.lane);
                if (!li)
                    lane.fail ("unknown lane \"" + hook.lane + "\"");
                else if (style.lanes[*li].kind != LaneKind::lead)
                    lane.fail ("the hook plays on a lead lane, and \"" + hook.lane + "\" is a " + std::string (laneKindName (style.lanes[*li].kind)) + " lane");
            }

            const Node reg = h["register"];
            reg.only ({ "center", "span", "floor" });
            hook.reg.center = reg["center"].int32 (0, 127);
            hook.reg.span = reg["span"].int32 (1, 127);
            hook.reg.floor = reg.int32Or ("floor", -1, 0, 127);
            hook.reg.line = reg.exists() ? reg.value()->line : 0;

            const Node contours = h["contours"];
            if (contours.isArray() && contours.size() == 0)
                contours.fail ("at least one contour is needed");
            std::int64_t contourWeight = 0;
            for (std::size_t i = 0; i < contours.size(); ++i)
            {
                const Node c = contours.at (i);
                c.only ({ "shape", "w" });
                const std::string shape = c["shape"].string();
                HookContour contour = HookContour::late;
                if (shape == "early")
                    contour = HookContour::early;
                else if (shape == "twin")
                    contour = HookContour::twin;
                else if (shape != "late")
                    c["shape"].fail ("a contour is late, early or twin, not \"" + shape + "\"");
                const std::int32_t w = c["w"].int32 (0, 100);
                contourWeight += w;
                hook.contours.emplace_back (contour, w);
            }
            if (contours.isArray() && contours.size() > 0 && contourWeight == 0)
                contours.fail ("every weight is 0: no contour could be picked");

            hook.cells = hookCells (h["cells"], pedal);

            const Node mutate = h["mutate"];
            mutate.only ({ "drop", "add", "extra" });
            hook.mutate.drop = mutate["drop"].int32 (0, 100);
            hook.mutate.add = mutate["add"].int32 (0, 100);
            hook.mutate.extra = mutate["extra"].ints (0, 16, 0, 15);
            if (pedal && containsInt (hook.mutate.extra, 15))
                mutate["extra"].fail ("in a pedal style no onset falls on 16th 15 (the note before it holds through it): no extra onset there");

            const Node answer = h["answer"];
            answer.only ({ "same", "move", "add", "drop", "headDiffers" });
            hook.answer.same = answer["same"].int32 (0, 100);
            hook.answer.move = answer["move"].int32 (0, 100);
            hook.answer.add = answer["add"].int32 (0, 100);
            hook.answer.drop = answer["drop"].int32 (0, 100);
            hook.answer.headDiffers = answer["headDiffers"].int32 (0, 1) == 1;
            if (answer.exists() && hook.answer.same + hook.answer.move + hook.answer.add + hook.answer.drop == 0)
                answer.fail ("every weight is 0: bar 2's edit (same, move, add or drop) could not be picked");

            const Node pitch = h["pitch"];
            pitch.only ({ "pool", "start", "end", "moves", "leap", "strong", "peak", "early", "tones", "clash" });
            HookPitch& p = hook.pitch;
            p.pool = pitch["pool"].ints (1, 7, 0, 6);
            for (std::size_t i = 0; i < p.pool.size(); ++i)
                if (std::count (p.pool.begin(), p.pool.end(), p.pool[i]) > 1)
                {
                    pitch["pool"].fail ("the degree " + std::to_string (p.pool[i]) + " is listed twice");
                    break;
                }
            for (const auto& [key, out] : { std::pair { "start", &p.start }, std::pair { "end", &p.end } })
            {
                const Node n = pitch[key];
                const auto w = n.ints (7, 7, 0, 100);
                std::int32_t total = 0;
                for (std::size_t d = 0; d < w.size(); ++d)
                {
                    (*out)[d] = w[d];
                    total += w[d];
                }
                if (w.size() == 7 && total == 0)
                    n.fail (std::string ("every weight is 0: no ") + key + " degree could be picked");
            }
            for (std::size_t d = 0; d < 7 && !p.pool.empty(); ++d)
                if ((p.start[d] > 0 || p.end[d] > 0) && !containsInt (p.pool, static_cast<std::int32_t> (d)))
                {
                    pitch["pool"].fail ("the start or end weights can draw degree " + std::to_string (d) + ", which the pool doesn't hold");
                    break;
                }
            const auto moves = pitch["moves"].ints (8, 8, 0, 100);
            for (std::size_t k = 0; k < moves.size(); ++k)
                p.moves[k] = moves[k];
            p.leap = pitch["leap"].int32 (1, 12);
            const std::string strong = pitch["strong"].string();
            if (strong == "beats")
                p.beats = true;
            else if (pitch["strong"].exists() && strong != "accents")
                pitch["strong"].fail ("a strong onset is a beat or an accent (\"accents\") or a beat alone (\"beats\"), not \"" + strong + "\"");
            p.peak = rangeOf (pitch["peak"], 0, 31);
            p.early = rangeOf (pitch["early"], 0, 31);
            p.tones = pitch["tones"].ints (1, 9, 0, 8);
            p.clash = pitch["clash"].ints (0, 9, 0, 8);

            const Node vel = h["vel"];
            vel.only ({ "down", "beat", "accent", "other", "peak", "shape" });
            hook.vel.down = vel.int32Or ("down", -1, 1, 127);
            hook.vel.beat = vel.int32Or ("beat", -1, 1, 127);
            hook.vel.accent = vel["accent"].int32 (1, 127);
            hook.vel.other = vel["other"].int32 (1, 127);
            hook.vel.peak = vel["peak"].int32 (0, 20);
            hook.vel.shape = vel["shape"].int32 (0, 12);

            const Node guard = h["guard"];
            guard.only ({ "second" });
            hook.guard = guard["second"].int32 (0, 127);

            const Node period = h["period"];
            HookPeriod& per = hook.period;
            const std::string idiom = period["idiom"].string();
            if (idiom == "organ")
            {
                per.idiom = HookIdiom::organ;
                period.only ({ "idiom", "halves", "octave", "cadence", "register" });
            }
            else if (idiom == "glass")
            {
                per.idiom = HookIdiom::glass;
                period.only ({ "idiom", "halves", "octave", "register" });
            }
            else if (idiom == "stab")
            {
                per.idiom = HookIdiom::stab;
                period.only ({ "idiom", "cells", "octave", "register" });
            }
            else if (period["idiom"].exists())
                period["idiom"].fail ("the new phrase's idiom is organ, glass or stab, not \"" + idiom + "\"");
            per.octave = period.int32Or ("octave", 0, -1, 1);
            if (idiom == "organ" || idiom == "glass")
            {
                const Node halves = period["halves"];
                if (halves.isArray() && halves.size() == 0)
                    halves.fail ("at least one second half is needed");
                std::int64_t total = 0;
                for (std::size_t i = 0; i < halves.size(); ++i)
                {
                    const Node hv = halves.at (i);
                    hv.only ({ "on", "w" });
                    auto on = hv["on"].ints (1, 8, 8, 15);
                    for (std::size_t k = 1; k < on.size(); ++k)
                        if (on[k] <= on[k - 1])
                        {
                            hv["on"].fail ("the onsets are 16ths of the second half (8-15) in ascending order, each once: " + std::to_string (on[k]) + " comes after " + std::to_string (on[k - 1]));
                            break;
                        }
                    const std::int32_t w = hv["w"].int32 (0, 100);
                    total += w;
                    per.halves.emplace_back (std::move (on), w);
                }
                if (halves.isArray() && halves.size() > 0 && total == 0)
                    halves.fail ("every weight is 0: no second half could be picked");
            }
            if (idiom == "organ")
            {
                const Node cadence = period["cadence"];
                const auto w = cadence.ints (7, 7, 0, 100);
                std::int32_t total = 0;
                for (std::size_t d = 0; d < w.size(); ++d)
                {
                    per.cadence[d] = w[d];
                    total += w[d];
                }
                if (w.size() == 7 && w[0] > 0)
                    cadence.fail ("the organ's fall ends on 5, 2 or b3, never on the tonic: the tonic's weight (the first) is 0, not " + std::to_string (w[0]));
                else if (w.size() == 7 && total == 0)
                    cadence.fail ("every weight is 0: the fall's end could not be picked");
            }
            if (idiom == "stab")
                per.cells = hookCells (period["cells"], pedal);
            if (const Node r = period.optional ("register"); r.exists())
            {
                per.range = rangeOf (r, 0, 127);
                per.rangeLine = r.value()->line;
            }

            if (const Node ped = h.optional ("pedal"); ped.exists())
            {
                ped.only ({ "degrees", "vel", "in", "max" });
                HookPedal pd;
                pd.degrees = ped["degrees"].ints (1, 7, -7, 7);
                const Node pv = ped["vel"];
                if (pv.isArray() && pv.size() != pd.degrees.size())
                    pv.fail ("one velocity per pedal degree: " + std::to_string (pd.degrees.size()) + " degrees, " + std::to_string (pv.size()) + " velocities");
                else
                    pd.vel = pv.ints (pd.degrees.size(), pd.degrees.size(), 1, 127);
                const Node in = ped["in"];
                for (const auto& name : in.strings (1, 4))
                {
                    const auto op = hookOpNamed (name);
                    if (!op || (*op != HookOp::full && *op != HookOp::answer && *op != HookOp::fragment && *op != HookOp::newPhrase))
                    {
                        in.fail ("the pedal plays under full, answer, fragment and new only, not \"" + name + "\"");
                        break;
                    }
                    if (std::find (pd.in.begin(), pd.in.end(), *op) != pd.in.end())
                    {
                        in.fail ("\"" + name + "\" is listed twice");
                        break;
                    }
                    pd.in.push_back (*op);
                }
                pd.max = ped["max"].int32 (1, 2);
                hook.pedal = std::move (pd);
            }

            const Node withhold = h["withhold"];
            hook.withhold = withhold.strings (0, 7);
            for (std::size_t i = 0; i < hook.withhold.size(); ++i)
            {
                if (!knownKind (hook.withhold[i]))
                {
                    withhold.fail ("unknown section kind \"" + hook.withhold[i] + "\"");
                    break;
                }
                if (std::count (hook.withhold.begin(), hook.withhold.end(), hook.withhold[i]) > 1)
                {
                    withhold.fail ("\"" + hook.withhold[i] + "\" is listed twice");
                    break;
                }
            }

            const Node plan = h["plan"];
            for (const auto& [kind, m] : plan.members())
            {
                if (kind == "after")
                {
                    for (const auto& [k, am] : m.members())
                    {
                        if (!knownKind (k))
                            am.fail ("unknown section kind \"" + k + "\"");
                        else if (isDropKind (k))
                            am.fail ("a " + k + " plays its own menu wherever it comes: no \"after\" menu for it");
                        else
                            hook.menus.push_back (hookMenu (am, k, true, hook.withhold));
                    }
                    continue;
                }
                if (!knownKind (kind))
                {
                    m.fail ("unknown section kind \"" + kind + "\"");
                    continue;
                }
                hook.menus.push_back (hookMenu (m, kind, false, hook.withhold));
            }
            hookAgainstForm (plan, style, hook);
            return hook;
        }

        // ------------------------------------------------------------------ feel

        FeelLane feelLane (const Node& n)
        {
            n.only ({ "swing", "sd", "clamp", "anchor", "offset", "vel", "vsd", "dip", "lock", "after", "spread", "artic", "tight" });
            FeelLane f;
            f.swing = optionalRange (n, "swing", -250, 250);
            f.sd = optionalRange (n, "sd", 0, 200);
            if (n.optional ("clamp").exists())
                f.clamp = n["clamp"].int32 (0, 60);
            if (n.optional ("anchor").exists())
                f.anchor = n["anchor"].int32 (0, 1) == 1;
            if (n.optional ("offset").exists())
                f.offset = gridOf (n["offset"], -30, 30);
            if (n.optional ("vel").exists())
                f.vel = gridOf (n["vel"], -40, 40);
            f.vsd = optionalRange (n, "vsd", 0, 200);
            if (n.optional ("dip").exists())
            {
                const auto d = n["dip"].ints (2, 2, 0, 127);
                if (d.size() == 2)
                    f.dip = Range { d[0], d[1] };
            }
            if (n.optional ("lock").exists())
                f.lock = n["lock"].string (1, 32);
            const Node after = n.optional ("after");
            if (after.exists())
            {
                std::vector<AfterRule> rules;
                for (std::size_t i = 0; i < after.size(); ++i)
                {
                    const Node a = after.at (i);
                    a.only ({ "voice", "after", "by" });
                    rules.push_back ({ a["voice"].string (1, 32), a["after"].string (1, 32), rangeOf (a["by"], -30, 30) });
                }
                f.after = std::move (rules);
            }
            f.spread = optionalRange (n, "spread", 0, 12);
            if (n.optional ("artic").exists())
                f.artic = n["artic"].int32 (0, 500);
            if (n.optional ("tight").exists())
                f.tight = n["tight"].int32 (1, 127);
            return f;
        }
    }

    // ------------------------------------------------------------------ lookups

    const KitLane* Kit::lane (std::string_view laneId) const noexcept
    {
        for (const auto& l : lanes)
            if (l.id == laneId)
                return &l;
        return nullptr;
    }

    std::string_view LaneSpec::gridFor (std::string_view sectionKind) const noexcept
    {
        for (const auto& [k, g] : use)
            if (k == sectionKind)
                return g;
        return {};
    }

    const ProgressionSlot* HarmonySpec::slot (std::string_view name) const noexcept
    {
        for (const auto& s : slots)
            if (s.name == name)
                return &s;
        return nullptr;
    }

    const VoicingSpec* HarmonySpec::voicing (std::string_view name) const noexcept
    {
        for (const auto& v : voicings)
            if (v.name == name)
                return &v;
        return nullptr;
    }

    std::string_view HarmonySpec::slotFor (std::string_view kind) const noexcept
    {
        for (const auto& [k, s] : use)
            if (k == kind)
                return s;
        return {};
    }

    const std::vector<std::string>* HarmonySpec::paletteFor (std::string_view kind) const noexcept
    {
        for (const auto& [k, list] : palette)
            if (k == kind)
                return &list;
        return nullptr;
    }

    const CcRow* CcSpec::row (std::string_view kind) const noexcept
    {
        for (const auto& r : rows)
            if (r.kind == kind)
                return &r;
        return nullptr;
    }

    const KindPlan* FormSpec::plan (std::string_view kind) const noexcept
    {
        for (const auto& p : plans)
            if (p.kind == kind)
                return &p;
        return nullptr;
    }

    bool FormSpec::mayPlay (std::string_view kind, std::string_view lane) const noexcept
    {
        const KindPlan* p = plan (kind);
        if (p == nullptr)
            return false;
        const auto has = [&] (const std::vector<std::string>& ids) { return std::find (ids.begin(), ids.end(), lane) != ids.end(); };
        if (has (p->lanes))
            return true;
        return std::any_of (p->choices.begin(), p->choices.end(), [&] (const BlockChoice& c) { return std::any_of (c.blocks.begin(), c.blocks.end(), has); });
    }

    const HookMenu* HookSpec::menu (std::string_view kind, bool afterFirstDrop) const noexcept
    {
        const bool drop = kind == "drop" || kind == "drop2";
        for (const auto& m : menus)
            if (m.kind == kind && m.after == (afterFirstDrop && !drop))
                return &m;
        return nullptr;
    }

    const SectionSpec* Style::section (std::string_view kind) const noexcept
    {
        for (const auto& s : sections)
            if (s.kind == kind)
                return &s;
        return nullptr;
    }

    std::optional<std::size_t> Style::laneIndex (std::string_view laneId) const noexcept
    {
        for (std::size_t i = 0; i < lanes.size(); ++i)
            if (lanes[i].id == laneId)
                return i;
        return std::nullopt;
    }

    FeelLane Feel::resolve (std::string_view laneId, std::string_view kind) const
    {
        FeelLane f;
        for (const auto& [name, lane] : lanes)
            if (name == laneId)
                f = lane;
        for (const auto& row : rows)
        {
            if (row.kind != kind)
                continue;
            for (const auto& [name, o] : row.lanes)
            {
                if (name != laneId)
                    continue;
                if (o.swing)
                    f.swing = o.swing;
                if (o.sd)
                    f.sd = o.sd;
                if (o.clamp)
                    f.clamp = o.clamp;
                if (o.anchor)
                    f.anchor = o.anchor;
                if (o.offset)
                    f.offset = o.offset;
                if (o.vel)
                    f.vel = o.vel;
                if (o.vsd)
                    f.vsd = o.vsd;
                if (o.dip)
                    f.dip = o.dip;
                if (o.lock)
                    f.lock = o.lock;
                if (o.after)
                    f.after = o.after;
                if (o.spread)
                    f.spread = o.spread;
                if (o.artic)
                    f.artic = o.artic;
                if (o.tight)
                    f.tight = o.tight;
            }
        }
        return f;
    }

    // ------------------------------------------------------------------ parsers

    util::Result<Kit> parseKit (std::string_view text, std::string_view source)
    {
        const auto parsed = json::parse (text);
        if (!parsed.ok())
            return util::Result<Kit>::failure (std::string (source) + ": " + parsed.error);
        json::Errors errors { std::string (source) };
        const Node root = json::root (parsed.value, errors);
        root.only ({ "format", "id", "name", "direction", "bpm", "key", "swing", "style", "feel", "lanes" });
        Kit kit;
        (void) root["format"].int32 (1, 1);
        kit.id = root["id"].string (1, 16);
        kit.name = root["name"].string (1, 64);
        kit.direction = root["direction"].string (1, 128);
        kit.bpm = root["bpm"].int32 (128, 140); // the content rule: every kit is 128-140 BPM
        const Node key = root["key"];
        key.only ({ "tonic", "mode", "name", "camelot" });
        kit.tonic = key["tonic"].int32 (0, 11);
        const std::string mode = key["mode"].string();
        if (mode != "minor")
            key["mode"].fail ("the generator writes minor keys only, not \"" + mode + "\"");
        kit.minor = true;
        kit.keyName = key["name"].string();
        kit.camelot = key["camelot"].string();
        if (!errors.any() && kit.keyName != keyName (kit.tonic, kit.minor))
            key["name"].fail ("\"" + kit.keyName + "\" doesn't match the tonic and mode, which make \"" + keyName (kit.tonic, kit.minor) + "\"");
        if (!errors.any() && kit.camelot != camelot (kit.tonic, kit.minor))
            key["camelot"].fail ("\"" + kit.camelot + "\" doesn't match the key, whose Camelot code is " + camelot (kit.tonic, kit.minor));
        kit.swing = root["swing"].int32 (500, 750);
        kit.style = root["style"].string (1, 64);
        kit.feel = root["feel"].string (1, 64);
        for (const auto& [id, ln] : root["lanes"].members())
        {
            ln.only ({ "track", "channel", "notes", "range", "root" });
            KitLane lane;
            lane.id = id;
            lane.track = ln["track"].string (1, 64);
            lane.channel = static_cast<std::uint8_t> (ln["channel"].int32 (0, 15));
            for (const auto& [voice, note] : ln.optional ("notes").members())
                lane.notes.push_back ({ voice, static_cast<std::uint8_t> (note.int32 (35, 59)) }); // plan §2.6: drums and perc stay in 35-59
            if (const Node range = ln.optional ("range"); range.exists())
            {
                lane.range = rangeOf (range, 0, 127);
                lane.rangeLine = range.value()->line;
            }
            if (ln.optional ("root").exists())
            {
                lane.root = ln["root"].int32 (0, 127);
                if (lane.root < lane.range.lo || lane.root > lane.range.hi)
                    ln["root"].fail ("the root " + std::to_string (lane.root) + " is outside the range " + std::to_string (lane.range.lo) + " to "
                                     + std::to_string (lane.range.hi));
                else if (pitchClass (lane.root) != kit.tonic)
                    ln["root"].fail ("the root " + std::to_string (lane.root) + " isn't the key's tonic");
            }
            kit.lanes.push_back (std::move (lane));
        }
        return finish (errors, std::move (kit));
    }

    util::Result<Style> parseStyle (std::string_view text, std::string_view source)
    {
        const auto parsed = json::parse (text);
        if (!parsed.ok())
            return util::Result<Style>::failure (std::string (source) + ": " + parsed.error);
        json::Errors errors { std::string (source) };
        const Node root = json::root (parsed.value, errors);
        root.only ({ "format", "id", "about", "sections", "form", "changes", "attempts", "harmony", "lanes", "hook" });
        Style style;
        (void) root["format"].int32 (1, 1);
        style.id = root["id"].string (1, 64);
        style.about = root.optional ("about").exists() ? root["about"].string() : std::string();
        style.attempts = root["attempts"].int32 (1, 64);

        // lanes first: everything else names them
        const Node lanes = root["lanes"];
        for (std::size_t i = 0; i < lanes.size(); ++i)
        {
            const Node ln = lanes.at (i);
            LaneSpec lane;
            lane.id = ln["id"].string (1, 32);
            const std::string kind = ln["kind"].string();
            if (kind == "drums")
                lane.kind = LaneKind::drums;
            else if (kind == "bass")
                lane.kind = LaneKind::bass;
            else if (kind == "chords")
                lane.kind = LaneKind::chords;
            else if (kind == "arp")
                lane.kind = LaneKind::arp;
            else if (kind == "lead")
                lane.kind = LaneKind::lead;
            else
                ln["kind"].fail ("a lane's kind is drums, bass, chords, arp or lead, not \"" + kind + "\"");
            if (style.laneIndex (lane.id))
                ln["id"].fail ("the lane \"" + lane.id + "\" is listed twice");
            lane.use = kindMap (ln["use"]);
            switch (lane.kind)
            {
                case LaneKind::drums:
                    drumLane (ln, lane);
                    break;
                case LaneKind::bass:
                    bassLane (ln, lane);
                    break;
                case LaneKind::chords:
                    chordLane (ln, lane);
                    break;
                case LaneKind::arp:
                    arpLane (ln, lane);
                    break;
                case LaneKind::lead:
                    leadLane (ln, lane);
                    break;
            }
            lane.cc = ccSpecs (ln.optional ("cc"));
            for (const auto& [k, g] : lane.use)
                if (!laneHasGrid (lane, g))
                    ln["use"].fail ("the section kind \"" + k + "\" uses the grid \"" + g + "\", which isn't defined");
            style.lanes.push_back (std::move (lane));
        }
        if (style.lanes.size() > 32)
            lanes.fail ("at most 32 lanes");
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
        {
            const LaneSpec& lane = style.lanes[i];
            if (lane.kind == LaneKind::bass && !lane.bass.follow.empty() && !isDrumLane (style, lane.bass.follow))
                lanes.at (i)["follow"].fail ("the bass follows \"" + lane.bass.follow + "\", which isn't a drum lane of this style");
        }

        for (const auto& [kind, s] : root["sections"].members())
        {
            s.only ({ "energy", "vel", "variant" });
            if (!knownKind (kind))
                s.fail ("unknown section kind \"" + kind + "\"");
            SectionSpec spec;
            spec.kind = kind;
            spec.energy = s["energy"].int32 (1, 4);
            spec.vel = s["vel"].int32 (500, 1200);
            spec.variant = s.int32Or ("variant", 0, 0, 15);
            style.sections.push_back (spec);
        }

        formOf (root["form"], style);
        // a lane's controllers need a row for every section kind the form plays the lane in (cc.h)
        for (std::size_t li = 0; li < style.lanes.size(); ++li)
            for (std::size_t ci = 0; ci < style.lanes[li].cc.size(); ++ci)
            {
                const LaneSpec& lane = style.lanes[li];
                for (const auto& plan : style.form.plans)
                {
                    if (style.form.mayPlay (plan.kind, lane.id) && lane.cc[ci].row (plan.kind) == nullptr)
                    {
                        lanes.at (li)["cc"].at (ci)["rows"].fail ("the form plays the lane \"" + lane.id + "\" in a " + plan.kind + " section, but controller "
                                                                  + std::to_string (lane.cc[ci].number) + " has no row for " + plan.kind);
                        break;
                    }
                }
            }

        const Node changes = root["changes"];
        changes.only ({ "turn", "change", "early", "roster" });
        style.changes.turn = changes["turn"].int32 (0, 100);
        style.changes.change = changes["change"].int32 (0, 100);
        style.changes.early = changes["early"].int32 (0, 100);
        style.changes.roster = changes["roster"].strings (0, 32);
        for (const auto& id : style.changes.roster)
            if (!style.laneIndex (id))
                changes["roster"].fail ("unknown lane \"" + id + "\"");

        harmony (root["harmony"], style.harmony);
        for (const auto& slot : style.form.slots)
        {
            if (style.harmony.slotFor (slot.kind).empty())
                root["harmony"]["use"].fail ("the form has a " + slot.kind + " section, but \"use\" gives it no slot");
            if (style.harmony.paletteFor (slot.kind) == nullptr)
                root["harmony"]["palette"].fail ("the form has a " + slot.kind + " section, but the palette has no voicings for it");
        }

        // the hook (hook.h): its block, and its carrier, the style's one lead lane
        style.hook = hookBlock (root.optional ("hook"), style);
        for (std::size_t i = 0; i < style.lanes.size(); ++i)
        {
            const LaneSpec& lane = style.lanes[i];
            if (lane.kind != LaneKind::lead)
                continue;
            if (!style.hook)
                lanes.at (i)["kind"].fail ("a lead lane carries the style's hook, but the style has no \"hook\" block");
            else if (!style.hook->lane.empty() && lane.id != style.hook->lane)
                lanes.at (i)["id"].fail ("\"" + lane.id + "\" is a second lead lane, but the hook has one carrier, \"" + style.hook->lane + "\"");
        }
        return finish (errors, std::move (style));
    }

    util::Result<Feel> parseFeel (std::string_view text, std::string_view source)
    {
        const auto parsed = json::parse (text);
        if (!parsed.ok())
            return util::Result<Feel>::failure (std::string (source) + ": " + parsed.error);
        json::Errors errors { std::string (source) };
        const Node root = json::root (parsed.value, errors);
        root.only ({ "format", "id", "about", "amount", "anchorClamp", "lanes", "rows" });
        Feel feel;
        (void) root["format"].int32 (1, 1);
        feel.id = root["id"].string (1, 64);
        feel.about = root.optional ("about").exists() ? root["about"].string() : std::string();
        feel.amount = root["amount"].int32 (0, 2000);
        feel.anchorClamp = root["anchorClamp"].int32 (0, 10);
        for (const auto& [id, ln] : root["lanes"].members())
            feel.lanes.emplace_back (id, feelLane (ln));
        for (const auto& [kind, row] : root["rows"].members())
        {
            if (!knownKind (kind))
            {
                row.fail ("unknown section kind \"" + kind + "\"");
                continue;
            }
            FeelRow r;
            r.kind = kind;
            for (const auto& [id, ln] : row.members())
                r.lanes.emplace_back (id, feelLane (ln));
            feel.rows.push_back (std::move (r));
        }
        for (const auto kind : sectionKinds)
            if (!errors.any() && std::none_of (feel.rows.begin(), feel.rows.end(), [&] (const FeelRow& r) { return r.kind == kind; }))
                root["rows"].fail ("missing the row for \"" + std::string (kind) + "\": every section kind has a feel row (it may be {})");
        return finish (errors, std::move (feel));
    }

    util::Result<Bundle> makeBundle (Kit kit, Style style, Feel feel)
    {
        using R = util::Result<Bundle>;
        const std::string where = "kit " + kit.id + " with style " + style.id + " and feel " + feel.id + ": ";
        if (kit.style != style.id)
            return R::failure (where + "the kit names the style \"" + kit.style + "\"");
        if (kit.feel != feel.id)
            return R::failure (where + "the kit names the feel \"" + kit.feel + "\"");
        for (const auto& lane : style.lanes)
        {
            const KitLane* kl = kit.lane (lane.id);
            if (kl == nullptr)
                return R::failure (where + "the kit has no lane \"" + lane.id + "\"");
            if (lane.kind == LaneKind::drums)
            {
                for (const auto& v : lane.drums.voices)
                    if (std::none_of (kl->notes.begin(), kl->notes.end(), [&] (const KitVoiceNote& n) { return n.voice == v.id; }))
                        return R::failure (where + "lanes." + lane.id + ".notes: no note for the voice \"" + v.id + "\"");
                for (const auto& n : kl->notes)
                    if (std::none_of (lane.drums.voices.begin(), lane.drums.voices.end(), [&] (const DrumVoice& v) { return v.id == n.voice; }))
                        return R::failure (where + "lanes." + lane.id + ".notes: \"" + n.voice + "\" isn't a voice of the lane");
                if (kl->channel != 9)
                    return R::failure (where + "lanes." + lane.id + ": drum lanes play on channel 10 (9 counted from 0)");
            }
            else
            {
                if (!kl->notes.empty())
                    return R::failure (where + "lanes." + lane.id + ": a pitched lane has a range, not drum notes");
                if (kl->range.hi - kl->range.lo < 12)
                    return R::failure (where + "lanes." + lane.id + ": the range must span an octave at least");
                if (lane.kind == LaneKind::bass && kl->root < 0)
                    return R::failure (where + "lanes." + lane.id + ": a bass lane needs its root");
                if (lane.kind == LaneKind::arp && (lane.arp.center < kl->range.lo || lane.arp.center > kl->range.hi))
                    return R::failure (where + "lanes." + lane.id + ": the arp's center " + std::to_string (lane.arp.center) + " is outside its range "
                                       + std::to_string (kl->range.lo) + " to " + std::to_string (kl->range.hi));
            }
        }
        for (const auto& kl : kit.lanes)
            if (!style.laneIndex (kl.id))
                return R::failure (where + "the kit's lane \"" + kl.id + "\" isn't a lane of the style");
        // the hook against its carrier's kit range: the range clear of the chords' low rubs (from the
        // style's mud.minorSecond up), and the register, its floor and the new phrase's register in it
        if (style.hook)
        {
            const HookSpec& hook = *style.hook;
            const KitLane* kl = kit.lane (hook.lane);
            if (kl == nullptr)
                return R::failure (where + "the kit has no lane \"" + hook.lane + "\", the hook's carrier");
            const Range r = kl->range;
            const std::string kitRange = std::to_string (r.lo) + " to " + std::to_string (r.hi);
            const auto at = [] (std::string_view file, const std::string& path, std::uint32_t line) {
                return "the " + std::string (file) + "'s " + path + (line > 0 ? ", line " + std::to_string (line) : std::string()) + ": ";
            };
            if (r.lo < style.harmony.mud.minorSecond)
                return R::failure (where + at ("kit", "lanes." + hook.lane + ".range", kl->rangeLine) + "the hook's carrier plays clear of the chords' low rubs, from the style's mud.minorSecond ("
                                   + std::to_string (style.harmony.mud.minorSecond) + ") up, but its range starts at " + std::to_string (r.lo));
            if (hook.reg.center < r.lo || hook.reg.center > r.hi)
                return R::failure (where + at ("style", "hook.register.center", hook.reg.line) + "the center " + std::to_string (hook.reg.center) + " is outside the carrier's kit range, " + kitRange);
            if (hook.reg.span > r.hi - r.lo)
                return R::failure (where + at ("style", "hook.register.span", hook.reg.line) + "a span of " + std::to_string (hook.reg.span) + " is wider than the carrier's kit range, " + kitRange);
            if (hook.reg.floor >= 0 && (hook.reg.floor < r.lo || hook.reg.floor > r.hi))
                return R::failure (where + at ("style", "hook.register.floor", hook.reg.line) + "the floor " + std::to_string (hook.reg.floor) + " is outside the carrier's kit range, " + kitRange);
            if (hook.period.range && (hook.period.range->lo < r.lo || hook.period.range->hi > r.hi))
                return R::failure (where + at ("style", "hook.period.register", hook.period.rangeLine) + "the new phrase's register, " + std::to_string (hook.period.range->lo) + " to "
                                   + std::to_string (hook.period.range->hi) + ", is outside the carrier's kit range, " + kitRange);
        }
        for (const auto& plan : style.form.plans)
            for (const auto& lane : style.lanes)
                if (style.form.mayPlay (plan.kind, lane.id) && lane.gridFor (plan.kind).empty())
                    return R::failure (where + "the form plays the lane \"" + lane.id + "\" in a " + plan.kind + " section, but the lane has no grid for " + plan.kind);
        // the feel
        for (const auto& lane : style.lanes)
            if (std::none_of (feel.lanes.begin(), feel.lanes.end(), [&] (const auto& p) { return p.first == lane.id; }))
                return R::failure (where + "the feel has no entry for the lane \"" + lane.id + "\"");
        const auto checkLane = [&] (const std::string& id, const FeelLane& f, const std::string& at) -> std::string {
            const auto li = style.laneIndex (id);
            if (!li)
                return at + ": unknown lane \"" + id + "\"";
            const LaneSpec& lane = style.lanes[*li];
            if (f.lock)
            {
                const auto target = style.laneIndex (*f.lock);
                if (!target || style.lanes[*target].kind != LaneKind::drums || *f.lock == id)
                    return at + ".lock: \"" + *f.lock + "\" isn't another drum lane";
            }
            if (f.after)
                for (const auto& a : *f.after)
                {
                    const auto isVoice = [&] (std::string_view v) {
                        return std::any_of (lane.drums.voices.begin(), lane.drums.voices.end(), [&] (const DrumVoice& x) { return x.id == v; });
                    };
                    if (lane.kind != LaneKind::drums || !isVoice (a.voice))
                        return at + ".after: unknown voice \"" + a.voice + "\"";
                    if (!isVoice (a.after) || a.after == a.voice)
                        return at + ".after: unknown voice \"" + a.after + "\"";
                }
            if (f.spread && lane.kind != LaneKind::chords)
                return at + ".spread: only chord lanes spread their stacks";
            return {};
        };
        for (const auto& [id, f] : feel.lanes)
            if (const auto e = checkLane (id, f, "feel lanes." + id); !e.empty())
                return R::failure (where + e);
        for (const auto& row : feel.rows)
            for (const auto& [id, f] : row.lanes)
            {
                if (const auto e = checkLane (id, f, "feel rows." + row.kind + "." + id); !e.empty())
                    return R::failure (where + e);
                // perform() orders the lanes by their locks and takes every target from the lane's
                // base, so the lock is the lane's, not a row's
                if (f.lock)
                    return R::failure (where + "feel rows." + row.kind + "." + id + ".lock: a lane's lock is set in feel lanes, not in a row");
            }
        // one level of locking: a lane locks to a lane that locks to nothing, so no chain or cycle
        // keeps a lane from being performed after its target
        for (const auto& [id, f] : feel.lanes)
            if (f.lock)
                for (const auto& [target, tf] : feel.lanes)
                    if (target == *f.lock && tf.lock)
                        return R::failure (where + "feel lanes." + id + ".lock: \"" + target + "\" locks to another lane itself");
        // a row only overrides, so a lane's base needs everything perform() reads in every section
        // kind (rows.drop alone would leave the other kinds without it)
        for (const auto& lane : style.lanes)
        {
            const FeelLane f = feel.resolve (lane.id, "");
            if (!f.swing || !f.sd || !f.clamp)
                return R::failure (where + "the feel's lane \"" + lane.id + "\" needs swing, sd and clamp");
        }
        return R::success (Bundle { std::move (kit), std::move (style), std::move (feel) });
    }

    util::Result<Bundle> loadBundle (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        using R = util::Result<Bundle>;
        const auto readText = [] (const std::filesystem::path& p) -> util::Result<std::string> {
            const auto bytes = util::readBinaryFile (p);
            if (!bytes.ok())
                return util::Result<std::string>::failure (bytes.error);
            return util::Result<std::string>::success (std::string (bytes.value.begin(), bytes.value.end()));
        };
        const auto shown = [] (const std::filesystem::path& p) {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        };
        const std::string file = lowered (kitId) + ".json";
        const auto kitPath = contentDir / "kits" / file;
        const auto kitText = readText (kitPath);
        if (!kitText.ok())
            return R::failure ("no kit \"" + std::string (kitId) + "\": " + kitText.error);
        auto kit = parseKit (kitText.value, shown (kitPath));
        if (!kit.ok())
            return R::failure (kit.error);
        if (lowered (kit.value.id) != lowered (kitId))
            return R::failure (shown (kitPath) + ": the kit's id is \"" + kit.value.id + "\", not \"" + std::string (kitId) + "\"");
        const auto stylePath = contentDir / "styles" / (kit.value.style + ".json");
        const auto styleText = readText (stylePath);
        if (!styleText.ok())
            return R::failure (styleText.error);
        auto style = parseStyle (styleText.value, shown (stylePath));
        if (!style.ok())
            return R::failure (style.error);
        const auto feelPath = contentDir / "feels" / (kit.value.feel + ".json");
        const auto feelText = readText (feelPath);
        if (!feelText.ok())
            return R::failure (feelText.error);
        auto feel = parseFeel (feelText.value, shown (feelPath));
        if (!feel.ok())
            return R::failure (feel.error);
        return makeBundle (std::move (kit.value), std::move (style.value), std::move (feel.value));
    }
}
