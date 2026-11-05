#pragma once

// The engine's WAV reader (P4). It uses no audio framework, so the plugin's kit loads and groovecore render decode
// every content file identically. It runs on the service thread (or a render's main thread) and allocates: never on
// the audio thread.
//
// It reads RIFF WAVE files at 48 kHz, mono or stereo, in one of three encodings, giving exactly the floats Python's
// soundfile (libsndfile) reads, with dtype float32 or float64 alike:
// - 16-bit PCM: s / 32768;
// - 24-bit PCM: s / 8388608;
// - 32-bit IEEE float: as stored, bit for bit (over-range values and subnormals included).
// The fmt chunk may be plain (format tag 1 or 3) or WAVE_FORMAT_EXTENSIBLE with the PCM or IEEE float subformat
// (its valid-bits count may be smaller than the container's, as libsndfile allows). Other chunks may come before,
// between and after (LIST, fact, PEAK, JUNK, cue...), odd sizes with their pad byte.
//
// Every error is one line that starts with its code:
// - "lfs_pointer: ..." a Git LFS pointer file: the samples were never fetched (git lfs pull);
// - "rate: ..." a WAV at another rate than 48 kHz;
// - "format: ..." not a RIFF WAVE file, an encoding other than the three above (8- or 32-bit PCM, 64-bit float,
//   A-law, ADPCM, an extensible fmt with another subformat...), more than 2 channels, or a non-finite float;
// - "truncated: ..." the file ends before its fmt or data chunk does.
// FileSampleSource adds the file's path after the code: "rate: Drums/Kick v1 rr1.wav: 44100 Hz; ...".
//
// The one place it departs from soundfile: a float file holding NaN or infinity is refused ("format"), since one
// such sample would silence a whole mix.

#include "kit/function_ref.h"
#include "kit/sample_source.h"
#include "util/result.h"

#include <cstdint>
#include <span>

namespace wubansia::engine
{
    // What a WAV file's header says: everything a content check needs, without decoding the audio.
    struct WavInfo
    {
        std::int32_t channels = 0; // 1 or 2
        std::int32_t sampleRate = 0; // always kContentSampleRate once read
        std::int32_t bitsPerSample = 0; // 16, 24 or 32
        bool isFloat = false; // 32-bit IEEE float, else integer PCM
        std::int64_t frames = 0;
        std::uint64_t dataOffset = 0; // where the audio starts in the file
        std::uint64_t dataBytes = 0; // frames x channels x bitsPerSample / 8

        friend bool operator== (const WavInfo&, const WavInfo&) = default;
    };

    // True if bytes start the way a Git LFS pointer file does: "version https://git-lfs.github.com/spec/v1" (or the
    // pre-release spec's "version https://hawser.github.com/spec/v1").
    [[nodiscard]] bool isLfsPointer (std::span<const std::uint8_t> bytes) noexcept;

    // The header of a whole WAV file in memory.
    [[nodiscard]] util::Result<WavInfo> readWavInfo (std::span<const std::uint8_t> bytes);

    // The header of a WAV file of fileSize bytes, read through read (offset, out), which fills out with the bytes at
    // offset and returns false if it can't (past the end, or an I/O error). It reads only the chunk headers and the
    // fmt chunk, so a check of hundreds of files costs a few reads each (FileSampleSource::probe).
    [[nodiscard]] util::Result<WavInfo> readWavInfo (std::uint64_t fileSize, FunctionRef<bool (std::uint64_t offset, std::span<std::uint8_t> out)> read);

    // The whole file, decoded: planar float audio (SampleData), mono files mono.
    [[nodiscard]] util::Result<SampleData> readWav (std::span<const std::uint8_t> bytes);
}
