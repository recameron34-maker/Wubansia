#include "gen/content.h"

#include "gen/json.h"
#include "util/file_io.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

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
            mud.only ({ "below", "gap" });
            h.mudBelow = mud["below"].int32 (0, 127);
            h.mudGap = mud["gap"].int32 (0, 12);
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
            if (ln.optional ("range").exists())
                lane.range = rangeOf (ln["range"], 0, 127);
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
        root.only ({ "format", "id", "about", "sections", "form", "changes", "attempts", "harmony", "lanes" });
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
            else
                ln["kind"].fail ("a lane's kind is drums, bass, chords or arp, not \"" + kind + "\"");
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

        const Node form = root["form"];
        for (std::size_t i = 0; i < form.size(); ++i)
        {
            const Node f = form.at (i);
            f.only ({ "kind", "bars", "blocks" });
            FormSection sec;
            sec.kind = f["kind"].string();
            if (!knownKind (sec.kind))
                f["kind"].fail ("unknown section kind \"" + sec.kind + "\"");
            else if (style.section (sec.kind) == nullptr)
                f["kind"].fail ("the kind \"" + sec.kind + "\" has no entry in \"sections\"");
            sec.bars = f["bars"].int32 (8, 512);
            if (sec.bars % 8 != 0)
                f["bars"].fail ("a section is a multiple of 8 bars long (sections start on 8-bar lines), not " + std::to_string (sec.bars));
            const Node blocks = f["blocks"];
            const std::size_t want = static_cast<std::size_t> (sec.bars / 8);
            if (blocks.exists() && blocks.size() != want)
                blocks.fail ("it must hold " + std::to_string (want) + " blocks, one per 8 bars, not " + std::to_string (blocks.size()));
            for (std::size_t k = 0; k < blocks.size(); ++k)
            {
                auto active = blocks.at (k).strings (0, 32);
                for (const auto& id : active)
                    if (!style.laneIndex (id))
                        blocks.at (k).fail ("unknown lane \"" + id + "\"");
                for (std::size_t a = 0; a < active.size(); ++a)
                    for (std::size_t b = a + 1; b < active.size(); ++b)
                        if (active[a] == active[b])
                            blocks.at (k).fail ("the lane \"" + active[a] + "\" is listed twice");
                sec.blocks.push_back (std::move (active));
            }
            style.form.push_back (std::move (sec));
        }
        if (form.exists() && style.form.empty())
            form.fail ("the form needs at least one section");
        if (!errors.any() && !style.form.empty())
        {
            // A DJ-friendly extended mix: mixed in from drums only, mixed out to drums only.
            if (style.form.front().kind != "intro")
                form.at (0)["kind"].fail ("the form starts with an intro, the mix-in");
            else if (!style.form.front().blocks.empty())
                for (const auto& id : style.form.front().blocks.front())
                    if (!isDrumLane (style, id))
                        form.at (0)["blocks"].at (0).fail ("the mix-in starts with drums only, but its first block has \"" + id + "\"");
            if (style.form.back().kind != "outro")
                form.at (style.form.size() - 1)["kind"].fail ("the form ends with an outro, the mix-out");
            else if (!style.form.back().blocks.empty())
                for (const auto& id : style.form.back().blocks.back())
                    if (!isDrumLane (style, id))
                        form.at (style.form.size() - 1)["blocks"]
                            .at (style.form.back().blocks.size() - 1)
                            .fail ("the mix-out ends with drums only, but its last block has \"" + id + "\"");
            // Both are 32 bars, and their lanes move one way, block by block: in the mix-in lanes only
            // join, in the mix-out they only leave, and at least one does every 8 or 16 bars (never
            // three blocks in a row with the same lanes).
            const auto edge = [&] (std::size_t si, bool in) {
                const FormSection& sec = style.form[si];
                const Node at = form.at (si);
                const std::string name = in ? "the mix-in" : "the mix-out";
                if (sec.bars != 32)
                {
                    at["bars"].fail (name + " is 32 bars long, not " + std::to_string (sec.bars));
                    return;
                }
                const auto has = [] (const std::vector<std::string>& block, const std::string& id) { return std::find (block.begin(), block.end(), id) != block.end(); };
                const auto same = [&] (const std::vector<std::string>& a, const std::vector<std::string>& b) {
                    return a.size() == b.size() && std::all_of (a.begin(), a.end(), [&] (const std::string& id) { return has (b, id); });
                };
                for (std::size_t k = 1; k < sec.blocks.size(); ++k)
                {
                    const auto& before = sec.blocks[k - 1];
                    const auto& here = sec.blocks[k];
                    for (const auto& id : in ? before : here)
                        if (!has (in ? here : before, id))
                        {
                            at["blocks"].at (k).fail (in ? "lanes only join in " + name + ", but \"" + id + "\" leaves here"
                                                         : "lanes only leave in " + name + ", but \"" + id + "\" joins here");
                            return;
                        }
                    if (k >= 2 && same (here, before) && same (before, sec.blocks[k - 2]))
                    {
                        at["blocks"].at (k).fail (name + (in ? " brings a lane in" : " takes a lane out") + " every 8 or 16 bars, but this is the third block in a row with the same lanes");
                        return;
                    }
                }
            };
            if (style.form.front().kind == "intro")
                edge (0, true);
            if (style.form.back().kind == "outro")
                edge (style.form.size() - 1, false);
        }
        // a lane's controllers need a row for every section kind the form plays the lane in (cc.h)
        for (std::size_t li = 0; li < style.lanes.size(); ++li)
            for (std::size_t ci = 0; ci < style.lanes[li].cc.size(); ++ci)
            {
                const LaneSpec& lane = style.lanes[li];
                for (const auto& sec : style.form)
                {
                    const bool plays = std::any_of (sec.blocks.begin(), sec.blocks.end(), [&] (const std::vector<std::string>& block) {
                        return std::find (block.begin(), block.end(), lane.id) != block.end();
                    });
                    if (plays && lane.cc[ci].row (sec.kind) == nullptr)
                    {
                        lanes.at (li)["cc"].at (ci)["rows"].fail ("the form plays the lane \"" + lane.id + "\" in a " + sec.kind + " section, but controller "
                                                                  + std::to_string (lane.cc[ci].number) + " has no row for " + sec.kind);
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
        for (const auto& sec : style.form)
        {
            if (style.harmony.slotFor (sec.kind).empty())
                root["harmony"]["use"].fail ("the form has a " + sec.kind + " section, but \"use\" gives it no slot");
            if (style.harmony.paletteFor (sec.kind) == nullptr)
                root["harmony"]["palette"].fail ("the form has a " + sec.kind + " section, but the palette has no voicings for it");
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
        for (const auto& sec : style.form)
            for (const auto& block : sec.blocks)
                for (const auto& id : block)
                {
                    const LaneSpec& lane = style.lanes[*style.laneIndex (id)];
                    if (lane.gridFor (sec.kind).empty())
                        return R::failure (where + "the form plays the lane \"" + id + "\" in a " + sec.kind + " section, but the lane has no grid for "
                                           + sec.kind);
                }
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
                if (const auto e = checkLane (id, f, "feel rows." + row.kind + "." + id); !e.empty())
                    return R::failure (where + e);
        for (const auto& lane : style.lanes)
        {
            const FeelLane f = feel.resolve (lane.id, "drop");
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
