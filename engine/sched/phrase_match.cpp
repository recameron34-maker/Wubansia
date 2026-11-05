// P0's stub, with P6's helpers declared: every note stays on the multisample. P6 replaces this file.

#include "sched/phrase_match.h"

namespace wubansia::engine
{
    void annotatePhrases (Timeline&, const KitManifest&) {}

    std::int64_t sixteenthOf (std::int64_t, std::int32_t) noexcept { return 0; }

    std::int64_t sixteenthStart (std::int64_t, std::int32_t) noexcept { return 0; }
}
