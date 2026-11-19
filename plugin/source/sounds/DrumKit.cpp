#include "sounds/DrumKit.h"

#include "dragout/DragFiles.h"
#include "sounds/SampleExport.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <numeric>
#include <string_view>
#include <utility>

namespace wubansia::sounds
{
    namespace
    {
        constexpr int kDrumChannel = 10;
        constexpr int kMaxSequence = 100; // seq_length above this is refused: the region list would run to thousands

        // A kit.json path's file name without folder and ".wav": "Drums/Kick Garage v1 rr1.wav" is "Kick Garage v1 rr1".
        std::string stemOf (std::string_view path)
        {
            const auto slash = path.find_last_of ('/');
            auto name = slash == std::string_view::npos ? path : path.substr (slash + 1);
            if (name.ends_with (".wav"))
                name.remove_suffix (4);
            return std::string (name);
        }

        // The sound of a file: its stem without its " rr<n>", then without its " v<n>": "Kick Garage v5 rr1" is "Kick Garage".
        std::string soundOf (std::string_view path)
        {
            auto s = stemOf (path);
            for (const std::string_view tag : { " rr", " v" })
                if (const auto at = s.rfind (tag); at != std::string::npos && at + tag.size() < s.size()
                                                   && std::all_of (s.begin() + static_cast<std::ptrdiff_t> (at + tag.size()), s.end(), [] (char c) { return c >= '0' && c <= '9'; }))
                    s.erase (at);
            return s;
        }

        // Never a hidden name: no leading dot or space (SampleExport's rule).
        std::string visible (std::string name)
        {
            const auto start = name.find_first_not_of (". ");
            return start == std::string::npos ? std::string() : name.substr (start);
        }

        // Floor division, as the texts round towards minus infinity (Python's //).
        std::int64_t floorDiv (std::int64_t a, std::int64_t b)
        {
            const auto q = a / b;
            return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
        }

        // milli-dB as decimal text, trailing zeros dropped: -14420 is "-14.42", -11000 "-11".
        std::string decibels (std::int64_t mdb)
        {
            const auto magnitude = mdb < 0 ? -mdb : mdb;
            auto text = std::string (mdb < 0 ? "-" : "") + std::to_string (magnitude / 1000);
            if (const auto frac = magnitude % 1000; frac != 0)
            {
                auto digits = std::to_string (frac);
                digits = std::string (3 - digits.size(), '0') + digits;
                text += "." + digits.substr (0, digits.find_last_not_of ('0') + 1);
            }
            return text;
        }

        // Per mille to SFZ's -100..100: 220 is "22", 125 "12.5".
        std::string sfzPan (std::int64_t pm)
        {
            const auto magnitude = pm < 0 ? -pm : pm;
            auto text = std::string (pm < 0 ? "-" : "") + std::to_string (magnitude / 10);
            if (magnitude % 10 != 0)
                text += "." + std::to_string (magnitude % 10);
            return text;
        }

        // Microseconds as seconds: 5000 is "0.005".
        std::string seconds (std::int64_t us)
        {
            auto text = std::to_string (us / 1000000);
            if (const auto frac = us % 1000000; frac != 0)
            {
                auto digits = std::to_string (frac);
                digits = std::string (6 - digits.size(), '0') + digits;
                text += "." + digits.substr (0, digits.find_last_not_of ('0') + 1);
            }
            return text;
        }

        // The README's pan: "centre", "22% right", "15% left".
        std::string panWords (std::int64_t pm)
        {
            if (pm == 0)
                return "centre";
            const auto magnitude = pm < 0 ? -pm : pm;
            auto text = std::to_string (magnitude / 10);
            if (magnitude % 10 != 0)
                text += "." + std::to_string (magnitude % 10);
            return text + "% " + (pm > 0 ? "right" : "left");
        }

        // The README's level: 0.1 dB, rounded half up (floor ((mdB + 50) / 100) tenths): -850 is "-0.8 dB", 0 "0 dB".
        std::string level (std::int64_t mdb)
        {
            const auto tenths = floorDiv (mdb + 50, 100);
            const auto magnitude = tenths < 0 ? -tenths : tenths;
            auto text = std::string (tenths < 0 ? "-" : "") + std::to_string (magnitude / 10);
            if (magnitude % 10 != 0)
                text += "." + std::to_string (magnitude % 10);
            return text + " dB";
        }

        std::string padded (const std::string& s, std::size_t width) { return s + std::string (width > s.size() ? width - s.size() : 0, ' '); }

        // Each layer's velocity range, as engine::pickLayer picks: the nearest layer velocity, the louder on a tie.
        std::vector<std::pair<int, int>> velocityRanges (const std::vector<engine::LayerEntry>& layers)
        {
            std::vector<std::pair<int, int>> out;
            for (std::size_t i = 0; i < layers.size(); ++i)
            {
                const int lo = i == 0 ? 1 : out.back().second + 1;
                const int hi = i + 1 == layers.size() ? 127 : static_cast<int> (floorDiv (layers[i].vel + layers[i + 1].vel - 1, 2));
                out.emplace_back (lo, hi);
            }
            return out;
        }

        // The pad's sequence: the lcm of its layers' round robins and its pan cycle.
        int sequenceOf (const engine::InstrumentEntry& instrument, const engine::ZoneEntry& zone)
        {
            int length = 1;
            for (const auto& layer : zone.layers)
                length = std::lcm (length, std::max<int> (1, static_cast<int> (layer.rr.size())));
            if (! instrument.panCyclePm.empty())
                length = std::lcm (length, static_cast<int> (instrument.panCyclePm.size()));
            return length;
        }

        struct Plan
        {
            const engine::KitManifest& kit;
            const engine::MixConfig& mix;
            DrumKitPlan out;
            int abSwitch = -1; // the drum keymap's A/B switch, -1 without one
            std::string suffix; // " - FUT01"
        };

        std::string padName (const DrumKitPad& pad) { return std::to_string (pad.key) + " " + drumNoteName (pad.key); }

        std::string refusal (const Plan& p, const DrumKitPad& pad, const std::string& why)
        {
            return p.kit.kit.id + "'s pad " + padName (pad) + " (" + pad.sound + ") " + why + ", which a drum kit can't carry";
        }

        // The channel's gain and pan inserts that apply on side (B, Wubansia's default, for a pad without a switch); why
        // not, for a refusal, or "".
        std::string channelLevel (const engine::Channel& channel, bool sideB, std::int32_t& gain, std::int32_t& pan)
        {
            gain = 0;
            pan = 0;
            int pans = 0;
            for (const auto& insert : channel.inserts)
            {
                if (insert.type != engine::InsertType::gain && insert.type != engine::InsertType::pan)
                    continue;
                if (insert.when.variant >= 0)
                    return "takes a level or pan that depends on the vox switch";
                if (insert.when.ab >= 0)
                {
                    if (insert.when.ab != static_cast<std::int8_t> (engine::AbSwitch::kick))
                        return "takes a level or pan that depends on another A/B switch";
                    if ((insert.when.is == 1) != sideB)
                        continue;
                }
                if (insert.type == engine::InsertType::gain)
                    gain += std::get<engine::GainParams> (insert.params).gainMdb;
                else
                {
                    pan = std::get<engine::PanParams> (insert.params).panPm;
                    ++pans;
                }
            }
            return pans > 1 ? "takes more than one pan" : "";
        }

        // What an SFZ drum pad can't carry, or "".
        std::string cantCarry (const Plan& p, const DrumKitPad& pad)
        {
            const auto& in = p.kit.instruments[static_cast<std::size_t> (pad.instrument)];
            const auto& zone = in.zones[static_cast<std::size_t> (pad.zone)];
            if (in.align == engine::Align::end)
                return "is end-aligned";
            if (in.mono)
                return "plays one note at a time (mono)";
            if (in.glide.curve != engine::GlideCurve::none)
                return "glides between notes";
            if (in.layeredWith >= 0)
                return "is layered with another sound";
            if (in.velocityLaw == engine::VelocityLaw::pow15)
                return "scales its layers' loudness with velocity";
            if (in.restrikeContinues && in.mode != engine::SampleMode::sustainLoop)
                return "carries on its voice when struck again, off a loop";
            if (zone.layers.empty())
                return "has no layers";
            for (const auto& layer : zone.layers)
            {
                if (layer.rr.empty())
                    return "has a layer without a file";
                for (const auto r : layer.rr)
                    if (p.kit.files[static_cast<std::size_t> (r)].prerollFrames != 0)
                        return "starts before its note (a pre-roll)";
            }
            if (in.mode == engine::SampleMode::sustainLoop)
            {
                const auto& first = p.kit.files[static_cast<std::size_t> (zone.layers.front().rr.front())];
                for (const auto& layer : zone.layers)
                    for (const auto r : layer.rr)
                        if (const auto& f = p.kit.files[static_cast<std::size_t> (r)]; f.loopStart != first.loopStart || f.loopEnd != first.loopEnd)
                            return "loops each file at other points";
                if (first.loopEnd <= first.loopStart)
                    return "loops no frames";
            }
            if (const auto length = sequenceOf (in, zone); length > kMaxSequence)
                return "takes a round robin and pan sequence of " + std::to_string (length) + " notes, over " + std::to_string (kMaxSequence);
            return {};
        }

        std::string sfzText (const Plan& p, int side, const std::string& label)
        {
            const auto& meta = p.kit.kit;
            std::vector<std::string> lines;
            auto title = meta.id + " " + meta.title + " drum kit";
            if (p.abSwitch >= 0)
                for (const auto& pad : p.out.pads)
                    if (pad.side >= 0)
                    {
                        title += ": " + label + " on " + std::to_string (pad.key);
                        break;
                    }
            lines.push_back ("// " + title + ", from Wubansia (content v" + std::to_string (meta.contentVersion) + ").");
            lines.push_back ("// " + meta.direction + ", " + std::to_string (meta.bpm) + " BPM, " + meta.key + " (" + meta.camelot
                             + "). Plays the kit's drum MIDI (channel 10).");
            lines.push_back ("// Notes as Live and Logic name them (36 is C1). A velocity plays the nearest layer, the louder on a tie;");
            lines.push_back ("// round robins take turns. Levels and pans are the kit's channel faders and pans in Wubansia's mix.");
            lines.push_back ("");
            lines.push_back ("<control>");
            lines.push_back ("default_path=Samples/");
            lines.push_back ("");
            lines.push_back ("<global>");
            lines.push_back ("amp_veltrack=0");

            std::vector<std::string> chokeGroups; // by first appearance: group g is 2g + 1, its choked voices 2g + 2
            for (const auto& pad : p.out.pads)
            {
                if (pad.side >= 0 && pad.side != side)
                    continue;
                const auto& in = p.kit.instruments[static_cast<std::size_t> (pad.instrument)];
                const auto& zone = in.zones[static_cast<std::size_t> (pad.zone)];
                lines.push_back ("");
                lines.push_back ("// " + padName (pad) + " " + pad.sound);
                lines.push_back ("<group>");
                lines.push_back (zone.root == pad.key ? "key=" + std::to_string (pad.key)
                                                      : "lokey=" + std::to_string (pad.key) + " hikey=" + std::to_string (pad.key)
                                                            + " pitch_keycenter=" + std::to_string (zone.root));
                switch (in.mode)
                {
                    case engine::SampleMode::oneShot: lines.push_back ("loop_mode=one_shot"); break;
                    case engine::SampleMode::gated: lines.push_back ("loop_mode=no_loop"); break;
                    case engine::SampleMode::sustainLoop:
                    {
                        const auto& f = p.kit.files[static_cast<std::size_t> (zone.layers.front().rr.front())];
                        lines.push_back ("loop_mode=loop_sustain");
                        lines.push_back ("loop_start=" + std::to_string (f.loopStart));
                        lines.push_back ("loop_end=" + std::to_string (f.loopEnd - 1));
                        break;
                    }
                }
                if (in.attackUs != 0)
                    lines.push_back ("ampeg_attack=" + seconds (in.attackUs));
                if (in.startFrames != 0)
                    lines.push_back ("offset=" + std::to_string (in.startFrames));
                if (in.mode != engine::SampleMode::oneShot && in.releaseUs != 0)
                    lines.push_back ("ampeg_release=" + seconds (in.releaseUs));
                if (pad.levelMdb != 0)
                    lines.push_back ("volume=" + decibels (pad.levelMdb));
                if (pad.panPm != 0)
                    lines.push_back ("pan=" + sfzPan (pad.panPm));
                if (in.choke.role != engine::ChokeRole::none)
                {
                    auto at = std::find (chokeGroups.begin(), chokeGroups.end(), in.choke.group);
                    if (at == chokeGroups.end())
                        at = chokeGroups.insert (chokeGroups.end(), in.choke.group);
                    const auto n = 2 * static_cast<int> (at - chokeGroups.begin()) + 1;
                    if (in.choke.role == engine::ChokeRole::chokes)
                        lines.push_back ("group=" + std::to_string (n));
                    else if (in.choke.role == engine::ChokeRole::choked)
                    {
                        lines.push_back ("group=" + std::to_string (n + 1));
                        lines.push_back ("off_by=" + std::to_string (n));
                    }
                    else
                    {
                        lines.push_back ("group=" + std::to_string (n));
                        lines.push_back ("off_by=" + std::to_string (n));
                    }
                    if (in.choke.role != engine::ChokeRole::chokes)
                    {
                        lines.push_back ("off_mode=normal");
                        lines.push_back ("ampeg_release=" + seconds (in.choke.fadeUs));
                    }
                }
                const auto length = sequenceOf (in, zone);
                if (length > 1)
                    lines.push_back ("seq_length=" + std::to_string (length));
                const auto ranges = velocityRanges (zone.layers);
                for (std::size_t l = 0; l < zone.layers.size(); ++l)
                {
                    const auto& layer = zone.layers[l];
                    for (int pos = 0; pos < length; ++pos)
                    {
                        const auto r = layer.rr[static_cast<std::size_t> (pos) % layer.rr.size()];
                        auto region = "<region> lovel=" + std::to_string (ranges[l].first) + " hivel=" + std::to_string (ranges[l].second);
                        if (length > 1)
                            region += " seq_position=" + std::to_string (pos + 1);
                        if (! in.panCyclePm.empty())
                            region += " pan=" + sfzPan (in.panCyclePm[static_cast<std::size_t> (pos) % in.panCyclePm.size()]);
                        region += " sample=" + dragout::safeName (padName (pad) + " " + stemOf (p.kit.files[static_cast<std::size_t> (r)].path) + p.suffix) + ".wav";
                        lines.push_back (region);
                    }
                }
            }
            std::string text;
            for (const auto& line : lines)
                text += line + "\n";
            return text;
        }

        std::string readmeText (const Plan& p, const std::vector<std::pair<std::string, int>>& sfzFiles)
        {
            const auto& meta = p.kit.kit;
            std::vector<std::string> lines;
            lines.push_back (meta.id + " " + meta.title + " drum kit, from Wubansia");
            lines.push_back ("");
            lines.push_back (meta.id + "'s drums as Wubansia plays them: " + meta.direction + ", " + std::to_string (meta.bpm) + " BPM, " + meta.key + " ("
                             + meta.camelot + "), content v" + std::to_string (meta.contentVersion)
                             + ". Every WAV here is the kit's own 48 kHz / 24-bit file, byte for byte.");
            lines.push_back ("");
            lines.push_back ("IN THIS FOLDER");
            lines.push_back ("- Pads: one WAV per drum note, its loudest hit, for your drum rack's pads.");
            lines.push_back ("- Samples: every velocity layer (v1 is the softest) and round robin (rr1, rr2...) of each pad, for the SFZ files.");
            for (const auto& [name, side] : sfzFiles)
            {
                if (p.abSwitch < 0)
                    lines.push_back ("- " + name + ": the whole kit for an SFZ player.");
                else
                {
                    const auto& labels = meta.ab[static_cast<std::size_t> (p.abSwitch)];
                    std::string key;
                    for (const auto& pad : p.out.pads)
                        if (pad.side >= 0)
                        {
                            key = std::to_string (pad.key);
                            break;
                        }
                    if (side == 1)
                        lines.push_back ("- " + name + ": the whole kit for an SFZ player, with " + labels.b + " on " + key + " (Wubansia's kick B, the default).");
                    else
                        lines.push_back ("- " + name + ": the same with " + labels.a + " on " + key + " (kick A).");
                }
            }
            lines.push_back ("");
            lines.push_back ("THE PADS");
            lines.push_back ("Wubansia's drum MIDI plays these notes, on channel 10: the Loop and Song drags, the Drums part and 1 drum track. Notes are "
                             "named as in Live and Logic, where middle C is C3, so 36 is C1.");
            lines.push_back ("");

            std::vector<std::array<std::string, 5>> rows { { "Note", "Sound", "Level", "Pan", "Plays" } };
            bool looped = false;
            std::vector<std::string> outside; // pads off Live's first page of pads, C1 to D#2
            for (const auto& pad : p.out.pads)
            {
                const auto& in = p.kit.instruments[static_cast<std::size_t> (pad.instrument)];
                std::string how = in.mode == engine::SampleMode::oneShot ? "one-shot"
                                  : in.mode == engine::SampleMode::gated ? "plays while the note is held"
                                                                         : "loops while the note is held";
                if (in.mode != engine::SampleMode::oneShot && in.releaseUs != 0)
                    how += ", " + std::to_string (in.releaseUs / 1000) + " ms release";
                if (in.restrikeContinues)
                    how += ", each note restarts the loop; seamless at the kit's " + std::to_string (meta.bpm) + " BPM";
                if (in.choke.role == engine::ChokeRole::chokes)
                    how += ", cuts the open hat";
                else if (in.choke.role != engine::ChokeRole::none)
                    how += ", cut by the closed hat over " + std::to_string (in.choke.fadeUs / 1000) + " ms";
                if (pad.side >= 0)
                    how += pad.side == 0 ? " (kick A)" : " (kick B)";
                looped = looped || in.mode != engine::SampleMode::oneShot;
                if ((pad.key < 36 || pad.key > 51) && std::find (outside.begin(), outside.end(), drumNoteName (pad.key)) == outside.end())
                    outside.push_back (drumNoteName (pad.key));
                rows.push_back ({ padName (pad), pad.sound, level (pad.levelMdb), in.panCyclePm.empty() ? panWords (pad.panPm) : "turns L/R", how });
            }
            std::array<std::size_t, 4> widths {};
            for (const auto& row : rows)
                for (std::size_t c = 0; c < 4; ++c)
                    widths[c] = std::max (widths[c], row[c].size());
            for (const auto& row : rows)
            {
                std::string line = " ";
                for (std::size_t c = 0; c < 4; ++c)
                    line += " " + padded (row[c], widths[c]) + " ";
                lines.push_back (line + " " + row[4]);
            }
            lines.push_back ("");
            lines.push_back ("Level and pan are each sound's channel fader and pan in Wubansia's mix, which the SFZ files apply. The rest of that mix (EQ, "
                             "ducks, sends, the drum bus, the club master) isn't here: these are the dry sounds.");
            lines.push_back ("");
            lines.push_back ("IN ABLETON LIVE");
            lines.push_back ("1. Put a Drum Rack on a MIDI track.");
            lines.push_back ("2. Drag each file in Pads onto its note's pad: each name starts with its note. Files dropped together fill pads in a row from "
                             "the one you drop on, so drop them one at a time.");
            lines.push_back ("3. Set each pad's volume and pan from the table, put the hats in one choke group, and save the rack as a preset.");
            int step = 4;
            if (! outside.empty())
            {
                std::string notes;
                for (std::size_t i = 0; i < outside.size(); ++i)
                    notes += (i == 0 ? "" : " and ") + outside[i];
                lines.push_back (std::to_string (step++) + ". The rack shows C1 to D#2 at first: scroll its pad overview for " + notes + ".");
            }
            if (looped)
                lines.push_back (std::to_string (step) + ". Set the looped sound's Simpler to Gate, so the note's length plays it.");
            lines.push_back ("");
            lines.push_back ("IN LOGIC PRO");
            lines.push_back ("1. Make a Drum Machine Designer track.");
            lines.push_back ("2. Drag each file in Pads onto a pad, and set the pad's input note to the note in the table.");
            lines.push_back ("3. Put the hats in one choke group, and save the patch.");
            if (looped)
                lines.push_back ("4. Set the looped sound's Quick Sampler to Classic, so the note's length plays it.");
            lines.push_back ("");
            lines.push_back ("IN AN SFZ PLAYER (sfizz, sforzando, Bitwig Studio's Sampler)");
            lines.push_back ("Load an .sfz file here, beside the Samples folder. It maps every layer and round robin, the levels, the pans and the hat "
                             "choke, and plays the drum MIDI as Wubansia does.");
            lines.push_back ("");
            lines.push_back ("YOUR OWN SOUNDS");
            lines.push_back ("Swap a pad's file for your own sample in your DAW. For the SFZ files, put your file in Samples under the name it replaces, "
                             "or point that pad's sample= lines at your file.");
            lines.push_back ("");
            lines.push_back ("THIS FOLDER IS YOURS");
            lines.push_back ("Wubansia never changes or deletes anything in it. If you change it and save the drum kit again, Wubansia saves a fresh "
                             "copy beside it, \""
                             + p.out.folder + " (2)\".");
            std::string text;
            for (const auto& line : lines)
                text += line + "\n";
            return text;
        }
    }

    std::string drumNoteName (int key)
    {
        static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return std::string (names[((key % 12) + 12) % 12]) + std::to_string (static_cast<int> (floorDiv (key, 12)) - 2);
    }

    std::string drumKitFolderName (const engine::KitMeta& kit)
    {
        const auto version = kit.contentVersion >= 2 ? " c" + std::to_string (kit.contentVersion) : std::string();
        return visible (dragout::safeName (kit.id + " " + kit.title + " Drum Kit" + version));
    }

    util::Result<DrumKitPlan> planDrumKit (const engine::KitManifest& kit, const engine::MixConfig& mix)
    {
        using R = util::Result<DrumKitPlan>;
        Plan p { kit, mix, {}, -1, " - " + kit.kit.id };
        p.out.folder = drumKitFolderName (kit.kit);
        if (p.out.folder.empty())
            return R::failure (kit.kit.id + " has no name a folder can take");

        // The pads: every key of every channel-10 entry, both sides of an A/B entry, by key and then side.
        for (const auto& e : kit.keymap)
        {
            if (e.ch != kDrumChannel)
                continue;
            if (e.b >= 0 && e.ab >= 0)
            {
                if (e.ab != static_cast<std::int32_t> (engine::AbSwitch::kick))
                    return R::failure (kit.kit.id + "'s drum notes " + std::to_string (e.lo) + "-" + std::to_string (e.hi)
                                       + " switch on an A/B switch other than the kick's, which a drum kit can't carry");
                p.abSwitch = e.ab;
            }
            for (int key = e.lo; key <= e.hi; ++key)
                for (int side = 0; side < 2; ++side)
                {
                    const auto instrument = side == 0 ? e.a : e.b;
                    if (instrument < 0 || static_cast<std::size_t> (instrument) >= kit.instruments.size())
                        continue;
                    const auto& in = kit.instruments[static_cast<std::size_t> (instrument)];
                    DrumKitPad pad;
                    pad.key = key;
                    pad.side = e.b >= 0 && e.ab >= 0 ? side : -1;
                    pad.instrument = instrument;
                    for (std::size_t z = 0; z < in.zones.size(); ++z)
                        if (in.zones[z].lo <= key && key <= in.zones[z].hi)
                        {
                            pad.zone = static_cast<int> (z);
                            break;
                        }
                    if (pad.zone < 0 || in.zones[static_cast<std::size_t> (pad.zone)].layers.empty()
                        || in.zones[static_cast<std::size_t> (pad.zone)].layers.back().rr.empty())
                        return R::failure (kit.kit.id + "'s pad " + std::to_string (key) + " " + drumNoteName (key) + " (" + in.name
                                           + ") has no zone and file, which a drum kit can't carry");
                    const auto& zone = in.zones[static_cast<std::size_t> (pad.zone)];
                    pad.sound = soundOf (kit.files[static_cast<std::size_t> (zone.layers.back().rr.front())].path);
                    p.out.pads.push_back (std::move (pad));
                }
        }
        std::stable_sort (p.out.pads.begin(), p.out.pads.end(), [] (const DrumKitPad& a, const DrumKitPad& b) {
            return a.key != b.key ? a.key < b.key : a.side < b.side;
        });
        if (p.out.pads.empty())
            return R::failure (kit.kit.id + " has no drum notes on channel 10");

        for (auto& pad : p.out.pads)
        {
            if (const auto why = cantCarry (p, pad); ! why.empty())
                return R::failure (refusal (p, pad, why));
            const auto& in = kit.instruments[static_cast<std::size_t> (pad.instrument)];
            const auto channel = mix.channelIndex (in.mixChannel);
            if (channel < 0)
                return R::failure (refusal (p, pad, "plays on no mix channel"));
            std::int32_t gain = 0, pan = 0;
            if (const auto why = channelLevel (mix.channels[static_cast<std::size_t> (channel)], pad.side != 0, gain, pan); ! why.empty())
                return R::failure (refusal (p, pad, why));
            pad.levelMdb = gain + in.gainMdb;
            pad.panPm = pan;
        }

        // The WAVs: Pads/ then Samples/, each name once (the same name for the same file, as a pad's two sides could give).
        std::map<std::string, int> taken; // lower-case name -> source
        std::vector<DrumKitFile> pads, samples;
        const auto add = [&] (std::vector<DrumKitFile>& list, const std::string& folder, const std::string& name, int source) -> std::string {
            if (name.size() > kMaxNameStem)
                return "the name " + name + " is too long";
            const auto path = folder + "/" + name;
            auto lower = path;
            std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
            if (const auto at = taken.find (lower); at != taken.end())
                return at->second == source ? std::string() : "two of its files would both be " + path;
            taken.emplace (lower, source);
            list.push_back ({ path, source, {} });
            return {};
        };
        for (const auto& pad : p.out.pads)
        {
            const auto& zone = kit.instruments[static_cast<std::size_t> (pad.instrument)].zones[static_cast<std::size_t> (pad.zone)];
            const auto prefix = padName (pad) + " ";
            const auto top = zone.layers.back().rr.front();
            if (auto why = add (pads, "Pads", dragout::safeName (prefix + pad.sound + p.suffix) + ".wav", top); ! why.empty())
                return R::failure (kit.kit.id + "'s drum kit can't be named: " + why);
            for (const auto& layer : zone.layers)
                for (const auto r : layer.rr)
                    if (auto why = add (samples, "Samples", dragout::safeName (prefix + stemOf (kit.files[static_cast<std::size_t> (r)].path) + p.suffix) + ".wav", r);
                        ! why.empty())
                        return R::failure (kit.kit.id + "'s drum kit can't be named: " + why);
        }
        p.out.files = std::move (pads);
        p.out.files.insert (p.out.files.end(), samples.begin(), samples.end());
        p.out.wavs = static_cast<int> (p.out.files.size());

        // The SFZ files: one per side of the switch, Wubansia's default (B) first; else one.
        std::vector<std::pair<std::string, int>> sfzFiles;
        if (p.abSwitch >= 0)
        {
            const auto& labels = kit.kit.ab[static_cast<std::size_t> (p.abSwitch)];
            for (const int side : { 1, 0 })
            {
                const auto& label = side == 0 ? labels.a : labels.b;
                const auto name = dragout::safeName (p.out.folder + " - " + label) + ".sfz";
                p.out.sfz[static_cast<std::size_t> (side)] = name;
                sfzFiles.emplace_back (name, side);
                p.out.files.push_back ({ name, -1, sfzText (p, side, label) });
            }
        }
        else
        {
            const auto name = p.out.folder + ".sfz";
            p.out.sfz = { name, name };
            sfzFiles.emplace_back (name, 1);
            p.out.files.push_back ({ name, -1, sfzText (p, 1, {}) });
        }
        p.out.files.push_back ({ "README.txt", -1, readmeText (p, sfzFiles) });
        p.out.texts = static_cast<int> (p.out.files.size()) - p.out.wavs;
        return R::success (std::move (p.out));
    }
}
