#pragma once

// FileSampleSource: a kit's samples from a folder on disk (P4). The root is one kit's folder, the one its kit.json
// paths are relative to: <content root>/fut01/v1/ or the repo's content/samples/fut01/. It reads on the service
// thread (or groovecore render's), never the audio thread.
//
// read () gives the decoded audio (wav_reader.h); probe () only the header, for a quick content check. Their errors
// start with the reader's codes (lfs_pointer, rate, format, truncated) or one of these, then the relative path:
// - "missing: Drums/Kick v1 rr1.wav: no such file in /path/to/fut01" (also a path that would leave the root);
// - "unreadable: Drums/Kick v1 rr1.wav: <why>" the file is there but can't be read (a folder, permissions, I/O).

#include "kit/sample_source.h"
#include "kit/wav_reader.h"
#include "util/result.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace wubansia::engine
{
    // What is wrong with a kit.json files[].path, or "" if nothing: a relative path inside the kit's folder,
    // '/'-separated, ending ".wav", with no "..", "." or empty part, no leading '/', and no '\' or ':' (which would
    // leave the folder on Windows) or control character. parseKitManifest and FileSampleSource both apply it.
    [[nodiscard]] std::string contentPathProblem (std::string_view path);

    class FileSampleSource : public SampleSource
    {
    public:
        explicit FileSampleSource (std::filesystem::path root);

        // The file at relativePath (kit.json's files[].path: UTF-8, '/'-separated), decoded.
        util::Result<SampleData> read (std::string_view relativePath) override;

        // The file's header only: it exists, isn't an LFS pointer, and is a WAV the reader takes. A few small reads.
        util::Result<WavInfo> probe (std::string_view relativePath) const;

        const std::filesystem::path& root() const noexcept { return root_; }

        // The largest file read () takes: the manifests' longest file (28 800 000 frames) as stereo float, plus 1 MiB
        // of other chunks. A bigger file can't be content, and isn't read into memory.
        static constexpr std::uint64_t kMaxFileBytes = 28'800'000ull * 2 * 4 + (1ull << 20);

    private:
        std::filesystem::path root_;
    };
}
