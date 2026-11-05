#include "testing/synthetic_kit.h"

#include "testing/manifest_json.h"
#include "testing/smf_builder.h"

#include "rng/rng.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace wubansia::engine::testing
{
    namespace
    {
        constexpr double kRate = 48000.0;

        // ---------------------------------------------------------------- arithmetic only (no libm)

        // sin (2 pi turns) to about 0.1 %: a parabola with one correction.
        double sine (double turns)
        {
            const double p = turns - std::floor (turns); // [0, 1)
            const double t = p < 0.5 ? p : p - 1.0; // [-0.5, 0.5)
            const double y = 8.0 * t * (1.0 - 2.0 * std::fabs (t));
            return 0.225 * (y * std::fabs (y) - y) + y;
        }

        // Uniform in [-1, 1), from a counter hash.
        double noise (std::uint64_t seed, std::int64_t n)
        {
            const auto bits = rng::mix64 (seed * 0x9E3779B97F4A7C15ull + static_cast<std::uint64_t> (n));
            return static_cast<double> (bits >> 11) * 0x1.0p-52 - 1.0;
        }

        // 2^(i / 12), correctly rounded.
        constexpr double kSemitones[12] = { 1.0, 1.0594630943592953, 1.122462048309373, 1.189207115002721,
            1.2599210498948732, 1.3348398541700344, 1.4142135623730951, 1.4983070768766815, 1.5874010519681994,
            1.681792830507429, 1.7817974362806785, 1.8877486253633868 };

        // The whole number of frames nearest one period of a key (A4 = 440 Hz), so tones loop exactly.
        int period (int key)
        {
            int fromA4 = key - 69;
            int octave = 0;
            while (fromA4 < 0)
            {
                fromA4 += 12;
                --octave;
            }
            octave += fromA4 / 12;
            double f = 440.0 * kSemitones[fromA4 % 12];
            for (; octave > 0; --octave)
                f *= 2.0;
            for (; octave < 0; ++octave)
                f *= 0.5;
            return static_cast<int> (std::floor (kRate / f + 0.5));
        }

        // Harmonics of kRate / period Hz with these weights, at frame n: exactly periodic in period frames.
        double tone (std::int64_t n, int period, std::span<const double> weights)
        {
            const std::int64_t k = ((n % period) + period) % period;
            double sum = 0.0, norm = 0.0;
            for (std::size_t h = 0; h < weights.size(); ++h)
            {
                const auto index = (k * static_cast<std::int64_t> (h + 1)) % period;
                sum += weights[h] * sine (static_cast<double> (index) / static_cast<double> (period));
                norm += weights[h];
            }
            return sum / norm;
        }

        double squared (double x) { return x * x; }

        // What 24-bit PCM holds: k / 2^23, so a WAV round trip is exact.
        float quantize24 (double x)
        {
            const double scaled = std::min (8388607.0, std::max (-8388608.0, std::floor (x * 8388608.0 + 0.5)));
            return static_cast<float> (scaled / 8388608.0);
        }

        // ---------------------------------------------------------------- the files

        struct FileSpec
        {
            std::string_view path;
            std::int32_t channels;
            std::int64_t frames;
            std::int32_t preroll;
        };

        constexpr std::array<FileSpec, syn::fileCount> kFiles { {
            { "Drums/Kick B v1 rr1.wav", 1, 14400, 0 },
            { "Drums/Kick B v1 rr2.wav", 1, 14400, 0 },
            { "Drums/Kick B v2 rr1.wav", 1, 14400, 0 },
            { "Drums/Kick B v2 rr2.wav", 1, 14400, 0 },
            { "Drums/Kick A.wav", 1, 12000, 0 },
            { "Drums/Snare v1 rr1.wav", 2, 9600, 0 },
            { "Drums/Snare v1 rr2.wav", 2, 9600, 0 },
            { "Drums/Snare v2 rr1.wav", 2, 9600, 0 },
            { "Drums/Snare v2 rr2.wav", 2, 9600, 0 },
            { "Drums/Closed Hat rr1.wav", 2, 4800, 0 },
            { "Drums/Closed Hat rr2.wav", 2, 4800, 0 },
            { "Drums/Open Hat.wav", 2, 38400, 0 },
            { "Bass/Bass A G1.wav", 1, 24000, 0 },
            { "Bass/Bass A D2.wav", 1, 24000, 0 },
            { "Bass/Bass A G2.wav", 1, 24000, 0 },
            { "Bass/Bass B G1 v1.wav", 1, 24000, 0 },
            { "Bass/Bass B G1 v2.wav", 1, 24000, 0 },
            { "Bass/Bass B D2 v1.wav", 1, 24000, 0 },
            { "Bass/Bass B D2 v2.wav", 1, 24000, 0 },
            { "Bass/Bass B G2 v1.wav", 1, 24000, 0 },
            { "Bass/Bass B G2 v2.wav", 1, 24000, 0 },
            { "Music/Lead C5.wav", 2, 24000, 0 },
            { "Music/Stab G3.wav", 2, 19200, 0 },
            { "Music/Stab D4.wav", 2, 19200, 0 },
            { "Music/Pad G3.wav", 2, 96000, 0 },
            { "Vox/Vox G4 v1.wav", 2, 24960, 960 },
            { "Vox/Vox G4 v2.wav", 2, 24960, 960 },
            { "Vox/Vox G4 v3.wav", 2, 24960, 960 },
            { "FX/Riser.wav", 2, 90000, 0 },
            { "Loops/Lead Phrase.wav", 2, 180000, 0 },
        } };

        constexpr int kBassLoopStart = 4800;
        constexpr int kPadLoopStart = 24000;

        // A sustain loop: a whole number of periods from loopStart, about `around` frames long.
        std::int64_t loopEndFor (int key, int loopStart, int around)
        {
            const int p = period (key);
            return loopStart + static_cast<std::int64_t> ((around + p / 2) / p) * p;
        }

        std::pair<std::int64_t, std::int64_t> loopOf (int file)
        {
            static constexpr int bassKeys[] = { 31, 38, 43 };
            if (file >= 12 && file <= 14)
                return { kBassLoopStart, loopEndFor (bassKeys[file - 12], kBassLoopStart, 11760) };
            if (file >= 15 && file <= 20)
                return { kBassLoopStart, loopEndFor (bassKeys[(file - 15) / 2], kBassLoopStart, 11760) };
            if (file == 24)
                return { kPadLoopStart, loopEndFor (55, kPadLoopStart, 48000) };
            return { 0, 0 };
        }

        // The lead phrase: 2 bars, every note the stem plays and the MIDI writes. The one at swung step 13
        // retriggers step 12's key: slice 3 holds it, so 9 notes make 8 slices.
        struct PhraseNote
        {
            int step;
            int key;
            int steps; // its length in 16ths
        };
        constexpr std::array<PhraseNote, 9> kPhrase { { { 0, 67, 3 }, { 4, 70, 3 }, { 8, 72, 3 }, { 12, 74, 1 }, { 13, 74, 2 }, { 16, 72, 3 }, { 20, 70, 3 }, { 24, 67, 3 }, { 27, 65, 3 } } };
        constexpr std::size_t kHeldNote = 4; // the retrigger, held by the slice before it
        constexpr std::int64_t kPhraseFadeFrames = 240; // 5 ms

        // The phrase's CC74 ride, value i of the 128 on its cycle's 60-tick grid: up from 32 over the first
        // bar, back down over the second. Its CC71 ride is empty (no constraint).
        int leadRide (int i)
        {
            return i < 64 ? 32 + i : 96 - (i - 64);
        }

        std::int64_t onsetFrames (int step)
        {
            return step * kSyntheticSixteenthFrames + (step % 2 != 0 ? kSyntheticSwingFrames : 0);
        }

        constexpr double kLeadWeights[] = { 1.0, 0.5, 0.25, 0.125 };

        std::vector<std::vector<double>> render (int file)
        {
            const auto& spec = kFiles[static_cast<std::size_t> (file)];
            const auto frames = spec.frames;
            const auto N = static_cast<double> (frames);
            std::vector<std::vector<double>> out (static_cast<std::size_t> (spec.channels), std::vector<double> (static_cast<std::size_t> (frames), 0.0));
            auto& left = out[0];
            auto at = [] (std::vector<double>& v, std::int64_t n) -> double& { return v[static_cast<std::size_t> (n)]; };

            if (file <= 4) // kicks: B v1 rr1 .. v2 rr2, then A
            {
                const bool a = file == 4;
                const double fStart = a ? 200.0 : 150.0 + 5.0 * (file % 2);
                const double fEnd = a ? 55.0 : 45.0;
                const double amp = a ? 0.8 : (file < 2 ? 0.45 : 0.9);
                double phase = 0.0;
                for (std::int64_t n = 0; n < frames; ++n)
                {
                    const double u = 1.0 - static_cast<double> (n) / N;
                    const double u2 = u * u;
                    const double f = fEnd + (fStart - fEnd) * u2 * u2;
                    at (left, n) = amp * std::min (1.0, static_cast<double> (n) / 48.0) * u2 * sine (phase);
                    phase += f / kRate;
                    phase -= std::floor (phase);
                }
            }
            else if (file <= 8) // snares: v1 rr1, v1 rr2, v2 rr1, v2 rr2
            {
                const int layer = (file - 5) / 2 + 1, rr = (file - 5) % 2 + 1;
                const double amp = layer == 1 ? 0.4 : 0.8;
                for (std::size_t c = 0; c < out.size(); ++c)
                    for (std::int64_t n = 0; n < frames; ++n)
                    {
                        const double u = 1.0 - static_cast<double> (n) / N;
                        const double u2 = u * u;
                        const double hiss = noise (0x5A00u + static_cast<std::uint64_t> (layer * 16 + rr * 4) + c, n);
                        const double body = sine (static_cast<double> (n % 240) / 240.0); // 200 Hz
                        at (out[c], n) = amp * (0.6 * hiss * u2 * u2 + 0.4 * body * u2);
                    }
            }
            else if (file <= 11) // hats: closed rr1, rr2, open
            {
                const bool open = file == 11;
                const std::uint64_t seed = open ? 0x0100u : 0xC100u + static_cast<std::uint64_t> (file - 9) * 4u;
                for (std::size_t c = 0; c < out.size(); ++c)
                    for (std::int64_t n = 0; n < frames; ++n)
                    {
                        const double u = 1.0 - static_cast<double> (n) / N;
                        const double hiss = noise (seed + c, n) - noise (seed + c, n - 1);
                        at (out[c], n) = open ? 0.2 * hiss * u * u : 0.25 * hiss * squared (u * u);
                    }
            }
            else if (file <= 20) // basses: A G1 D2 G2, then B G1 v1 v2, D2 v1 v2, G2 v1 v2
            {
                static constexpr int keys[] = { 31, 38, 43 };
                const bool a = file <= 14;
                const int key = keys[a ? file - 12 : (file - 15) / 2];
                const int layer = a ? 2 : (file - 15) % 2 + 1;
                const double amp = a ? 0.7 : (layer == 1 ? 0.35 : 0.7);
                static constexpr double full[] = { 1.0, 1.0 / 2.0, 1.0 / 3.0, 1.0 / 4.0, 1.0 / 5.0, 1.0 / 6.0 };
                const std::span<const double> weights (full, layer == 2 ? 6u : 3u);
                const auto [loopStart, loopEnd] = loopOf (file);
                (void) loopStart;
                const int p = period (key);
                for (std::int64_t n = 0; n < frames; ++n)
                {
                    double env = 1.0;
                    if (n < 480)
                        env = static_cast<double> (n) / 480.0;
                    else if (n >= loopEnd)
                        env = squared (1.0 - static_cast<double> (n - loopEnd) / static_cast<double> (frames - loopEnd));
                    at (left, n) = amp * env * tone (n, p, weights);
                }
            }
            else if (file == 21) // lead C5
            {
                const int p = period (72);
                for (std::int64_t n = 0; n < frames; ++n)
                {
                    const double u = 1.0 - static_cast<double> (n) / N;
                    at (left, n) = 0.5 * std::min (1.0, static_cast<double> (n) / 96.0) * u * u * tone (n, p, kLeadWeights);
                }
            }
            else if (file <= 23) // stabs G3, D4: minor triads
            {
                const int root = file == 22 ? 55 : 62;
                static constexpr double weights[] = { 1.0, 0.5, 0.33 };
                const int periods[] = { period (root), period (root + 3), period (root + 7) };
                for (std::int64_t n = 0; n < frames; ++n)
                {
                    const double u = 1.0 - static_cast<double> (n) / N;
                    double chord = 0.0;
                    for (const int p : periods)
                        chord += tone (n, p, weights);
                    at (left, n) = 0.6 * std::min (1.0, static_cast<double> (n) / 48.0) * u * u * u * chord / 3.0;
                }
            }
            else if (file == 24) // pad G3, looped
            {
                static constexpr double weights[] = { 1.0, 0.5, 0.33, 0.2 };
                const int p = period (55);
                const auto loopEnd = loopOf (file).second;
                for (std::int64_t n = 0; n < frames; ++n)
                {
                    double env = 1.0;
                    if (n < 4800)
                        env = static_cast<double> (n) / 4800.0;
                    else if (n >= loopEnd)
                        env = squared (1.0 - static_cast<double> (n - loopEnd) / static_cast<double> (frames - loopEnd));
                    at (left, n) = 0.3 * env * tone (n, p, weights);
                }
            }
            else if (file <= 27) // vox G4, three vowels; a breath before the onset
            {
                const int layer = file - 24;
                static constexpr double ooh[] = { 1.0, 0.6, 0.2 };
                static constexpr double ahh[] = { 1.0, 0.3, 0.5, 0.2 };
                static constexpr double eee[] = { 1.0, 0.2, 0.2, 0.6, 0.3 };
                const std::span<const double> weights = layer == 1 ? std::span<const double> (ooh) : layer == 2 ? std::span<const double> (ahh) : std::span<const double> (eee);
                const double amp = 0.2 + 0.1 * layer;
                const int p = period (67);
                const std::int64_t onset = spec.preroll;
                for (std::int64_t n = 0; n < frames; ++n)
                {
                    if (n < onset)
                    {
                        at (left, n) = 0.05 * noise (0xF0u + static_cast<std::uint64_t> (layer), n) * static_cast<double> (n) / static_cast<double> (onset);
                        continue;
                    }
                    const auto m = n - onset;
                    const double env = std::min (1.0, static_cast<double> (m) / 240.0) * (1.0 - static_cast<double> (m) / static_cast<double> (frames - onset));
                    at (left, n) = amp * env * tone (m, p, weights);
                }
            }
            else if (file == 28) // riser: a rising tone and noise, swelling to its end
            {
                for (std::size_t c = 0; c < out.size(); ++c)
                {
                    double phase = 0.0;
                    for (std::int64_t n = 0; n < frames; ++n)
                    {
                        const double x = static_cast<double> (n) / N;
                        const double edge = std::min (1.0, static_cast<double> (frames - n) / 240.0);
                        at (out[c], n) = edge * x * x * (0.45 * sine (phase) + 0.15 * noise (0x7150u + c, n));
                        phase += (200.0 + 1800.0 * x) / kRate;
                        phase -= std::floor (phase);
                    }
                }
            }
            else // the lead phrase's stem
            {
                for (const auto& note : kPhrase)
                {
                    const int p = period (note.key);
                    const auto onset = onsetFrames (note.step);
                    const auto length = note.steps * kSyntheticSixteenthFrames;
                    for (std::int64_t m = 0; m < length; ++m)
                    {
                        double env = std::min (1.0, static_cast<double> (m) / 96.0) * (1.0 - 0.5 * static_cast<double> (m) / static_cast<double> (length));
                        if (m >= length - kPhraseFadeFrames)
                            env *= static_cast<double> (length - m) / static_cast<double> (kPhraseFadeFrames);
                        at (left, onset + m) += 0.5 * env * tone (m, p, kLeadWeights);
                    }
                }
            }

            if (out.size() == 2 && file >= 21 && file != 28) // pitched stereo files: the right side a touch quieter
                for (std::int64_t n = 0; n < frames; ++n)
                    at (out[1], n) = 0.9 * at (left, n);
            return out;
        }

        SampleData audioOf (int file)
        {
            const auto channels = render (file);
            SampleData data;
            data.frames = kFiles[static_cast<std::size_t> (file)].frames;
            data.channels = static_cast<std::int32_t> (channels.size());
            data.sampleRate = kContentSampleRate;
            data.audio.reserve (channels.size() * static_cast<std::size_t> (data.frames));
            for (const auto& c : channels)
                for (const double x : c)
                    data.audio.push_back (quantize24 (x));
            return data;
        }

        // ---------------------------------------------------------------- the manifests

        LayerEntry layer (std::int32_t vel, std::vector<std::int32_t> rr)
        {
            LayerEntry l;
            l.vel = vel;
            l.rr = std::move (rr);
            return l;
        }

        ZoneEntry zone (std::int32_t root, std::int32_t lo, std::int32_t hi, std::vector<LayerEntry> layers)
        {
            ZoneEntry z;
            z.root = root;
            z.lo = lo;
            z.hi = hi;
            z.layers = std::move (layers);
            return z;
        }

        InstrumentEntry instrument (std::string id, std::string name, Lane lane, std::string mixChannel, SampleMode mode, std::vector<ZoneEntry> zones)
        {
            InstrumentEntry in;
            in.id = std::move (id);
            in.name = std::move (name);
            in.lane = lane;
            in.mixChannel = std::move (mixChannel);
            in.mode = mode;
            in.zones = std::move (zones);
            return in;
        }

        KitManifest kitManifest (const std::vector<std::vector<std::uint8_t>>& wavs)
        {
            KitManifest m;
            m.kit.id = std::string (kSyntheticKitId);
            m.kit.title = "Synthetic Test Kit";
            m.kit.direction = "test kit";
            m.kit.bpm = kSyntheticBpm;
            m.kit.key = "G minor";
            m.kit.camelot = "6A";
            m.kit.swingPm = kSyntheticSwingPm;
            m.kit.contentVersion = 1;
            m.kit.lanes = { "Kick", "Snare", "Hats", "", "Bass", "Lead", "Chords", "Vox", "", "FX" };
            m.kit.ab = { AbLabels { "Kick A", "Kick B" }, AbLabels { "Bass A", "Bass B" }, AbLabels {} };

            for (std::size_t i = 0; i < kFiles.size(); ++i)
            {
                FileEntry f;
                f.path = std::string (kFiles[i].path);
                f.frames = kFiles[i].frames;
                f.channels = kFiles[i].channels;
                f.sha256 = util::sha256Hex (wavs[i]);
                const auto [loopStart, loopEnd] = loopOf (static_cast<int> (i));
                f.loopStart = loopStart;
                f.loopEnd = loopEnd;
                f.prerollFrames = kFiles[i].preroll;
                m.files.push_back (std::move (f));
            }

            auto& ins = m.instruments;
            ins.push_back (instrument ("kick_a", "Kick A", Lane::kick, "kick", SampleMode::oneShot, { zone (36, 36, 36, { layer (127, { 4 }) }) }));
            ins.push_back (instrument ("kick_b", "Kick B", Lane::kick, "kick", SampleMode::oneShot, { zone (36, 36, 36, { layer (64, { 0, 1 }), layer (127, { 2, 3 }) }) }));
            ins.push_back (instrument ("snare", "Snare", Lane::snare, "snare", SampleMode::oneShot, { zone (38, 38, 38, { layer (64, { 5, 6 }), layer (127, { 7, 8 }) }) }));
            ins.push_back (instrument ("hat_closed", "Closed Hat", Lane::hats, "hats", SampleMode::oneShot, { zone (42, 42, 42, { layer (127, { 9, 10 }) }) }));
            ins.push_back (instrument ("hat_open", "Open Hat", Lane::hats, "hats", SampleMode::oneShot, { zone (46, 46, 46, { layer (127, { 11 }) }) }));
            ins.push_back (instrument ("bass_a", "Bass A", Lane::bass, "bass", SampleMode::sustainLoop,
                { zone (31, 24, 37, { layer (127, { 12 }) }), zone (38, 38, 42, { layer (127, { 13 }) }), zone (43, 43, 60, { layer (127, { 14 }) }) }));
            ins.push_back (instrument ("bass_b", "Bass B", Lane::bass, "bass", SampleMode::sustainLoop,
                { zone (31, 24, 37, { layer (76, { 15 }), layer (127, { 16 }) }),
                    zone (38, 38, 42, { layer (76, { 17 }), layer (127, { 18 }) }),
                    zone (43, 43, 60, { layer (76, { 19 }), layer (127, { 20 }) }) }));
            ins.push_back (instrument ("lead", "Lead", Lane::lead, "lead", SampleMode::gated, { zone (72, 48, 96, { layer (127, { 21 }) }) }));
            ins.push_back (instrument ("stab", "Stab", Lane::chords, "chords", SampleMode::gated,
                { zone (55, 40, 58, { layer (127, { 22 }) }), zone (62, 59, 80, { layer (127, { 23 }) }) }));
            ins.push_back (instrument ("pad", "Pad", Lane::chords, "chords", SampleMode::sustainLoop, { zone (55, 40, 80, { layer (127, { 24 }) }) }));
            ins.push_back (instrument ("vox", "Vox", Lane::vox, "vox", SampleMode::gated, { zone (67, 55, 84, { layer (64, { 25 }), layer (95, { 26 }), layer (127, { 27 }) }) }));
            ins.push_back (instrument ("riser", "Riser", Lane::fx, "fx", SampleMode::oneShot, { zone (48, 48, 48, { layer (127, { 28 }) }) }));

            auto at = [&ins] (int i) -> InstrumentEntry& { return ins[static_cast<std::size_t> (i)]; };
            for (auto i : { syn::kickA, syn::kickB, syn::snare })
                at (i).duckKey = true;
            auto& closed = at (syn::hatClosed);
            closed.choke = { "hats", ChokeRole::chokes, 4000, ChokeCurve::cos2 };
            closed.panCyclePm = { -200, 200 };
            at (syn::hatOpen).choke = { "hats", ChokeRole::choked, 4000, ChokeCurve::cos2 };
            for (auto i : { syn::bassA, syn::bassB })
            {
                auto& bass = at (i);
                bass.attackUs = 1000;
                bass.releaseUs = 30000;
                bass.velocityLaw = VelocityLaw::pow15;
                bass.mono = true;
            }
            at (syn::bassA).interp = Interp::hermite;
            at (syn::bassA).glide = { GlideCurve::raisedCosine, 70000, 30000 };
            at (syn::bassB).glide = { GlideCurve::exponential, 18000, 0 };
            auto& lead = at (syn::lead);
            lead.attackUs = 2000;
            lead.releaseUs = 40000;
            lead.velocityLaw = VelocityLaw::pow15;
            auto& stab = at (syn::stab);
            stab.releaseUs = 30000;
            stab.velocityLaw = VelocityLaw::pow15;
            auto& pad = at (syn::pad);
            pad.attackUs = 5000;
            pad.releaseUs = 20000;
            pad.velocityLaw = VelocityLaw::pow15;
            pad.restrikeContinues = true;
            auto& vox = at (syn::vox);
            vox.releaseUs = 40000;
            vox.velocityLaw = VelocityLaw::pow15;
            auto& riser = at (syn::riser);
            riser.align = Align::end;
            riser.gainMdb = -3000;
            riser.startFrames = 4800;
            riser.attackUs = 2000;

            auto key = [] (std::int32_t ch, std::int32_t lo, std::int32_t hi, std::int32_t a, std::int32_t b = -1, std::int32_t ab = -1) {
                KeymapEntry k;
                k.ch = ch;
                k.lo = lo;
                k.hi = hi;
                k.a = a;
                k.b = b;
                k.ab = ab;
                return k;
            };
            m.keymap = {
                key (10, 36, 36, syn::kickA, syn::kickB, static_cast<std::int32_t> (AbSwitch::kick)),
                key (10, 38, 38, syn::snare),
                key (10, 42, 42, syn::hatClosed),
                key (10, 46, 46, syn::hatOpen),
                key (1, 24, 60, syn::bassA, syn::bassB, static_cast<std::int32_t> (AbSwitch::bass)),
                key (2, 48, 96, syn::lead),
                key (4, 40, 80, syn::stab),
                key (5, 55, 84, syn::vox),
                key (7, 48, 48, syn::riser),
                key (11, 40, 80, syn::pad),
            };

            ExtraEntry extra;
            extra.ch = 11;
            extra.name = "Pad";
            extra.lane = Lane::chords;
            m.extras.push_back (extra);

            PhraseEntry phrase;
            phrase.lane = Lane::lead;
            phrase.ch = 2;
            phrase.mixChannel = "lead_stem";
            phrase.stemA = syn::stemFile;
            phrase.cycle16ths = 32;
            phrase.tolerancePm = 20;
            phrase.fadeUs = 5000;
            for (int i = 0; i < phrase.cycle16ths * 240 / kRideTicks; ++i)
                phrase.ride74.push_back (static_cast<std::uint8_t> (leadRide (i)));
            for (std::size_t i = 0; i < kPhrase.size(); ++i)
            {
                const auto& note = kPhrase[i];
                const auto soundEnd = onsetFrames (note.step) + note.steps * kSyntheticSixteenthFrames;
                if (i == kHeldNote) // the slice before plays it: hold its key, and fade after it
                {
                    auto& holder = phrase.slices.back();
                    holder.holds[static_cast<std::size_t> (holder.holdCount++)] = note.key;
                    holder.fade = soundEnd - kPhraseFadeFrames;
                    continue;
                }
                SliceEntry s;
                s.step = note.step;
                s.key = note.key;
                s.lead = i == 0 ? 0 : 48;
                s.start = onsetFrames (s.step) - s.lead;
                s.fade = soundEnd - kPhraseFadeFrames;
                phrase.slices.push_back (s);
            }
            for (std::size_t i = 0; i < phrase.slices.size(); ++i) // contiguous: each ends where the next starts
                phrase.slices[i].end = i + 1 < phrase.slices.size() ? phrase.slices[i + 1].start : kFiles[static_cast<std::size_t> (syn::stemFile)].frames;
            m.phrases.push_back (std::move (phrase));
            return m;
        }

        template <class Params>
        Insert insert (InsertType type, Params params, When when = {})
        {
            Insert in;
            in.type = type;
            in.when = when;
            in.params = std::move (params);
            return in;
        }

        PassParams pass (std::int32_t freqHz, std::int32_t order, std::int32_t stages = 1, Band band = {})
        {
            PassParams p;
            p.freqHz = freqHz;
            p.order = order;
            p.stages = stages;
            p.band = band;
            return p;
        }

        Channel channel (std::string id, Lane lane, Group group, std::vector<Insert> inserts, std::array<std::int32_t, kSends> sends)
        {
            Channel c;
            c.id = std::move (id);
            c.lane = static_cast<std::uint8_t> (lane);
            c.group = group;
            c.inserts = std::move (inserts);
            c.sendsPm = sends;
            return c;
        }

        DuckParams onsetDuck (std::int32_t depthMdb, std::int32_t holdUs, std::int32_t releaseUs, Band band = {})
        {
            DuckParams d;
            d.mode = DuckMode::onset;
            d.keyLanes = static_cast<std::uint16_t> (laneBit (Lane::kick));
            d.minVelocity = 1;
            d.depthMdb = depthMdb;
            d.rampUs = 2000;
            d.holdUs = holdUs;
            d.releaseUs = releaseUs;
            d.band = band;
            d.pump = true;
            return d;
        }

        MixConfig mixConfig()
        {
            MixConfig m;
            m.kit = std::string (kSyntheticKitId);
            m.contentVersion = 1;

            When onBassB;
            onBassB.ab = static_cast<std::int8_t> (AbSwitch::bass);
            onBassB.is = 1;
            When noVox;
            noVox.variant = static_cast<std::int8_t> (Variant::novox);

            CcFilterParams sweep;
            sweep.response = FilterResponse::lowpass;
            sweep.cc74LoHz = 200;
            sweep.cc74HiHz = 8000;
            sweep.cc74Default = 127;
            sweep.cc71LoPm = 707;
            sweep.cc71HiPm = 4000;
            sweep.cc71Default = 0;

            CompParams glue;
            glue.thresholdMdb = -18000;
            glue.ratioPm = 4000;
            glue.kneeMdb = 6000;
            glue.attackUs = 5000;
            glue.releaseFastUs = 150000;
            glue.releaseSlowUs = 1500000;
            glue.makeupMdb = 2500;

            m.channels = {
                channel ("kick", Lane::kick, Group::drums, { insert (InsertType::gain, GainParams { -1000 }) }, { 0, 0, 0 }),
                channel ("snare", Lane::snare, Group::drums, { insert (InsertType::peak, EqParams { 200, 2000, 1000 }), insert (InsertType::pan, PanParams { 100 }) }, { 150, 0, 0 }),
                channel ("hats", Lane::hats, Group::drums, { insert (InsertType::highpass, pass (300, 2)), insert (InsertType::width, WidthParams { 1200, 3000, 2 }) }, { 40, 0, 0 }),
                channel ("bass", Lane::bass, Group::music,
                    { insert (InsertType::ccFilter, sweep),
                        insert (InsertType::monoBelow, MonoBelowParams { 120, 8 }),
                        insert (InsertType::duck, onsetDuck (6000, 78000, 60000, Band { 0, 120 })),
                        insert (InsertType::clip, ClipParams { -3000, 1500 }, onBassB) },
                    { 0, 0, 0 }),
                channel ("lead", Lane::lead, Group::music, { insert (InsertType::lowShelf, EqParams { 200, -2000, 707 }), insert (InsertType::highShelf, EqParams { 6000, 1500, 707 }) }, { 300, 250, 0 }),
                channel ("lead_stem", Lane::lead, Group::music, { insert (InsertType::gain, GainParams { -1500 }) }, { 300, 250, 0 }),
                channel ("chords", Lane::chords, Group::music, { insert (InsertType::lowpass, pass (6000, 4, 2)), insert (InsertType::duck, onsetDuck (3000, 40000, 80000)) }, { 250, 0, 100 }),
                channel ("vox", Lane::vox, Group::music, { insert (InsertType::comp, glue), insert (InsertType::gain, GainParams { -500 }, noVox) }, { 300, 100, 150 }),
                channel ("fx", Lane::fx, Group::music, { insert (InsertType::highpass, pass (150, 4)) }, { 250, 250, 0 }),
            };

            ParallelParams grit;
            grit.levelMdb = -8000;
            grit.chain = {
                insert (InsertType::highpass, pass (250, 4)),
                insert (InsertType::peak, EqParams { 3000, 3000, 700 }),
                insert (InsertType::drive, DriveParams { 2500, 0, 0 }),
                insert (InsertType::peak, EqParams { 600, -3000, 1000 }),
                insert (InsertType::lowpass, pass (9000, 2)),
            };
            m.drums.inserts = { insert (InsertType::parallel, grit), insert (InsertType::clip, ClipParams { -1000, 1500 }) };
            m.music.inserts = { insert (InsertType::peak, EqParams { 900, -3500, 900 }), insert (InsertType::peak, EqParams { 4000, 1500, 900 }) };

            auto& s = m.space;
            s.delaysFrames = { 515, 583, 682, 799, 885, 1029, 1167, 1361 };
            s.signs = { -1, -1, -1, 1, 1, -1, -1, -1 };
            s.t60Ms = 1100;
            s.t60LowMs = 400;
            s.lowHz = 500;
            s.dampHz = 9000;
            s.predelayUs = 27990;
            s.sendHpHz = 500;
            s.sendOrder = 4;
            s.shimmerPm = 30;
            s.shimmerLoHz = 300;
            s.shimmerHiHz = 7000;
            s.windowFrames = 2048;
            s.monoHz = 150;
            s.returnMdb = 3000;
            DuckParams underVox;
            underVox.mode = DuckMode::level;
            underVox.keyChannel = m.channelIndex ("vox");
            underVox.depthMdb = 6000;
            underVox.rangeMdb = 18000;
            underVox.topMdb = -12000;
            underVox.attackUs = 10000;
            underVox.releaseUs = 300000;
            s.inserts = { insert (InsertType::duck, underVox), insert (InsertType::duck, onsetDuck (4000, 20000, 250000)) };

            auto& e = m.echo;
            e.steps = 3;
            e.feedbackPm = 380;
            e.lpHz = 2600;
            e.hpHz = 600;
            e.drivePm = 1500;
            e.wowPpm = 400;
            e.wowMhz = 500;
            e.flutterPpm = 150;
            e.flutterMhz = 4700;
            e.phasesMrad = { 3054, 183 };
            e.returnMdb = 3000;

            auto& r = m.resonator;
            r.tonesMhz = { 783991, 1174659 };
            r.tauMs = 400;
            r.sendHpHz = 600;
            r.returnMdb = -21000;

            m.busEq[0] = { insert (InsertType::lowShelf, EqParams { 120, 1000, 707 }) };
            m.busEq[1] = { insert (InsertType::lowShelf, EqParams { 120, 1500, 707 }), insert (InsertType::highShelf, EqParams { 8000, 1000, 707 }) };

            MasterConfig master; // the defaults are master.py's
            m.master = { master, master };
            m.master[0].glueThresholdMdb = -12940;
            m.master[0].driveMdb = 4170;
            m.master[1].glueThresholdMdb = -13060;
            m.master[1].driveMdb = 4210;

            m.reference[0].preMasterLufsMlu = -11810;
            m.reference[0].clubLufsMlu = -9730;
            m.reference[0].channelTruePeakMdb = { -3000, -6100, -12400, -4200, -9000, -8800, -10300, -5100, -11900 };
            m.reference[1].preMasterLufsMlu = -11870;
            m.reference[1].clubLufsMlu = -9710;
            m.reference[1].channelTruePeakMdb = { -3000, -6100, -12400, -4200, -9000, -8800, -9800, -120000, -11900 };
            return m;
        }

        // ---------------------------------------------------------------- the MIDI

        std::int64_t tick (int step)
        {
            return step * 240 + (step % 2 != 0 ? kSyntheticSwingTicks : 0);
        }

        void loopKicks (SmfBuilder::Track& t, int o)
        {
            for (int s = 0; s < 64; s += 4)
                t.note (tick (o + s), 120, 10, 36, 127);
            t.note (tick (o + 62), 120, 10, 36, 64); // a ghost: the soft layer
        }

        void plainKicks (SmfBuilder::Track& t, int from, int to)
        {
            for (int s = from; s < to; s += 4)
                t.note (tick (s), 120, 10, 36, 127);
        }

        void loopSnares (SmfBuilder::Track& t, int o)
        {
            for (int s = 4; s < 64; s += 8)
                t.note (tick (o + s), 120, 10, 38, 127);
            t.note (tick (o + 59), 120, 10, 38, 70);
        }

        void loopHats (SmfBuilder::Track& t, int o)
        {
            for (int s = 0; s < 64; ++s)
            {
                if (s % 16 == 6)
                    t.note (tick (o + s), 240, 10, 46, 100); // the open hat, choked by the next closed one
                else if (s % 2 == 1)
                    t.note (tick (o + s), 60, 10, 42, 90);
                else if (s % 4 == 2)
                    t.note (tick (o + s), 60, 10, 42, 110);
            }
        }

        void loopBass (SmfBuilder::Track& t, int o)
        {
            struct Note
            {
                int step, key, velocity;
                std::int64_t length;
            };
            const std::int64_t slide = 12; // midi.py: the previous note held 12 ticks into the next
            for (const int rep : { o, o + 32 })
            {
                const Note notes[] {
                    { 0, 31, 127, 720 },
                    { 3, 38, 100, 240 },
                    { 6, 43, 127, 480 },
                    { 8, 29, 110, 960 }, // F1: below the lowest zone's root, played on G1 pitched down
                    { 12, 31, 127, tick (16) - tick (12) + slide }, // slides into the next
                    { 16, 38, 127, 1920 },
                    { 24, 36, 100, tick (26) - tick (24) + slide }, // legato into a dip...
                    { 26, 38, 100, tick (27) - tick (26) + slide }, // ...which returns to 36
                    { 27, 36, 90, tick (32) - tick (27) - 120 },
                };
                for (const auto& n : notes)
                    t.note (tick (rep + n.step), n.length, 1, n.key, n.velocity);
            }
            t.cc (tick (o), 1, 71, 20);
            for (std::int64_t at = 0; at <= 3840; at += 60) // the CC74 ride: every 60 ticks while it moves
                t.cc (tick (o) + at, 1, 74, static_cast<int> (40 + 80 * at / 3840));
        }

        void loopLead (SmfBuilder::Track& t, int o)
        {
            for (const int rep : { o, o + 32 })
            {
                for (int i = 0; i < 128; ++i) // the ride the phrase's stem matches: CC74 every 60 ticks
                    t.cc (tick (rep) + i * kRideTicks, 2, 74, leadRide (i));
                for (const auto& n : kPhrase)
                    t.note (tick (rep + n.step), n.steps * 240, 2, n.key, 100);
            }
        }

        void loopVoicing (SmfBuilder::Track& t, int o)
        {
            for (const int rep : { o, o + 32 })
                for (const int key : { 55, 58, 62 })
                    t.note (tick (rep), 7680, 3, key, 80);
        }

        void loopChords (SmfBuilder::Track& t, int o, bool vox)
        {
            t.note (tick (o), 360, 4, 55, 127);
            t.note (tick (o + 14), 360, 4, 62, 100);
            t.note (tick (o + 32), 360, 4, 55, 127);
            if (vox)
                t.note (tick (o + 46), 360, 4, 62, 100); // the no-vox file drops this stab
        }

        void loopVox (SmfBuilder::Track& t, int o)
        {
            t.note (tick (o + 16), 480, 5, 67, 64); // velocity picks the vowel: 64, 95, 127
            t.note (tick (o + 18), 480, 5, 70, 95);
            t.note (tick (o + 20), 480, 5, 72, 127);
            t.cc (tick (o + 23), 5, 91, 127); // a Space throw on the next note, back to 0 at its note-off
            t.note (tick (o + 23), 480, 5, 74, 95);
            t.cc (tick (o + 23) + 480, 5, 91, 0);
        }

        std::vector<std::uint8_t> smf (bool song, bool vox)
        {
            SmfBuilder b (960);
            b.name (song ? "SYN01 Synthetic Test Kit - Song" : "SYN01 Synthetic Test Kit - Loop");
            b.tempo (0, midi::microsPerQuarter (kSyntheticBpm)).timeSignature (0, 4, 2).keySignature (0, midi::sharpsFor (7, true), true);
            if (song)
            {
                b.marker (0, "Intro").marker (15360, "Drop").marker (46080, "Outro");
                b.cue (11520, "Riser").cue (15360, "Vox");
            }

            auto& kick = b.track ("Kick");
            auto& snare = b.track ("Snare");
            auto& hats = b.track ("Hats");
            auto& bass = b.track ("Bass");
            auto& lead = b.track ("Lead");
            auto& voicing = b.track ("Chord voicing");
            auto& chords = b.track ("Chords");
            auto& pad = b.track ("Pad");
            SmfBuilder::Track* voxTrack = vox ? &b.track ("Vox") : nullptr;
            auto& fx = b.track ("FX");

            if (! song)
            {
                loopKicks (kick, 0);
                loopSnares (snare, 0);
                loopHats (hats, 0);
                loopBass (bass, 0);
                loopLead (lead, 0);
                loopVoicing (voicing, 0);
                loopChords (chords, 0, vox);
                pad.note (0, 15360, 11, 55, 100); // held for the whole loop, struck again at the wrap
                if (voxTrack != nullptr)
                    loopVox (*voxTrack, 0);
                fx.note (11520, 3840, 7, 48, 127); // end-aligned: ends on the loop's end
                return b.bytes (kSyntheticLoopTicks);
            }

            plainKicks (kick, 0, 64); // Intro: kick and hats
            loopHats (hats, 0);
            for (const int o : { 64, 128 }) // Drop: everything
            {
                loopKicks (kick, o);
                loopSnares (snare, o);
                loopHats (hats, o);
                loopBass (bass, o);
                loopLead (lead, o);
                loopVoicing (voicing, o);
                loopChords (chords, o, vox);
                if (voxTrack != nullptr)
                    loopVox (*voxTrack, o);
            }
            pad.note (15360, 15360, 11, 55, 100);
            pad.note (30720, 15360, 11, 55, 100); // struck again on its own end tick
            plainKicks (kick, 192, 256); // Outro: kick and pad
            pad.note (46080, 15360, 11, 55, 100);
            fx.note (11520, 3840, 7, 48, 127); // the riser ends on the Drop's first beat
            return b.bytes (kSyntheticSongTicks);
        }

        // ---------------------------------------------------------------- the Kit

        template <class... Args>
        util::Result<std::shared_ptr<Kit>> refuse (Args&&... parts)
        {
            std::string message;
            (message += ... += std::string (std::forward<Args> (parts)));
            return util::Result<std::shared_ptr<Kit>>::failure (std::move (message));
        }
    }

    std::vector<std::uint8_t> wavBytes (const SampleData& audio)
    {
        const auto channels = static_cast<std::uint32_t> (audio.channels);
        const auto frames = static_cast<std::uint64_t> (audio.frames);
        const auto dataBytes = static_cast<std::uint32_t> (frames * channels * 3u);
        std::vector<std::uint8_t> out;
        out.reserve (44u + dataBytes + 1u);
        auto u32 = [&out] (std::uint32_t v) {
            for (int i = 0; i < 4; ++i)
                out.push_back (static_cast<std::uint8_t> (v >> (8 * i)));
        };
        auto u16 = [&out] (std::uint32_t v) {
            out.push_back (static_cast<std::uint8_t> (v));
            out.push_back (static_cast<std::uint8_t> (v >> 8));
        };
        auto tag = [&out] (const char* t) { out.insert (out.end(), t, t + 4); };

        const std::uint32_t pad = dataBytes % 2u;
        tag ("RIFF");
        u32 (4u + 8u + 16u + 8u + dataBytes + pad);
        tag ("WAVE");
        tag ("fmt ");
        u32 (16u);
        u16 (1u); // PCM
        u16 (channels);
        u32 (static_cast<std::uint32_t> (audio.sampleRate));
        u32 (static_cast<std::uint32_t> (audio.sampleRate) * channels * 3u);
        u16 (channels * 3u);
        u16 (24u);
        tag ("data");
        u32 (dataBytes);
        for (std::uint64_t n = 0; n < frames; ++n)
            for (std::uint32_t c = 0; c < channels; ++c)
            {
                const float x = audio.audio[c * frames + n];
                const auto v = static_cast<std::int32_t> (static_cast<double> (x) * 8388608.0); // exact: x is k / 2^23
                const auto bits = static_cast<std::uint32_t> (v);
                out.push_back (static_cast<std::uint8_t> (bits));
                out.push_back (static_cast<std::uint8_t> (bits >> 8));
                out.push_back (static_cast<std::uint8_t> (bits >> 16));
            }
        if (pad != 0)
            out.push_back (0);
        return out;
    }

    SyntheticContent buildSyntheticContent()
    {
        SyntheticContent content;
        for (int i = 0; i < syn::fileCount; ++i)
        {
            content.audio.push_back (audioOf (i));
            content.wavs.push_back (wavBytes (content.audio.back()));
        }
        content.kit = kitManifest (content.wavs);
        content.mix = mixConfig();
        content.kitJson = kitManifestJson (content.kit);
        content.mixJson = mixManifestJson (content.mix);
        content.smfs[0] = smf (false, true);
        content.smfs[1] = smf (false, false);
        content.smfs[2] = smf (true, true);
        content.smfs[3] = smf (true, false);
        return content;
    }

    const SyntheticContent& syntheticContent()
    {
        static const SyntheticContent content = buildSyntheticContent();
        return content;
    }

    KitManifest syntheticKitManifest() { return syntheticContent().kit; }
    MixConfig syntheticMixConfig() { return syntheticContent().mix; }
    std::string syntheticKitJson() { return syntheticContent().kitJson; }
    std::string syntheticMixJson() { return syntheticContent().mixJson; }

    std::vector<std::uint8_t> syntheticSmf (PlayMode mode, Variant variant)
    {
        const int index = timelineIndex (mode, variant);
        return index < 0 ? std::vector<std::uint8_t> {} : syntheticContent().smfs[static_cast<std::size_t> (index)];
    }

    util::Result<SampleData> SyntheticSampleSource::read (std::string_view relativePath)
    {
        const auto& content = syntheticContent();
        for (std::size_t i = 0; i < content.kit.files.size(); ++i)
            if (content.kit.files[i].path == relativePath)
            {
                ++reads;
                return util::Result<SampleData>::success (content.audio[i]);
            }
        return util::Result<SampleData>::failure ("missing: " + std::string (relativePath) + " isn't in the synthetic kit");
    }

    util::Result<int> writeSyntheticKitFolder (const std::filesystem::path& kitFolder)
    {
        const auto& content = syntheticContent();
        int written = 0;
        for (std::size_t i = 0; i < content.kit.files.size(); ++i)
        {
            const auto path = kitFolder / std::filesystem::path (content.kit.files[i].path);
            std::error_code error;
            std::filesystem::create_directories (path.parent_path(), error);
            if (error)
                return util::Result<int>::failure ("can't create " + path.parent_path().string() + ": " + error.message());
            const auto wrote = util::writeBinaryFile (path, content.wavs[i]);
            if (! wrote.ok())
                return util::Result<int>::failure (wrote.error);
            ++written;
        }
        return util::Result<int>::success (written);
    }

    util::Result<std::shared_ptr<Kit>> layOutKit (const KitManifest& m, const MixConfig& mix, SampleSource& source)
    {
        auto kit = std::make_shared<Kit>();

        auto& info = kit->info;
        info.id = m.kit.id;
        info.title = m.kit.title;
        info.direction = m.kit.direction;
        info.key = m.kit.key;
        info.camelot = m.kit.camelot;
        info.bpm = m.kit.bpm;
        info.swingPm = m.kit.swingPm;
        info.contentVersion = m.kit.contentVersion;
        info.laneLabels = m.kit.lanes;
        info.ab = m.kit.ab;
        for (std::size_t i = 0; i < m.kit.lanes.size(); ++i)
            if (! m.kit.lanes[i].empty())
                info.lanes |= 1u << i;
        for (std::size_t i = 0; i < m.kit.ab.size(); ++i)
            if (m.kit.ab[i].exists())
                info.abSwitches |= 1u << i;
        for (const auto& e : m.extras)
            info.extras.push_back ({ static_cast<std::uint8_t> (e.ch - 1), e.name, e.lane });

        const auto fileCount = m.files.size();
        auto validFile = [fileCount] (std::int64_t index) { return index >= 0 && static_cast<std::size_t> (index) < fileCount; };

        for (const auto& f : m.files)
        {
            auto read = source.read (f.path);
            if (! read.ok())
                return refuse (read.error);
            if (read.value.frames != f.frames || read.value.channels != f.channels)
                return refuse ("mismatch: ", f.path, " has ", std::to_string (read.value.frames), " frames and ",
                    std::to_string (read.value.channels), " channels; the manifest says ", std::to_string (f.frames), " and ", std::to_string (f.channels));
            kit->files.push_back ({ f.path, f.frames, f.channels, f.loopStart, f.loopEnd, f.prerollFrames });
            kit->audio.push_back (std::move (read.value));
        }

        std::vector<std::string> chokeGroups;
        for (std::size_t i = 0; i < m.instruments.size(); ++i)
        {
            const auto& e = m.instruments[i];
            const int mixChannel = mix.channelIndex (e.mixChannel);
            if (mixChannel < 0)
                return refuse ("kit.json: instruments[", std::to_string (i), "].mix_channel: no mix.json channel \"", e.mixChannel, "\"");

            Instrument in;
            in.id = e.id;
            in.name = e.name;
            in.lane = e.lane;
            in.mixChannel = static_cast<std::uint8_t> (mixChannel);
            in.mode = e.mode;
            in.align = e.align;
            in.attackUs = e.attackUs;
            in.releaseUs = e.releaseUs;
            in.gainMdb = e.gainMdb;
            in.startFrames = e.startFrames;
            in.velocityLaw = e.velocityLaw;
            in.interp = e.interp;
            in.mono = e.mono;
            in.glide = e.glide;
            in.restrikeContinues = e.restrikeContinues;
            in.choke.role = e.choke.role;
            in.choke.fadeUs = e.choke.fadeUs;
            in.choke.curve = e.choke.curve;
            if (! e.choke.group.empty())
            {
                auto found = std::find (chokeGroups.begin(), chokeGroups.end(), e.choke.group);
                if (found == chokeGroups.end())
                    found = chokeGroups.insert (chokeGroups.end(), e.choke.group);
                in.choke.group = static_cast<std::uint8_t> (1 + (found - chokeGroups.begin()));
            }
            in.duckKey = e.duckKey;
            in.firstPan = static_cast<std::uint16_t> (kit->pans.size());
            in.panCount = static_cast<std::uint8_t> (e.panCyclePm.size());
            for (const auto pan : e.panCyclePm)
                kit->pans.push_back (static_cast<std::int16_t> (pan));
            in.rrBlock16ths = e.rrBlock16ths;
            if (e.layeredWith >= static_cast<std::int32_t> (m.instruments.size()))
                return refuse ("kit.json: instruments[", std::to_string (i), "].layered_with: no instrument ", std::to_string (e.layeredWith));
            in.layeredWith = static_cast<std::int16_t> (e.layeredWith);

            // Zones, layers and round robins, nested in this order, so each table is appended in kit.h's order.
            in.firstZone = static_cast<std::uint16_t> (kit->zones.size());
            in.zoneCount = static_cast<std::uint8_t> (e.zones.size());
            for (const auto& z : e.zones)
            {
                Zone zone;
                zone.root = static_cast<std::uint8_t> (z.root);
                zone.lo = static_cast<std::uint8_t> (z.lo);
                zone.hi = static_cast<std::uint8_t> (z.hi);
                zone.firstLayer = static_cast<std::uint16_t> (kit->layers.size());
                zone.layerCount = static_cast<std::uint8_t> (z.layers.size());
                for (const auto& l : z.layers)
                {
                    kit->layers.push_back ({ static_cast<std::uint8_t> (l.vel), static_cast<std::uint16_t> (kit->rr.size()), static_cast<std::uint8_t> (l.rr.size()) });
                    for (const auto file : l.rr)
                    {
                        if (! validFile (file))
                            return refuse ("kit.json: instruments[", std::to_string (i), "]: rr file ", std::to_string (file), " doesn't exist (",
                                std::to_string (fileCount), " files)");
                        kit->rr.push_back (static_cast<std::uint16_t> (file));
                    }
                }
                kit->zones.push_back (zone);
            }
            kit->instruments.push_back (std::move (in));
        }

        const auto instrumentCount = static_cast<std::int32_t> (m.instruments.size());
        for (std::size_t i = 0; i < m.keymap.size(); ++i)
        {
            const auto& k = m.keymap[i];
            if (k.ch < 1 || k.ch > 16 || k.lo < 0 || k.hi > 127 || k.lo > k.hi || k.a < 0 || k.a >= instrumentCount || k.b >= instrumentCount)
                return refuse ("kit.json: keymap[", std::to_string (i), "] names a channel, key or instrument that doesn't exist");
            for (int key = k.lo; key <= k.hi; ++key)
                kit->keys[static_cast<std::size_t> (k.ch - 1)][static_cast<std::size_t> (key)] = { static_cast<std::int16_t> (k.a), static_cast<std::int16_t> (k.b), static_cast<std::int8_t> (k.b >= 0 ? k.ab : -1) };
            auto& bits = kit->ccMixChannels[static_cast<std::size_t> (k.ch - 1)];
            for (const auto target : { k.a, k.b })
            {
                if (target < 0)
                    continue;
                const auto& in = kit->instruments[static_cast<std::size_t> (target)];
                bits |= 1u << in.mixChannel;
                if (in.layeredWith >= 0)
                    bits |= 1u << kit->instruments[static_cast<std::size_t> (in.layeredWith)].mixChannel;
            }
        }

        for (std::size_t i = 0; i < m.phrases.size(); ++i)
        {
            const auto& e = m.phrases[i];
            const int mixChannel = mix.channelIndex (e.mixChannel);
            if (mixChannel < 0)
                return refuse ("kit.json: phrases[", std::to_string (i), "].mix_channel: no mix.json channel \"", e.mixChannel, "\"");
            if (! validFile (e.stemA) || (e.stemB >= 0 && ! validFile (e.stemB)) || e.ch < 1 || e.ch > 16)
                return refuse ("kit.json: phrases[", std::to_string (i), "] names a stem file or channel that doesn't exist");
            Phrase p;
            p.lane = e.lane;
            p.channel = static_cast<std::uint8_t> (e.ch - 1);
            p.ab = static_cast<std::int8_t> (e.stemB >= 0 ? e.ab : -1);
            p.mixChannel = static_cast<std::uint8_t> (mixChannel);
            p.gainMdb = e.gainMdb;
            p.stem = { static_cast<std::uint16_t> (e.stemA), static_cast<std::uint16_t> (e.stemB >= 0 ? e.stemB : e.stemA) };
            p.cycle16ths = e.cycle16ths;
            p.tolerancePm = e.tolerancePm;
            p.fadeUs = e.fadeUs;
            p.firstSlice = static_cast<std::uint16_t> (kit->slices.size());
            p.sliceCount = static_cast<std::uint16_t> (e.slices.size());
            for (const auto& s : e.slices)
            {
                PhraseSlice slice;
                slice.step = s.step;
                slice.key = static_cast<std::uint8_t> (s.key);
                slice.start = s.start;
                slice.end = s.end;
                slice.fade = s.fade;
                slice.lead = s.lead;
                slice.holdCount = static_cast<std::uint8_t> (std::clamp (s.holdCount, 0, kMaxHolds));
                for (std::size_t h = 0; h < slice.holdCount; ++h)
                    slice.holds[h] = static_cast<std::uint8_t> (s.holds[h]);
                kit->slices.push_back (slice);
            }
            kit->ccMixChannels[p.channel] |= 1u << p.mixChannel;
            kit->phrases.push_back (p);
        }

        kit->mix = mix;
        return util::Result<std::shared_ptr<Kit>>::success (std::move (kit));
    }

    std::shared_ptr<const Kit> makeSyntheticKit()
    {
        static const std::shared_ptr<const Kit> kit = [] {
            SyntheticSampleSource source;
            auto laidOut = layOutKit (syntheticKitManifest(), syntheticMixConfig(), source);
            return laidOut.ok() ? std::shared_ptr<const Kit> (std::move (laidOut.value)) : std::shared_ptr<const Kit> {};
        }();
        return kit;
    }
}
