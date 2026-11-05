// groovecore render (P12; the ROADMAP's S1d command): a kit played through the engine (wubansia_engine) offline,
// from its committed MIDI and its samples, the way the plugin plays it under a host transport, into a WAV. It is the
// fidelity harness: tools/sound/compare_render.py holds its renders to the approved round 5 loops.
//
// The render is deterministic: the same options, manifests, MIDI and samples give the same bytes on Linux. The host
// is a fake transport at a fixed tempo, playing from the first sample, offline (TransportInfo::nonRealtime), so the
// Kit is adopted on the first sample; nothing reaches the engine as MIDI in. The output is in aligned time by default:
// the engine's fixed latency (Engine::latencySamples) is rendered past the end and dropped from the start, so sample 0
// is the start position's own sample; --keep-latency writes what the host would hear instead.
//
// --solo and --taps read what the voices play into each mix channel before its inserts (the dry buses the mixer
// takes), through the engine's own block loop (engine_loop.h) on voices that copy the buses as they render: the same
// loop, the same voices and the same mixer as Engine's, so the mix output of a tapped render is the same, bit for bit.

#include "commands.h"
#include "render_kit.h"
#include "wav_writer.h"

#include "engine.h"
#include "engine_loop.h"
#include "sampler/sampler.h"
#include "util/file_io.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if ! defined(_WIN32)
    #include <time.h>
#endif

namespace wubansia::cli
{
    namespace
    {
        using engine::kMilliticksPerBar;
        using engine::kMilliticksPerQuarter;

        void printHelp (std::ostream& os)
        {
            os << "Usage: groovecore render --kit <ID> --out <FILE.wav> [options]\n"
                  "\n"
                  "Plays a kit through the engine, offline, from its committed MIDI and its samples, as the plugin plays\n"
                  "it under a host transport at a fixed tempo, and writes a stereo WAV (and FILE.json beside it: the\n"
                  "settings, the latency and the frames). The same options give the same bytes.\n"
                  "\n"
                  "What plays:\n"
                  "  --kit <ID>             the kit, e.g. FUT01 (<manifests>/fut01/FUT01_kit.json and FUT01_mix.json)\n"
                  "  --mode loop|song       the 4-bar Loop (default) or the Song (the extended mix)\n"
                  "  --variant vox|novox    default vox\n"
                  "  --kick a|b --bass a|b --stab a|b\n"
                  "                         the A/B switches (defaults: kick B, bass B, stab A, what the club loops play)\n"
                  "  --passes <N>           Loop: N passes of the loop (default 3)\n"
                  "  --start-bar <N>        where play starts, bar 1 first (default 1)\n"
                  "  --bars <N>             how many bars to render (Loop: instead of --passes; Song: default to the end)\n"
                  "  --section <NAME|N>     Song: start at this section (its marker's name, or its number from 1); with\n"
                  "                         --bars, that many bars of it (default: the whole section)\n"
                  "  --tempo <BPM>          the host's tempo (default: the kit's)\n"
                  "  --phrases auto|off     phrase lanes play their recordings near the kit's tempo (auto, default) or never\n"
                  "\n"
                  "The mix:\n"
                  "  --space <P> --echo <P> --pump <P>\n"
                  "                         the macros, 0 to 200 percent (default 100)\n"
                  "  --club on|off          the club master (default on); off: the mix before it, just as late\n"
                  "  --master lookback|exact\n"
                  "                         the limiter's hold: lookback (the plugin's, default) or exact (master.py's\n"
                  "                         two-sided hold, for verification only; the latency follows it)\n"
                  "  --solo <CHANNEL|LANE>  write instead what the voices play into one mix channel (its mix.json id) or,\n"
                  "                         for a name no channel has, a lane's channels (kick ... fx), before any insert:\n"
                  "                         the dry bus, in aligned time\n"
                  "  --taps <DIR>           also write every mix channel's dry bus to DIR/<channel>.wav and the voices'\n"
                  "                         throws to DIR/send_space.wav, send_echo.wav (at most 64 bars)\n"
                  "  --taps-of <LIST>       with --taps, only these: lanes (each of their channels) and mix channel ids,\n"
                  "                         comma-separated, and \"sends\" for the throws\n"
                  "\n"
                  "The host:\n"
                  "  --rate <HZ>            the sample rate, 11025 to 192000 (default 48000)\n"
                  "  --block <N>            the host's block size, 1 to 8192 (default 512)\n"
                  "  --keep-latency         keep the engine's latency at the start (default: dropped, so the render is in\n"
                  "                         aligned time)\n"
                  "  --hold <N>             a CPU stress: N notes held from the start through MIDI in, spread over the kit's\n"
                  "                         polyphonic sustained instruments (with the sequence, up to the 64 voices)\n"
                  "\n"
                  "Files:\n"
                  "  --out <FILE>           the WAV to write (its folder must exist)\n"
                  "  --format float|pcm24   32-bit float (default) or 24-bit PCM\n"
                  "  --content <DIR>        the content folder holding <kit>/ or <kit>/v<N>/ (default: $WUBANSIA_CONTENT_DIR,\n"
                  "                         then the platform's content folder)\n"
                  "  --manifests <DIR>      default content/plugin, from the current folder\n"
                  "  --midi <DIR>           default content/midi, from the current folder\n"
                  "  --help                 this text\n";
        }

        bool parseInt (std::string_view text, long long lo, long long hi, long long& out)
        {
            if (text.empty() || text.size() > 18)
                return false;
            const std::string s (text);
            char* end = nullptr;
            const long long v = std::strtoll (s.c_str(), &end, 10);
            if (end != s.c_str() + s.size() || v < lo || v > hi)
                return false;
            out = v;
            return true;
        }

        bool parseReal (std::string_view text, double lo, double hi, double& out)
        {
            if (text.empty() || text.size() > 32)
                return false;
            const std::string s (text);
            char* end = nullptr;
            const double v = std::strtod (s.c_str(), &end);
            if (end != s.c_str() + s.size() || ! std::isfinite (v) || v < lo || v > hi)
                return false;
            out = v;
            return true;
        }

        std::string shown (const std::filesystem::path& p)
        {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        }

        std::string upper (std::string_view s)
        {
            std::string u (s);
            for (auto& c : u)
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char> (c - 'a' + 'A');
            return u;
        }

        std::string jsonString (std::string_view s)
        {
            std::string out = "\"";
            for (const char c : s)
            {
                if (c == '"' || c == '\\')
                    out += '\\';
                if (static_cast<unsigned char> (c) < 0x20)
                {
                    std::ostringstream hex;
                    hex << "\\u" << std::hex << std::setw (4) << std::setfill ('0') << static_cast<int> (c);
                    out += hex.str();
                    continue;
                }
                out += c;
            }
            return out + "\"";
        }

        std::string fixed (double v, int digits)
        {
            std::ostringstream os;
            os << std::fixed << std::setprecision (digits) << v;
            return os.str();
        }

        // ------------------------------------------------------------------ the taps
        // Each mix channel's dry bus (L and R) and each send's input, as the voices render them, from aligned time 0.
        struct Taps
        {
            int channels = 0;
            std::int64_t frames = 0;
            std::vector<std::vector<float>> bus; // 2 per channel: [2c] L, [2c + 1] R
            std::array<std::vector<float>, engine::kSends> sends;

            void prepare (int numChannels, std::int64_t numFrames)
            {
                channels = numChannels;
                frames = numFrames;
                bus.assign (static_cast<std::size_t> (2 * numChannels), std::vector<float> (static_cast<std::size_t> (numFrames), 0.0f));
                for (auto& s : sends)
                    s.assign (static_cast<std::size_t> (numFrames), 0.0f);
            }

            void add (const engine::MixBus& from, const engine::SendBus& sendBus, std::int64_t at, int n) noexcept
            {
                const auto count = std::min<std::int64_t> (n, frames - at);
                if (count <= 0)
                    return;
                const auto k = static_cast<std::size_t> (at);
                const int numChannels = std::min (channels, from.numChannels());
                for (int c = 0; c < numChannels; ++c)
                {
                    std::copy_n (from.left (c), count, bus[static_cast<std::size_t> (2 * c)].begin() + static_cast<std::ptrdiff_t> (k));
                    std::copy_n (from.right (c), count, bus[static_cast<std::size_t> (2 * c + 1)].begin() + static_cast<std::ptrdiff_t> (k));
                }
                for (int s = 0; s < engine::kSends; ++s)
                    std::copy_n (sendBus.send (static_cast<engine::Send> (s)), count,
                        sends[static_cast<std::size_t> (s)].begin() + static_cast<std::ptrdiff_t> (k));
            }
        };

        // P5's sampler, copying the buses after each render (): the engine's block loop clears them before the voices
        // add into them, so after render () they hold exactly what the voices played.
        class TapVoices : public engine::Sampler
        {
        public:
            void setTaps (Taps* t) noexcept { taps = t; }

            void prepare (double sampleRate, int maxBlock)
            {
                engine::Sampler::prepare (sampleRate, maxBlock);
                written = 0;
            }

            void reset() noexcept
            {
                engine::Sampler::reset();
                written = 0;
            }

            void render (engine::MixBus& bus, engine::SendBus& sends, int numSamples) noexcept
            {
                engine::Sampler::render (bus, sends, numSamples);
                if (taps != nullptr)
                    taps->add (bus, sends, written, numSamples);
                written += numSamples;
            }

        private:
            Taps* taps = nullptr;
            std::int64_t written = 0;
        };

        // ------------------------------------------------------------------ the players
        // What the render loop drives: Engine itself, or the same block loop on TapVoices.
        class Player
        {
        public:
            virtual ~Player() = default;
            virtual void prepare (double rate, int block, engine::MasterHold hold) = 0;
            virtual void publish (const engine::Kit* kit) = 0;
            virtual void process (float* const* out, int numSamples, const engine::TransportInfo& transport, const engine::Controls& controls,
                const engine::MidiEvent* midi, int numMidi) = 0;
            virtual int latency() const = 0;
            virtual int voices() const = 0;
        };

        class EnginePlayer final : public Player
        {
        public:
            void prepare (double rate, int block, engine::MasterHold hold) override { eng.prepare (rate, block, 2, hold); }
            void publish (const engine::Kit* kit) override { (void) eng.publish (kit); }
            void process (float* const* out, int n, const engine::TransportInfo& t, const engine::Controls& c, const engine::MidiEvent* midi,
                int numMidi) override
            {
                eng.process (out, 2, n, t, c, midi, numMidi);
            }
            int latency() const override { return eng.latencySamples(); }
            int voices() const override { return eng.telemetry().voices.load (std::memory_order_relaxed); }

        private:
            engine::Engine eng;
        };

        class TapPlayer final : public Player
        {
        public:
            explicit TapPlayer (Taps& taps) { loop.voices().setTaps (&taps); }
            void prepare (double rate, int block, engine::MasterHold hold) override { loop.prepare (rate, block, 2, hold); }
            void publish (const engine::Kit* kit) override { (void) pending.publish (kit); }
            void process (float* const* out, int n, const engine::TransportInfo& t, const engine::Controls& c, const engine::MidiEvent* midi,
                int numMidi) override
            {
                loop.process (out, 2, n, t, c, midi, numMidi, pending, retired, telemetry);
            }
            int latency() const override { return loop.latencySamples(); }
            int voices() const override { return telemetry.voices.load (std::memory_order_relaxed); }

        private:
            engine::BlockLoop<TapVoices> loop;
            engine::PendingSlot<engine::Kit> pending;
            engine::RetireFifo<engine::Kit, engine::Engine::kRetireSlots> retired;
            engine::Telemetry telemetry;
        };

        // ------------------------------------------------------------------ the options
        struct Options
        {
            std::string kit;
            engine::PlayMode mode = engine::PlayMode::loop;
            engine::Variant variant = engine::Variant::vox;
            std::array<std::uint8_t, engine::kAbSwitches> ab { 1, 1, 0 };
            long long passes = 0; // 0: not given
            long long startBar = 0; // 0: not given
            long long bars = 0; // 0: not given
            std::string section;
            double tempo = 0.0; // 0: the kit's
            engine::PhraseMode phrases = engine::PhraseMode::automatic;
            long long space = 100, echo = 100, pump = 100;
            bool club = true;
            bool exactMaster = false;
            std::string solo;
            std::string taps;
            std::string tapsOf;
            double rate = 48000.0;
            long long block = 512;
            bool keepLatency = false;
            long long hold = 0;
            std::string out;
            WavFormat format = WavFormat::float32;
            std::string content;
            std::string manifests = "content/plugin";
            std::string midi = "content/midi";
        };

        constexpr std::string_view kValued[] = { "--kit", "--mode", "--variant", "--kick", "--bass", "--stab", "--passes", "--start-bar",
            "--bars", "--section", "--tempo", "--phrases", "--space", "--echo", "--pump", "--club", "--master", "--solo", "--taps", "--taps-of", "--rate",
            "--block", "--hold", "--out", "--format", "--content", "--manifests", "--midi" };

        // 0: parsed; else the exit code (help printed, or a usage error reported).
        int parseOptions (std::span<const std::string_view> args, Options& o, std::ostream& out, std::ostream& err)
        {
            const auto usage = [&] (const std::string& what) {
                err << "groovecore render: " << what << "\n";
                return exitUsage;
            };
            for (std::size_t i = 0; i < args.size(); ++i)
            {
                const std::string_view a = args[i];
                if (a == "--help" || a == "-h")
                {
                    printHelp (out);
                    return -1;
                }
                if (a == "--keep-latency")
                {
                    o.keepLatency = true;
                    continue;
                }
                const bool valued = std::find (std::begin (kValued), std::end (kValued), a) != std::end (kValued);
                if (! valued)
                {
                    err << "groovecore render: unknown option \"" << a << "\"\n\n";
                    printHelp (err);
                    return exitUsage;
                }
                if (i + 1 >= args.size())
                    return usage ("missing value for \"" + std::string (a) + "\"");
                const std::string_view v = args[++i];
                const std::string vs (v);
                const auto ab = [&] (int which) {
                    if (v != "a" && v != "b")
                        return usage (std::string (a) + " takes a or b, not \"" + vs + "\"");
                    o.ab[static_cast<std::size_t> (which)] = v == "b" ? 1 : 0;
                    return 0;
                };
                const auto percent = [&] (long long& p) {
                    if (! parseInt (v, 0, 200, p))
                        return usage (std::string (a) + " takes a percentage from 0 to 200, not \"" + vs + "\"");
                    return 0;
                };
                int r = 0;
                if (a == "--kit")
                    o.kit = upper (v);
                else if (a == "--mode")
                {
                    if (v == "loop")
                        o.mode = engine::PlayMode::loop;
                    else if (v == "song")
                        o.mode = engine::PlayMode::song;
                    else
                        r = usage ("--mode takes loop or song, not \"" + vs + "\"");
                }
                else if (a == "--variant")
                {
                    if (v == "vox")
                        o.variant = engine::Variant::vox;
                    else if (v == "novox")
                        o.variant = engine::Variant::novox;
                    else
                        r = usage ("--variant takes vox or novox, not \"" + vs + "\"");
                }
                else if (a == "--kick")
                    r = ab (0);
                else if (a == "--bass")
                    r = ab (1);
                else if (a == "--stab")
                    r = ab (2);
                else if (a == "--passes")
                {
                    if (! parseInt (v, 1, 1000, o.passes))
                        r = usage ("--passes takes a whole number from 1 to 1000, not \"" + vs + "\"");
                }
                else if (a == "--start-bar")
                {
                    if (! parseInt (v, 1, 100000, o.startBar))
                        r = usage ("--start-bar takes a bar number from 1, not \"" + vs + "\"");
                }
                else if (a == "--bars")
                {
                    if (! parseInt (v, 1, 100000, o.bars))
                        r = usage ("--bars takes a whole number of bars from 1, not \"" + vs + "\"");
                }
                else if (a == "--section")
                    o.section = vs;
                else if (a == "--tempo")
                {
                    if (! parseReal (v, 20.0, 999.0, o.tempo))
                        r = usage ("--tempo takes a tempo from 20 to 999 BPM, not \"" + vs + "\"");
                }
                else if (a == "--phrases")
                {
                    if (v == "auto")
                        o.phrases = engine::PhraseMode::automatic;
                    else if (v == "off")
                        o.phrases = engine::PhraseMode::off;
                    else
                        r = usage ("--phrases takes auto or off, not \"" + vs + "\"");
                }
                else if (a == "--space")
                    r = percent (o.space);
                else if (a == "--echo")
                    r = percent (o.echo);
                else if (a == "--pump")
                    r = percent (o.pump);
                else if (a == "--club")
                {
                    if (v == "on" || v == "off")
                        o.club = v == "on";
                    else
                        r = usage ("--club takes on or off, not \"" + vs + "\"");
                }
                else if (a == "--master")
                {
                    if (v == "lookback" || v == "exact")
                        o.exactMaster = v == "exact";
                    else
                        r = usage ("--master takes lookback or exact, not \"" + vs + "\"");
                }
                else if (a == "--solo")
                    o.solo = vs;
                else if (a == "--taps")
                    o.taps = vs;
                else if (a == "--taps-of")
                    o.tapsOf = vs;
                else if (a == "--rate")
                {
                    if (! parseReal (v, 11025.0, 192000.0, o.rate) || std::fmod (o.rate, 1.0) > 0.0)
                        r = usage ("--rate takes a whole number of Hz from 11025 to 192000, not \"" + vs + "\"");
                }
                else if (a == "--block")
                {
                    if (! parseInt (v, 1, 8192, o.block))
                        r = usage ("--block takes a block size from 1 to 8192, not \"" + vs + "\"");
                }
                else if (a == "--hold")
                {
                    if (! parseInt (v, 0, 256, o.hold))
                        r = usage ("--hold takes a number of notes from 0 to 256, not \"" + vs + "\"");
                }
                else if (a == "--out")
                    o.out = vs;
                else if (a == "--format")
                {
                    if (v == "float")
                        o.format = WavFormat::float32;
                    else if (v == "pcm24")
                        o.format = WavFormat::pcm24;
                    else
                        r = usage ("--format takes float or pcm24, not \"" + vs + "\"");
                }
                else if (a == "--content")
                    o.content = vs;
                else if (a == "--manifests")
                    o.manifests = vs;
                else if (a == "--midi")
                    o.midi = vs;
                if (r != 0)
                    return r;
            }
            if (o.kit.empty() || o.out.empty())
            {
                err << "groovecore render: --kit and --out are required\n\n";
                printHelp (err);
                return exitUsage;
            }
            if (o.mode == engine::PlayMode::loop && ! o.section.empty())
                return usage ("--section is for --mode song");
            if (o.mode == engine::PlayMode::song && o.passes > 0)
                return usage ("--passes is for --mode loop; a Song takes --start-bar or --section, and --bars");
            if (o.passes > 0 && o.bars > 0)
                return usage ("--passes and --bars each set the length: give one");
            if (! o.section.empty() && o.startBar > 0)
                return usage ("--section and --start-bar each set the start: give one");
            if (! o.solo.empty() && ! o.taps.empty())
                return usage ("--solo and --taps: give one");
            if (! o.tapsOf.empty() && o.taps.empty())
                return usage ("--taps-of goes with --taps <DIR>");
            return 0;
        }

        // The section the option names in the timeline's markers: its index, or -1.
        int findSection (const engine::Timeline& song, const std::string& wanted)
        {
            long long number = 0;
            if (parseInt (wanted, 1, static_cast<long long> (song.sections.size()), number))
                return static_cast<int> (number - 1);
            const auto key = upper (wanted);
            for (std::size_t i = 0; i < song.sections.size(); ++i)
                if (upper (song.sections[i].name) == key)
                    return static_cast<int> (i);
            return -1;
        }

        // --hold: n note-ons on the kit's polyphonic sustain-loop instruments, round robin over them, each instrument's
        // keys from its lowest up, through the keymap's A sides (channel, key).
        std::vector<engine::MidiEvent> heldNotes (const engine::Kit& kit, int n)
        {
            std::vector<std::vector<std::pair<int, int>>> perInstrument (kit.instruments.size());
            for (int ch = 0; ch < 16; ++ch)
                for (int key = 0; key < 128; ++key)
                {
                    const auto& t = kit.keys[static_cast<std::size_t> (ch)][static_cast<std::size_t> (key)];
                    if (t.a < 0 || static_cast<std::size_t> (t.a) >= kit.instruments.size())
                        continue;
                    const auto& in = kit.instruments[static_cast<std::size_t> (t.a)];
                    if (in.mode == engine::SampleMode::sustainLoop && ! in.mono)
                        perInstrument[static_cast<std::size_t> (t.a)].emplace_back (ch, key);
                }
            std::vector<engine::MidiEvent> out;
            for (std::size_t round = 0; static_cast<int> (out.size()) < n; ++round)
            {
                bool any = false;
                for (const auto& keys : perInstrument)
                    if (round < keys.size() && static_cast<int> (out.size()) < n)
                    {
                        engine::MidiEvent e;
                        e.offset = 0;
                        e.status = static_cast<std::uint8_t> (0x90 | keys[round].first);
                        e.data1 = static_cast<std::uint8_t> (keys[round].second);
                        e.data2 = 100;
                        out.push_back (e);
                        any = true;
                    }
                if (! any)
                    break;
            }
            return out;
        }

        // This thread's CPU time in seconds (the engine runs on it): what a block costs, without the time other
        // processes take the core. Windows has no such clock here: wall time there.
        double threadSeconds()
        {
#if defined(_WIN32)
            return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
#else
            timespec ts {};
            clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts);
            return static_cast<double> (ts.tv_sec) + 1e-9 * static_cast<double> (ts.tv_nsec);
#endif
        }

        double peakDb (const std::vector<float>& l, const std::vector<float>& r)
        {
            float peak = 0.0f;
            for (const auto* c : { &l, &r })
                for (const float x : *c)
                    peak = std::max (peak, std::fabs (x));
            return peak > 0.0f ? 20.0 * std::log10 (static_cast<double> (peak)) : -std::numeric_limits<double>::infinity();
        }

        bool allFinite (const std::vector<float>& l, const std::vector<float>& r)
        {
            for (const auto* c : { &l, &r })
                for (const float x : *c)
                    if (! std::isfinite (x))
                        return false;
            return true;
        }
    }

    int runRender (std::span<const std::string_view> args, std::ostream& out, std::ostream& err)
    {
        Options o;
        if (const int r = parseOptions (args, o, out, err); r != 0)
            return r < 0 ? exitOk : r;

        // The kit.
        const auto loaded = loadRenderKit (o.kit, o.manifests, o.midi, contentRoots (o.content));
        if (! loaded.ok())
        {
            err << "groovecore render: " << loaded.error << '\n';
            std::error_code ec;
            if (! std::filesystem::is_directory (o.manifests, ec))
                err << "(no manifests folder at \"" << o.manifests << "\": run from the repository root, or pass --manifests <DIR>)\n";
            return exitFailed;
        }
        const engine::Kit& kit = *loaded.value.kit;
        const auto& mixChannels = kit.mix.channels;

        // What --solo names: a mix channel's id, else a lane's channels.
        std::vector<int> solo;
        if (! o.solo.empty())
        {
            const int channel = kit.mix.channelIndex (o.solo);
            const int lane = engine::laneIndex (o.solo);
            for (std::size_t c = 0; c < mixChannels.size(); ++c)
                if (static_cast<int> (c) == channel || (channel < 0 && static_cast<int> (mixChannels[c].lane) == lane))
                    solo.push_back (static_cast<int> (c));
            if (solo.empty())
            {
                std::string ids;
                for (const auto& c : mixChannels)
                    ids += (ids.empty() ? "" : ", ") + c.id;
                err << "groovecore render: --solo " << o.solo << ": " << (lane >= 0 ? "the kit has no mix channel on that lane" : "no such lane or mix channel")
                    << " (lanes: kick, snare, hats, perc, bass, lead, chords, vox, accents, fx; " << o.kit << "'s channels: " << ids << ")\n";
                return exitUsage;
            }
        }

        // What --taps-of names: mix channels by id or by lane, and "sends". Everything when it isn't given.
        std::vector<bool> tapChannel (mixChannels.size(), o.tapsOf.empty());
        bool tapSends = o.tapsOf.empty();
        for (std::size_t from = 0; from < o.tapsOf.size();)
        {
            auto to = o.tapsOf.find (',', from);
            if (to == std::string::npos)
                to = o.tapsOf.size();
            const std::string item = o.tapsOf.substr (from, to - from);
            from = to + 1;
            if (item == "sends")
            {
                tapSends = true;
                continue;
            }
            // a lane's channels and a channel by its id, both (FUT02's channel "bass" is one of lane bass's two)
            const int channel = kit.mix.channelIndex (item);
            const int lane = engine::laneIndex (item);
            bool any = false;
            for (std::size_t c = 0; c < mixChannels.size(); ++c)
                if (static_cast<int> (c) == channel || static_cast<int> (mixChannels[c].lane) == lane)
                    any = tapChannel[c] = true;
            if (! any && lane < 0)
            {
                err << "groovecore render: --taps-of " << item << ": no such lane or mix channel\n";
                return exitUsage;
            }
        }

        // Where play starts and how long it lasts, in quarters.
        const double tempo = o.tempo > 0.0 ? o.tempo : static_cast<double> (kit.info.bpm);
        const auto& song = kit.timelines[static_cast<std::size_t> (engine::timelineIndex (engine::PlayMode::song, o.variant))];
        const long long songBars = song.length / kMilliticksPerBar;
        long long startBar = o.startBar > 0 ? o.startBar : 1;
        long long bars = 0;
        std::string sectionName;
        if (o.mode == engine::PlayMode::loop)
            bars = o.bars > 0 ? o.bars : 4 * (o.passes > 0 ? o.passes : 3);
        else
        {
            long long sectionBars = 0;
            if (! o.section.empty())
            {
                const int s = findSection (song, o.section);
                if (s < 0)
                {
                    std::string names;
                    for (const auto& sec : song.sections)
                        names += (names.empty() ? "" : ", ") + sec.name;
                    err << "groovecore render: no section \"" << o.section << "\" in the " << (o.variant == engine::Variant::vox ? "vox" : "no-vox")
                        << " Song (sections: " << names << ")\n";
                    return exitUsage;
                }
                const auto& sec = song.sections[static_cast<std::size_t> (s)];
                sectionName = sec.name;
                startBar = sec.pos / kMilliticksPerBar + 1;
                const std::int64_t end = static_cast<std::size_t> (s) + 1 < song.sections.size() ? song.sections[static_cast<std::size_t> (s) + 1].pos : song.length;
                sectionBars = (end - sec.pos) / kMilliticksPerBar;
            }
            if (startBar > songBars)
            {
                err << "groovecore render: the Song is " << songBars << " bars: bar " << startBar << " is past its end\n";
                return exitUsage;
            }
            bars = o.bars > 0 ? o.bars : (sectionBars > 0 ? sectionBars : songBars - startBar + 1);
        }
        const double startPpq = 4.0 * static_cast<double> (startBar - 1);
        const double quarters = 4.0 * static_cast<double> (bars);
        const auto frames = static_cast<std::int64_t> (std::floor (quarters * 60.0 / tempo * o.rate + 0.5));
        const bool tapping = ! o.taps.empty() || ! solo.empty();
        if (tapping && bars > 64)
        {
            err << "groovecore render: --taps and --solo keep every channel in memory: at most 64 bars, not " << bars << "\n";
            return exitUsage;
        }

        // The engine.
        Taps taps;
        std::unique_ptr<Player> player;
        if (tapping)
        {
            taps.prepare (static_cast<int> (mixChannels.size()), frames);
            player = std::make_unique<TapPlayer> (taps);
        }
        else
            player = std::make_unique<EnginePlayer>();
        const int block = static_cast<int> (o.block);
        player->prepare (o.rate, block, o.exactMaster ? engine::MasterHold::twoSided : engine::MasterHold::lookBack);
        player->publish (loaded.value.kit.get());
        const int latency = player->latency();
        const std::int64_t skip = o.keepLatency ? 0 : latency;
        const std::int64_t total = frames + skip;

        engine::Controls controls;
        controls.mode = o.mode;
        controls.variant = o.variant;
        controls.ab = o.ab;
        controls.space = static_cast<float> (o.space) / 100.0f;
        controls.echo = static_cast<float> (o.echo) / 100.0f;
        controls.pump = static_cast<float> (o.pump) / 100.0f;
        controls.club = o.club;
        controls.phrases = o.phrases;

        const auto held = heldNotes (kit, static_cast<int> (o.hold));
        if (o.hold > 0 && held.empty())
        {
            err << "groovecore render: --hold: " << o.kit << " has no polyphonic sustained instrument to hold notes on\n";
            return exitUsage;
        }

        engine::TransportInfo transport;
        transport.hasPosition = true;
        transport.playing = true;
        transport.bpm = tempo;
        transport.nonRealtime = true;

        std::vector<float> left (static_cast<std::size_t> (frames), 0.0f), right (static_cast<std::size_t> (frames), 0.0f);
        std::vector<float> blockL (static_cast<std::size_t> (block)), blockR (static_cast<std::size_t> (block));
        float* chunk[2] { blockL.data(), blockR.data() };
        std::vector<double> blockLoad; // each block's CPU time over its duration
        blockLoad.reserve (static_cast<std::size_t> (total / block + 2));
        double wall = 0.0;
        int maxVoices = 0;
        const double cpu0 = threadSeconds();
        for (std::int64_t done = 0; done < total;)
        {
            const int n = static_cast<int> (std::min<std::int64_t> (block, total - done));
            transport.ppq = startPpq + static_cast<double> (done) * tempo / (60.0 * o.rate);
            const auto t0 = std::chrono::steady_clock::now();
            const double c0 = threadSeconds();
            const bool first = done == 0;
            player->process (chunk, n, transport, controls, first ? held.data() : nullptr, first ? static_cast<int> (held.size()) : 0);
            blockLoad.push_back ((threadSeconds() - c0) / (static_cast<double> (n) / o.rate));
            wall += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            maxVoices = std::max (maxVoices, player->voices());
            for (int i = 0; i < n; ++i)
            {
                const std::int64_t at = done + i - skip;
                if (at >= 0)
                {
                    left[static_cast<std::size_t> (at)] = blockL[static_cast<std::size_t> (i)];
                    right[static_cast<std::size_t> (at)] = blockR[static_cast<std::size_t> (i)];
                }
            }
            done += n;
        }
        const double cpu = threadSeconds() - cpu0;
        const double audioSeconds = static_cast<double> (total) / o.rate;
        // the worst block after the first (which pays the cold caches and the held notes' starts), and the 99th percentile
        std::vector<double> sorted (blockLoad.begin() + (blockLoad.size() > 1 ? 1 : 0), blockLoad.end());
        std::sort (sorted.begin(), sorted.end());
        const double worstBlock = sorted.empty() ? 0.0 : sorted.back();
        const double p99Block = sorted.empty() ? 0.0 : sorted[static_cast<std::size_t> (0.99 * static_cast<double> (sorted.size() - 1))];
        const double firstBlock = blockLoad.empty() ? 0.0 : blockLoad.front();

        // --solo: the soloed channels' dry buses, summed, in place of the mix.
        if (! solo.empty())
        {
            std::fill (left.begin(), left.end(), 0.0f);
            std::fill (right.begin(), right.end(), 0.0f);
            for (const int c : solo)
                for (std::size_t i = 0; i < left.size(); ++i)
                {
                    left[i] += taps.bus[static_cast<std::size_t> (2 * c)][i];
                    right[i] += taps.bus[static_cast<std::size_t> (2 * c + 1)][i];
                }
        }
        if (! allFinite (left, right))
        {
            err << "groovecore render: the render holds a NaN or an infinity\n";
            return exitFailed;
        }

        // The files.
        const std::filesystem::path outPath (o.out);
        const std::array<std::span<const float>, 2> stereo { std::span<const float> (left), std::span<const float> (right) };
        const int rate = static_cast<int> (o.rate);
        if (const auto w = writeWav (outPath, stereo, rate, o.format); ! w.ok())
        {
            err << "groovecore render: " << w.error << '\n';
            return exitFailed;
        }
        std::vector<std::string> tapFiles;
        if (! o.taps.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories (o.taps, ec);
            const auto write = [&] (const std::string& name, std::span<const std::span<const float>> chans) {
                const auto path = std::filesystem::path (o.taps) / (name + ".wav");
                const auto w = writeWav (path, chans, rate, WavFormat::float32);
                if (w.ok())
                    tapFiles.push_back (name);
                return w;
            };
            for (std::size_t c = 0; c < mixChannels.size(); ++c)
            {
                if (! tapChannel[c])
                    continue;
                const std::array<std::span<const float>, 2> chans { std::span<const float> (taps.bus[2 * c]), std::span<const float> (taps.bus[2 * c + 1]) };
                if (const auto w = write (mixChannels[c].id, chans); ! w.ok())
                {
                    err << "groovecore render: " << w.error << '\n';
                    return exitFailed;
                }
            }
            constexpr std::array<std::string_view, engine::kSends> sendNames { "send_space", "send_echo", "send_resonator" };
            for (std::size_t s = 0; tapSends && s < sendNames.size(); ++s)
            {
                const std::array<std::span<const float>, 1> mono { std::span<const float> (taps.sends[s]) };
                if (const auto w = write (std::string (sendNames[s]), mono); ! w.ok())
                {
                    err << "groovecore render: " << w.error << '\n';
                    return exitFailed;
                }
            }
        }

        // The sidecar: what was rendered, for compare_render.py.
        const double peak = peakDb (left, right);
        const char* abName[2] { "a", "b" };
        std::ostringstream j;
        j << "{\n"
          << "  \"kit\": " << jsonString (o.kit) << ",\n"
          << "  \"kit_bpm\": " << kit.info.bpm << ",\n"
          << "  \"mode\": \"" << (o.mode == engine::PlayMode::song ? "song" : "loop") << "\",\n"
          << "  \"variant\": \"" << (o.variant == engine::Variant::novox ? "novox" : "vox") << "\",\n"
          << "  \"kick\": \"" << abName[o.ab[0]] << "\", \"bass\": \"" << abName[o.ab[1]] << "\", \"stab\": \"" << abName[o.ab[2]] << "\",\n"
          << "  \"bpm\": " << fixed (tempo, 6) << ",\n"
          << "  \"rate\": " << rate << ",\n"
          << "  \"block\": " << block << ",\n"
          << "  \"start_bar\": " << startBar << ",\n"
          << "  \"bars\": " << bars << ",\n"
          << "  \"section\": " << (sectionName.empty() ? std::string ("null") : jsonString (sectionName)) << ",\n"
          << "  \"frames\": " << frames << ",\n"
          << "  \"latency\": " << latency << ",\n"
          << "  \"latency_kept\": " << (o.keepLatency ? "true" : "false") << ",\n"
          << "  \"phrases\": \"" << (o.phrases == engine::PhraseMode::off ? "off" : "auto") << "\",\n"
          << "  \"space\": " << o.space << ", \"echo\": " << o.echo << ", \"pump\": " << o.pump << ",\n"
          << "  \"club\": " << (o.club ? "true" : "false") << ",\n"
          << "  \"master\": \"" << (o.exactMaster ? "exact" : "lookback") << "\",\n"
          << "  \"solo\": " << (o.solo.empty() ? std::string ("null") : jsonString (o.solo)) << ",\n"
          << "  \"solo_channels\": [";
        for (std::size_t i = 0; i < solo.size(); ++i)
            j << (i ? ", " : "") << jsonString (mixChannels[static_cast<std::size_t> (solo[i])].id);
        j << "],\n  \"channels\": [";
        for (std::size_t c = 0; c < mixChannels.size(); ++c)
            j << (c ? ", " : "") << "{\"id\": " << jsonString (mixChannels[c].id) << ", \"lane\": " << jsonString (engine::kLaneIds[mixChannels[c].lane]) << "}";
        j << "],\n  \"taps\": " << (o.taps.empty() ? std::string ("null") : jsonString (shown (std::filesystem::absolute (o.taps)))) << ",\n"
          << "  \"tap_files\": [";
        for (std::size_t i = 0; i < tapFiles.size(); ++i)
            j << (i ? ", " : "") << jsonString (tapFiles[i]);
        j << "],\n"
          << "  \"samples\": " << jsonString (shown (loaded.value.samples)) << ",\n"
          << "  \"format\": \"" << (o.format == WavFormat::pcm24 ? "pcm24" : "float") << "\",\n"
          << "  \"peak_dbfs\": " << (std::isfinite (peak) ? fixed (peak, 3) : std::string ("null")) << ",\n"
          << "  \"held_notes\": " << held.size() << ",\n"
          << "  \"max_voices\": " << maxVoices << ",\n"
          << "  \"cpu_seconds\": " << fixed (cpu, 3) << ",\n"
          << "  \"wall_seconds\": " << fixed (wall, 3) << ",\n"
          << "  \"cpu_percent\": " << fixed (100.0 * cpu / audioSeconds, 2) << ",\n"
          << "  \"p99_block_percent\": " << fixed (100.0 * p99Block, 1) << ",\n"
          << "  \"worst_block_percent\": " << fixed (100.0 * worstBlock, 1) << ",\n"
          << "  \"first_block_percent\": " << fixed (100.0 * firstBlock, 1) << "\n"
          << "}\n";
        auto sidecar = outPath;
        sidecar.replace_extension (".json");
        const std::string text = j.str();
        if (const auto w = util::writeBinaryFile (sidecar, std::span<const std::uint8_t> (reinterpret_cast<const std::uint8_t*> (text.data()), text.size()));
            ! w.ok())
        {
            err << "groovecore render: " << w.error << '\n';
            return exitFailed;
        }

        out << o.kit << " \"" << kit.info.title << "\": " << (o.mode == engine::PlayMode::song ? "song " : "loop ")
            << (o.variant == engine::Variant::novox ? "no-vox" : "vox") << ", kick " << upper (abName[o.ab[0]]) << ", bass " << upper (abName[o.ab[1]])
            << ", stab " << upper (abName[o.ab[2]]) << ", " << fixed (tempo, std::fmod (tempo, 1.0) > 0.0 ? 3 : 0) << " BPM, " << rate << " Hz, blocks of "
            << block << '\n'
            << "  " << bars << " bars from bar " << startBar << (sectionName.empty() ? "" : " (" + sectionName + ")") << ": " << frames << " frames ("
            << fixed (static_cast<double> (frames) / o.rate, 3) << " s); latency " << latency << " samples, " << (o.keepLatency ? "kept" : "dropped")
            << '\n'
            << "  samples from " << shown (loaded.value.samples) << '\n'
            << "  peak " << (std::isfinite (peak) ? fixed (peak, 2) + " dBFS" : std::string ("silence")) << "; up to " << maxVoices << " voices; "
            << fixed (cpu, 2) << " s of CPU, " << fixed (100.0 * cpu / audioSeconds, 1) << " % of one core in real time (blocks: 99 % under "
            << fixed (100.0 * p99Block, 1) << " %, worst " << fixed (100.0 * worstBlock, 1) << " %)\n"
            << "wrote " << shown (outPath) << (tapFiles.empty() ? "" : " and " + std::to_string (tapFiles.size()) + " taps") << '\n';
        return exitOk;
    }
}
