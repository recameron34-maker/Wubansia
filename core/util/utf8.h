#pragma once

// UTF-8 validation (RFC 3629). MIDI strings are UTF-8 (CLAUDE.md), so the MIDI writer checks every
// name and marker with this before it goes into a file.

#include <cstddef>
#include <string_view>

namespace wubansia::util
{
    // True if s is well-formed UTF-8: no stray continuation bytes, no truncated sequences, no
    // overlong forms, no surrogates (U+D800-U+DFFF) and nothing above U+10FFFF. "" is valid.
    constexpr bool isValidUtf8 (std::string_view s) noexcept
    {
        const auto byte = [&s] (std::size_t i) { return static_cast<unsigned char> (s[i]); };
        const auto isCont = [] (unsigned char b) { return (b & 0xC0u) == 0x80u; };

        std::size_t i = 0;
        while (i < s.size())
        {
            const unsigned char b0 = byte (i);
            if (b0 < 0x80u)
            {
                ++i;
                continue;
            }

            std::size_t need = 0; // continuation bytes after b0
            unsigned char lo = 0x80u; // allowed range of the first continuation byte
            unsigned char hi = 0xBFu;
            if (b0 >= 0xC2u && b0 <= 0xDFu)
                need = 1;
            else if (b0 == 0xE0u)
            {
                need = 2;
                lo = 0xA0u; // no overlongs below U+0800
            }
            else if ((b0 >= 0xE1u && b0 <= 0xECu) || b0 == 0xEEu || b0 == 0xEFu)
                need = 2;
            else if (b0 == 0xEDu)
            {
                need = 2;
                hi = 0x9Fu; // no surrogates
            }
            else if (b0 == 0xF0u)
            {
                need = 3;
                lo = 0x90u; // no overlongs below U+10000
            }
            else if (b0 >= 0xF1u && b0 <= 0xF3u)
                need = 3;
            else if (b0 == 0xF4u)
            {
                need = 3;
                hi = 0x8Fu; // nothing above U+10FFFF
            }
            else
                return false; // 0x80-0xC1 (continuation or overlong lead) and 0xF5-0xFF

            if (s.size() - i <= need)
                return false;
            const unsigned char b1 = byte (i + 1);
            if (b1 < lo || b1 > hi)
                return false;
            for (std::size_t k = 2; k <= need; ++k)
                if (!isCont (byte (i + k)))
                    return false;
            i += need + 1;
        }
        return true;
    }
}
