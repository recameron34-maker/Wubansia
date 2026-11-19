#pragma once

// Test-only: reads the SFZ subset the drum kits write (ADR 0030) into regions with the opcodes they inherit. Headers
// <control>, <global>, <group> and <region>; key=value opcodes separated by spaces; sample= runs to the end of its line
// (its value may hold spaces); // comments to the end of a line. A <group> starts from <global>'s opcodes; a <region>
// from its group's. Anything else is an error the test reports.

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace wubansia::test
{
    struct SfzRegion
    {
        std::map<std::string, std::string> opcodes; // its own, its group's and <global>'s, the nearest winning
        std::string groupComment; // the "// ..." line before its group

        bool has (const std::string& op) const { return opcodes.count (op) != 0; }
        std::string at (const std::string& op) const
        {
            const auto it = opcodes.find (op);
            return it == opcodes.end() ? std::string() : it->second;
        }
        int number (const std::string& op, int fallback) const { return has (op) ? std::stoi (at (op)) : fallback; }
    };

    struct SfzFile
    {
        std::map<std::string, std::string> control;
        std::map<std::string, std::string> global;
        std::vector<SfzRegion> regions;
        std::vector<std::map<std::string, std::string>> groups; // each group's own opcodes, in order
        std::string error; // "" when it read

        // Whether a region plays this key: key=, or lokey..hikey.
        static bool playsKey (const SfzRegion& r, int key)
        {
            if (r.has ("key"))
                return r.number ("key", -1) == key;
            return r.number ("lokey", 0) <= key && key <= r.number ("hikey", 127);
        }
    };

    inline SfzFile readSfz (const std::string& text)
    {
        SfzFile file;
        enum class Where { none, control, global, group, region } where = Where::none;
        std::map<std::string, std::string> group;
        std::string comment, groupComment;
        SfzRegion* region = nullptr;
        std::istringstream lines (text);
        std::string line;
        int number = 0;
        const auto fail = [&] (const std::string& why) {
            if (file.error.empty())
                file.error = "line " + std::to_string (number) + ": " + why;
        };
        while (std::getline (lines, line))
        {
            ++number;
            if (line.rfind ("//", 0) == 0)
            {
                comment = line;
                continue;
            }
            std::size_t at = 0;
            while (at < line.size())
            {
                if (line[at] == ' ')
                {
                    ++at;
                    continue;
                }
                if (line[at] == '<')
                {
                    const auto close = line.find ('>', at);
                    if (close == std::string::npos)
                        return fail ("an unclosed header"), file;
                    const auto header = line.substr (at + 1, close - at - 1);
                    at = close + 1;
                    region = nullptr;
                    if (header == "control")
                        where = Where::control;
                    else if (header == "global")
                        where = Where::global;
                    else if (header == "group")
                    {
                        where = Where::group;
                        file.groups.emplace_back();
                        group.clear();
                        groupComment = comment;
                    }
                    else if (header == "region")
                    {
                        if (where != Where::group && where != Where::region)
                            fail ("a region outside a group");
                        where = Where::region;
                        auto& r = file.regions.emplace_back();
                        r.opcodes = file.global;
                        for (const auto& [k, v] : group)
                            r.opcodes[k] = v;
                        r.groupComment = groupComment;
                        region = &r;
                    }
                    else
                        fail ("an unknown header <" + header + ">");
                    continue;
                }
                const auto eq = line.find ('=', at);
                if (eq == std::string::npos)
                    return fail ("text that is no opcode: " + line.substr (at)), file;
                const auto name = line.substr (at, eq - at);
                if (name.find (' ') != std::string::npos || name.empty())
                    return fail ("a bad opcode name '" + name + "'"), file;
                std::string value;
                if (name == "sample")
                {
                    value = line.substr (eq + 1);
                    at = line.size();
                }
                else
                {
                    const auto end = line.find (' ', eq + 1);
                    value = line.substr (eq + 1, end == std::string::npos ? std::string::npos : end - eq - 1);
                    at = end == std::string::npos ? line.size() : end;
                }
                switch (where)
                {
                    case Where::none: fail ("an opcode before any header"); break;
                    case Where::control: file.control[name] = value; break;
                    case Where::global: file.global[name] = value; break;
                    case Where::group:
                        group[name] = value;
                        file.groups.back()[name] = value;
                        break;
                    case Where::region: region->opcodes[name] = value; break;
                }
            }
        }
        return file;
    }
}
