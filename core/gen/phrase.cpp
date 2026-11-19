// The phrase library (phrase.h, ADR 0031): the spec and its loader, the generator, its checks, names and files. The library is still a stub.

#include "gen/phrase.h"

#include "gen/hook_material.h"
#include "gen/json.h"
#include "gen/theory.h"
#include "util/ints.h"
#include "util/file_io.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <set>
#include <system_error>

namespace wubansia::gen
{
    std::optional<PhraseKind> phraseKindFromId (std::string_view id) noexcept
    {
        for (std::size_t i = 0; i < phraseKindCount; ++i)
            if (phraseKindIds[i] == id)
                return static_cast<PhraseKind> (i);
        return std::nullopt;
    }

    std::string_view phraseKindWord (PhraseKind kind) noexcept
    {
        static constexpr std::array<std::string_view, phraseKindCount> words { "Hook", "Call & answer", "Period", "Sparse", "Arp", "Stabs", "Counter-line", "Bounce" };
        return words[static_cast<std::size_t> (kind)];
    }

    std::string_view phraseKindFileWord (PhraseKind kind) noexcept
    {
        static constexpr std::array<std::string_view, phraseKindCount> words { "Hook", "Answer", "Period", "Sparse", "Arp", "Stabs", "Counter", "Bounce" };
        return words[static_cast<std::size_t> (kind)];
    }

    std::int32_t phraseFormCount (PhraseKind kind) noexcept
    {
        static constexpr std::array<std::int32_t, phraseKindCount> forms { 4, 3, 2, 2, 2, 2, 2, 3 };
        return forms[static_cast<std::size_t> (kind)];
    }

    std::string phraseId (PhraseKind kind, std::uint64_t seed) { return std::string (phraseKindIds[static_cast<std::size_t> (kind)]) + "-" + std::to_string (seed); }

    std::optional<std::pair<PhraseKind, std::uint64_t>> parsePhraseId (std::string_view id)
    {
        const auto dash = id.rfind ('-');
        if (dash == std::string_view::npos || dash + 1 >= id.size())
            return std::nullopt;
        const auto kind = phraseKindFromId (id.substr (0, dash));
        if (!kind)
            return std::nullopt;
        const std::string_view digits = id.substr (dash + 1);
        if (digits.size() > 1 && digits.front() == '0')
            return std::nullopt;
        std::uint64_t seed = 0;
        const auto [end, ec] = std::from_chars (digits.data(), digits.data() + digits.size(), seed);
        if (ec != std::errc() || end != digits.data() + digits.size())
            return std::nullopt;
        return std::pair { *kind, seed };
    }

    // ------------------------------------------------------------------ the spec

    namespace
    {
        using json::Node;

        std::string lowered (std::string_view s)
        {
            std::string out (s);
            for (char& c : out)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char> (c - 'A' + 'a');
            return out;
        }

        // Weights: count of them, each 0-100, not all 0.
        template <std::size_t N>
        std::array<std::int32_t, N> weightsOf (const Node& n)
        {
            std::array<std::int32_t, N> out {};
            const auto w = n.ints (N, N, 0, 100);
            std::int32_t total = 0;
            for (std::size_t i = 0; i < w.size() && i < N; ++i)
            {
                out[i] = w[i];
                total += w[i];
            }
            if (w.size() == N && total == 0)
                n.fail ("every weight is 0: none could be picked");
            return out;
        }

        // A kind's bar-1 rhythms: onsets 0-15 ascending, one length (1-16) per onset, the accents among the onsets.
        std::vector<HookCell> cellsOf (const Node& n)
        {
            std::vector<HookCell> out;
            if (n.isArray() && n.size() == 0)
                n.fail ("at least one cell is needed");
            std::int32_t total = 0;
            for (std::size_t i = 0; i < n.size(); ++i)
            {
                const Node c = n.at (i);
                c.only ({ "on", "len", "acc", "w" });
                HookCell cell;
                cell.on = c["on"].ints (1, 16, 0, 15);
                for (std::size_t k = 1; k < cell.on.size(); ++k)
                    if (cell.on[k] <= cell.on[k - 1])
                    {
                        c["on"].fail ("the onsets are 16ths in ascending order, each once");
                        break;
                    }
                const Node len = c["len"];
                if (len.isArray() && len.size() != cell.on.size())
                    len.fail ("one length per onset: " + std::to_string (cell.on.size()) + " onsets, " + std::to_string (len.size()) + " lengths");
                else
                    cell.len = len.ints (cell.on.size(), cell.on.size(), 1, 16);
                cell.acc = c["acc"].ints (0, 16, 0, 15);
                for (const auto a : cell.acc)
                    if (std::find (cell.on.begin(), cell.on.end(), a) == cell.on.end())
                    {
                        c["acc"].fail ("the accent on 16th " + std::to_string (a) + " isn't one of the onsets");
                        break;
                    }
                cell.weight = c["w"].int32 (0, 100);
                total += cell.weight;
                out.push_back (std::move (cell));
            }
            if (n.size() > 0 && total == 0)
                n.fail ("every weight is 0: no cell could be picked");
            return out;
        }

        PhraseKindSpec kindOf (const Node& k, PhraseKind kind, Range range)
        {
            k.only ({ "quota", "forms", "channel", "track", "register", "cells", "pool", "start", "end", "moves", "leap", "center", "span", "vel", "ladders" });
            PhraseKindSpec ks;
            ks.quota = k["quota"].int32 (1, 64);
            const Node forms = k["forms"];
            const auto count = static_cast<std::size_t> (phraseFormCount (kind));
            if (forms.isArray() && forms.size() != count)
                forms.fail ("a " + std::string (phraseKindIds[static_cast<std::size_t> (kind)]) + " has " + std::to_string (count) + " form weights, not "
                            + std::to_string (forms.size()));
            else
            {
                ks.forms = forms.ints (count, count, 0, 100);
                std::int32_t total = 0;
                for (const auto w : ks.forms)
                    total += w;
                if (ks.forms.size() == count && total == 0)
                    forms.fail ("every weight is 0: no form could be picked");
            }
            ks.channel = static_cast<std::uint8_t> (k.int32Or ("channel", 2, 1, 16));
            if (const Node t = k.optional ("track"); t.exists())
                ks.track = t.string (1, 32);
            if (const Node r = k.optional ("register"); r.exists())
            {
                const auto keys = r.ints (2, 2, 0, 127);
                if (keys.size() == 2 && keys[0] > keys[1])
                    r.fail ("the register is empty: " + std::to_string (keys[0]) + " is above " + std::to_string (keys[1]));
                else if (keys.size() == 2)
                    ks.range = Range { keys[0], keys[1] };
            }
            if (const Node c = k.optional ("cells"); c.exists())
                ks.cells = cellsOf (c);
            if (const Node pool = k.optional ("pool"); pool.exists())
                ks.pool = pool.ints (1, 7, 0, 6);
            if (const Node w = k.optional ("start"); w.exists())
                ks.start = weightsOf<7> (w);
            if (const Node w = k.optional ("end"); w.exists())
                ks.end = weightsOf<7> (w);
            if (const Node w = k.optional ("moves"); w.exists())
                ks.moves = weightsOf<8> (w);
            ks.leap = k.int32Or ("leap", 0, 1, 12);
            ks.center = k.int32Or ("center", 0, range.lo, range.hi);
            ks.span = k.int32Or ("span", 0, 1, 24);
            ks.velShift = k.int32Or ("vel", 0, -40, 40);
            const Node ladders = kind == PhraseKind::arp ? k["ladders"] : k.optional ("ladders");
            if (kind != PhraseKind::arp && ladders.exists())
                ladders.fail ("ladders are the arp's only");
            else if (ladders.exists())
            {
                if (ladders.isArray() && ladders.size() == 0)
                    ladders.fail ("at least one ladder is needed");
                for (std::size_t i = 0; i < ladders.size(); ++i)
                    ks.ladders.push_back (ladders.at (i).ints (1, 16, 0, 8));
            }
            return ks;
        }

        std::vector<std::string> wordsOf (const Node& n)
        {
            if (n.isArray() && n.size() != 24)
            {
                n.fail ("24 words, not " + std::to_string (n.size()));
                return {};
            }
            auto words = n.strings (24, 24);
            for (std::size_t i = 0; i < words.size(); ++i)
            {
                const auto& w = words[i];
                bool ok = !w.empty() && w.size() <= 16;
                for (const char c : w)
                    ok = ok && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
                if (!ok)
                {
                    n.at (i).fail ("a word is 1-16 letters, A-Z and a-z, not \"" + w + "\"");
                    break;
                }
                if (std::find (words.begin(), words.begin() + static_cast<std::ptrdiff_t> (i), w) != words.begin() + static_cast<std::ptrdiff_t> (i))
                {
                    n.at (i).fail ("\"" + w + "\" twice");
                    break;
                }
            }
            return words;
        }
    }

    util::Result<PhraseSpec> parsePhraseSpec (std::string_view text, std::string_view source)
    {
        using R = util::Result<PhraseSpec>;
        const auto parsed = json::parse (text);
        if (!parsed.ok())
            return R::failure (std::string (source) + ": " + parsed.error);
        json::Errors errors { std::string (source) };
        const Node root = json::root (parsed.value, errors);
        root.only ({ "format", "kit", "tonic", "bpm", "about", "range", "swing", "chords", "material", "ending", "nearDuplicate", "kinds", "names", "rename", "cut" });
        PhraseSpec spec;
        (void) root["format"].int32 (1, 1);
        spec.kitId = root["kit"].string (1, 16);
        spec.tonic = root["tonic"].int32 (0, 11);
        spec.bpm = root["bpm"].int32 (128, 140);
        spec.about = root["about"].string (1, 400);
        {
            const Node range = root["range"];
            const auto r = range.ints (2, 2, 0, 127);
            if (r.size() == 2)
            {
                spec.range = { r[0], r[1] };
                if (r[0] > r[1])
                    range.fail ("the range is empty: " + std::to_string (r[0]) + " is above " + std::to_string (r[1]));
            }
        }
        spec.swing = root["swing"].int32 (0, 120);
        {
            const Node chords = root["chords"];
            if (chords.isArray() && (chords.size() < 1 || chords.size() > 4))
                chords.fail ("1-4 chords, one a bar, not " + std::to_string (chords.size()));
            else
                for (std::size_t i = 0; i < chords.size(); ++i)
                {
                    const Node c = chords.at (i);
                    c.only ({ "name", "degree", "keys" });
                    AppChord ch;
                    ch.name = c["name"].string (1, 16);
                    ch.degree = c["degree"].int32 (0, 6);
                    ch.keys = c["keys"].ints (1, 8, 0, 127);
                    spec.chords.push_back (std::move (ch));
                }
        }
        {
            const Node m = root["material"];
            m.only ({ "register", "contours", "cells", "mutate", "answer", "pitch", "vel", "guard", "period", "pedal" });
            if (m.exists())
                readHookMaterial (m, spec.material);
        }
        spec.ending = weightsOf<7> (root["ending"]);
        spec.nearDuplicate = root["nearDuplicate"].int32 (1, 1000);
        {
            const Node kinds = root["kinds"];
            kinds.only ({ "hook", "answer", "period", "sparse", "arp", "stabs", "counter", "bounce" });
            for (std::size_t i = 0; i < phraseKindCount; ++i)
                spec.kinds[i] = kindOf (kinds[phraseKindIds[i]], static_cast<PhraseKind> (i), spec.range);
        }
        {
            const Node names = root["names"];
            names.only ({ "first", "second" });
            spec.words[0] = wordsOf (names["first"]);
            spec.words[1] = wordsOf (names["second"]);
            for (std::size_t i = 0; i < spec.words[1].size() && !errors.any(); ++i)
                if (std::find (spec.words[0].begin(), spec.words[0].end(), spec.words[1][i]) != spec.words[0].end())
                    names["second"].at (i).fail ("\"" + spec.words[1][i] + "\" is in both lists");
        }
        for (const auto& [id, name] : root["rename"].members())
        {
            const auto parsedId = parsePhraseId (id);
            if (!parsedId || parsedId->second >= firstMoreSeed)
            {
                name.fail ("no library phrase can be \"" + id + "\": a library id is <kind>-<seed>, the seed under " + std::to_string (firstMoreSeed));
                continue;
            }
            spec.rename.emplace_back (id, name.string (1, 40));
        }
        for (const auto& [kindId, c] : root["cut"].members())
        {
            const auto kind = phraseKindFromId (kindId);
            if (!kind)
            {
                c.fail ("unknown kind \"" + kindId + "\"");
                continue;
            }
            PhraseKindSpec& ks = spec.kinds[static_cast<std::size_t> (*kind)];
            if (c.isString())
            {
                if (c.string() != "all")
                    c.fail ("a cut is a list of seeds, or \"all\" for the whole kind, not \"" + c.string() + "\"");
                ks.cutAll = true;
                continue;
            }
            for (std::size_t i = 0; i < c.size(); ++i)
            {
                const auto seed = static_cast<std::uint64_t> (c.at (i).integer (0, static_cast<std::int64_t> (firstMoreSeed) - 1));
                if (std::find (ks.cut.begin(), ks.cut.end(), seed) != ks.cut.end())
                {
                    c.fail (std::to_string (seed) + " is listed twice");
                    break;
                }
                ks.cut.push_back (seed);
            }
        }
        if (errors.any())
            return R::failure (errors.first());
        return R::success (std::move (spec));
    }

    std::filesystem::path phraseSpecPath (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        return contentDir / "phrases" / lowered (kitId) / (std::string (kitId) + "_phrases.json");
    }

    std::filesystem::path phraseLibraryPath (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        return contentDir / "phrases" / lowered (kitId) / (std::string (kitId) + "_phrase_library.json");
    }

    util::Result<PhraseSpec> loadPhraseSpec (const std::filesystem::path& contentDir, std::string_view kitId)
    {
        using R = util::Result<PhraseSpec>;
        const auto shown = [] (const std::filesystem::path& p) {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        };
        const auto specPath = phraseSpecPath (contentDir, kitId);
        const auto bytes = util::readBinaryFile (specPath);
        if (!bytes.ok())
            return R::failure ("no phrase spec for \"" + std::string (kitId) + "\": " + bytes.error);
        const std::string text (bytes.value.begin(), bytes.value.end());
        auto spec = parsePhraseSpec (text, shown (specPath));
        if (!spec.ok())
            return spec;
        const auto kitPath = contentDir / "kits" / (lowered (kitId) + ".json");
        const auto kitBytes = util::readBinaryFile (kitPath);
        if (!kitBytes.ok())
            return R::failure ("no kit \"" + std::string (kitId) + "\": " + kitBytes.error);
        const auto kit = parseKit (std::string_view (reinterpret_cast<const char*> (kitBytes.value.data()), kitBytes.value.size()), shown (kitPath));
        if (!kit.ok())
            return R::failure (kit.error);
        const auto lineOf = [&text] (std::string_view key) {
            const auto v = json::parse (text);
            const json::Value* x = v.ok() ? v.value.find (key) : nullptr;
            return x != nullptr ? x->line : 0u;
        };
        const auto refuse = [&] (std::string_view key, const std::string& message) {
            return R::failure (shown (specPath) + ": " + std::string (key) + ": line " + std::to_string (lineOf (key)) + ": " + message);
        };
        if (spec.value.kitId != kitId)
            return refuse ("kit", "the spec is for \"" + spec.value.kitId + "\", not this folder's " + std::string (kitId));
        if (spec.value.tonic != kit.value.tonic)
            return refuse ("tonic", "the tonic is " + std::to_string (spec.value.tonic) + ", the kit's (" + shown (kitPath) + ") " + std::to_string (kit.value.tonic));
        if (spec.value.bpm != kit.value.bpm)
            return refuse ("bpm", "the bpm is " + std::to_string (spec.value.bpm) + ", the kit's (" + shown (kitPath) + ") " + std::to_string (kit.value.bpm));
        return spec;
    }

    // ------------------------------------------------------------------ the generator

    namespace
    {
        using namespace material;

        constexpr std::int32_t phraseSteps = phraseBars * 16;

        const AppChord& chordOfBar (const PhraseSpec& ps, std::int32_t bar)
        {
            return ps.chords[static_cast<std::size_t> (util::floorMod (bar, static_cast<std::int32_t> (ps.chords.size())))];
        }

        const PhraseKindSpec& kindSpecOf (const PhraseSpec& ps, PhraseKind kind) { return ps.kinds[static_cast<std::size_t> (kind)]; }

        // Only the riff's own textures on the lead keep FUT02's pedal and its top-voice floor; a kind on another
        // channel (FUT02's glass periods on Glass Ping) drops both, as the arps, stabs, counter-lines and bounces do.
        bool keepsPedal (const PhraseSpec& ps, PhraseKind kind)
        {
            const bool riff = kind == PhraseKind::hook || kind == PhraseKind::answer || kind == PhraseKind::period || kind == PhraseKind::sparse;
            return riff && kindSpecOf (ps, kind).channel == 2;
        }

        // The fit table over the app's chord loop: 16th st of a 2-bar riff sounds over bar st / 16's chord.
        Fit phraseFit (const PhraseSpec& ps, const HookPitch& pitch)
        {
            Fit f;
            f.tonic = ps.tonic;
            f.mode = ps.mode;
            f.tones = pitch.tones;
            for (std::size_t st = 0; st < f.at.size(); ++st)
            {
                const AppChord& ch = chordOfBar (ps, static_cast<std::int32_t> (st / 16));
                f.at[st].push_back ({ degreePc (ps.tonic, ps.mode, ch.degree), ps.mode, ch.degree });
            }
            return f;
        }

        Setting phraseSetting (const PhraseSpec& ps, const HookSpec& spec)
        {
            Setting s;
            s.spec = &spec;
            s.lane = 0;
            s.mode = ps.mode;
            s.tonic = ps.tonic;
            s.fit = phraseFit (ps, spec.pitch);
            s.a = Range { spec.reg.floor >= 0 ? spec.reg.floor : ps.range.lo, ps.range.hi };
            s.b = spec.period.range.value_or (s.a);
            s.pedal = spec.pedal.has_value();
            s.pedalTonic = s.a.lo + util::floorMod (ps.tonic - s.a.lo, 12);
            return s;
        }

        // The line context: per 16th of the 8 bars, the avoid notes of the bar's chord and its voicing's keys, sounding
        // through the whole bar (a pad or a held stab, whichever the person plays the chords with).
        LineContext phraseContext (const PhraseSpec& ps, const HookSpec& spec)
        {
            LineContext c;
            c.mode = ps.mode;
            c.reach = 1000;
            for (std::size_t q = 0; q < c.avoid.size(); ++q)
            {
                const AppChord& ch = chordOfBar (ps, static_cast<std::int32_t> (q / 16));
                const std::int32_t rootPc = degreePc (ps.tonic, ps.mode, ch.degree);
                for (const std::int32_t t : spec.pitch.clash)
                    c.avoid[q] = static_cast<std::uint16_t> (c.avoid[q] | (1u << pitchClass (rootPc + chordInterval (ps.mode, ch.degree, t) + 1)));
                for (const std::int32_t k : ch.keys)
                    if (k >= 0 && k < 128)
                        c.chord[q][static_cast<std::size_t> (k / 64)] |= std::uint64_t (1) << (k % 64);
            }
            return c;
        }

        // The material as the kind changes it.
        HookSpec kindMaterial (const PhraseSpec& ps, PhraseKind kind)
        {
            HookSpec spec = ps.material;
            const PhraseKindSpec& k = kindSpecOf (ps, kind);
            if (!k.cells.empty())
                spec.cells = k.cells;
            if (!k.pool.empty())
                spec.pitch.pool = k.pool;
            const auto any = [] (const auto& w) { return std::any_of (w.begin(), w.end(), [] (std::int32_t x) { return x > 0; }); };
            if (any (k.start))
                spec.pitch.start = k.start;
            if (any (k.end))
                spec.pitch.end = k.end;
            if (any (k.moves))
                spec.pitch.moves = k.moves;
            if (k.leap > 0)
                spec.pitch.leap = k.leap;
            if (k.center > 0)
                spec.reg.center = k.center;
            if (k.span > 0)
                spec.reg.span = k.span;
            if (!keepsPedal (ps, kind))
            {
                spec.pedal.reset();
                spec.reg.floor = -1;
            }
            if (k.velShift != 0)
                for (std::int32_t* v : { &spec.vel.down, &spec.vel.beat, &spec.vel.accent, &spec.vel.other })
                    if (*v >= 0)
                        *v = std::clamp (*v + k.velShift, 1, 127);
            // a start or end weight on a degree outside the pool goes to the tonic
            for (auto* w : { &spec.pitch.start, &spec.pitch.end })
                for (std::size_t d = 1; d < 7; ++d)
                    if ((*w)[d] > 0 && std::find (spec.pitch.pool.begin(), spec.pitch.pool.end(), static_cast<std::int32_t> (d)) == spec.pitch.pool.end())
                    {
                        (*w)[0] += (*w)[d];
                        (*w)[d] = 0;
                    }
            return spec;
        }

        // A segment of the phrase: bars [from, to) of the line `st` plays.
        struct Segment
        {
            std::int32_t from = 0;
            std::int32_t to = 0;
            LineState st;
        };

        LineState lineOf (HookForm form, std::uint8_t motif, HookEnding ending = HookEnding::none)
        {
            LineState st;
            st.form = form;
            st.motif = motif;
            st.ending = ending;
            return st;
        }

        bool byTickThenKey (const Note& a, const Note& b) { return a.tick < b.tick || (a.tick == b.tick && a.key < b.key); }

        // Lengths never into the next onset (10 ticks short), never past the end.
        template <class N>
        void fitLengthsTo (std::vector<N>& notes, std::int64_t end, std::int64_t gap)
        {
            for (std::size_t i = 0; i < notes.size(); ++i)
            {
                std::int64_t limit = end;
                for (std::size_t j = i + 1; j < notes.size(); ++j)
                    if (notes[j].tick > notes[i].tick)
                    {
                        limit = std::min (limit, notes[j].tick - gap);
                        break;
                    }
                notes[i].length = std::max<std::int64_t> (1, std::min (notes[i].length, limit - notes[i].tick));
            }
        }

        // The segments' notes, each from its own realized line, the lengths fitted across the joins.
        std::vector<Note> realizePhrase (const Setting& s, const LineContext& c, const SongHook& h, const std::vector<Segment>& segs)
        {
            std::vector<Note> out;
            for (const Segment& g : segs)
            {
                const Realized r = realizeLine (s, c, h, g.st);
                for (const LineNote& n : r.notes)
                {
                    if (n.step < g.from * 16 || n.step >= g.to * 16)
                        continue;
                    Note x;
                    x.step = n.step;
                    x.tick = n.step * ticksPerStep;
                    x.length = n.length;
                    x.key = static_cast<std::uint8_t> (std::clamp (n.key, 0, 127));
                    x.velocity = static_cast<std::uint8_t> (std::clamp<std::int32_t> (n.velocity, 1, 127));
                    x.flags = static_cast<std::uint16_t> ((n.accent ? noteFlag::top : 0) | (n.pedal ? noteFlag::pedal : 0));
                    out.push_back (x);
                }
            }
            std::stable_sort (out.begin(), out.end(), byTickThenKey);
            fitLengthsTo (out, phraseSteps * ticksPerStep, 10);
            return out;
        }

        // The tonic's key whose octave puts `degree` nearest center.
        std::int32_t baseNear (const PhraseSpec& ps, std::int32_t degree, std::int32_t center)
        {
            std::int32_t best = ps.tonic, bestCost = INT32_MAX;
            for (std::int32_t base = ps.tonic; base < 128; base += 12)
            {
                const std::int32_t cost = std::abs (base + semitones (ps.mode, degree) - center);
                if (cost < bestCost)
                {
                    bestCost = cost;
                    best = base;
                }
            }
            return best;
        }

        // The arp: chord tones (1 3 5 7 9, scale steps 0 2 4 6 8 above each bar's root) by a ladder cell, restarting
        // on the downbeat accent; B, the turnaround, the cell a tone higher.
        void drawArp (const PhraseSpec& ps, const HookSpec& spec, const PhraseKindSpec& ks, SongHook& h, rng::Key k)
        {
            static constexpr std::array<std::int32_t, 5> tones { 0, 2, 4, 6, 8 };
            h.mode = ps.mode;
            h.a.bars = 2;
            h.a.notes = riffRhythm (spec.cells, spec, spec.pedal.has_value(), k.with (rng::tag ("rhythm")));
            std::vector<std::int32_t> cell { 0, 1, 2, 3 };
            if (!ks.ladders.empty())
                cell = ks.ladders[rng::pickIndex (k.with (rng::tag ("cell")), ks.ladders.size())];
            if (rng::chance (k.with (rng::tag ("down")), p16FromPercent (30)))
                std::reverse (cell.begin(), cell.end());
            const std::int32_t shift = rng::between (k.with (rng::tag ("shift")), 0, 2);
            h.base = baseNear (ps, 0, spec.reg.center - 5);
            const auto degreeOf = [&] (std::int32_t bar, std::int32_t index) {
                const AppChord& ch = chordOfBar (ps, bar);
                const std::int32_t i = index + shift;
                const auto n = static_cast<std::int32_t> (tones.size());
                return ch.degree + tones[static_cast<std::size_t> (util::floorMod (i, n))] + 7 * util::floorDiv (i, n);
            };
            const auto fill = [&] (HookPhrase& p, std::int32_t extra) {
                std::size_t at = 0;
                for (HookNote& n : p.notes)
                {
                    if (n.accent && n.step % 16 == 0)
                        at = 0;
                    n.degree = degreeOf (n.step / 16, cell[at % cell.size()] + extra);
                    ++at;
                }
            };
            fill (h.a, 0);
            h.b = h.a;
            fill (h.b, 1);
            shapeVelocity (spec.vel, h.a.notes, k.with (rng::tag ("vel")));
            shapeVelocity (spec.vel, h.b.notes, k.with (rng::tag ("vel")));
            h.varyDegree = pickDegree (k.with (rng::tag ("vary")), spec.pitch.end);
        }

        // Stabs: the top voice a chord tone of each bar's chord (3 5 7 9), voice-led from the bar before, on the kit's
        // stab cells; the second voice comes after the realization (addStabVoice).
        void drawStabs (const PhraseSpec& ps, const HookSpec& spec, SongHook& h, rng::Key k)
        {
            static constexpr std::array<std::int32_t, 4> tops { 2, 4, 6, 8 };
            h.mode = ps.mode;
            h.a.bars = 2;
            h.a.notes = riffRhythm (spec.cells, spec, spec.pedal.has_value(), k.with (rng::tag ("rhythm")));
            h.base = baseNear (ps, 0, spec.reg.center - 4);
            std::int32_t prev = chordOfBar (ps, 0).degree + tops[rng::pickIndex (k.with (rng::tag ("first")), tops.size())];
            std::array<std::int32_t, 2> barTop {};
            for (std::int32_t bar = 0; bar < 2; ++bar)
            {
                const AppChord& ch = chordOfBar (ps, bar);
                std::int32_t best = prev, bestCost = INT32_MAX;
                for (const std::int32_t t : tops)
                    for (std::int32_t o = -7; o <= 7; o += 7)
                    {
                        const std::int32_t d = ch.degree + t + o;
                        const std::int32_t key = h.base + semitones (ps.mode, d);
                        const std::int32_t cost = std::abs (d - prev) * 10 + std::abs (key - spec.reg.center)
                                                  + rng::between (k.with (rng::tag ("voice")).with (static_cast<std::uint64_t> (bar)).with (static_cast<std::uint64_t> (t + o + 16)), 0, 9);
                        if (cost < bestCost)
                        {
                            bestCost = cost;
                            best = d;
                        }
                    }
                if (bar == 1 && ps.chords.size() == 1 && best == barTop[0])
                    best = barTop[0] + (rng::chance (k.with (rng::tag ("move")), p16FromPercent (50)) ? 2 : -2); // one chord: bar 2 moves
                barTop[static_cast<std::size_t> (bar)] = best;
                prev = best;
            }
            for (HookNote& n : h.a.notes)
                n.degree = barTop[static_cast<std::size_t> (n.step / 16)];
            if (!h.a.notes.empty() && rng::chance (k.with (rng::tag ("lift")), p16FromPercent (50)))
                h.a.notes.back().degree += 2; // the last stab lifts a 3rd
            shapeVelocity (spec.vel, h.a.notes, k.with (rng::tag ("vel")));
            h.b = h.a;
            h.varyDegree = pickDegree (k.with (rng::tag ("vary")), spec.pitch.end);
        }

        // The stabs' second voice: a chord tone 3, 4 or 5 semitones under each accented stab, in the key, clean
        // against the voicing, at most 5 under the register.
        void addStabVoice (const PhraseSpec& ps, const HookSpec& spec, const LineContext& c, std::vector<Note>& notes)
        {
            std::vector<Note> extra;
            for (const Note& n : notes)
            {
                if ((n.flags & noteFlag::top) == 0)
                    continue;
                const AppChord& ch = chordOfBar (ps, static_cast<std::int32_t> (n.step / 16));
                const std::int32_t rootPc = degreePc (ps.tonic, ps.mode, ch.degree);
                for (const std::int32_t down : { 3, 4, 5 })
                {
                    const std::int32_t key = n.key - down;
                    const std::int32_t pc = pitchClass (key);
                    bool tone = false;
                    for (const std::int32_t t : { 0, 2, 4, 6, 8 })
                        tone = tone || pitchClass (rootPc + chordInterval (ps.mode, ch.degree, t)) == pc;
                    if (!tone || !inMinorKey (ps.tonic, pc) || key < ps.range.lo - 5)
                        continue;
                    if (!clean (c, spec.guard, key, static_cast<std::int32_t> (n.step % 128), n.length))
                        continue;
                    Note x = n;
                    x.key = static_cast<std::uint8_t> (key);
                    x.velocity = static_cast<std::uint8_t> (std::max (1, n.velocity - 8));
                    x.flags = 0;
                    extra.push_back (x);
                    break;
                }
            }
            notes.insert (notes.end(), extra.begin(), extra.end());
            std::stable_sort (notes.begin(), notes.end(), byTickThenKey);
        }

        std::int32_t pickForm (rng::Key k, const std::vector<std::int32_t>& weights)
        {
            const auto i = rng::pickWeighted (k.with (rng::tag ("form")), std::span<const std::int32_t> (weights));
            return static_cast<std::int32_t> (i < weights.size() ? i : 0);
        }
    }

    rng::Key phraseKey (std::string_view kitId, PhraseKind kind, std::uint64_t seed) noexcept
    {
        return rng::seedKey (seed).with (rng::tag ("phrase")).with (rng::tag (kitId)).with (rng::tag (phraseKindIds[static_cast<std::size_t> (kind)]));
    }

    Phrase generatePhrase (const PhraseSpec& ps, PhraseKind kind, std::uint64_t seed)
    {
        Phrase out;
        out.kind = kind;
        out.seed = seed;
        if (ps.chords.empty())
            return out;
        const rng::Key k = phraseKey (ps.kitId, kind, seed);
        const HookSpec spec = kindMaterial (ps, kind);
        Setting s = phraseSetting (ps, spec);
        const LineContext c = phraseContext (ps, spec);
        const PhraseKindSpec& ks = kindSpecOf (ps, kind);
        if (ks.range)
        {
            s.a = *ks.range; // the kind's own register, for its riff and its period alike
            s.b = *ks.range;
        }

        SongHook h;
        if (kind == PhraseKind::arp)
            drawArp (ps, spec, ks, h, k.with (rng::tag ("arp")));
        else if (kind == PhraseKind::stabs)
        {
            s.pedal = false;
            drawStabs (ps, spec, h, k.with (rng::tag ("stab")));
        }
        else
            h = materialBelow (s, k.with (rng::tag ("material"))).first;
        if (kind == PhraseKind::arp || kind == PhraseKind::stabs)
            s.b = s.a;
        {
            // VARY's last note: by the spec's ending weights, never the riff's own last degree
            std::array<std::int32_t, 7> ending = ps.ending;
            if (!h.a.notes.empty())
                ending[static_cast<std::size_t> (degreeClass (h.a.notes.back().degree))] = 0;
            if (std::all_of (ending.begin(), ending.end(), [] (std::int32_t w) { return w == 0; }))
                ending = ps.ending;
            h.varyDegree = pickDegree (k.with (rng::tag ("ending")), ending);
        }

        using F = HookForm;
        using E = HookEnding;
        std::vector<Segment> segs;
        out.form = pickForm (k, ks.forms);
        switch (kind)
        {
            case PhraseKind::hook:
                if (out.form == 0)
                    segs = { { 0, 8, lineOf (F::full, 0, E::vary) } };
                else if (out.form == 1)
                    segs = { { 0, 8, lineOf (F::full, 0, E::cadence) } };
                else if (out.form == 2)
                    segs = { { 0, 6, lineOf (F::full, 0) }, { 6, 8, lineOf (F::answer, 0, E::cadence) } };
                else
                    segs = { { 0, 4, lineOf (F::full, 0) }, { 4, 6, lineOf (F::fragment, 0) }, { 6, 8, lineOf (F::full, 0, E::vary) } };
                break;
            case PhraseKind::answer:
                if (out.form == 0)
                    segs = { { 0, 8, lineOf (F::answer, 0, E::cadence) } };
                else if (out.form == 1)
                    segs = { { 0, 2, lineOf (F::full, 0) }, { 2, 4, lineOf (F::answer, 0) }, { 4, 6, lineOf (F::full, 0) }, { 6, 8, lineOf (F::answer, 0, E::cadence) } };
                else
                    segs = { { 0, 2, lineOf (F::fragment, 0) }, { 2, 4, lineOf (F::answer, 0) }, { 4, 6, lineOf (F::fragment, 0) }, { 6, 8, lineOf (F::answer, 0, E::vary) } };
                break;
            case PhraseKind::period:
                if (out.form == 0 && spec.period.idiom == HookIdiom::stab)
                    segs = { { 0, 6, lineOf (F::full, 1) }, { 6, 8, lineOf (F::fragment, 1, E::cadence) } }; // the stab riff, then its turnaround
                else if (out.form == 0)
                    segs = { { 0, 8, lineOf (F::full, 1, E::cadence) } };
                else
                    segs = { { 0, 4, lineOf (F::full, 0) }, { 4, 8, lineOf (F::full, 1, E::cadence) } };
                break;
            case PhraseKind::sparse:
                segs = { { 0, 8, lineOf (F::skeleton, 0, out.form == 0 ? E::breath : E::cadence) } };
                break;
            case PhraseKind::arp:
                if (out.form == 0)
                    segs = { { 0, 6, lineOf (F::full, 0) }, { 6, 8, lineOf (F::full, 1, E::cadence) } };
                else
                    segs = { { 0, 2, lineOf (F::full, 0) }, { 2, 4, lineOf (F::full, 1) }, { 4, 6, lineOf (F::full, 0) }, { 6, 8, lineOf (F::full, 1, E::vary) } };
                break;
            case PhraseKind::stabs:
                segs = { { 0, 8, lineOf (F::full, 0, out.form == 0 ? E::cadence : E::breath) } };
                break;
            case PhraseKind::counter:
                if (out.form == 0)
                    segs = { { 0, 4, lineOf (F::full, 0) }, { 4, 8, lineOf (F::answer, 0, E::cadence) } };
                else
                    segs = { { 0, 8, lineOf (F::full, 0, E::vary) } };
                break;
            case PhraseKind::bounce:
                if (out.form == 0)
                    segs = { { 0, 8, lineOf (F::full, 0, E::vary) } };
                else if (out.form == 1)
                    segs = { { 0, 8, lineOf (F::full, 0, E::cadence) } };
                else
                    segs = { { 0, 6, lineOf (F::full, 0) }, { 6, 8, lineOf (F::skeleton, 0, E::cadence) } };
                break;
        }
        out.notes = realizePhrase (s, c, h, segs);
        if (kind == PhraseKind::stabs)
            addStabVoice (ps, spec, c, out.notes);
        return out;
    }

    // ------------------------------------------------------------------ the checks

    namespace
    {
        bool chordTone (const PhraseSpec& ps, std::int32_t bar, std::int32_t key, std::initializer_list<std::int32_t> steps)
        {
            const AppChord& ch = chordOfBar (ps, bar);
            const std::int32_t rootPc = degreePc (ps.tonic, ps.mode, ch.degree);
            for (const std::int32_t t : steps)
                if (pitchClass (rootPc + chordInterval (ps.mode, ch.degree, t)) == pitchClass (key))
                    return true;
            return false;
        }

        using Print = std::set<std::pair<std::int64_t, std::int32_t>>;
    }

    bool PhraseChecks::pass() const noexcept
    {
        return inKey && inRange && everyBar && motif && variation && ending && rhythm && clean && synth && melody && strongChords;
    }

    PhraseChecks checkPhrase (const PhraseSpec& ps, const Phrase& p)
    {
        PhraseChecks c;
        if (ps.chords.empty())
            return c;
        c.notes = static_cast<std::int32_t> (p.notes.size());
        c.low = 127;
        std::array<std::int32_t, phraseBars> perBar {};
        std::array<Print, phraseBars / 2> units;
        for (const Note& n : p.notes)
        {
            const auto bar = static_cast<std::int32_t> (n.step / 16);
            c.low = std::min<std::int32_t> (c.low, n.key);
            c.high = std::max<std::int32_t> (c.high, n.key);
            c.inKey = c.inKey && inMinorKey (ps.tonic, pitchClass (n.key));
            if (bar >= 0 && bar < phraseBars)
            {
                ++perBar[static_cast<std::size_t> (bar)];
                units[static_cast<std::size_t> (bar / 2)].insert ({ n.step % 32, n.key });
            }
            if (n.step % 4 == 0 && (n.flags & noteFlag::pedal) == 0)
            {
                ++c.strong;
                c.strongTones += chordTone (ps, bar, n.key, { 0, 2, 4, 6, 8 }) ? 1 : 0;
            }
            c.offBeat += n.step % 4 != 0 ? 1 : 0;
            for (const std::int32_t v : chordOfBar (ps, bar).keys)
                if (std::abs (v - n.key) == 13 || (std::abs (v - n.key) == 1 && std::min<std::int32_t> (v, n.key) < ps.material.guard))
                    ++c.rubs;
        }
        if (p.notes.empty())
            c.low = 0;
        c.clean = c.rubs == 0;
        c.inRange = !p.notes.empty() && c.low >= ps.range.lo - 6 && c.high <= ps.range.hi + 2;
        for (std::size_t b = 0; b < perBar.size(); ++b)
        {
            c.maxPerBar = std::max (c.maxPerBar, perBar[b]);
            if (perBar[b] == 0 && b + 1 != perBar.size())
                c.everyBar = false;
        }
        const std::set<Print> distinct (units.begin(), units.end());
        c.units = static_cast<std::int32_t> (distinct.size());
        c.motif = c.units < 4; // some 2-bar unit comes back
        c.variation = c.units > 1; // and not all four alike
        c.rhythm = c.notes > 0 && c.maxPerBar <= 16 && c.offBeat * 100 >= c.notes * 10;
        // the ending: the last top note the key's 1, b3 or 5, or a 1, 3 or 5 of bar 8's chord; or nothing starts
        // after bar 8's beat 3
        std::int64_t lastTick = -1;
        std::int32_t lastKey = -1;
        for (const Note& n : p.notes)
            if ((n.flags & noteFlag::pedal) == 0 && (n.tick > lastTick || (n.tick == lastTick && n.key > lastKey)))
            {
                lastTick = n.tick;
                lastKey = n.key;
            }
        const bool breath = lastTick < (7 * 16 + 8) * ticksPerStep;
        const std::int32_t lastPc = lastKey >= 0 ? pitchClass (lastKey) : -1;
        const bool triad = lastPc >= 0
                           && (lastPc == pitchClass (ps.tonic) || lastPc == degreePc (ps.tonic, ps.mode, 2) || lastPc == degreePc (ps.tonic, ps.mode, 4));
        c.ending = breath || triad || (lastKey >= 0 && chordTone (ps, phraseBars - 1, lastKey, { 0, 2, 4 }));
        // synth-ready: no overlap on the track (a dyad's notes start together)
        for (std::size_t i = 0; i < p.notes.size(); ++i)
            for (std::size_t j = i + 1; j < p.notes.size(); ++j)
                if (p.notes[j].tick > p.notes[i].tick)
                {
                    c.synth = c.synth && p.notes[i].tick + p.notes[i].length <= p.notes[j].tick;
                    break;
                }
        // the top line: no key struck more than 4 times running, and 3 keys at least (stabs: 2 tops)
        std::set<std::int32_t> keys;
        std::int32_t run = 0, last = -1;
        for (std::size_t i = 0; i < p.notes.size(); ++i)
        {
            const Note& n = p.notes[i];
            if ((n.flags & noteFlag::pedal) != 0 || (i + 1 < p.notes.size() && p.notes[i + 1].tick == n.tick))
                continue; // the pedal, and a dyad's lower note
            keys.insert (n.key);
            run = n.key == last ? run + 1 : 1;
            last = n.key;
            c.maxRun = std::max (c.maxRun, run);
        }
        c.keys = static_cast<std::int32_t> (keys.size());
        c.melody = p.kind == PhraseKind::stabs ? c.keys >= 2 : (c.keys >= 3 && c.maxRun <= 4);
        c.strongChords = c.strongTones * 1000 >= c.strong * 800; // a phrase with nothing on the beats passes
        return c;
    }

    std::int32_t phraseSimilarity (const Phrase& a, const Phrase& b)
    {
        Print pa, pb;
        for (const Note& n : a.notes)
            pa.insert ({ n.step, n.key });
        for (const Note& n : b.notes)
            pb.insert ({ n.step, n.key });
        std::size_t both = 0;
        for (const auto& x : pa)
            both += pb.count (x);
        const std::size_t either = pa.size() + pb.size() - both;
        return either == 0 ? 1000 : static_cast<std::int32_t> (both * 1000 / either);
    }

    std::vector<std::string> phraseTags (const PhraseSpec& spec, const Phrase& p)
    {
        std::vector<std::string> tags;
        const auto n = static_cast<std::int32_t> (p.notes.size());
        tags.emplace_back (n < 4 * phraseBars ? "sparse" : n <= 8 * phraseBars ? "steady" : "busy");
        std::vector<std::int32_t> keys;
        for (const Note& x : p.notes)
            if ((x.flags & noteFlag::pedal) == 0)
                keys.push_back (x.key);
        if (!keys.empty())
        {
            std::sort (keys.begin(), keys.end());
            const std::int32_t median = keys[keys.size() / 2];
            const std::int32_t third = spec.range.hi - spec.range.lo + 1;
            const std::int32_t at = (median - spec.range.lo) * 3;
            tags.emplace_back (at < third ? "low" : at < 2 * third ? "mid" : "high");
        }
        bool pedal = false, dyads = false;
        std::int64_t last = -1;
        for (std::size_t i = 0; i < p.notes.size(); ++i)
        {
            pedal = pedal || (p.notes[i].flags & noteFlag::pedal) != 0;
            dyads = dyads || (i > 0 && p.notes[i].tick == p.notes[i - 1].tick);
            last = std::max (last, p.notes[i].tick);
        }
        if (pedal)
            tags.emplace_back ("pedal");
        if (dyads)
            tags.emplace_back ("dyads");
        if (!p.notes.empty() && last < (7 * 16 + 8) * ticksPerStep)
            tags.emplace_back ("breath");
        return tags;
    }

    // ------------------------------------------------------------------ names

    std::string phraseNameAt (const PhraseSpec& spec, std::size_t index)
    {
        if (spec.words[0].size() != 24 || spec.words[1].size() != 24)
            return {};
        return spec.words[0][index % 24] + " " + spec.words[1][(7 * index + index / 24) % 24];
    }

    std::string phraseName (const PhraseSpec& spec, PhraseKind kind, std::uint64_t seed)
    {
        const std::string id = phraseId (kind, seed);
        for (const auto& [from, to] : spec.rename)
            if (from == id)
                return to;
        const auto k = static_cast<std::uint64_t> (kind);
        const bool more = seed >= firstMoreSeed;
        const std::uint64_t s = more ? seed - firstMoreSeed : seed;
        const std::uint64_t laps = more ? 24 : 48;
        const std::uint64_t index = 72 * k + (more ? 48 : 0) + s % laps;
        const std::uint64_t lap = s / laps;
        std::string name = phraseNameAt (spec, static_cast<std::size_t> (index));
        if (lap > 0)
            name += " " + std::to_string (lap + 1);
        return name;
    }

    // ------------------------------------------------------------------ the file

    midi::SongSpec phraseSongSpec (const PhraseSpec& ps, const Phrase& p, std::string_view name)
    {
        constexpr std::int64_t ppq = 960, sixteenth = ppq / 4;
        midi::SongSpec spec;
        spec.ppq = static_cast<std::uint16_t> (ppq);
        spec.endTick = phraseBars * 4 * ppq;
        spec.name = ps.kitId + " " + std::string (name);
        if (ps.bpm > 0)
            spec.tempos.push_back ({ 0, static_cast<std::uint32_t> ((60000000 * 2 + ps.bpm) / (2 * ps.bpm)) });
        spec.timeSignatures.push_back ({});
        spec.keySignatures.push_back ({ 0, midi::sharpsFor (ps.tonic, true), true });
        const PhraseKindSpec& ks = kindSpecOf (ps, p.kind);
        midi::TrackSpec t;
        t.name = ks.track;
        for (const Note& n : p.notes)
        {
            midi::Note m;
            m.tick = n.step * sixteenth + (n.step % 2 == 1 ? ps.swing : 0);
            m.length = std::max<std::int64_t> (sixteenth, n.length * (ppq / 480));
            m.channel = static_cast<std::uint8_t> (ks.channel - 1);
            m.key = n.key;
            m.velocity = n.velocity;
            t.notes.push_back (m);
        }
        std::stable_sort (t.notes.begin(), t.notes.end(), [] (const midi::Note& a, const midi::Note& b) { return a.tick < b.tick || (a.tick == b.tick && a.key < b.key); });
        fitLengthsTo (t.notes, spec.endTick, 10);
        spec.tracks.push_back (std::move (t));
        return spec;
    }

    util::Result<std::vector<std::uint8_t>> phraseSmf (const PhraseSpec& spec, const Phrase& phrase, std::string_view name)
    {
        return midi::writeSmf (phraseSongSpec (spec, phrase, name));
    }

    std::vector<LibraryRow> selectLibrary (const PhraseSpec&, std::uint64_t) { return {}; }
    std::string libraryJson (const PhraseSpec&, std::string_view, const std::vector<LibraryRow>&) { return {}; }
    util::Result<PhraseLibrary> parsePhraseLibrary (std::string_view, std::string_view source)
    {
        return util::Result<PhraseLibrary>::failure (std::string (source) + ": not there yet");
    }

    MorePress morePhrases (const PhraseSpec&, PhraseKind, std::uint64_t fromSeed, std::span<const Phrase>, std::int32_t, std::int32_t)
    {
        MorePress m;
        m.nextSeed = fromSeed;
        return m;
    }
}
