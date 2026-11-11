// groovecore, the groove core's command-line tool: `groovecore <command> [options]`.
// This file only dispatches; the commands live in their own files (commands.h).

#include "commands.h"

#include <array>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#ifndef WUBANSIA_VERSION
    #error "WUBANSIA_VERSION must be defined: cli/CMakeLists.txt sets it from the VERSION file"
#endif

namespace
{
    using Command = int (*) (std::span<const std::string_view>, std::ostream&, std::ostream&);

    struct Entry
    {
        std::string_view name;
        Command run;
        std::string_view summary;
    };

    constexpr std::array<Entry, 3> commands = { {
        { "gen", &wubansia::cli::runGen, "write MIDI from a kit and a seed" },
        { "metrics", &wubansia::cli::runMetrics, "score MIDI files with the generator metrics" },
        { "render", &wubansia::cli::runRender, "play a kit through the engine into a WAV" },
    } };

    const Entry* findCommand (std::string_view name)
    {
        for (const auto& c : commands)
            if (c.name == name)
                return &c;
        return nullptr;
    }

    void printUsage (std::ostream& os)
    {
        os << "Usage: groovecore <command> [options]\n"
              "\n"
              "Commands:\n";
        for (const auto& c : commands)
        {
            os << "  " << c.name;
            for (auto n = c.name.size(); n < 10; ++n)
                os << ' ';
            os << c.summary << '\n';
        }
        os << "\n"
              "groovecore <command> --help (or groovecore help <command>) shows a command's options.\n"
              "groovecore --version shows the version.\n";
    }

    int dispatch (std::span<const std::string_view> args, std::ostream& out, std::ostream& err)
    {
        using namespace wubansia::cli;

        if (args.empty())
        {
            printUsage (err);
            return exitUsage;
        }

        const std::string_view first = args.front();
        const auto rest = args.subspan (1);

        if (first == "--help" || first == "-h" || first == "help")
        {
            if (first == "help" && !rest.empty()) // groovecore help gen == groovecore gen --help
            {
                if (const auto* c = findCommand (rest.front()))
                {
                    const std::array<std::string_view, 1> help = { "--help" };
                    return c->run (help, out, err);
                }
                err << "groovecore: unknown command \"" << rest.front() << "\"\n\n";
                printUsage (err);
                return exitUsage;
            }
            printUsage (out);
            return exitOk;
        }

        if (first == "--version")
        {
            out << "groovecore " << WUBANSIA_VERSION << '\n';
            return exitOk;
        }

        if (const auto* c = findCommand (first))
            return c->run (rest, out, err);

        err << "groovecore: unknown command \"" << first << "\"\n\n";
        printUsage (err);
        return exitUsage;
    }
}

int main (int argc, char* argv[])
{
    // No exception may escape: a command that throws (a full disk, std::bad_alloc) fails cleanly.
    try
    {
        std::vector<std::string_view> args;
        for (int i = 1; i < argc; ++i)
            args.emplace_back (argv[i]);
        return dispatch (args, std::cout, std::cerr);
    } catch (const std::exception& e)
    {
        std::cerr << "groovecore: error: " << e.what() << '\n';
    } catch (...)
    {
        std::cerr << "groovecore: error: unknown exception\n";
    }
    return wubansia::cli::exitFailed;
}
