#pragma once

// What a note plays: its zone, its velocity layer and their gains, as the recipes choose them
// (phrase.hits_lane, context4.zoned_lane, basslines.zone_of and layer_of) and docs/plugin/manifest.md
// writes them down. Round robins and pan indices come with the note (the engine counts them).

#include "kit/kit.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace wubansia::engine
{
    // The layer a velocity plays: the one whose velocity is nearest, the louder on a tie (layers by
    // velocity ascending, as the manifest keeps them). -1 with no layers.
    //
    // The manifest stores each layer as round (layer x 127), so this integer rule is the contract. It
    // differs from the recipes' floating-point rule (the nearest layer to velocity / 127) at four
    // (set, velocity) points of the kits' layer sets, where rounding makes a tie of a near miss; the MIDI
    // writer checks its velocities with this rule (docs/midi-map.md), and test_selection.cpp lists them.
    inline int pickLayer (std::span<const Layer> layers, int velocity) noexcept
    {
        int best = -1;
        int bestDistance = 0;
        for (std::size_t i = 0; i < layers.size(); ++i)
        {
            const int d = std::abs (static_cast<int> (layers[i].velocity) - velocity);
            if (best < 0 || d <= bestDistance) // <=: on a tie the later, louder layer wins
            {
                best = static_cast<int> (i);
                bestDistance = d;
            }
        }
        return best;
    }

    // The zone a key plays: the one whose lo..hi holds it (the exporter sets the ranges so that is the
    // highest root at or below the key, the lowest zone reaching down). -1 if none does.
    inline int pickZone (std::span<const Zone> zones, int key) noexcept
    {
        for (std::size_t i = 0; i < zones.size(); ++i)
            if (key >= zones[i].lo && key <= zones[i].hi)
                return static_cast<int> (i);
        return -1;
    }

    // An instrument's zones and a zone's layers inside a Kit's flat tables (empty if out of range).
    inline std::span<const Zone> zonesOf (const Kit& kit, const Instrument& instrument) noexcept
    {
        const std::size_t first = instrument.firstZone;
        if (first + instrument.zoneCount > kit.zones.size())
            return {};
        return { kit.zones.data() + first, instrument.zoneCount };
    }

    inline std::span<const Layer> layersOf (const Kit& kit, const Zone& zone) noexcept
    {
        const std::size_t first = zone.firstLayer;
        if (first + zone.layerCount > kit.layers.size())
            return {};
        return { kit.layers.data() + first, zone.layerCount };
    }

    // The velocity law's gain for a note of velocity on a layer made for layerVelocity: 1 for layer_only,
    // (velocity / layerVelocity) ^ 1.5 for pow15.
    inline double velocityGain (VelocityLaw law, int velocity, int layerVelocity) noexcept
    {
        if (law != VelocityLaw::pow15 || layerVelocity <= 0)
            return 1.0;
        return std::pow (static_cast<double> (velocity) / static_cast<double> (layerVelocity), 1.5);
    }

    // Equal-power pan, unity in the centre (phrase.pan): {left, right} = sqrt 2 (cos a, sin a) with
    // a = (p + 1) pi / 4, p = panPm / 1000 in -1..1. Exactly {1, 1} in the centre.
    inline std::array<double, 2> panGains (int panPm) noexcept
    {
        if (panPm == 0)
            return { 1.0, 1.0 };
        const double p = static_cast<double> (panPm < -1000 ? -1000 : (panPm > 1000 ? 1000 : panPm)) / 1000.0;
        const double a = (p + 1.0) * 3.14159265358979323846 / 4.0;
        return { std::sqrt (2.0) * std::cos (a), std::sqrt (2.0) * std::sin (a) };
    }

    // A gain in milli-decibels as a factor.
    inline double gainOfMdb (std::int32_t mdb) noexcept
    {
        return mdb == 0 ? 1.0 : std::pow (10.0, static_cast<double> (mdb) / 20000.0);
    }
}
