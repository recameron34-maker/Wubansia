#include "kit/file_sample_source.h"

#include <array>
#include <fstream>
#include <ios>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace wubansia::engine
{
    namespace
    {
        // A kit.json path as a filesystem path: UTF-8 on every platform (a plain std::string would be read in the
        // Windows code page).
        std::filesystem::path fromUtf8 (std::string_view s)
        {
            std::u8string u (s.size(), u8'\0');
            for (std::size_t i = 0; i < s.size(); ++i)
                u[i] = static_cast<char8_t> (s[i]);
            return std::filesystem::path (u);
        }

        std::string toUtf8 (const std::filesystem::path& p)
        {
            const auto u = p.u8string();
            return std::string (u.begin(), u.end());
        }

        // "code: rest" from the reader becomes "code: path: rest".
        std::string withPath (const std::string& error, std::string_view path)
        {
            const auto colon = error.find (": ");
            if (colon == std::string::npos)
                return "unreadable: " + std::string (path) + ": " + error;
            return error.substr (0, colon) + ": " + std::string (path) + error.substr (colon);
        }

        // The file at root/relative, checked: a regular file there, small enough to be content. Gives its size.
        util::Result<std::uint64_t> locate (const std::filesystem::path& root, std::string_view relative, std::filesystem::path& full)
        {
            using R = util::Result<std::uint64_t>;
            if (const auto problem = contentPathProblem (relative); ! problem.empty())
                return R::failure ("missing: " + std::string (relative) + ": not a file inside " + toUtf8 (root) + " (the path " + problem + ")");
            full = root / fromUtf8 (relative);
            std::error_code error;
            const auto status = std::filesystem::status (full, error);
            if (status.type() == std::filesystem::file_type::not_found)
                return R::failure ("missing: " + std::string (relative) + ": no such file in " + toUtf8 (root));
            if (error)
                return R::failure ("unreadable: " + std::string (relative) + ": " + error.message());
            if (status.type() == std::filesystem::file_type::directory)
                return R::failure ("unreadable: " + std::string (relative) + ": it is a folder, not a file");
            const auto size = std::filesystem::file_size (full, error);
            if (error)
                return R::failure ("unreadable: " + std::string (relative) + ": " + error.message());
            if (size > FileSampleSource::kMaxFileBytes)
                return R::failure ("format: " + std::string (relative) + ": " + std::to_string (size) + " bytes, more than any content file holds");
            return R::success (static_cast<std::uint64_t> (size));
        }
    }

    std::string contentPathProblem (std::string_view path)
    {
        if (path.empty())
            return "is empty";
        for (const char c : path)
        {
            const auto u = static_cast<unsigned char> (c);
            if (u < 0x20 || u == 0x7F)
                return "holds a control character";
            if (c == '\\')
                return "holds '\\': kit paths are '/'-separated";
            if (c == ':')
                return "holds ':'";
        }
        if (path.front() == '/')
            return "starts with '/': kit paths are relative to the kit's folder";
        if (! path.ends_with (".wav"))
            return "doesn't end in .wav";
        std::size_t start = 0;
        while (start <= path.size())
        {
            auto end = path.find ('/', start);
            if (end == std::string_view::npos)
                end = path.size();
            const auto part = path.substr (start, end - start);
            if (part.empty())
                return "has an empty part";
            if (part == "." || part == "..")
                return "has a \"" + std::string (part) + "\" part";
            start = end + 1;
        }
        return {};
    }

    FileSampleSource::FileSampleSource (std::filesystem::path root) : root_ (std::move (root)) {}

    util::Result<SampleData> FileSampleSource::read (std::string_view relativePath)
    {
        using R = util::Result<SampleData>;
        std::filesystem::path full;
        const auto size = locate (root_, relativePath, full);
        if (! size.ok())
            return R::failure (size.error);

        std::vector<std::uint8_t> bytes (static_cast<std::size_t> (size.value));
        std::ifstream in (full, std::ios::binary);
        if (! in)
            return R::failure ("unreadable: " + std::string (relativePath) + ": it can't be opened");
        if (! bytes.empty() && ! in.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size())))
            return R::failure ("unreadable: " + std::string (relativePath) + ": reading it failed");

        auto wav = readWav (bytes);
        if (! wav.ok())
            return R::failure (withPath (wav.error, relativePath));
        return wav;
    }

    util::Result<WavInfo> FileSampleSource::probe (std::string_view relativePath) const
    {
        using R = util::Result<WavInfo>;
        std::filesystem::path full;
        const auto size = locate (root_, relativePath, full);
        if (! size.ok())
            return R::failure (size.error);

        std::ifstream in (full, std::ios::binary);
        if (! in)
            return R::failure ("unreadable: " + std::string (relativePath) + ": it can't be opened");
        auto info = readWavInfo (size.value, [&] (std::uint64_t offset, std::span<std::uint8_t> out) {
            if (offset > static_cast<std::uint64_t> (std::numeric_limits<std::streamoff>::max()))
                return false;
            in.clear();
            in.seekg (static_cast<std::streamoff> (offset));
            return static_cast<bool> (in.read (reinterpret_cast<char*> (out.data()), static_cast<std::streamsize> (out.size())));
        });
        if (! info.ok())
            return R::failure (withPath (info.error, relativePath));
        return info;
    }
}
