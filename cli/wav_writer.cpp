// wav_writer.h: RIFF WAVE bytes from planar float audio.

#include "wav_writer.h"

#include "util/file_io.h"

#include <cmath>
#include <cstring>
#include <string>

namespace wubansia::cli
{
    namespace
    {
        void put32 (std::vector<std::uint8_t>& b, std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                b.push_back (static_cast<std::uint8_t> ((v >> (8 * i)) & 0xffu));
        }

        void put16 (std::vector<std::uint8_t>& b, std::uint16_t v)
        {
            b.push_back (static_cast<std::uint8_t> (v & 0xffu));
            b.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xffu));
        }

        void putTag (std::vector<std::uint8_t>& b, const char (&tag)[5])
        {
            for (int i = 0; i < 4; ++i)
                b.push_back (static_cast<std::uint8_t> (tag[i]));
        }

        std::int32_t pcm24 (float x)
        {
            const double v = std::floor (static_cast<double> (x) * 8388608.0 + 0.5);
            if (! (v > -8388608.0)) // NaN goes to the floor too, so the bytes stay defined
                return -8388608;
            if (v > 8388607.0)
                return 8388607;
            return static_cast<std::int32_t> (v);
        }
    }

    util::Result<std::vector<std::uint8_t>> wavBytes (std::span<const std::span<const float>> channels, int sampleRate, WavFormat format)
    {
        using R = util::Result<std::vector<std::uint8_t>>;
        if (channels.empty() || channels.size() > 2)
            return R::failure ("a WAV of " + std::to_string (channels.size()) + " channels: 1 or 2");
        const std::size_t frames = channels[0].size();
        for (const auto& c : channels)
            if (c.size() != frames)
                return R::failure ("channels of different lengths");
        if (sampleRate < 8000 || sampleRate > 384000)
            return R::failure ("a sample rate of " + std::to_string (sampleRate) + " Hz");

        const bool isFloat = format == WavFormat::float32;
        const std::uint32_t numChannels = static_cast<std::uint32_t> (channels.size());
        const std::uint32_t bytesPerSample = isFloat ? 4u : 3u;
        const std::uint32_t blockAlign = numChannels * bytesPerSample;
        const std::uint64_t dataBytes = static_cast<std::uint64_t> (frames) * blockAlign;
        const std::uint32_t fmtBytes = isFloat ? 18u : 16u;
        const std::uint64_t headerBytes = 4 + (8 + fmtBytes) + (isFloat ? 12u : 0u) + 8;
        if (headerBytes + dataBytes + (dataBytes & 1u) > 0xffffffffull)
            return R::failure ("too long for a WAV file: " + std::to_string (frames) + " frames");

        std::vector<std::uint8_t> b;
        b.reserve (static_cast<std::size_t> (8 + headerBytes + dataBytes + 1));
        putTag (b, "RIFF");
        put32 (b, static_cast<std::uint32_t> (headerBytes + dataBytes + (dataBytes & 1u)));
        putTag (b, "WAVE");
        putTag (b, "fmt ");
        put32 (b, fmtBytes);
        put16 (b, isFloat ? 3 : 1); // WAVE_FORMAT_IEEE_FLOAT or WAVE_FORMAT_PCM
        put16 (b, static_cast<std::uint16_t> (numChannels));
        put32 (b, static_cast<std::uint32_t> (sampleRate));
        put32 (b, static_cast<std::uint32_t> (sampleRate) * blockAlign);
        put16 (b, static_cast<std::uint16_t> (blockAlign));
        put16 (b, static_cast<std::uint16_t> (8 * bytesPerSample));
        if (isFloat)
        {
            put16 (b, 0); // cbSize
            putTag (b, "fact");
            put32 (b, 4);
            put32 (b, static_cast<std::uint32_t> (frames));
        }
        putTag (b, "data");
        put32 (b, static_cast<std::uint32_t> (dataBytes));
        for (std::size_t i = 0; i < frames; ++i)
            for (const auto& c : channels)
            {
                if (isFloat)
                {
                    std::uint32_t bits = 0;
                    std::memcpy (&bits, &c[i], 4);
                    put32 (b, bits);
                }
                else
                {
                    const auto v = static_cast<std::uint32_t> (pcm24 (c[i]));
                    b.push_back (static_cast<std::uint8_t> (v & 0xffu));
                    b.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xffu));
                    b.push_back (static_cast<std::uint8_t> ((v >> 16) & 0xffu));
                }
            }
        if (dataBytes & 1u)
            b.push_back (0); // RIFF chunks are padded to an even size
        return R::success (std::move (b));
    }

    util::Result<std::size_t> writeWav (const std::filesystem::path& path, std::span<const std::span<const float>> channels, int sampleRate,
        WavFormat format)
    {
        auto bytes = wavBytes (channels, sampleRate, format);
        if (! bytes.ok())
            return util::Result<std::size_t>::failure (bytes.error);
        return util::writeBinaryFile (path, bytes.value);
    }
}
