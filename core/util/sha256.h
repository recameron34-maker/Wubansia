#pragma once

// SHA-256 (FIPS 180-4), to name file contents: the generator snapshots
// (core/tests/snapshots/gen.sha256) and the checks of the legacy export against its manifest. It
// is plain integer code, so it gives the same digest on every platform. It is not for secrets.

#include <cstdint>
#include <span>
#include <string>

namespace wubansia::util
{
    // The SHA-256 of bytes as 64 lower-case hex digits, as sha256sum prints it.
    [[nodiscard]] std::string sha256Hex (std::span<const std::uint8_t> bytes);
}
