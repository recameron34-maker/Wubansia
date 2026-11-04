// groovecore_cut_check: a --bars cut through groovecore, checked from the files on disk alone
// (cli/tests/cut.cmake runs it).
//
//   groovecore_cut_check <the whole song's --out> <the cut song's --out> <bars>
//
// A cut at bar N is the head of the whole song (core/gen/cut.h). For each version, feel/ and
// quantized/, the cut's folder must hold:
// - its full file, one region per section that starts before the cut, regions.json, and nothing else;
// - regions.json: the whole song's, with the cut's "bars" and "length" and its regions up to the one
//   the cut ends, that one shortened to it ("file", "bars", "lengthTicks", "blocks" and the lanes of
//   the blocks it keeps);
// - the region files before that one: the whole song's, byte for byte;
// - the full file and that last region: the bytes the MIDI writer gives the whole song's head. Per
//   lane track, the notes whose score onsets (in the whole song's quantized file) are before the cut,
//   ended by it (a performed onset at or after it on its last tick, notes that then overlap on one
//   key trimmed as the export trims them), and the controllers before it; in the conductor track,
//   the tempo, meter, key and markers before it, the shortened region's marker (and a region file's
//   name) giving its new length.
//
// The files don't say which score note a feel file's note plays. The feel keeps every note inside
// its section (ADR 0013 §3), so only the section the cut ends needs pairing, key by key: its
// performed notes in time order with its score notes in time order. That is right wherever the feel
// keeps a key's notes in score order across the cut, and on ticks of their own, as the shipped feels
// do at every 8-bar line (64 seeds of every kit). At the feel's extremes it can play a note after a
// later one, or two on one tick, which the file merges into one, and the files can't show which
// notes the cut keeps. So, per key of that section, the cut's notes are:
// - the head's: they are with the notes paired in order (all a pass can say where the feel might
//   have played them out of order);
// - unknown: they aren't, but they are for another choice of the section's performed notes (the
//   feel played some out of score order across the cut), or the file merged two of them, so the
//   counts differ;
// - wrong: no choice of the section's performed notes gives them.
// Prints every difference and exits 1 if anything is wrong; else, if a key is unknown, prints what the
// files can't tell and exits 3; else prints one line and exits 0. Exits 2 on bad arguments.

#include "gen/json.h"
#include "midi/reader.h"
#include "midi/writer.h"
#include "util/file_io.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    using namespace wubansia;
    namespace fs = std::filesystem;
    namespace json = wubansia::gen::json;

    constexpr std::int64_t ticksPerBar = 1920;

    std::vector<std::string> problems; // the cut is wrong
    std::vector<std::string> unknowns; // what the files can't tell

    void problem (std::string what) { problems.push_back (std::move (what)); }

    std::string shown (const fs::path& p)
    {
        const auto s = p.generic_u8string();
        return std::string (s.begin(), s.end());
    }

    std::string twoDigits (std::int64_t n) { return (n < 10 ? "0" : "") + std::to_string (n); }

    // A section's marker text, as the export writes it: "drop (04, 32 bars)".
    std::string markerText (const std::string& kind, std::int64_t order, std::int64_t bars)
    {
        return kind + " (" + twoDigits (order) + ", " + std::to_string (bars) + " bars)";
    }

    bool readBytes (const fs::path& p, std::vector<std::uint8_t>& out)
    {
        auto r = util::readBinaryFile (p);
        if (!r.ok())
        {
            problem (r.error);
            return false;
        }
        out = std::move (r.value);
        return true;
    }

    bool readJson (const fs::path& p, json::Value& out)
    {
        std::vector<std::uint8_t> bytes;
        if (!readBytes (p, bytes))
            return false;
        auto parsed = json::parse (std::string_view (reinterpret_cast<const char*> (bytes.data()), bytes.size()));
        if (!parsed.ok())
        {
            problem (shown (p) + ": " + parsed.error);
            return false;
        }
        out = std::move (parsed.value);
        return true;
    }

    bool readMidi (const fs::path& p, midi::File& out)
    {
        std::vector<std::uint8_t> bytes;
        if (!readBytes (p, bytes))
            return false;
        auto parsed = midi::readSmf (bytes);
        if (!parsed.ok())
        {
            problem (shown (p) + ": " + parsed.error);
            return false;
        }
        out = std::move (parsed.value);
        return true;
    }

    json::Value* memberOf (json::Value& v, std::string_view key)
    {
        for (auto& m : v.members)
            if (m.key == key)
                return &m.value;
        return nullptr;
    }

    json::Value stringValue (std::string s)
    {
        json::Value v;
        v.type = json::Value::Type::string;
        v.string = std::move (s);
        return v;
    }

    // The path of the first difference between two JSON values ("" if none), line and column aside.
    std::string jsonDifference (const json::Value& a, const json::Value& b, const std::string& path)
    {
        if (a.type != b.type || a.boolean != b.boolean || a.integer != b.integer || a.string != b.string || a.items.size() != b.items.size()
            || a.members.size() != b.members.size())
            return path.empty() ? "the top" : path;
        for (std::size_t i = 0; i < a.items.size(); ++i)
            if (auto d = jsonDifference (a.items[i], b.items[i], path + "[" + std::to_string (i) + "]"); !d.empty())
                return d;
        for (std::size_t i = 0; i < a.members.size(); ++i)
        {
            if (a.members[i].key != b.members[i].key)
                return path + "." + a.members[i].key;
            if (auto d = jsonDifference (a.members[i].value, b.members[i].value, path + "." + a.members[i].key); !d.empty())
                return d;
        }
        return {};
    }

    std::string eventText (const midi::Event& e)
    {
        std::string s = "tick " + std::to_string (e.tick) + ": ";
        if (e.isNoteOn())
            return s + "note-on, channel " + std::to_string (e.channel() + 1) + ", key " + std::to_string (e.data1) + ", velocity " + std::to_string (e.data2);
        if (e.isNoteOff())
            return s + "note-off, channel " + std::to_string (e.channel() + 1) + ", key " + std::to_string (e.data1);
        if (e.isController())
            return s + "CC" + std::to_string (e.data1) + " " + std::to_string (e.data2);
        if (e.isMeta() && midi::meta::isText (e.data1))
            return s + "text \"" + std::string (e.payload.begin(), e.payload.end()) + "\"";
        return s + "status " + std::to_string (e.status) + ", " + std::to_string (e.data1);
    }

    // Where two files first differ, event by event.
    std::string fileDifference (const midi::File& got, const midi::File& want)
    {
        if (got.format != want.format || got.ppq != want.ppq)
            return "the header";
        if (got.tracks.size() != want.tracks.size())
            return std::to_string (got.tracks.size()) + " tracks, the head " + std::to_string (want.tracks.size());
        for (std::size_t t = 0; t < got.tracks.size(); ++t)
        {
            const auto& g = got.tracks[t].events;
            const auto& w = want.tracks[t].events;
            for (std::size_t i = 0; i < std::min (g.size(), w.size()); ++i)
                if (!(g[i] == w[i]))
                    return "track " + std::to_string (t) + ", event " + std::to_string (i) + ": " + eventText (g[i]) + ", the head's " + eventText (w[i]);
            if (g.size() != w.size())
                return "track " + std::to_string (t) + ": " + std::to_string (g.size()) + " events, the head " + std::to_string (w.size());
            if (got.tracks[t].endTick != want.tracks[t].endTick)
                return "track " + std::to_string (t) + " ends on " + std::to_string (got.tracks[t].endTick) + ", the head on " + std::to_string (want.tracks[t].endTick);
        }
        return "the bytes";
    }

    using Key = std::pair<std::uint8_t, std::uint8_t>; // (channel, key)
    using Groups = std::map<Key, std::vector<midi::Note>>;

    // A track's notes by (channel, key), each list in time order.
    Groups byKey (const midi::Track& track)
    {
        Groups out;
        for (const auto& n : midi::notesOf (track))
            out[{ n.channel, n.key }].push_back (n);
        return out;
    }

    // Notes as the cut writes them: each ended by `end` (an onset at or after it on its last tick),
    // then trimmed as the export trims a track's notes (midi::trimOverlaps).
    std::vector<midi::Note> headOf (std::vector<midi::Note> notes, std::int64_t end)
    {
        for (auto& n : notes)
        {
            const std::int64_t stop = std::min (n.tick + n.length, end);
            n.tick = std::min (n.tick, end - 1);
            n.length = std::max<std::int64_t> (1, stop - n.tick);
        }
        midi::trimOverlaps (notes);
        return notes;
    }

    // Whether `got`, the cut's notes on one key, are the head for some choice of `kept` of the notes
    // the feel played on it in the section the cut ends (`inSection`, from `from`, each on a tick of
    // its own), after the notes before that section (`before`). If so, `chosen` is one such choice.
    // A chosen note played before the cut's last tick keeps its tick; those played on it or later
    // merge into one note there, with the highest of their velocities.
    bool explained (const std::vector<midi::Note>& before, const std::vector<midi::Note>& inSection, std::size_t kept, std::int64_t from, std::int64_t end, const std::vector<midi::Note>& got, std::vector<midi::Note>& chosen)
    {
        chosen.clear();
        const midi::Note* onLast = nullptr;
        for (const auto& g : got)
        {
            if (g.tick < from)
                continue; // before the section: compared below
            if (g.tick >= end || (g.tick == end - 1 && onLast != nullptr))
                return false;
            if (g.tick == end - 1)
            {
                onLast = &g;
                continue;
            }
            const auto q = std::find_if (inSection.begin(), inSection.end(), [&] (const midi::Note& n) { return n.tick == g.tick; });
            if (q == inSection.end())
                return false;
            chosen.push_back (*q);
        }
        if (onLast != nullptr)
        {
            if (chosen.size() >= kept)
                return false;
            std::size_t more = kept - chosen.size();
            std::vector<bool> used (inSection.size(), false);
            for (const bool top : { true, false }) // one with the merged note's velocity, then any below it
                for (std::size_t i = 0; i < inSection.size() && more > 0; ++i)
                {
                    const midi::Note& q = inSection[i];
                    if (used[i] || q.tick < end - 1 || (top ? q.velocity != onLast->velocity : q.velocity > onLast->velocity))
                        continue;
                    used[i] = true;
                    chosen.push_back (q);
                    --more;
                    if (top)
                        break;
                }
            if (more > 0 || std::none_of (chosen.begin(), chosen.end(), [&] (const midi::Note& q) { return q.tick >= end - 1 && q.velocity == onLast->velocity; }))
                return false;
        }
        else if (chosen.size() != kept)
            return false;
        std::vector<midi::Note> all = before;
        all.insert (all.end(), chosen.begin(), chosen.end());
        return headOf (std::move (all), end) == got;
    }

    std::string ticksOf (const std::vector<midi::Note>& notes)
    {
        std::string out;
        for (const auto& n : notes)
            out += (out.empty() ? "" : ", ") + std::to_string (n.tick);
        return out;
    }

    // Checks that `cutPath` holds the head at tick `end` of the whole song's file `wholePath`, whose
    // score (the quantized version of the same file) is `scorePath`; the section the cut ends spans
    // [from, to) in that file. In the shortened region's marker and name, oldMarker becomes newMarker.
    void checkHead (const fs::path& wholePath, const fs::path& scorePath, const fs::path& cutPath, std::int64_t from, std::int64_t end, std::int64_t to, const std::string& oldMarker, const std::string& newMarker)
    {
        midi::File whole, score, cut;
        std::vector<std::uint8_t> got;
        if (!readMidi (wholePath, whole) || !readMidi (scorePath, score) || !readBytes (cutPath, got))
            return;
        const std::string what = shown (cutPath);
        if (whole.tracks.empty() || whole.tracks.size() != score.tracks.size())
        {
            problem (what + ": " + shown (wholePath) + " and " + shown (scorePath) + " don't have the same tracks");
            return;
        }
        if (auto parsed = midi::readSmf (got); parsed.ok())
            cut = std::move (parsed.value);
        else
        {
            problem (what + ": " + parsed.error);
            return;
        }
        const auto renamed = [&] (std::string text) {
            if (const auto at = text.find (oldMarker); at != std::string::npos)
                text.replace (at, oldMarker.size(), newMarker);
            return text;
        };
        midi::SongSpec spec;
        spec.ppq = whole.ppq;
        spec.endTick = end;
        spec.name = renamed (midi::trackName (whole.tracks[0]));
        for (const auto& t : midi::tempoMap (whole))
            if (t.tick < end)
                spec.tempos.push_back (t);
        for (const auto& t : midi::timeSignaturesOf (whole))
            if (t.tick < end)
                spec.timeSignatures.push_back (t);
        for (const auto& k : midi::keySignaturesOf (whole))
            if (k.tick < end)
                spec.keySignatures.push_back (k);
        for (auto m : midi::markersOf (whole))
            if (m.tick < end)
            {
                m.text = renamed (m.text);
                spec.markers.push_back (m);
            }
        for (std::size_t t = 1; t < whole.tracks.size(); ++t)
        {
            midi::TrackSpec track;
            track.name = midi::trackName (whole.tracks[t]);
            const Groups performed = byKey (whole.tracks[t]);
            const Groups written = byKey (score.tracks[t]);
            const Groups cutNotes = t < cut.tracks.size() ? byKey (cut.tracks[t]) : Groups {};
            std::set<Key> keys;
            for (const Groups* g : { &performed, &written, &cutNotes })
                for (const auto& entry : *g)
                    keys.insert (entry.first);
            for (const Key& key : keys)
            {
                const auto notesAt = [&key] (const Groups& g) { const auto it = g.find (key); return it != g.end() ? it->second : std::vector<midi::Note> {}; };
                const std::vector<midi::Note> played = notesAt (performed), scored = notesAt (written), cutHas = notesAt (cutNotes);
                const std::string where = what + ", track " + std::to_string (t) + ", key " + std::to_string (key.second);
                // the notes before the section are all kept; the section's, those its score starts before the cut
                std::vector<midi::Note> keep, inSection;
                for (const auto& n : played)
                    (n.tick < from ? keep : inSection).push_back (n);
                std::erase_if (inSection, [&] (const midi::Note& n) { return n.tick >= to; });
                std::size_t inScore = 0, kept = 0;
                for (const auto& n : scored)
                    if (n.tick >= from && n.tick < to)
                    {
                        ++inScore;
                        kept += n.tick < end ? 1u : 0u;
                    }
                const std::vector<midi::Note> before = keep;
                if (kept == inScore)
                    keep.insert (keep.end(), inSection.begin(), inSection.end());
                else if (kept > 0 && inSection.size() != inScore)
                {
                    unknowns.push_back (where + ": in the section the cut ends, the feel file has " + std::to_string (inSection.size()) + " notes on it where the quantized file has "
                                        + std::to_string (inScore) + ": the file merges notes played on one tick into one, so the files can't pair them");
                    track.notes.insert (track.notes.end(), cutHas.begin(), cutHas.end());
                    continue;
                }
                else
                    keep.insert (keep.end(), inSection.begin(), inSection.begin() + static_cast<std::ptrdiff_t> (kept));
                const std::vector<midi::Note> head = headOf (keep, end);
                std::vector<midi::Note> chosen;
                if (head != cutHas && kept > 0 && kept < inScore && explained (before, inSection, kept, from, end, cutHas, chosen))
                {
                    const std::vector<midi::Note> inOrder (inSection.begin(), inSection.begin() + static_cast<std::ptrdiff_t> (kept));
                    std::vector<midi::Note> instead, skipped;
                    for (const auto& n : chosen)
                        if (std::find (inOrder.begin(), inOrder.end(), n) == inOrder.end())
                            instead.push_back (n);
                    for (const auto& n : inOrder)
                        if (std::find (chosen.begin(), chosen.end(), n) == chosen.end())
                            skipped.push_back (n);
                    unknowns.push_back (where + ": the cut's notes are the head's if the feel played them out of score order across the cut (keeping those it played at "
                                        + ticksOf (instead) + " instead of " + ticksOf (skipped) + "), which the files can't show");
                    track.notes.insert (track.notes.end(), cutHas.begin(), cutHas.end());
                    continue;
                }
                track.notes.insert (track.notes.end(), head.begin(), head.end()); // if it isn't the cut's, the bytes differ below
            }
            for (const auto& c : midi::controllersOf (whole.tracks[t]))
                if (c.tick < end)
                    track.controllers.push_back (c);
            spec.tracks.push_back (std::move (track));
        }
        const auto want = midi::writeSmf (spec);
        if (!want.ok())
        {
            problem (what + ": the whole song's head can't be written: " + want.error);
            return;
        }
        if (got == want.value)
            return;
        const auto wantFile = midi::readSmf (want.value);
        problem (what + " isn't the head of " + shown (wholePath) + ": " + (wantFile.ok() ? fileDifference (cut, wantFile.value) : wantFile.error));
    }

    struct Region
    {
        std::int64_t order = 0, startBar = 0, bars = 0;
        std::string file, kind;
    };

    // One version folder of the cut against the whole song's. Counts the files it compared.
    void checkVersion (const fs::path& wholeRoot, const fs::path& cutRoot, const std::string& version, std::int64_t bars, std::size_t& compared)
    {
        const fs::path whole = wholeRoot / version, cut = cutRoot / version, score = wholeRoot / "quantized";
        json::Value wholeJson, cutJson;
        if (!readJson (whole / "regions.json", wholeJson) || !readJson (cut / "regions.json", cutJson))
            return;
        json::Value* regions = memberOf (wholeJson, "regions");
        json::Value* lanes = memberOf (wholeJson, "lanes");
        const json::Value* full = wholeJson.find ("full");
        const json::Value* bpm = wholeJson.find ("bpm");
        if (regions == nullptr || lanes == nullptr || full == nullptr || bpm == nullptr || bpm->integer <= 0)
        {
            problem (shown (whole / "regions.json") + " has no regions, lanes, full file or tempo");
            return;
        }
        // the regions the cut keeps: those that start before it
        std::vector<Region> kept;
        for (const auto& r : regions->items)
        {
            const json::Value *order = r.find ("order"), *file = r.find ("file"), *kind = r.find ("kind"), *start = r.find ("startBar"), *length = r.find ("bars");
            if (order == nullptr || file == nullptr || kind == nullptr || start == nullptr || length == nullptr)
            {
                problem (shown (whole / "regions.json") + ": a region without its order, file, kind, startBar or bars");
                return;
            }
            if (start->integer < bars)
                kept.push_back ({ order->integer, start->integer, length->integer, file->string, kind->string });
        }
        if (kept.empty() || kept.back().startBar + kept.back().bars < bars)
        {
            problem (shown (whole / "regions.json") + ": the whole song is shorter than " + std::to_string (bars) + " bars");
            return;
        }
        const Region& last = kept.back();
        const std::int64_t lastBars = bars - last.startBar;
        const std::string lastFile = "regions/" + twoDigits (last.order) + "_" + last.kind + "_" + std::to_string (lastBars) + "bars.mid";

        // regions.json: the whole song's, but for the length and the regions
        json::Value want = wholeJson;
        json::Value& shortened = memberOf (want, "regions")->items[kept.size() - 1];
        json::Value *songBars = memberOf (want, "bars"), *songLength = memberOf (want, "length"), *file = memberOf (shortened, "file"),
                    *regionBars = memberOf (shortened, "bars"), *ticks = memberOf (shortened, "lengthTicks"), *blocks = memberOf (shortened, "blocks"),
                    *regionLanes = memberOf (shortened, "lanes");
        if (songBars == nullptr || songLength == nullptr || file == nullptr || regionBars == nullptr || ticks == nullptr || blocks == nullptr || regionLanes == nullptr)
        {
            problem (shown (whole / "regions.json") + " lacks the song's bars or length, or a region's file, bars, lengthTicks, blocks or lanes");
            return;
        }
        const std::int64_t seconds = bars * 240 / bpm->integer;
        songBars->integer = bars;
        songLength->string = std::to_string (seconds / 60) + ":" + twoDigits (seconds % 60);
        file->string = lastFile;
        regionBars->integer = lastBars;
        ticks->integer = lastBars * ticksPerBar;
        blocks->items.resize (static_cast<std::size_t> (lastBars / 8));
        std::set<std::string> playing; // the lanes of the blocks it keeps, then in track order
        for (const auto& b : blocks->items)
            if (const json::Value* l = b.find ("lanes"))
                for (const auto& id : l->items)
                    playing.insert (id.string);
        regionLanes->items.clear();
        for (const auto& lane : lanes->items)
            if (const json::Value* id = lane.find ("id"); id != nullptr && playing.contains (id->string))
                regionLanes->items.push_back (stringValue (id->string));
        memberOf (want, "regions")->items.resize (kept.size());
        if (const auto d = jsonDifference (cutJson, want, ""); !d.empty())
            problem (shown (cut / "regions.json") + " isn't the whole song's with the last region shortened: it differs at " + d);

        // the files: exactly these
        std::set<std::string> expected = { full->string, "regions.json", lastFile };
        for (std::size_t i = 0; i + 1 < kept.size(); ++i)
            expected.insert (kept[i].file);
        std::set<std::string> found;
        std::error_code ec;
        for (fs::recursive_directory_iterator it (cut, ec), stop; !ec && it != stop; it.increment (ec))
            if (it->is_regular_file (ec))
                found.insert (shown (it->path().lexically_relative (cut)));
        for (const auto& f : expected)
            if (!found.contains (f))
                problem (shown (cut / f) + " is missing");
        for (const auto& f : found)
            if (!expected.contains (f))
                problem (shown (cut / f) + " shouldn't be there");

        // the regions before the last: the whole song's, byte for byte
        for (std::size_t i = 0; i + 1 < kept.size(); ++i)
        {
            std::vector<std::uint8_t> a, b;
            if (readBytes (whole / kept[i].file, a) && readBytes (cut / kept[i].file, b) && a != b)
                problem (shown (cut / kept[i].file) + " isn't the whole song's region");
            ++compared;
        }
        // the full file and the last region: the whole song's head; the section the cut ends spans the
        // whole region file, and its bars of the full file
        const std::string oldMarker = markerText (last.kind, last.order, last.bars), newMarker = markerText (last.kind, last.order, lastBars);
        checkHead (whole / full->string, score / full->string, cut / full->string, last.startBar * ticksPerBar, bars * ticksPerBar, (last.startBar + last.bars) * ticksPerBar, oldMarker, newMarker);
        checkHead (whole / last.file, score / last.file, cut / lastFile, 0, lastBars * ticksPerBar, last.bars * ticksPerBar, oldMarker, newMarker);
        compared += 3;
    }
}

int main (int argc, char** argv)
{
    std::int64_t bars = 0;
    const std::string_view text = argc == 4 ? std::string_view (argv[3]) : std::string_view();
    const auto [ptr, ec] = std::from_chars (text.data(), text.data() + text.size(), bars);
    if (argc != 4 || ec != std::errc() || ptr != text.data() + text.size() || bars <= 0 || bars % 8 != 0)
    {
        std::cerr << "Usage: groovecore_cut_check <the whole song's --out> <the cut song's --out> <bars, a multiple of 8>\n";
        return 2;
    }
    std::size_t compared = 0;
    for (const char* version : { "feel", "quantized" })
        checkVersion (fs::path (argv[1]), fs::path (argv[2]), version, bars, compared);
    for (const auto& p : problems)
        std::cerr << "groovecore_cut_check: " << p << '\n';
    for (const auto& u : unknowns)
        std::cerr << "groovecore_cut_check: can't tell: " << u << '\n';
    if (!problems.empty())
        return 1;
    if (!unknowns.empty())
    {
        std::cout << "groovecore_cut_check: the cut at bar " << bars << " is the whole song's head wherever the files can tell (" << compared << " files compared, both versions), but they can't tell "
                  << unknowns.size() << " key(s)\n";
        return 3;
    }
    std::cout << "groovecore_cut_check: the cut at bar " << bars << " is the whole song's head (" << compared << " files compared, both versions)\n";
    return 0;
}
