// What a note plays (selection.h): the layer, the zone, the gains, and the round robin and pan a note
// brings with it.

#include "sampler_fixtures.h"

#include "sampler/selection.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <set>
#include <tuple>
#include <vector>

using namespace sampler_test;

TEST_CASE ("the nearest layer, the louder on a tie, for every velocity 1-127 on the recipes' layer sets", "[sampler][selection][vectors]")
{
    // layers.f32 (tools/sound/export_sampler_vectors.py): every multi-layer set of the four kits' items, with
    // what the engine's rule picks (on round (layer x 127)) and what the recipes' rule picks (on velocity / 127,
    // in floating point) for each velocity.
    const auto v = readVector ("layers.f32");
    REQUIRE (! v.empty());
    const auto sets = static_cast<int> (v[0]);
    CHECK (sets == 10);
    std::size_t i = 1;
    std::set<std::tuple<std::vector<int>, int>> differ;
    for (int s = 0; s < sets; ++s)
    {
        REQUIRE (i < v.size());
        const auto n = static_cast<std::size_t> (v[i++]);
        REQUIRE (i + n + 254 <= v.size());
        std::vector<Layer> layers (n);
        std::vector<int> vels (n);
        for (std::size_t k = 0; k < n; ++k)
        {
            vels[k] = static_cast<int> (v[i + k]);
            layers[k].velocity = static_cast<std::uint8_t> (vels[k]);
        }
        const float* engine = v.data() + i + n;
        const float* recipe = engine + 127;
        for (int vel = 1; vel <= 127; ++vel)
        {
            CAPTURE (vels, vel);
            CHECK (pickLayer (layers, vel) == static_cast<int> (engine[vel - 1]));
            if (! same (engine[vel - 1], recipe[vel - 1]))
                differ.emplace (vels, vel);
        }
        i += n + 254;
    }
    CHECK (i == v.size());

    // Where round (layer x 127) turns a near miss into a tie. The MIDI writer checks its velocities with the
    // engine's rule (docs/midi-map.md), so the committed files never ask for these.
    const std::set<std::tuple<std::vector<int>, int>> expected {
        { { 25, 51, 76, 102, 127 }, 38 }, // 0.2 .. 1.0: the recipe's 0.3 ghost clap is 0.2's in floating point
        { { 32, 64, 95, 127 }, 111 },
        { { 38, 76, 127 }, 57 },
        { { 64, 95, 127 }, 111 },
    };
    CHECK (differ == expected);
}

TEST_CASE ("pickLayer: ties go to the louder layer; one layer takes every velocity", "[sampler][selection]")
{
    const std::vector<Layer> two { Layer { 64, 0, 1 }, Layer { 127, 0, 1 } };
    CHECK (pickLayer (two, 1) == 0);
    CHECK (pickLayer (two, 95) == 0);
    CHECK (pickLayer (two, 96) == 1); // 32 away from 64, 31 from 127
    CHECK (pickLayer (std::vector<Layer> { Layer { 60, 0, 1 }, Layer { 100, 0, 1 } }, 80) == 1); // a tie
    CHECK (pickLayer (std::vector<Layer> { Layer { 90, 0, 1 } }, 1) == 0);
    CHECK (pickLayer (std::vector<Layer> {}, 64) == -1);
}

TEST_CASE ("pickZone: the highest root at or below the key, the lowest zone reaching down", "[sampler][selection]")
{
    // Ranges as the exporter writes them from the recipes' rule (basslines.zone_of): each zone from its root
    // to the next root - 1, the lowest from 0, the highest to 127.
    auto zonesFor = [] (std::vector<int> roots) {
        std::vector<Zone> zones;
        for (std::size_t k = 0; k < roots.size(); ++k)
        {
            Zone z;
            z.root = static_cast<std::uint8_t> (roots[k]);
            z.lo = static_cast<std::uint8_t> (k == 0 ? 0 : roots[k]);
            z.hi = static_cast<std::uint8_t> (k + 1 < roots.size() ? roots[k + 1] - 1 : 127);
            zones.push_back (z);
        }
        return zones;
    };

    for (const auto& roots : { std::vector<int> { 30, 33, 37 }, std::vector<int> { 31, 43 }, std::vector<int> { 60, 63, 66, 69, 72, 75, 78, 81, 84 } })
    {
        const auto zones = zonesFor (roots);
        for (int key = 0; key < 128; ++key)
        {
            int expected = 0; // below the lowest root: the lowest zone, pitched down
            for (std::size_t k = 0; k < roots.size(); ++k)
                if (roots[k] <= key)
                    expected = static_cast<int> (k);
            CAPTURE (roots, key);
            CHECK (pickZone (zones, key) == expected);
        }
    }
    // FUT03's F1 (29) plays the G1 zone (31) two semitones down.
    CHECK (pickZone (zonesFor ({ 31, 43 }), 29) == 0);

    // A key no range holds plays nothing.
    Zone only;
    only.root = 60;
    only.lo = 55;
    only.hi = 70;
    CHECK (pickZone (std::vector<Zone> { only }, 54) == -1);
    CHECK (pickZone (std::vector<Zone> { only }, 71) == -1);
    CHECK (pickZone (std::vector<Zone> { only }, 55) == 0);
}

TEST_CASE ("velocityGain: the layer alone, or (velocity / layer) ^ 1.5", "[sampler][selection]")
{
    CHECK (same (velocityGain (VelocityLaw::layerOnly, 30, 127), 1.0));
    CHECK (same (velocityGain (VelocityLaw::pow15, 89, 89), 1.0));
    CHECK (std::fabs (velocityGain (VelocityLaw::pow15, 44, 89) - std::pow (44.0 / 89.0, 1.5)) < 1.0e-15);
    CHECK (std::fabs (velocityGain (VelocityLaw::pow15, 127, 76) - std::pow (127.0 / 76.0, 1.5)) < 1.0e-15);
}

TEST_CASE ("panGains: equal power, unity in the centre (phrase.pan)", "[sampler][selection]")
{
    const auto centre = panGains (0);
    CHECK (same (centre[0], 1.0));
    CHECK (same (centre[1], 1.0));
    const auto left = panGains (-1000);
    CHECK (std::fabs (left[0] - std::sqrt (2.0)) < 1.0e-12);
    CHECK (std::fabs (left[1]) < 1.0e-12);
    for (const int p : { -700, -220, 150, 300, 1000 })
    {
        const auto g = panGains (p);
        const double a = (p / 1000.0 + 1.0) * kPi / 4.0;
        CHECK (std::fabs (g[0] - std::sqrt (2.0) * std::cos (a)) < 1.0e-12);
        CHECK (std::fabs (g[1] - std::sqrt (2.0) * std::sin (a)) < 1.0e-12);
        CHECK (std::fabs (g[0] * g[0] + g[1] * g[1] - 2.0) < 1.0e-12); // equal power
    }
}

TEST_CASE ("a note plays its layer's round robin rr % count and its pan cycle's entry panIndex % length", "[sampler][selection]")
{
    // Each file is a different constant, so the level says which played; the pans make left and right differ.
    TestKit t;
    const int soft1 = t.addFile (constant (64, 0.1f));
    const int soft2 = t.addFile (constant (64, 0.2f));
    const int hard1 = t.addFile (constant (64, 0.3f));
    const int hard2 = t.addFile (constant (64, 0.4f));
    const int hard3 = t.addFile (constant (64, 0.5f));
    const std::vector<int> pans { 0, -500, 400 };
    const int inst = t.addInstrument (instrument (SampleMode::oneShot), { ZoneSpec { 60, 0, 127, { LayerSpec { 40, { soft1, soft2 } }, LayerSpec { 120, { hard1, hard2, hard3 } } } } }, pans);

    struct Hit
    {
        int velocity, rr, panIndex;
        float level;
        int pan;
    };
    const Hit hits[] {
        { 40, 0, 0, 0.1f, 0 }, { 40, 1, 1, 0.2f, -500 }, { 40, 2, 2, 0.1f, 400 }, { 40, 3, 3, 0.2f, 0 },
        { 127, 0, 4, 0.3f, -500 }, { 127, 1, 5, 0.4f, 400 }, { 127, 2, 6, 0.5f, 0 }, { 127, 4, 7, 0.4f, -500 },
        { 80, 5, 0, 0.5f, 0 }, // 80: 40 away from both layers, so the louder
    };
    for (const auto& h : hits)
    {
        CAPTURE (h.velocity, h.rr, h.panIndex);
        Rig rig;
        rig.at (0, [&] (Sampler& s, int offset) {
            auto n = note (inst, 60, h.velocity, offset);
            n.rr = static_cast<std::uint8_t> (h.rr);
            n.panIndex = static_cast<std::uint8_t> (h.panIndex);
            s.noteOn (t.kit, n);
        });
        rig.runUntil (32);
        const auto g = panGains (h.pan);
        CHECK (std::fabs (rig.left[0][10] - h.level * g[0]) < 1.0e-6);
        CHECK (std::fabs (rig.right[0][10] - h.level * g[1]) < 1.0e-6);
    }
}

TEST_CASE ("a note no zone holds, or an instrument out of range, plays nothing", "[sampler][selection]")
{
    TestKit t;
    const int file = t.addFile (constant (64, 0.5f));
    const int inst = t.addSimple (instrument (SampleMode::oneShot), file, 60, 50, 70);
    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (inst, 49, 100, offset));
        s.noteOn (t.kit, note (inst, 71, 100, offset));
        s.noteOn (t.kit, note (5, 60, 100, offset));
        s.noteOn (t.kit, note (-1, 60, 100, offset));
    });
    rig.runUntil (64);
    CHECK (silent (rig.left[0]));
    CHECK (rig.sampler.activeVoices() == 0);
}
