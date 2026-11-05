#pragma once

// The mix manifest, content/plugin/<kit>/<KIT>_mix.json, as the engine holds it: parseMixManifest
// (kit/manifest.h) fills a MixConfig, the Kit keeps it, and the Mixer plays it. Every field's unit,
// range and meaning is in docs/plugin/manifest.md; the names here follow the JSON's (gain_mdb is
// gainMdb). Integers only, as in the file: milli-dB (Mdb), per mille (Pm), Hz, mHz, microseconds
// (Us), milliseconds (Ms), frames at 48 kHz (Frames), milli-LU (Mlu).
//
// The topology is fixed. Each channel runs its inserts, then the lane level and mute, then goes to
// its group (drums or music) or straight to the mix bus, and post-fader to the three sends. Each
// group runs its inserts into the mix bus. Each return (Space, Echo, Resonator) runs its effect and
// its inserts into the mix bus. The mix bus runs the variant's bus EQ, then the variant's club
// master.
//
// Lanes, variants and A/B switches are plain integers here (a Lane, Variant or AbSwitch value), so
// this header needs nothing from kit/kit.h or engine.h, which include it.
//
// After a parse, every reference is resolved and checked: channel indices are in range, a level
// duck's key channel exists, and every Insert's params hold the alternative its type names (the
// table at Insert).

#include "ids.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wubansia::engine
{
    inline constexpr int kMaxMixChannels = 24;

    // The three sends, in sends_pm's order.
    enum class Send : std::uint8_t {
        space,
        echo,
        resonator
    };
    inline constexpr int kSends = 3;

    enum class Group : std::uint8_t {
        none, // straight to the mix bus
        drums,
        music
    };
    inline constexpr std::array<std::string_view, 3> kGroupIds { "none", "drums", "music" };

    // The closed insert set. A manifest naming any other type is refused.
    enum class InsertType : std::uint8_t {
        gain,
        pan,
        width,
        peak,
        lowShelf,
        highShelf,
        highpass,
        lowpass,
        ccFilter,
        monoBelow,
        clip,
        drive,
        duck,
        comp,
        parallel
    };
    inline constexpr std::array<std::string_view, 15> kInsertTypeIds {
        "gain", "pan", "width", "peak", "low_shelf", "high_shelf", "highpass", "lowpass", "cc_filter",
        "mono_below", "clip", "drive", "duck", "comp", "parallel"
    };

    enum class FilterResponse : std::uint8_t {
        lowpass,
        bandpass,
        highpass
    };
    inline constexpr std::array<std::string_view, 3> kFilterResponseIds { "lowpass", "bandpass", "highpass" };

    enum class DuckMode : std::uint8_t {
        onset, // phrase.pump_db on scheduled key onsets
        level // sends.duck's detector on a key channel's level, with a fixed top
    };
    inline constexpr std::array<std::string_view, 2> kDuckModeIds { "onset", "level" };

    // A band an insert works in, split off with LR4 crossovers: [loHz, hiHz). 0 leaves that side
    // open, so {0, 0} is the whole signal (no split), {0, 120} is under 120 Hz, {500, 0} above 500 Hz.
    struct Band
    {
        std::int32_t loHz = 0;
        std::int32_t hiHz = 0;

        friend bool operator== (const Band&, const Band&) = default;
    };

    // When an insert is active: only for one variant, only for one side of an A/B switch, or both
    // (both must hold). Inactive inserts are bypassed; a change crossfades over 10 ms.
    struct When
    {
        std::int8_t variant = -1; // -1 any, else a Variant value (0 vox, 1 novox)
        std::int8_t ab = -1; // -1 none, else an AbSwitch value (0 kick, 1 bass, 2 stab)
        std::int8_t is = 0; // with ab: 0 when the switch is on A, 1 on B

        bool always() const noexcept { return variant < 0 && ab < 0; }

        friend bool operator== (const When&, const When&) = default;
    };

    // Each insert type's parameters (the JSON fields minus "type" and "when").

    struct GainParams // gain
    {
        std::int32_t gainMdb = 0;

        friend bool operator== (const GainParams&, const GainParams&) = default;
    };

    struct PanParams // pan: equal power, unity in the centre (phrase.pan)
    {
        std::int32_t panPm = 0; // -1000 left .. 1000 right

        friend bool operator== (const PanParams&, const PanParams&) = default;
    };

    struct WidthParams // width: the side channel times widthPm / 1000
    {
        std::int32_t widthPm = 1000;

        friend bool operator== (const WidthParams&, const WidthParams&) = default;
    };

    struct EqParams // peak, low_shelf, high_shelf: RBJ cookbook biquads
    {
        std::int32_t freqHz = 1000;
        std::int32_t gainMdb = 0;
        std::int32_t qPm = 707; // Q x 1000

        friend bool operator== (const EqParams&, const EqParams&) = default;
    };

    struct PassParams // highpass, lowpass
    {
        std::int32_t freqHz = 1000;
        std::int32_t order = 2; // Butterworth order 1-8 (dsp48.bfilt)
        std::int32_t qPm = 0; // 0: Butterworth; else order is 2 and this is the RBJ biquad's Q x 1000

        friend bool operator== (const PassParams&, const PassParams&) = default;
    };

    // cc_filter: a TPT state-variable filter (dsp48.svf_tpt) whose cutoff follows CC74 and whose Q
    // follows CC71, on the MIDI channels that reach this mix channel (Kit::ccMixChannels):
    //     cutoff (v) = cc74LoHz x (cc74HiHz / cc74LoHz) ^ (v / 127)   Hz
    //     Q (v)      = (cc71LoPm + (cc71HiPm - cc71LoPm) x v / 127) / 1000
    // The CC value is interpolated linearly between sequenced points, then mapped. Until a CC
    // arrives (and after a reset) the defaults hold.
    struct CcFilterParams
    {
        FilterResponse response = FilterResponse::lowpass;
        std::int32_t cc74LoHz = 20;
        std::int32_t cc74HiHz = 20000;
        std::int32_t cc74Default = 127; // 0-127
        std::int32_t cc71LoPm = 707;
        std::int32_t cc71HiPm = 707;
        std::int32_t cc71Default = 0; // 0-127
        Band band; // where the filter works; the rest passes dry

        friend bool operator== (const CcFilterParams&, const CcFilterParams&) = default;
    };

    struct MonoBelowParams // mono_below: the side high-passed at freqHz (dsp48.mono_below)
    {
        std::int32_t freqHz = 150;
        std::int32_t order = 8; // Butterworth order 1-8 of the side's high-pass

        friend bool operator== (const MonoBelowParams&, const MonoBelowParams&) = default;
    };

    struct ClipParams // clip: master.soft_clip with ADAA, 4x oversampled, at an absolute ceiling
    {
        std::int32_t ceilingMdb = 0; // dBFS x 1000
        std::int32_t kneeMdb = 1500; // linear up to this far under the ceiling

        friend bool operator== (const ClipParams&, const ClipParams&) = default;
    };

    // drive: dsp48.tanh_adaa at 4x, (tanh (d x + b) - tanh (b)) / tanh (d), then outputMdb.
    struct DriveParams
    {
        std::int32_t drivePm = 1000; // d x 1000
        std::int32_t biasPm = 0; // b x 1000
        std::int32_t outputMdb = 0;

        friend bool operator== (const DriveParams&, const DriveParams&) = default;
    };

    // duck. A negative depth lifts instead (FUT02's clap snap). pump: the Pump macro scales the depth.
    // - onset: phrase.pump_db. At each key onset (a note of an instrument whose duck_key is set, on
    //   a lane in keyLanes, with velocity >= minVelocity), the gain goes down depthMdb over rampUs,
    //   holds holdUs, then comes back over releaseUs on a raised cosine; overlapping ducks take the
    //   deepest. preUs starts the ramp that much before the onset: the timeline places the keys early,
    //   so every onset duck keyed on one lane must share one preUs. Live keys (MIDI in) can't be
    //   anticipated and duck at their onset.
    // - level: sends.duck. The key channel's signal (after its inserts, before its lane level and
    //   mute) is measured in 64-sample RMS blocks (at 48 kHz), smoothed in dB with attackUs and
    //   releaseUs, and mapped from (topMdb - rangeMdb) .. topMdb onto 0 .. depthMdb of reduction.
    struct DuckParams
    {
        DuckMode mode = DuckMode::onset;

        // onset
        std::uint16_t keyLanes = 0; // bit i: Lane i keys this duck
        std::int32_t minVelocity = 1; // 1-127
        std::int32_t rampUs = 0;
        std::int32_t holdUs = 0;
        std::int32_t releaseUs = 0; // also the level duck's release
        std::int32_t preUs = 0;

        // level
        std::int32_t keyChannel = -1; // index into MixConfig::channels
        std::int32_t rangeMdb = 0;
        std::int32_t topMdb = 0; // dBFS x 1000: the key's loudest block in the reference loop
        std::int32_t attackUs = 0;

        // both
        std::int32_t depthMdb = 0;
        Band band; // where the duck works; the rest passes untouched
        bool pump = false;

        friend bool operator== (const DuckParams&, const DuckParams&) = default;
    };

    // comp: a feed-forward peak compressor with a soft knee.
    struct CompParams
    {
        std::int32_t thresholdMdb = 0;
        std::int32_t ratioPm = 2000; // 2:1 is 2000
        std::int32_t kneeMdb = 0;
        std::int32_t attackUs = 10000;
        std::int32_t releaseUs = 100000;
        std::int32_t makeupMdb = 0;

        friend bool operator== (const CompParams&, const CompParams&) = default;
    };

    struct Insert;

    // parallel: the input runs through chain, which is mixed back at levelMdb over the dry signal
    // (FUT04's drum-bus grit). A chain holds no parallel insert.
    struct ParallelParams
    {
        std::int32_t levelMdb = 0;
        std::vector<Insert> chain;

        friend bool operator== (const ParallelParams&, const ParallelParams&) = default;
    };

    // One insert. params holds the alternative for its type:
    //     gain GainParams; pan PanParams; width WidthParams;
    //     peak, low_shelf, high_shelf EqParams; highpass, lowpass PassParams;
    //     cc_filter CcFilterParams; mono_below MonoBelowParams; clip ClipParams; drive DriveParams;
    //     duck DuckParams; comp CompParams; parallel ParallelParams.
    struct Insert
    {
        InsertType type = InsertType::gain;
        When when;
        std::variant<GainParams, PanParams, WidthParams, EqParams, PassParams, CcFilterParams, MonoBelowParams,
            ClipParams, DriveParams, DuckParams, CompParams, ParallelParams>
            params;

        friend bool operator== (const Insert&, const Insert&) = default;
    };

    struct Channel
    {
        std::string id; // unique in the manifest: [a-z0-9_]+; kit.json names channels by it
        std::uint8_t lane = 0; // a Lane value: the lane level and mute that apply
        Group group = Group::none;
        std::vector<Insert> inserts;
        std::array<std::int32_t, kSends> sendsPm {}; // post-fader send levels, by Send

        friend bool operator== (const Channel&, const Channel&) = default;
    };

    struct GroupConfig
    {
        std::vector<Insert> inserts;

        friend bool operator== (const GroupConfig&, const GroupConfig&) = default;
    };

    // Space: sends.space. An 8-line Hadamard FDN at the exported delays (scaled to the host rate)
    // and input signs, one-pole damping, a low band with its own T60, a predelay, the send's
    // high-pass, the shimmer, and mono under monoHz.
    struct SpaceConfig
    {
        std::array<std::int32_t, 8> delaysFrames {}; // at 48 kHz
        std::array<std::int32_t, 8> signs {}; // each -1 or 1
        std::int32_t t60Ms = 0;
        std::int32_t t60LowMs = 0; // under lowHz; 0: the same as t60Ms
        std::int32_t lowHz = 0;
        std::int32_t dampHz = 0;
        std::int32_t predelayUs = 0;
        std::int32_t sendHpHz = 0;
        std::int32_t sendOrder = 4;
        std::int32_t shimmerPm = 0; // 0: no shimmer
        std::int32_t shimmerLoHz = 0;
        std::int32_t shimmerHiHz = 0;
        std::int32_t windowFrames = 0; // the octave shifter's window, at 48 kHz
        std::int32_t monoHz = 0; // 0: no mono fold
        std::int32_t returnMdb = 0;
        std::vector<Insert> inserts;

        friend bool operator== (const SpaceConfig&, const SpaceConfig&) = default;
    };

    // Echo: sends.echo. A tape ping-pong of `steps` 16ths at the host tempo.
    struct EchoConfig
    {
        std::int32_t steps = 3; // 3: a dotted 8th
        std::int32_t feedbackPm = 0;
        std::int32_t lpHz = 0;
        std::int32_t hpHz = 0;
        std::int32_t drivePm = 1000; // the feedback's tanh (drive u) / drive, drive x 1000
        std::int32_t wowPpm = 0; // the delay's wobble, as a fraction of it, x 1e6
        std::int32_t wowMhz = 0;
        std::int32_t flutterPpm = 0;
        std::int32_t flutterMhz = 0;
        std::array<std::int32_t, 2> phasesMrad {}; // wow, flutter
        std::int32_t returnMdb = 0;
        std::vector<Insert> inserts;

        friend bool operator== (const EchoConfig&, const EchoConfig&) = default;
    };

    // Resonator: sends.resonator. One 2-pole resonator per tone; no tones, no resonator.
    struct ResonatorConfig
    {
        std::vector<std::int32_t> tonesMhz;
        std::int32_t tauMs = 0;
        std::int32_t sendHpHz = 0;
        std::int32_t returnMdb = 0;
        std::vector<Insert> inserts;

        friend bool operator== (const ResonatorConfig&, const ResonatorConfig&) = default;
    };

    // The club master for one variant (master.py's chain; the limiter holds looking back only, and
    // a causal DC blocker replaces the zero-phase clean). The JSON gives the shared settings once
    // and the solved glue threshold and drive per variant; the parser fills both entries.
    struct MasterConfig
    {
        std::int32_t lowcutHz = 25;
        std::int32_t lowcutOrder = 4;
        std::int32_t monoBelowHz = 150;

        std::int32_t glueRatioPm = 2000;
        std::int32_t glueKneeMdb = 6000;
        std::int32_t glueAttackUs = 30000;
        std::int32_t glueReleaseFastUs = 150000;
        std::int32_t glueReleaseSlowUs = 1500000;
        std::int32_t glueThresholdMdb = 0; // per variant
        std::int32_t driveMdb = 0; // per variant

        std::int32_t clipKneeMdb = 1500;
        std::int32_t clipCeilingMdb = -520;

        std::int32_t limiterCeilingMdb = -1020;
        std::int32_t limiterLookaheadUs = 5000;
        std::int32_t limiterHoldUs = 60000;
        std::int32_t limiterReleaseUs = 80000;

        std::int32_t dcHz = 5;

        friend bool operator== (const MasterConfig&, const MasterConfig&) = default;
    };

    // What the reference loops measured, per variant, for the fidelity checks (P12). The engine
    // doesn't play these.
    struct Reference
    {
        std::int32_t preMasterLufsMlu = 0;
        std::int32_t clubLufsMlu = 0;
        std::vector<std::int32_t> channelTruePeakMdb; // one per channel, in channels' order

        friend bool operator== (const Reference&, const Reference&) = default;
    };

    struct MixConfig
    {
        std::string kit; // the kit's id; it must match kit.json's
        std::int32_t contentVersion = 0; // it must match kit.json's
        std::vector<Channel> channels; // at most kMaxMixChannels
        GroupConfig drums;
        GroupConfig music;
        SpaceConfig space;
        EchoConfig echo;
        ResonatorConfig resonator;
        std::array<std::vector<Insert>, 2> busEq; // by Variant: [0] vox, [1] novox
        std::array<MasterConfig, 2> master; // by Variant
        std::array<Reference, 2> reference; // by Variant

        // The channel with that id, or -1.
        int channelIndex (std::string_view id) const noexcept
        {
            for (std::size_t i = 0; i < channels.size(); ++i)
                if (channels[i].id == id)
                    return static_cast<int> (i);
            return -1;
        }

        friend bool operator== (const MixConfig&, const MixConfig&) = default;
    };
}
