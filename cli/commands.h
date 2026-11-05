#pragma once

// The groovecore commands. main.cpp reads the command name and hands the remaining arguments to one
// of these; each command parses its own options, prints its own --help and returns an exit code.

#include <ostream>
#include <span>
#include <string_view>

namespace wubansia::cli
{
    // Exit codes, the same for every command.
    inline constexpr int exitOk = 0; // done
    inline constexpr int exitFailed = 1; // ran but failed: a file it couldn't read or write, a check that didn't pass
    inline constexpr int exitUsage = 2; // refused: bad arguments, or not there yet

    // groovecore gen [args]: MIDI from a kit and a seed (cli/gen_cmd.cpp). args are the
    // arguments after "gen"; normal output goes to out, errors and usage problems to err.
    int runGen (std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

    // groovecore metrics [args]: the generator metrics on MIDI files (cli/metrics_cmd.cpp).
    int runMetrics (std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

    // groovecore render [args]: a kit through the engine into a WAV (cli/render.cpp).
    int runRender (std::span<const std::string_view> args, std::ostream& out, std::ostream& err);
}
