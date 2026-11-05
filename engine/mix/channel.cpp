#include "mix/channel.h"

#include <cmath>

namespace wubansia::engine::mix
{
    float staticGain (const Channel& channel, Variant variant, const std::array<std::uint8_t, kAbSwitches>& ab) noexcept
    {
        Controls controls;
        controls.variant = variant;
        controls.ab = ab;
        double gain = 1.0;
        for (const auto& insert : channel.inserts)
        {
            if (insert.type != InsertType::gain || ! whenHolds (insert.when, controls))
                continue;
            if (const auto* p = std::get_if<GainParams> (&insert.params))
                gain *= std::pow (10.0, static_cast<double> (p->gainMdb) / 20000.0);
        }
        return static_cast<float> (gain);
    }
}
