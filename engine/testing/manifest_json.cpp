#include "testing/manifest_json.h"

#include "kit/kit.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <variant>

namespace wubansia::engine::testing
{
    Json Json::integer (std::int64_t v)
    {
        Json j;
        j.kind = Kind::integer;
        j.number = v;
        return j;
    }

    Json Json::string (std::string v)
    {
        Json j;
        j.kind = Kind::string;
        j.text = std::move (v);
        return j;
    }

    Json Json::flag (bool v)
    {
        Json j;
        j.kind = Kind::flag;
        j.on = v;
        return j;
    }

    Json Json::array()
    {
        Json j;
        j.kind = Kind::array;
        return j;
    }

    Json Json::object()
    {
        return Json {};
    }

    Json Json::ints (const std::vector<std::int32_t>& values)
    {
        Json j = array();
        for (const auto v : values)
            j.push (integer (v));
        return j;
    }

    Json Json::strings (const std::vector<std::string>& values)
    {
        Json j = array();
        for (const auto& v : values)
            j.push (string (v));
        return j;
    }

    Json& Json::add (std::string key, Json value)
    {
        keys.push_back (std::move (key));
        items.push_back (std::move (value));
        return *this;
    }

    Json& Json::push (Json value)
    {
        items.push_back (std::move (value));
        return *this;
    }

    namespace
    {
        bool isScalar (const Json& j)
        {
            return j.kind == Json::Kind::integer || j.kind == Json::Kind::string || j.kind == Json::Kind::flag;
        }

        // Only ", \ and control characters are escaped; UTF-8 passes through (its bytes are all >= 0x80).
        void writeString (std::string& out, const std::string& s)
        {
            out += '"';
            for (const char ch : s)
            {
                const auto c = static_cast<unsigned char> (ch);
                switch (c)
                {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (c < 0x20)
                        {
                            char escaped[8];
                            std::snprintf (escaped, sizeof (escaped), "\\u%04x", static_cast<unsigned> (c));
                            out += escaped;
                        }
                        else
                        {
                            out += ch;
                        }
                }
            }
            out += '"';
        }

        void writeScalar (std::string& out, const Json& j)
        {
            switch (j.kind)
            {
                case Json::Kind::integer: out += std::to_string (j.number); break;
                case Json::Kind::string: writeString (out, j.text); break;
                case Json::Kind::flag: out += j.on ? "true" : "false"; break;
                case Json::Kind::array:
                case Json::Kind::object: break;
            }
        }

        void indent (std::string& out, int depth)
        {
            out.append (static_cast<std::size_t> (2 * depth), ' ');
        }

        void write (std::string& out, const Json& j, int depth)
        {
            if (j.kind == Json::Kind::object)
            {
                if (j.keys.empty())
                {
                    out += "{}";
                    return;
                }
                out += "{\n";
                for (std::size_t i = 0; i < j.keys.size(); ++i)
                {
                    indent (out, depth + 1);
                    writeString (out, j.keys[i]);
                    out += ": ";
                    write (out, j.items[i], depth + 1);
                    out += i + 1 < j.keys.size() ? ",\n" : "\n";
                }
                indent (out, depth);
                out += '}';
            }
            else if (j.kind == Json::Kind::array)
            {
                if (j.items.empty())
                {
                    out += "[]";
                    return;
                }
                bool scalars = true;
                for (const auto& item : j.items)
                    scalars = scalars && isScalar (item);
                if (scalars)
                {
                    out += '[';
                    for (std::size_t i = 0; i < j.items.size(); ++i)
                    {
                        if (i > 0)
                            out += ", ";
                        writeScalar (out, j.items[i]);
                    }
                    out += ']';
                    return;
                }
                out += "[\n";
                for (std::size_t i = 0; i < j.items.size(); ++i)
                {
                    indent (out, depth + 1);
                    write (out, j.items[i], depth + 1);
                    out += i + 1 < j.items.size() ? ",\n" : "\n";
                }
                indent (out, depth);
                out += ']';
            }
            else
            {
                writeScalar (out, j);
            }
        }

        Json str (std::string_view s) { return Json::string (std::string (s)); }
        Json num (std::int64_t v) { return Json::integer (v); }

        template <std::size_t N>
        Json id (const std::array<std::string_view, N>& ids, int index)
        {
            return str (index >= 0 && static_cast<std::size_t> (index) < N ? ids[static_cast<std::size_t> (index)] : std::string_view {});
        }

        Json band (const Band& b)
        {
            return Json::object().add ("lo_hz", num (b.loHz)).add ("hi_hz", num (b.hiHz));
        }

        Json lanes (std::uint16_t bits)
        {
            Json j = Json::array();
            for (int lane = 0; lane < kLanes; ++lane)
                if ((bits >> lane) & 1u)
                    j.push (str (kLaneIds[static_cast<std::size_t> (lane)]));
            return j;
        }

        Json inserts (const std::vector<Insert>& list, const MixConfig& mix);

        Json insert (const Insert& in, const MixConfig& mix)
        {
            Json j = Json::object();
            j.add ("type", id (kInsertTypeIds, static_cast<int> (in.type)));
            switch (in.type)
            {
                case InsertType::gain:
                {
                    const auto& p = std::get<GainParams> (in.params);
                    j.add ("gain_mdb", num (p.gainMdb));
                    break;
                }
                case InsertType::pan:
                {
                    const auto& p = std::get<PanParams> (in.params);
                    j.add ("pan_pm", num (p.panPm));
                    break;
                }
                case InsertType::width:
                {
                    const auto& p = std::get<WidthParams> (in.params);
                    j.add ("width_pm", num (p.widthPm)).add ("freq_hz", num (p.freqHz)).add ("order", num (p.order));
                    break;
                }
                case InsertType::peak:
                case InsertType::lowShelf:
                case InsertType::highShelf:
                {
                    const auto& p = std::get<EqParams> (in.params);
                    j.add ("freq_hz", num (p.freqHz)).add ("gain_mdb", num (p.gainMdb)).add ("q_pm", num (p.qPm));
                    break;
                }
                case InsertType::highpass:
                case InsertType::lowpass:
                {
                    const auto& p = std::get<PassParams> (in.params);
                    j.add ("freq_hz", num (p.freqHz)).add ("order", num (p.order)).add ("q_pm", num (p.qPm));
                    j.add ("stages", num (p.stages)).add ("band", band (p.band));
                    break;
                }
                case InsertType::ccFilter:
                {
                    const auto& p = std::get<CcFilterParams> (in.params);
                    j.add ("response", id (kFilterResponseIds, static_cast<int> (p.response)));
                    j.add ("cc74_lo_hz", num (p.cc74LoHz)).add ("cc74_hi_hz", num (p.cc74HiHz)).add ("cc74_default", num (p.cc74Default));
                    j.add ("cc71_lo_pm", num (p.cc71LoPm)).add ("cc71_hi_pm", num (p.cc71HiPm)).add ("cc71_default", num (p.cc71Default));
                    j.add ("band", band (p.band));
                    break;
                }
                case InsertType::monoBelow:
                {
                    const auto& p = std::get<MonoBelowParams> (in.params);
                    j.add ("freq_hz", num (p.freqHz)).add ("order", num (p.order));
                    break;
                }
                case InsertType::clip:
                {
                    const auto& p = std::get<ClipParams> (in.params);
                    j.add ("ceiling_mdb", num (p.ceilingMdb)).add ("knee_mdb", num (p.kneeMdb));
                    break;
                }
                case InsertType::drive:
                {
                    const auto& p = std::get<DriveParams> (in.params);
                    j.add ("drive_pm", num (p.drivePm)).add ("bias_pm", num (p.biasPm)).add ("output_mdb", num (p.outputMdb));
                    break;
                }
                case InsertType::duck:
                {
                    const auto& p = std::get<DuckParams> (in.params);
                    j.add ("mode", id (kDuckModeIds, static_cast<int> (p.mode)));
                    if (p.mode == DuckMode::onset)
                    {
                        j.add ("key_lanes", lanes (p.keyLanes)).add ("min_velocity", num (p.minVelocity));
                        j.add ("depth_mdb", num (p.depthMdb)).add ("ramp_us", num (p.rampUs)).add ("hold_us", num (p.holdUs));
                        j.add ("release_us", num (p.releaseUs)).add ("pre_us", num (p.preUs));
                    }
                    else
                    {
                        const bool known = p.keyChannel >= 0 && static_cast<std::size_t> (p.keyChannel) < mix.channels.size();
                        j.add ("key_channel", str (known ? mix.channels[static_cast<std::size_t> (p.keyChannel)].id : std::string {}));
                        j.add ("depth_mdb", num (p.depthMdb)).add ("range_mdb", num (p.rangeMdb)).add ("top_mdb", num (p.topMdb));
                        j.add ("attack_us", num (p.attackUs)).add ("release_us", num (p.releaseUs));
                    }
                    j.add ("band", band (p.band)).add ("pump", Json::flag (p.pump));
                    break;
                }
                case InsertType::comp:
                {
                    const auto& p = std::get<CompParams> (in.params);
                    j.add ("threshold_mdb", num (p.thresholdMdb)).add ("ratio_pm", num (p.ratioPm)).add ("knee_mdb", num (p.kneeMdb));
                    j.add ("attack_us", num (p.attackUs)).add ("release_fast_us", num (p.releaseFastUs));
                    j.add ("release_slow_us", num (p.releaseSlowUs)).add ("makeup_mdb", num (p.makeupMdb));
                    break;
                }
                case InsertType::parallel:
                {
                    const auto& p = std::get<ParallelParams> (in.params);
                    j.add ("level_mdb", num (p.levelMdb)).add ("inserts", inserts (p.chain, mix));
                    break;
                }
            }
            if (! in.when.always())
            {
                static constexpr std::array<std::string_view, 2> variants { "vox", "novox" };
                Json when = Json::object();
                when.add ("variant", in.when.variant < 0 ? str ("any") : id (variants, in.when.variant));
                when.add ("ab", in.when.ab < 0 ? str ("none") : id (kAbSwitchIds, in.when.ab));
                when.add ("is", str (in.when.is != 0 ? "b" : "a"));
                j.add ("when", std::move (when));
            }
            return j;
        }

        Json inserts (const std::vector<Insert>& list, const MixConfig& mix)
        {
            Json j = Json::array();
            for (const auto& in : list)
                j.push (insert (in, mix));
            return j;
        }

        Json glide (const Glide& g)
        {
            return Json::object()
                .add ("curve", id (kGlideCurveIds, static_cast<int> (g.curve)))
                .add ("us", num (g.us))
                .add ("dip_us", num (g.dipUs));
        }

        Json choke (const ChokeEntry& c)
        {
            return Json::object()
                .add ("group", str (c.group))
                .add ("role", id (kChokeRoleIds, static_cast<int> (c.role)))
                .add ("fade_us", num (c.fadeUs))
                .add ("curve", id (kChokeCurveIds, static_cast<int> (c.curve)));
        }

        std::string instrumentId (const KitManifest& m, int index)
        {
            return index >= 0 && static_cast<std::size_t> (index) < m.instruments.size() ? m.instruments[static_cast<std::size_t> (index)].id : std::string {};
        }
    }

    std::string dump (const Json& value)
    {
        std::string out;
        write (out, value, 0);
        out += '\n';
        return out;
    }

    Json kitManifestToJson (const KitManifest& m)
    {
        Json kit = Json::object();
        kit.add ("id", str (m.kit.id)).add ("title", str (m.kit.title)).add ("direction", str (m.kit.direction));
        kit.add ("bpm", num (m.kit.bpm)).add ("key", str (m.kit.key)).add ("camelot", str (m.kit.camelot));
        kit.add ("swing_pm", num (m.kit.swingPm)).add ("content_version", num (m.kit.contentVersion));
        Json laneLabels = Json::object();
        for (std::size_t i = 0; i < kLaneIds.size(); ++i)
            laneLabels.add (std::string (kLaneIds[i]), str (m.kit.lanes[i]));
        kit.add ("lanes", std::move (laneLabels));
        Json ab = Json::object();
        for (std::size_t i = 0; i < kAbSwitchIds.size(); ++i)
            ab.add (std::string (kAbSwitchIds[i]), Json::object().add ("a", str (m.kit.ab[i].a)).add ("b", str (m.kit.ab[i].b)));
        kit.add ("ab", std::move (ab));

        Json files = Json::array();
        for (const auto& f : m.files)
        {
            files.push (Json::object()
                    .add ("path", str (f.path))
                    .add ("frames", num (f.frames))
                    .add ("channels", num (f.channels))
                    .add ("sha256", str (f.sha256))
                    .add ("loop_start", num (f.loopStart))
                    .add ("loop_end", num (f.loopEnd))
                    .add ("preroll_frames", num (f.prerollFrames)));
        }

        Json instruments = Json::array();
        for (const auto& in : m.instruments)
        {
            Json zones = Json::array();
            for (const auto& z : in.zones)
            {
                Json layers = Json::array();
                for (const auto& l : z.layers)
                    layers.push (Json::object().add ("vel", num (l.vel)).add ("rr", Json::ints (l.rr)));
                zones.push (Json::object().add ("root", num (z.root)).add ("lo", num (z.lo)).add ("hi", num (z.hi)).add ("layers", std::move (layers)));
            }
            Json j = Json::object();
            j.add ("id", str (in.id)).add ("name", str (in.name)).add ("lane", str (laneId (in.lane))).add ("mix_channel", str (in.mixChannel));
            j.add ("mode", id (kSampleModeIds, static_cast<int> (in.mode))).add ("align", id (kAlignIds, static_cast<int> (in.align)));
            j.add ("attack_us", num (in.attackUs)).add ("release_us", num (in.releaseUs)).add ("gain_mdb", num (in.gainMdb));
            j.add ("start_frames", num (in.startFrames));
            j.add ("velocity_law", id (kVelocityLawIds, static_cast<int> (in.velocityLaw))).add ("interp", id (kInterpIds, static_cast<int> (in.interp)));
            j.add ("mono", Json::flag (in.mono)).add ("glide", glide (in.glide)).add ("restrike_continues", Json::flag (in.restrikeContinues));
            j.add ("choke", choke (in.choke)).add ("duck_key", Json::flag (in.duckKey)).add ("pan_cycle_pm", Json::ints (in.panCyclePm));
            j.add ("rr_block_16ths", num (in.rrBlock16ths)).add ("layered_with", str (instrumentId (m, in.layeredWith)));
            j.add ("zones", std::move (zones));
            instruments.push (std::move (j));
        }

        Json keymap = Json::array();
        for (const auto& k : m.keymap)
        {
            keymap.push (Json::object()
                    .add ("ch", num (k.ch))
                    .add ("keys", Json::ints ({ k.lo, k.hi }))
                    .add ("a", str (instrumentId (m, k.a)))
                    .add ("b", str (instrumentId (m, k.b)))
                    .add ("ab", id (kAbSwitchIds, k.ab)));
        }

        Json extras = Json::array();
        for (const auto& e : m.extras)
            extras.push (Json::object().add ("ch", num (e.ch)).add ("name", str (e.name)).add ("lane", str (laneId (e.lane))));

        Json phrases = Json::array();
        for (const auto& p : m.phrases)
        {
            Json slices = Json::array();
            for (const auto& s : p.slices)
            {
                slices.push (Json::object()
                        .add ("step", num (s.step))
                        .add ("key", num (s.key))
                        .add ("start", num (s.start))
                        .add ("end", num (s.end))
                        .add ("fade", num (s.fade))
                        .add ("lead", num (s.lead))
                        .add ("holds", Json::ints ({ s.holds.begin(), s.holds.begin() + std::max (0, std::min (s.holdCount, kMaxHolds)) })));
            }
            Json j = Json::object();
            j.add ("lane", str (laneId (p.lane))).add ("ch", num (p.ch)).add ("ab", id (kAbSwitchIds, p.ab));
            j.add ("mix_channel", str (p.mixChannel)).add ("gain_mdb", num (p.gainMdb));
            j.add ("stems", Json::object().add ("a", num (p.stemA)).add ("b", num (p.stemB)));
            j.add ("cycle_16ths", num (p.cycle16ths)).add ("tolerance_pm", num (p.tolerancePm)).add ("fade_us", num (p.fadeUs));
            j.add ("slices", std::move (slices));
            phrases.push (std::move (j));
        }

        Json top = Json::object();
        top.add ("kit", std::move (kit)).add ("files", std::move (files)).add ("instruments", std::move (instruments));
        top.add ("keymap", std::move (keymap)).add ("extras", std::move (extras)).add ("phrases", std::move (phrases));
        return top;
    }

    Json mixManifestToJson (const MixConfig& mix)
    {
        Json channels = Json::array();
        for (const auto& c : mix.channels)
        {
            channels.push (Json::object()
                    .add ("id", str (c.id))
                    .add ("lane", id (kLaneIds, c.lane))
                    .add ("group", id (kGroupIds, static_cast<int> (c.group)))
                    .add ("inserts", inserts (c.inserts, mix))
                    .add ("sends_pm", Json::ints ({ c.sendsPm[0], c.sendsPm[1], c.sendsPm[2] })));
        }

        Json groups = Json::object();
        groups.add ("drums", Json::object().add ("inserts", inserts (mix.drums.inserts, mix)));
        groups.add ("music", Json::object().add ("inserts", inserts (mix.music.inserts, mix)));

        const auto& s = mix.space;
        Json space = Json::object();
        space.add ("delays_frames", Json::ints ({ s.delaysFrames.begin(), s.delaysFrames.end() }));
        space.add ("signs", Json::ints ({ s.signs.begin(), s.signs.end() }));
        space.add ("t60_ms", num (s.t60Ms)).add ("t60_low_ms", num (s.t60LowMs)).add ("low_hz", num (s.lowHz)).add ("damp_hz", num (s.dampHz));
        space.add ("predelay_us", num (s.predelayUs)).add ("send_hp_hz", num (s.sendHpHz)).add ("send_order", num (s.sendOrder));
        space.add ("shimmer_pm", num (s.shimmerPm)).add ("shimmer_lo_hz", num (s.shimmerLoHz)).add ("shimmer_hi_hz", num (s.shimmerHiHz));
        space.add ("window_frames", num (s.windowFrames)).add ("mono_hz", num (s.monoHz)).add ("return_mdb", num (s.returnMdb));
        space.add ("inserts", inserts (s.inserts, mix));

        const auto& e = mix.echo;
        Json echo = Json::object();
        echo.add ("steps", num (e.steps)).add ("feedback_pm", num (e.feedbackPm)).add ("lp_hz", num (e.lpHz)).add ("hp_hz", num (e.hpHz));
        echo.add ("drive_pm", num (e.drivePm)).add ("wow_ppm", num (e.wowPpm)).add ("wow_mhz", num (e.wowMhz));
        echo.add ("flutter_ppm", num (e.flutterPpm)).add ("flutter_mhz", num (e.flutterMhz));
        echo.add ("phases_mrad", Json::ints ({ e.phasesMrad[0], e.phasesMrad[1] })).add ("return_mdb", num (e.returnMdb));
        echo.add ("inserts", inserts (e.inserts, mix));

        const auto& r = mix.resonator;
        Json resonator = Json::object();
        resonator.add ("tones_mhz", Json::ints (r.tonesMhz)).add ("tau_ms", num (r.tauMs)).add ("send_hp_hz", num (r.sendHpHz));
        resonator.add ("return_mdb", num (r.returnMdb)).add ("inserts", inserts (r.inserts, mix));

        Json returns = Json::object();
        returns.add ("space", std::move (space)).add ("echo", std::move (echo)).add ("resonator", std::move (resonator));

        Json busEq = Json::object();
        busEq.add ("vox", inserts (mix.busEq[0], mix)).add ("novox", inserts (mix.busEq[1], mix));

        const auto& m = mix.master[0]; // the shared settings; the parser fills both entries with them
        Json master = Json::object();
        master.add ("lowcut_hz", num (m.lowcutHz)).add ("lowcut_order", num (m.lowcutOrder)).add ("mono_below_hz", num (m.monoBelowHz));
        master.add ("glue", Json::object()
                .add ("ratio_pm", num (m.glueRatioPm))
                .add ("knee_mdb", num (m.glueKneeMdb))
                .add ("attack_us", num (m.glueAttackUs))
                .add ("release_fast_us", num (m.glueReleaseFastUs))
                .add ("release_slow_us", num (m.glueReleaseSlowUs)));
        master.add ("vox", Json::object().add ("glue_threshold_mdb", num (mix.master[0].glueThresholdMdb)).add ("drive_mdb", num (mix.master[0].driveMdb)));
        master.add ("novox", Json::object().add ("glue_threshold_mdb", num (mix.master[1].glueThresholdMdb)).add ("drive_mdb", num (mix.master[1].driveMdb)));
        master.add ("clip", Json::object().add ("knee_mdb", num (m.clipKneeMdb)).add ("ceiling_mdb", num (m.clipCeilingMdb)));
        master.add ("limiter", Json::object()
                .add ("ceiling_mdb", num (m.limiterCeilingMdb))
                .add ("lookahead_us", num (m.limiterLookaheadUs))
                .add ("hold_us", num (m.limiterHoldUs))
                .add ("release_us", num (m.limiterReleaseUs)));
        master.add ("dc_hz", num (m.dcHz));

        Json reference = Json::object();
        static constexpr std::array<std::string_view, 2> variants { "vox", "novox" };
        for (std::size_t v = 0; v < 2; ++v)
        {
            const auto& ref = mix.reference[v];
            reference.add (std::string (variants[v]), Json::object()
                    .add ("pre_master_lufs_mlu", num (ref.preMasterLufsMlu))
                    .add ("club_lufs_mlu", num (ref.clubLufsMlu))
                    .add ("channel_true_peak_mdb", Json::ints (ref.channelTruePeakMdb)));
        }

        Json top = Json::object();
        top.add ("kit", str (mix.kit)).add ("content_version", num (mix.contentVersion)).add ("channels", std::move (channels));
        top.add ("groups", std::move (groups)).add ("returns", std::move (returns)).add ("bus_eq", std::move (busEq));
        top.add ("master", std::move (master)).add ("reference", std::move (reference));
        return top;
    }

    std::string kitManifestJson (const KitManifest& kit)
    {
        return dump (kitManifestToJson (kit));
    }

    std::string mixManifestJson (const MixConfig& mix)
    {
        return dump (mixManifestToJson (mix));
    }
}
