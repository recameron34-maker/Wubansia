#include "kit/file_sample_source.h"

#include <algorithm>
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
            const auto shown = escapeControlCharacters (path);
            const auto colon = error.find (": ");
            if (colon == std::string::npos)
                return "unreadable: " + shown + ": " + error;
            return error.substr (0, colon) + ": " + shown + error.substr (colon);
        }

        // An I/O error as fixed English text. error_code::message () is the platform's localized text, in the ANSI
        // code page on Windows, so it never goes into an error.
        std::string ioProblem (const std::error_code& error)
        {
            using E = std::errc;
            if (error == E::permission_denied || error == E::operation_not_permitted)
                return "permission denied";
            if (error == E::too_many_symbolic_link_levels)
                return "too many levels of symbolic links";
            if (error == E::not_supported || error == E::operation_not_supported)
                return "not a regular file";
            if (error == E::io_error)
                return "an I/O error";
            if (error == E::filename_too_long)
                return "the path is too long";
            if (error == E::no_such_device || error == E::no_such_device_or_address)
                return "no such device";
            if (error == E::not_a_directory)
                return "a part of the path isn't a folder";
            if (error == E::is_a_directory)
                return "it is a folder, not a file";
            if (error == E::device_or_resource_busy)
                return "the device is busy";
            if (error == E::no_such_file_or_directory)
                return "no such file";
            if (error == E::timed_out || error == E::host_unreachable || error == E::network_unreachable || error == E::connection_reset)
                return "the storage didn't answer";
            return "system error " + std::to_string (error.value());
        }

        // The file at root/relative, checked: a regular file there, small enough to be content. Gives its size.
        util::Result<std::uint64_t> locate (const std::filesystem::path& root, std::string_view relative, std::filesystem::path& full)
        {
            using R = util::Result<std::uint64_t>;
            const auto shown = escapeControlCharacters (relative);
            if (const auto problem = contentPathProblem (relative); ! problem.empty())
                return R::failure ("missing: " + shown + ": not a file inside " + escapeControlCharacters (toUtf8 (root)) + " (the path " + problem + ")");
            full = root / fromUtf8 (relative);
            std::error_code error;
            const auto status = std::filesystem::status (full, error);
            if (status.type() == std::filesystem::file_type::not_found)
                return R::failure ("missing: " + shown + ": no such file in " + escapeControlCharacters (toUtf8 (root)));
            if (error)
                return R::failure ("unreadable: " + shown + ": " + ioProblem (error));
            if (status.type() == std::filesystem::file_type::directory)
                return R::failure ("unreadable: " + shown + ": it is a folder, not a file");
            if (status.type() != std::filesystem::file_type::regular)
                return R::failure ("unreadable: " + shown + ": not a regular file");
            const auto size = std::filesystem::file_size (full, error);
            if (error)
                return R::failure ("unreadable: " + shown + ": " + ioProblem (error));
            if (size > FileSampleSource::kMaxFileBytes)
                return R::failure ("format: " + shown + ": " + std::to_string (size) + " bytes, more than any content file holds");
            return R::success (static_cast<std::uint64_t> (size));
        }
    }

    bool hasControlCharacter (std::string_view s) noexcept
    {
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const auto c = static_cast<unsigned char> (s[i]);
            if (c < 0x20 || c == 0x7F)
                return true;
            if (c == 0xC2 && i + 1 < s.size() && static_cast<unsigned char> (s[i + 1]) >= 0x80 && static_cast<unsigned char> (s[i + 1]) <= 0x9F)
                return true;
        }
        return false;
    }

    std::string escapeControlCharacters (std::string_view s)
    {
        static constexpr char hex[] = "0123456789abcdef";
        std::string out;
        out.reserve (s.size());
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const auto c = static_cast<unsigned char> (s[i]);
            unsigned code = 0x100; // none
            if (c < 0x20 || c == 0x7F)
                code = c;
            else if (c == 0xC2 && i + 1 < s.size() && static_cast<unsigned char> (s[i + 1]) >= 0x80 && static_cast<unsigned char> (s[i + 1]) <= 0x9F)
                code = static_cast<unsigned char> (s[++i]);
            if (code == 0x100)
            {
                out += s[i];
                continue;
            }
            out += "\\u00";
            out += hex[code >> 4];
            out += hex[code & 15];
        }
        return out;
    }

    std::string contentPathProblem (std::string_view path)
    {
        if (path.empty())
            return "is empty";
        if (hasControlCharacter (path))
            return "holds a control character";
        for (const char c : path)
        {
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

    FileSampleSource::FileSampleSource (std::filesystem::path root, const std::atomic<bool>* cancel) : root_ (std::move (root)), cancel_ (cancel) {}

    util::Result<SampleData> FileSampleSource::read (std::string_view relativePath)
    {
        using R = util::Result<SampleData>;
        const auto bytes = readBytes (relativePath);
        if (! bytes.ok())
            return R::failure (bytes.error);

        auto wav = readWav (bytes.value);
        if (! wav.ok())
            return R::failure (withPath (wav.error, relativePath));
        return wav;
    }

    util::Result<std::vector<std::uint8_t>> FileSampleSource::readBytes (std::string_view relativePath) const
    {
        using R = util::Result<std::vector<std::uint8_t>>;
        std::filesystem::path full;
        const auto size = locate (root_, relativePath, full); // a regular file, small enough to be content
        if (! size.ok())
            return R::failure (size.error);

        std::vector<std::uint8_t> bytes (static_cast<std::size_t> (size.value));
        std::ifstream in (full, std::ios::binary);
        if (! in)
            return R::failure ("unreadable: " + escapeControlCharacters (relativePath) + ": it can't be opened");
        // Exactly its size, in chunks, with the cancel flag checked before each: a slow read stops within one chunk.
        for (std::size_t done = 0; done < bytes.size();)
        {
            if (cancel_ != nullptr && cancel_->load (std::memory_order_acquire))
                return R::failure ("cancelled: " + escapeControlCharacters (relativePath) + ": the read was cancelled");
            const auto n = std::min (kReadChunkBytes, bytes.size() - done);
            if (! in.read (reinterpret_cast<char*> (bytes.data() + done), static_cast<std::streamsize> (n)))
                return R::failure ("unreadable: " + escapeControlCharacters (relativePath) + ": reading it failed");
            done += n;
        }
        return R::success (std::move (bytes));
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
            return R::failure ("unreadable: " + escapeControlCharacters (relativePath) + ": it can't be opened");
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
