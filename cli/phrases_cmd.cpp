// groovecore phrases: a kit's phrase library (gen/phrase.h, ADR 0031). Not there yet.

#include "commands.h"

namespace wubansia::cli
{
    int runPhrases (std::span<const std::string_view>, std::ostream&, std::ostream& err)
    {
        err << "groovecore phrases: not there yet\n";
        return exitUsage;
    }
}
