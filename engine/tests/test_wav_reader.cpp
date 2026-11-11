// The engine's WAV reader (kit/wav_reader.h) and FileSampleSource: the same floats as Python's soundfile for every
// encoding the content uses, and a precise, coded error for every way a file can be wrong.
//
// fixtures/wav/ holds tiny WAVs written by libsndfile (through soundfile) and by hand, and expected.json with each
// one's SHA-256 and what soundfile reads from it, as float bit patterns (make_fixtures.py writes both).

#include "kit/file_sample_source.h"
#include "kit/wav_reader.h"

#include "gen/json.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if ! defined(_WIN32)
    #include <sys/stat.h>
#endif

using namespace wubansia;
using namespace wubansia::engine;

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    const std::filesystem::path kFixtures = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "engine" / "tests" / "fixtures" / "wav";

    Bytes readFixture (const std::string& name)
    {
        auto read = util::readBinaryFile (kFixtures / name);
        REQUIRE (read.ok());
        return std::move (read.value);
    }

    std::string_view codeOf (const std::string& error)
    {
        const auto colon = error.find (':');
        return colon == std::string::npos ? std::string_view {} : std::string_view (error).substr (0, colon);
    }

    // Little-endian builders for files the fixtures don't cover.
    void put16 (Bytes& b, std::uint32_t v)
    {
        b.push_back (static_cast<std::uint8_t> (v & 0xFFu));
        b.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xFFu));
    }

    void put32 (Bytes& b, std::uint32_t v)
    {
        put16 (b, v & 0xFFFFu);
        put16 (b, v >> 16);
    }

    void putId (Bytes& b, std::string_view id)
    {
        for (const char c : id)
            b.push_back (static_cast<std::uint8_t> (c));
    }

    Bytes chunk (std::string_view id, const Bytes& payload)
    {
        Bytes b;
        putId (b, id);
        put32 (b, static_cast<std::uint32_t> (payload.size()));
        b.insert (b.end(), payload.begin(), payload.end());
        if (payload.size() % 2 != 0)
            b.push_back (0);
        return b;
    }

    Bytes riff (std::initializer_list<Bytes> chunks, std::string_view form = "WAVE", std::string_view magic = "RIFF")
    {
        Bytes body;
        putId (body, form);
        for (const auto& c : chunks)
            body.insert (body.end(), c.begin(), c.end());
        Bytes b;
        putId (b, magic);
        put32 (b, static_cast<std::uint32_t> (body.size()));
        b.insert (b.end(), body.begin(), body.end());
        return b;
    }

    Bytes fmt (std::uint32_t tag, std::uint32_t channels, std::uint32_t bits, std::uint32_t rate = 48000, std::uint32_t align = 0)
    {
        if (align == 0)
            align = channels * bits / 8;
        Bytes p;
        put16 (p, tag);
        put16 (p, channels);
        put32 (p, rate);
        put32 (p, rate * align);
        put16 (p, align);
        put16 (p, bits);
        return chunk ("fmt ", p);
    }

    Bytes pcm16 (std::initializer_list<int> samples)
    {
        Bytes p;
        for (const int s : samples)
            put16 (p, static_cast<std::uint32_t> (static_cast<std::uint16_t> (static_cast<std::int16_t> (s))));
        return p;
    }

    Bytes floats (std::initializer_list<float> samples)
    {
        Bytes p;
        for (const float f : samples)
            put32 (p, std::bit_cast<std::uint32_t> (f));
        return p;
    }

    std::vector<std::uint32_t> bitsOf (const std::vector<float>& audio)
    {
        std::vector<std::uint32_t> bits;
        for (const float f : audio)
            bits.push_back (std::bit_cast<std::uint32_t> (f));
        return bits;
    }

    // A scratch folder in the build tree, emptied first.
    std::filesystem::path scratch (std::string_view name)
    {
        const auto dir = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "wav_reader" / std::string (name);
        std::filesystem::remove_all (dir);
        std::filesystem::create_directories (dir);
        return dir;
    }

    void writeFile (const std::filesystem::path& path, const Bytes& bytes)
    {
        std::filesystem::create_directories (path.parent_path());
        REQUIRE (util::writeBinaryFile (path, bytes).ok());
    }

    const std::string kLfsPointer = "version https://git-lfs.github.com/spec/v1\n"
                                    "oid sha256:4d7a214614ab2935c943f9e0ff69d22eadbb8f32b1258daaa5e2ca24d17e2393\n"
                                    "size 12345\n";

    Bytes bytesOf (std::string_view s) { return Bytes (s.begin(), s.end()); }
}

TEST_CASE ("readWav reads every fixture as soundfile does", "[wav]")
{
    const auto text = readFixture ("expected.json");
    const auto parsed = gen::json::parse (std::string_view (reinterpret_cast<const char*> (text.data()), text.size()));
    REQUIRE (parsed.ok());
    const auto* files = parsed.value.find ("files");
    REQUIRE (files != nullptr);
    REQUIRE (files->items.size() == 19);

    int readable = 0;
    for (const auto& f : files->items)
    {
        const std::string name = f.find ("name")->string;
        const std::string result = f.find ("result")->string;
        CAPTURE (name);
        const auto bytes = readFixture (name);
        // The fixture is the one make_fixtures.py wrote: a changed file fails here, not with a puzzling value.
        CHECK (util::sha256Hex (bytes) == f.find ("sha256")->string);

        const auto read = readWav (bytes);
        const auto info = readWavInfo (bytes);
        if (result == "ok")
        {
            ++readable;
            REQUIRE (read.ok());
            REQUIRE (info.ok());
            const auto& data = read.value;
            CHECK (data.frames == f.find ("frames")->integer);
            CHECK (data.channels == f.find ("channels")->integer);
            CHECK (data.sampleRate == 48000);
            CHECK (info.value.frames == data.frames);
            CHECK (info.value.channels == data.channels);
            CHECK (info.value.sampleRate == 48000);
            std::vector<std::uint32_t> expected;
            for (const auto& b : f.find ("bits")->items)
                expected.push_back (static_cast<std::uint32_t> (b.integer));
            CHECK (bitsOf (data.audio) == expected);
        }
        else
        {
            CHECK_FALSE (read.ok());
            CHECK (codeOf (read.error) == result);
            CHECK_FALSE (info.ok());
            CHECK (codeOf (info.error) == result);
        }
    }
    CHECK (readable == 10);
}

TEST_CASE ("readWav: the values each encoding gives", "[wav]")
{
    SECTION ("16-bit PCM is s / 32768, extremes included")
    {
        const auto read = readWav (riff ({ fmt (1, 1, 16), chunk ("data", pcm16 ({ -32768, -1, 0, 1, 16384, 32767 })) }));
        REQUIRE (read.ok());
        CHECK (read.value.audio == std::vector<float> { -1.0f, -1.0f / 32768, 0.0f, 1.0f / 32768, 0.5f, 32767.0f / 32768 });
    }

    SECTION ("24-bit PCM is s / 8388608, and stereo comes out planar")
    {
        Bytes p;
        for (const std::int32_t s : { -8388608, 8388607, 1, -2 }) // L R L R
        {
            const auto u = static_cast<std::uint32_t> (s);
            p.push_back (static_cast<std::uint8_t> (u & 0xFFu));
            p.push_back (static_cast<std::uint8_t> ((u >> 8) & 0xFFu));
            p.push_back (static_cast<std::uint8_t> ((u >> 16) & 0xFFu));
        }
        const auto read = readWav (riff ({ fmt (1, 2, 24), chunk ("data", p) }));
        REQUIRE (read.ok());
        CHECK (read.value.frames == 2);
        CHECK (read.value.channels == 2);
        CHECK (read.value.audio == std::vector<float> { -1.0f, 1.0f / 8388608, 8388607.0f / 8388608, -2.0f / 8388608 });
    }

    SECTION ("an empty data chunk is a file of no frames")
    {
        const auto read = readWav (riff ({ fmt (1, 2, 16), chunk ("data", {}) }));
        REQUIRE (read.ok());
        CHECK (read.value.frames == 0);
        CHECK (read.value.audio.empty());
    }
}

TEST_CASE ("readWav names what's wrong, with a code", "[wav]")
{
    struct Case
    {
        std::string_view what;
        Bytes bytes;
        std::string_view code;
        std::string_view says; // a piece of the message
    };
    const auto data = chunk ("data", pcm16 ({ 1, 2, 3, 4 }));
    const auto good = riff ({ fmt (1, 1, 16), data });
    Bytes headerOnly (good.begin(), good.begin() + 30); // inside the fmt chunk
    Bytes cutData (good.begin(), good.end() - 3);
    Bytes oddData = riff ({ fmt (1, 2, 16), chunk ("data", pcm16 ({ 1, 2, 3 })) }); // 3 samples in 2-channel frames
    const Case cases[] {
        { "a Git LFS pointer", bytesOf (kLfsPointer), "lfs_pointer", "git lfs pull" },
        { "the pre-release LFS spec", bytesOf ("version https://hawser.github.com/spec/v1\noid sha256:00\nsize 1\n"), "lfs_pointer", "Git LFS pointer" },
        { "an empty file", {}, "format", "empty" },
        { "text", bytesOf ("hello, this is not a WAV file at all"), "format", "RIFF WAVE" },
        { "FLAC", bytesOf ("fLaC\0\0\0\x22 and so on"), "format", "RIFF WAVE" },
        { "big-endian RIFX", riff ({ fmt (1, 1, 16), data }, "WAVE", "RIFX"), "format", "RIFX" },
        { "RF64", riff ({ fmt (1, 1, 16), data }, "WAVE", "RF64"), "format", "RF64" },
        { "a RIFF that isn't WAVE", riff ({ data }, "AVI "), "format", "RIFF WAVE" },
        { "no fmt chunk", riff ({ data }), "format", "fmt" },
        { "a fmt chunk too short", riff ({ chunk ("fmt ", { 1, 0, 1, 0 }), data }), "format", "fmt" },
        { "an extensible fmt too short", riff ({ chunk ("fmt ", [] { auto f = fmt (0xFFFE, 1, 16); return Bytes (f.begin() + 8, f.end()); }()), data }), "format", "extensible" },
        { "no channels", riff ({ fmt (1, 0, 16, 48000, 2), data }), "format", "channels" },
        { "a block align that isn't channels x bytes", riff ({ fmt (1, 1, 16, 48000, 4), data }), "format", "block" },
        { "data that isn't whole frames", oddData, "format", "frames" },
        { "44.1 kHz", riff ({ fmt (1, 1, 16, 44100), data }), "rate", "44100" },
        { "0 Hz", riff ({ fmt (1, 1, 16, 0), data }), "rate", "0 Hz" },
        { "a NaN in a float file", riff ({ fmt (3, 1, 32), chunk ("data", floats ({ 0.0f, std::numeric_limits<float>::quiet_NaN() })) }), "format", "frame 1" },
        { "an infinity in a float file", riff ({ fmt (3, 1, 32), chunk ("data", floats ({ -std::numeric_limits<float>::infinity() })) }), "format", "frame 0" },
        { "a file cut inside its fmt chunk", headerOnly, "truncated", "fmt" },
        { "a file cut inside its data", cutData, "truncated", "data" },
        { "a header with no data chunk", riff ({ fmt (1, 1, 16) }), "truncated", "data" },
        { "a chunk running past the end before the data", [&] { auto b = riff ({ fmt (1, 1, 16), chunk ("LIST", Bytes (40, 0)) }); b.resize (b.size() - 20); return b; }(), "truncated", "LIST" },
    };
    for (const auto& c : cases)
    {
        CAPTURE (c.what);
        const auto read = readWav (c.bytes);
        REQUIRE_FALSE (read.ok());
        CAPTURE (read.error);
        CHECK (codeOf (read.error) == c.code);
        CHECK (read.error.find (c.says) != std::string::npos);
        CHECK (read.error.find ('\n') == std::string::npos);
        CHECK (read.value.audio.empty());

        const auto info = readWavInfo (c.bytes);
        // The header alone can't see a NaN: those files' headers are fine.
        if (c.says.starts_with ("frame "))
            CHECK (info.ok());
        else
        {
            CHECK_FALSE (info.ok());
            CHECK (codeOf (info.error) == c.code);
        }
    }
}

TEST_CASE ("isLfsPointer knows the pointer files and nothing else", "[wav]")
{
    CHECK (isLfsPointer (bytesOf (kLfsPointer)));
    CHECK (isLfsPointer (bytesOf ("version https://git-lfs.github.com/spec/v1\n")));
    CHECK_FALSE (isLfsPointer (bytesOf ("version https://git-lfs.github.com/spec/v")));
    CHECK_FALSE (isLfsPointer (bytesOf ("RIFF")));
    CHECK_FALSE (isLfsPointer ({}));
    CHECK_FALSE (isLfsPointer (readFixture ("pcm16_mono.wav")));
}

TEST_CASE ("readWavInfo through a reader reads only the headers", "[wav]")
{
    // A 1 MB LIST chunk before the fmt: the reader skips it without reading it.
    Bytes big = riff ({ chunk ("LIST", Bytes (1 << 20, 0x55)), fmt (1, 2, 24), chunk ("data", Bytes (6 * 1000, 0)) });
    std::uint64_t bytesRead = 0;
    int reads = 0;
    const auto info = readWavInfo (big.size(), [&] (std::uint64_t offset, std::span<std::uint8_t> out) {
        ++reads;
        if (offset > big.size() || out.size() > big.size() - offset)
            return false;
        std::memcpy (out.data(), big.data() + offset, out.size());
        bytesRead += out.size();
        return true;
    });
    REQUIRE (info.ok());
    CHECK (info.value.frames == 1000);
    CHECK (info.value.channels == 2);
    CHECK (info.value.bitsPerSample == 24);
    CHECK_FALSE (info.value.isFloat);
    CHECK (info.value.dataOffset == big.size() - 6000);
    CHECK (info.value.dataBytes == 6000);
    CHECK (bytesRead < 1024); // the first 256 bytes, then chunk headers and the fmt chunk: never the 1 MB LIST
    CHECK (reads < 10);

    // A plain file's headers all sit in the first 256 bytes: one read.
    const auto plain = readFixture ("float32_stereo.wav");
    int plainReads = 0;
    const auto plainInfo = readWavInfo (plain.size(), [&] (std::uint64_t offset, std::span<std::uint8_t> out) {
        ++plainReads;
        if (offset > plain.size() || out.size() > plain.size() - offset)
            return false;
        std::memcpy (out.data(), plain.data() + offset, out.size());
        return true;
    });
    REQUIRE (plainInfo.ok());
    CHECK (plainInfo.value == readWavInfo (plain).value);
    CHECK (plainReads == 1);
}

TEST_CASE ("FileSampleSource reads a kit folder by its manifest paths", "[wav]")
{
    const auto root = scratch ("kit");
    writeFile (root / "Drums" / "Kick v1 rr1.wav", readFixture ("pcm24_stereo.wav"));
    writeFile (root / "Bass" / "Sub C#2 v1.wav", readFixture ("pcm16_mono.wav"));
    writeFile (root / "Vox" / u8"Ahh été.wav", readFixture ("float32_mono.wav")); // a UTF-8 name
    writeFile (root / "Drums" / "Pointer.wav", bytesOf (kLfsPointer));
    writeFile (root / "Drums" / "Slow.wav", readFixture ("rate_44100.wav"));
    std::filesystem::create_directories (root / "Drums" / "Folder.wav");
    FileSampleSource source (root);
    CHECK (source.root() == root);

    SECTION ("stereo and mono, as readWav gives them")
    {
        const auto kick = source.read ("Drums/Kick v1 rr1.wav");
        REQUIRE (kick.ok());
        CHECK (kick.value == readWav (readFixture ("pcm24_stereo.wav")).value);
        const auto sub = source.read ("Bass/Sub C#2 v1.wav");
        REQUIRE (sub.ok());
        CHECK (sub.value.channels == 1);
        CHECK (sub.value == readWav (readFixture ("pcm16_mono.wav")).value);
        const auto vox = source.read (reinterpret_cast<const char*> (u8"Vox/Ahh été.wav"));
        REQUIRE (vox.ok());
        CHECK (vox.value.frames == 11);
    }

    SECTION ("probe gives the header without the audio")
    {
        const auto kick = source.probe ("Drums/Kick v1 rr1.wav");
        REQUIRE (kick.ok());
        CHECK (kick.value.frames == 8);
        CHECK (kick.value.channels == 2);
        CHECK (kick.value == readWavInfo (readFixture ("pcm24_stereo.wav")).value);
    }

    SECTION ("errors start with their code, then the path")
    {
        struct Case
        {
            std::string_view path;
            std::string_view starts;
        };
        const Case cases[] {
            { "Drums/Nothing.wav", "missing: Drums/Nothing.wav: " },
            { "Drums/Pointer.wav", "lfs_pointer: Drums/Pointer.wav: " },
            { "Drums/Slow.wav", "rate: Drums/Slow.wav: " },
            { "Drums/Folder.wav", "unreadable: Drums/Folder.wav: " },
            { "../kit/Drums/Kick v1 rr1.wav", "missing: ../kit/Drums/Kick v1 rr1.wav: " },
            { "/etc/passwd", "missing: /etc/passwd: " },
            { "Drums\\Kick v1 rr1.wav", "missing: Drums\\Kick v1 rr1.wav: " },
            { "", "missing: : " },
        };
        for (const auto& c : cases)
        {
            CAPTURE (c.path);
            const auto read = source.read (c.path);
            REQUIRE_FALSE (read.ok());
            CHECK (read.error.starts_with (c.starts));
            const auto probed = source.probe (c.path);
            REQUIRE_FALSE (probed.ok());
            CHECK (probed.error.starts_with (c.starts));
        }
        // The pointer message says what to do.
        CHECK (source.read ("Drums/Pointer.wav").error.find ("git lfs pull") != std::string::npos);
    }
}

TEST_CASE ("FileSampleSource on the checkout's samples: LFS pointers or real WAVs", "[wav]")
{
    // CI checks out without LFS, so the repo's samples are pointer files there; a dev checkout may have pulled them.
    // Either way the source says which, for one file of each kit.
    const auto samples = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "samples";
    if (! std::filesystem::is_directory (samples))
        SKIP ("no content/samples in this checkout");
    int checked = 0;
    for (const auto* kit : { "fut01", "fut02", "fut03", "fut04" })
    {
        const auto folder = samples / kit / "Loops";
        if (! std::filesystem::is_directory (folder))
            continue;
        for (const auto& entry : std::filesystem::directory_iterator (folder))
        {
            if (entry.path().extension() != ".wav")
                continue;
            auto head = util::readBinaryFile (entry.path());
            REQUIRE (head.ok());
            FileSampleSource source (folder);
            const auto name = entry.path().filename().u8string();
            const std::string relative (name.begin(), name.end());
            CAPTURE (relative);
            const auto probed = source.probe (relative);
            if (isLfsPointer (head.value))
            {
                REQUIRE_FALSE (probed.ok());
                CHECK (probed.error.starts_with ("lfs_pointer: " + relative + ": "));
            }
            else
            {
                REQUIRE (probed.ok());
                CHECK (probed.value.sampleRate == 48000);
            }
            ++checked;
            break;
        }
    }
    if (checked == 0)
        SKIP ("no Loops/ WAVs in this checkout");
}

TEST_CASE ("FileSampleSource reads in chunks, stops on its cancel flag, and names I/O problems in fixed English", "[wav]")
{
    const auto root = scratch ("chunks");
    // A 24-bit stereo file of 200 000 frames: 1.2 MB, several read chunks.
    Bytes data;
    for (std::uint32_t i = 0; i < 200000 * 2; ++i)
    {
        const auto v = (i * 2654435761u) >> 8;
        data.push_back (static_cast<std::uint8_t> (v & 0xFFu));
        data.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xFFu));
        data.push_back (static_cast<std::uint8_t> ((v >> 16) & 0xFFu));
    }
    const auto big = riff ({ fmt (1, 2, 24), chunk ("data", data) });
    REQUIRE (big.size() > 4 * FileSampleSource::kReadChunkBytes);
    writeFile (root / "Loops" / "Big.wav", big);

    std::atomic<bool> cancel { false };
    FileSampleSource source (root, &cancel);
    const auto read = source.read ("Loops/Big.wav");
    REQUIRE (read.ok());
    CHECK (read.value == readWav (big).value);

    cancel = true;
    const auto cancelled = source.read ("Loops/Big.wav");
    REQUIRE_FALSE (cancelled.ok());
    CHECK (cancelled.error == "cancelled: Loops/Big.wav: the read was cancelled");
    CHECK (source.probe ("Loops/Big.wav").ok()); // a probe is a few small reads: it doesn't stop
    cancel = false;

    // A C1 control in a path is escaped in the message, which stays one line.
    const auto c1 = source.read ("Drums/Kick\xC2\x85" "B.wav");
    REQUIRE_FALSE (c1.ok());
    CHECK (c1.error.starts_with ("missing: Drums/Kick\\u0085B.wav: "));

#if ! defined(_WIN32)
    // Not regular files: a FIFO, and a loop of symbolic links. The messages are fixed English text.
    std::filesystem::create_directories (root / "Odd");
    REQUIRE (::mkfifo ((root / "Odd" / "Pipe.wav").c_str(), 0600) == 0);
    std::filesystem::create_symlink (root / "Odd" / "B.wav", root / "Odd" / "A.wav");
    std::filesystem::create_symlink (root / "Odd" / "A.wav", root / "Odd" / "B.wav");
    CHECK (source.read ("Odd/Pipe.wav").error == "unreadable: Odd/Pipe.wav: not a regular file");
    CHECK (source.probe ("Odd/Pipe.wav").error == "unreadable: Odd/Pipe.wav: not a regular file");
    CHECK (source.read ("Odd/A.wav").error == "unreadable: Odd/A.wav: too many levels of symbolic links");
#endif
}
