// Stubs (S1b2b, "test: the hook, failing"): the declarations compile and core/tests/test_gen_hook.cpp
// fails against them until the hook's material, line plan and guard land.

#include "gen/hook.h"

namespace wubansia::gen
{
    rng::Key hookKey (rng::Key root) noexcept { return root; }

    rng::Key hookCellKey (rng::Key root) noexcept { return root; }

    bool onTheLine (const LinePrints& before, const LinePrints& after) noexcept
    {
        (void) before;
        (void) after;
        return false;
    }

    void planHook (Song& song, const Bundle& bundle, rng::Key root)
    {
        (void) song;
        (void) bundle;
        (void) root;
    }
}
