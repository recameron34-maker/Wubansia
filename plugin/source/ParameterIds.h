#pragma once

namespace wubansia::ParameterIds
{
    // Parameter IDs are PERMANENT once released (CLAUDE.md, docs/decisions/0002-plugin-identity.md):
    // rename the label, never the ID. plugin/tests/ParameterTests.cpp lists every ID.
    inline constexpr const char* masterGain = "master_gain";
}
