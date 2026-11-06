#include "util/file_io.h"

#include <fstream>
#include <ios>
#include <string>
#include <system_error>

namespace wubansia::util
{
    namespace
    {
        // The path for a message, as UTF-8 on every platform.
        std::string shown (const std::filesystem::path& path)
        {
            const auto s = path.u8string();
            return std::string (s.begin(), s.end());
        }
    }

    Result<std::vector<std::uint8_t>> readBinaryFile (const std::filesystem::path& path)
    {
        using R = Result<std::vector<std::uint8_t>>;

        std::error_code ec;
        if (std::filesystem::is_directory (path, ec))
            return R::failure ("cannot read " + shown (path) + ": it is a folder");

        std::ifstream in (path, std::ios::binary);
        if (!in)
            return R::failure ("cannot open " + shown (path));

        std::vector<std::uint8_t> bytes;
        std::vector<char> buffer (1 << 16);
        while (in)
        {
            in.read (buffer.data(), static_cast<std::streamsize> (buffer.size()));
            const auto n = static_cast<std::size_t> (in.gcount());
            for (std::size_t i = 0; i < n; ++i)
                bytes.push_back (static_cast<std::uint8_t> (static_cast<unsigned char> (buffer[i])));
        }
        if (in.bad())
            return R::failure ("read failed: " + shown (path));
        return R::success (std::move (bytes));
    }

    Result<std::size_t> writeBinaryFile (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
    {
        using R = Result<std::size_t>;

        std::ofstream out (path, std::ios::binary | std::ios::trunc);
        if (!out)
            return R::failure ("cannot open " + shown (path) + " for writing");
        out.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
        out.close();
        if (!out)
            return R::failure ("write failed: " + shown (path));
        return R::success (bytes.size());
    }
}
