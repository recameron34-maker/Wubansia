#pragma once

// A value or an error message, for core/ functions that can fail on bad input (a truncated MIDI
// file, a spec with a velocity of 0). No exceptions: the caller checks ok () and reads error.

#include <string>
#include <utility>

namespace wubansia::util
{
    template <typename T>
    struct Result
    {
        T value {}; // meaningful only when ok ()
        std::string error; // empty when ok (); otherwise one line saying what is wrong and where

        [[nodiscard]] bool ok() const noexcept { return error.empty(); }
        explicit operator bool() const noexcept { return ok(); }

        [[nodiscard]] static Result success (T v) { return Result { std::move (v), {} }; }

        [[nodiscard]] static Result failure (std::string message)
        {
            if (message.empty())
                message = "unknown error";
            return Result { T {}, std::move (message) };
        }
    };
}
