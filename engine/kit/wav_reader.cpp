#include "kit/wav_reader.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace wubansia::engine
{
    namespace
    {
        using InfoResult = util::Result<WavInfo>;

        constexpr std::string_view kLfsSpec = "version https://git-lfs.github.com/spec/v1";
        constexpr std::string_view kHawserSpec = "version https://hawser.github.com/spec/v1"; // git-lfs still reads these

        // KSDATAFORMAT_SUBTYPE_PCM and _IEEE_FLOAT: {0000000X-0000-0010-8000-00AA00389B71}, as stored (little-endian
        // fields) after the 4-byte format code.
        constexpr std::array<std::uint8_t, 12> kSubformatTail { 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };

        constexpr std::uint32_t kTagPcm = 1;
        constexpr std::uint32_t kTagFloat = 3;
        constexpr std::uint32_t kTagExtensible = 0xFFFE;

        std::uint32_t le16 (const std::uint8_t* p) noexcept { return static_cast<std::uint32_t> (p[0]) | (static_cast<std::uint32_t> (p[1]) << 8); }

        std::uint32_t le32 (const std::uint8_t* p) noexcept
        {
            return static_cast<std::uint32_t> (p[0]) | (static_cast<std::uint32_t> (p[1]) << 8) | (static_cast<std::uint32_t> (p[2]) << 16)
                 | (static_cast<std::uint32_t> (p[3]) << 24);
        }

        bool startsWith (std::span<const std::uint8_t> bytes, std::string_view prefix) noexcept
        {
            return bytes.size() >= prefix.size() && std::memcmp (bytes.data(), prefix.data(), prefix.size()) == 0;
        }

        // A chunk id for messages: 'LIST', or its bytes in hex if they aren't printable.
        std::string chunkName (const std::uint8_t* id)
        {
            std::string s = "'";
            for (int i = 0; i < 4; ++i)
            {
                if (id[i] < 0x20 || id[i] > 0x7E)
                {
                    static constexpr char hex[] = "0123456789ABCDEF";
                    std::string h = "0x";
                    for (int k = 0; k < 4; ++k)
                    {
                        h += hex[id[k] >> 4];
                        h += hex[id[k] & 15];
                    }
                    return h;
                }
                s += static_cast<char> (id[i]);
            }
            return s + "'";
        }

        std::string plural (std::uint64_t n, std::string_view one, std::string_view many)
        {
            return std::to_string (n) + " " + std::string (n == 1 ? one : many);
        }

        // An LFS pointer's message: what it stands for, and how to get the audio.
        std::string lfsMessage (std::span<const std::uint8_t> head)
        {
            std::string text (reinterpret_cast<const char*> (head.data()), head.size());
            std::string size;
            const auto at = text.find ("\nsize ");
            if (at != std::string::npos)
                for (std::size_t i = at + 6; i < text.size() && text[i] >= '0' && text[i] <= '9' && size.size() < 15; ++i)
                    size += text[i];
            return "lfs_pointer: a Git LFS pointer file" + (size.empty() ? std::string() : " for " + size + " bytes of audio")
                 + ", not the audio itself: fetch the samples (git lfs pull) or install the content";
        }

        std::string encodingName (std::uint32_t tag)
        {
            switch (tag)
            {
                case 2: return "Microsoft ADPCM";
                case 6: return "A-law";
                case 7: return "mu-law";
                case 0x11: return "IMA ADPCM";
                case 0x31: return "GSM 6.10";
                case 0x55: return "MPEG layer III";
                default: return "format tag " + std::to_string (tag);
            }
        }

        constexpr std::string_view kTakes = "the reader takes 16- and 24-bit PCM and 32-bit float";

        // Walks the chunks of a WAV file of fileSize bytes through read and gives its header, checked.
        InfoResult parseHeader (std::uint64_t fileSize, FunctionRef<bool (std::uint64_t, std::span<std::uint8_t>)> read)
        {
            const auto fail = [] (std::string message) { return InfoResult::failure (std::move (message)); };
            // Every read is within the file, so a refused one is an I/O error.
            const auto unreadable = [&] (std::uint64_t at) { return fail ("unreadable: reading byte " + std::to_string (at) + " of the file failed"); };

            if (fileSize == 0)
                return fail ("format: the file is empty");

            std::array<std::uint8_t, 256> head {};
            const auto headSize = static_cast<std::size_t> (std::min<std::uint64_t> (fileSize, head.size()));
            if (! read (0, { head.data(), headSize }))
                return unreadable (0);
            const std::span<const std::uint8_t> start (head.data(), headSize);
            if (isLfsPointer (start))
                return fail (lfsMessage (start));
            if (startsWith (start, "RIFX"))
                return fail ("format: a big-endian RIFX file; the content is little-endian RIFF WAVE");
            if (startsWith (start, "RF64") || startsWith (start, "BW64"))
                return fail ("format: an RF64 file (a WAV over 4 GB); the content is plain RIFF WAVE");
            if (! startsWith (start, "RIFF"))
                return fail ("format: not a RIFF WAVE file");
            if (headSize < 12)
                return fail ("truncated: the file ends inside its RIFF header (" + plural (fileSize, "byte", "bytes") + ")");
            if (std::memcmp (head.data() + 8, "WAVE", 4) != 0)
                return fail ("format: a RIFF " + chunkName (head.data() + 8) + " file, not RIFF WAVE");

            WavInfo info;
            std::uint32_t blockAlign = 0;
            bool haveFmt = false;
            bool haveData = false;
            std::uint64_t dataSize = 0;
            std::uint64_t pos = 12;
            while (pos + 8 <= fileSize && ! (haveFmt && haveData))
            {
                std::array<std::uint8_t, 8> header {};
                if (! read (pos, header))
                    return unreadable (pos);
                const std::uint64_t size = le32 (header.data() + 4);
                const std::uint64_t payload = pos + 8;
                const bool fits = size <= fileSize - payload;

                if (std::memcmp (header.data(), "fmt ", 4) == 0)
                {
                    if (! fits)
                        return fail ("truncated: the file ends inside its fmt chunk");
                    if (size < 16)
                        return fail ("format: the fmt chunk is " + plural (size, "byte", "bytes") + "; a WAV's has at least 16");
                    std::array<std::uint8_t, 40> fmt {};
                    if (! read (payload, { fmt.data(), static_cast<std::size_t> (std::min<std::uint64_t> (size, fmt.size())) }))
                        return unreadable (payload);
                    auto tag = le16 (fmt.data());
                    const auto channels = le16 (fmt.data() + 2);
                    const auto rate = le32 (fmt.data() + 4);
                    blockAlign = le16 (fmt.data() + 12);
                    const auto bits = le16 (fmt.data() + 14);
                    if (tag == kTagExtensible)
                    {
                        if (size < 40)
                            return fail ("format: the extensible fmt chunk is " + plural (size, "byte", "bytes") + "; WAVE_FORMAT_EXTENSIBLE needs 40");
                        const auto validBits = le16 (fmt.data() + 18);
                        const auto code = le32 (fmt.data() + 24);
                        if (std::memcmp (fmt.data() + 28, kSubformatTail.data(), kSubformatTail.size()) != 0
                            || (code != kTagPcm && code != kTagFloat))
                        {
                            std::string guid;
                            static constexpr char hex[] = "0123456789ABCDEF";
                            for (int i = 0; i < 16; ++i)
                            {
                                guid += hex[fmt[static_cast<std::size_t> (24 + i)] >> 4];
                                guid += hex[fmt[static_cast<std::size_t> (24 + i)] & 15];
                            }
                            const bool known = std::memcmp (fmt.data() + 28, kSubformatTail.data(), kSubformatTail.size()) == 0;
                            return fail ("format: a WAVE_FORMAT_EXTENSIBLE file of subformat "
                                         + (known ? encodingName (code) : guid) + ", not PCM or IEEE float; " + std::string (kTakes));
                        }
                        if (validBits > bits)
                            return fail ("format: " + std::to_string (validBits) + " valid bits in " + std::to_string (bits) + "-bit samples");
                        tag = code;
                    }
                    if (tag != kTagPcm && tag != kTagFloat)
                        return fail ("format: " + encodingName (tag) + "; " + std::string (kTakes));
                    if (channels < 1 || channels > 2)
                        return fail ("format: " + plural (channels, "channel", "channels") + "; a content file is mono or stereo");
                    if (tag == kTagPcm && bits != 16 && bits != 24)
                        return fail ("format: " + std::to_string (bits) + "-bit PCM; " + std::string (kTakes));
                    if (tag == kTagFloat && bits != 32)
                        return fail ("format: " + std::to_string (bits) + "-bit float; " + std::string (kTakes));
                    if (blockAlign != channels * bits / 8)
                        return fail ("format: a block align of " + std::to_string (blockAlign) + " bytes, not " + plural (channels, "channel", "channels")
                                     + " of " + std::to_string (bits / 8) + " bytes");
                    if (rate != static_cast<std::uint32_t> (kContentSampleRate))
                        return fail ("rate: " + std::to_string (rate) + " Hz; the content is " + std::to_string (kContentSampleRate) + " Hz");
                    info.channels = static_cast<std::int32_t> (channels);
                    info.sampleRate = static_cast<std::int32_t> (rate);
                    info.bitsPerSample = static_cast<std::int32_t> (bits);
                    info.isFloat = tag == kTagFloat;
                    haveFmt = true;
                }
                else if (std::memcmp (header.data(), "data", 4) == 0)
                {
                    haveData = true;
                    info.dataOffset = payload;
                    dataSize = size;
                    if (! fits)
                        break; // nothing after it: what's missing is said below
                }
                else if (! fits)
                    return fail ("truncated: the file ends inside its " + chunkName (header.data()) + " chunk, before its "
                                 + (haveFmt ? "data chunk" : haveData ? "fmt chunk" : "fmt and data chunks"));

                pos = payload + size + (size & 1u); // odd chunks have a pad byte
            }

            if (haveData && dataSize > fileSize - info.dataOffset)
                return fail ("truncated: the data chunk holds " + plural (dataSize, "byte", "bytes") + ", but the file ends after "
                             + std::to_string (fileSize - info.dataOffset));
            if (! haveFmt)
                return fail (haveData ? "format: there is no fmt chunk" : "truncated: the file ends before its fmt and data chunks");
            if (! haveData)
                return fail ("truncated: the file ends before its data chunk");
            if (dataSize % blockAlign != 0)
                return fail ("format: the data chunk's " + plural (dataSize, "byte", "bytes") + " aren't whole " + std::to_string (blockAlign) + "-byte frames");
            info.dataBytes = dataSize;
            info.frames = static_cast<std::int64_t> (dataSize / blockAlign);
            return InfoResult::success (info);
        }
    }

    bool isLfsPointer (std::span<const std::uint8_t> bytes) noexcept
    {
        return startsWith (bytes, kLfsSpec) || startsWith (bytes, kHawserSpec);
    }

    util::Result<WavInfo> readWavInfo (std::uint64_t fileSize, FunctionRef<bool (std::uint64_t offset, std::span<std::uint8_t> out)> read)
    {
        return parseHeader (fileSize, read);
    }

    util::Result<WavInfo> readWavInfo (std::span<const std::uint8_t> bytes)
    {
        return parseHeader (bytes.size(), [bytes] (std::uint64_t offset, std::span<std::uint8_t> out) {
            if (offset > bytes.size() || out.size() > bytes.size() - offset)
                return false;
            std::memcpy (out.data(), bytes.data() + offset, out.size());
            return true;
        });
    }

    util::Result<SampleData> readWav (std::span<const std::uint8_t> bytes)
    {
        using R = util::Result<SampleData>;
        const auto header = readWavInfo (bytes);
        if (! header.ok())
            return R::failure (header.error);
        const auto& info = header.value;

        SampleData out;
        out.frames = info.frames;
        out.channels = info.channels;
        out.sampleRate = info.sampleRate;
        const auto frames = static_cast<std::size_t> (info.frames);
        const auto channels = static_cast<std::size_t> (info.channels);
        out.audio.resize (frames * channels);

        // Interleaved in the file, planar out: channel c's frame f goes to audio[c * frames + f].
        const std::uint8_t* p = bytes.data() + info.dataOffset;
        float* const audio = out.audio.data();
        if (info.isFloat)
        {
            for (std::size_t f = 0; f < frames; ++f)
                for (std::size_t c = 0; c < channels; ++c, p += 4)
                {
                    const float v = std::bit_cast<float> (le32 (p));
                    if (! std::isfinite (v))
                        return R::failure ("format: a " + std::string (std::isnan (v) ? "NaN" : "infinite") + " sample at frame " + std::to_string (f)
                                           + ", channel " + std::to_string (c) + "; content floats are finite");
                    audio[c * frames + f] = v;
                }
        }
        else if (info.bitsPerSample == 16)
        {
            // soundfile's s / 32768, exact in float.
            for (std::size_t f = 0; f < frames; ++f)
                for (std::size_t c = 0; c < channels; ++c, p += 2)
                    audio[c * frames + f] = static_cast<float> (static_cast<std::int16_t> (le16 (p))) * (1.0f / 32768.0f);
        }
        else
        {
            // soundfile's s / 8388608 (libsndfile: (s << 8) / 2^31), exact in float.
            for (std::size_t f = 0; f < frames; ++f)
                for (std::size_t c = 0; c < channels; ++c, p += 3)
                {
                    const auto u = static_cast<std::uint32_t> (p[0]) | (static_cast<std::uint32_t> (p[1]) << 8) | (static_cast<std::uint32_t> (p[2]) << 16);
                    const auto s = static_cast<std::int32_t> (u ^ 0x800000u) - 0x800000; // sign-extend 24 bits
                    audio[c * frames + f] = static_cast<float> (s) * (1.0f / 8388608.0f);
                }
        }
        return R::success (std::move (out));
    }
}
