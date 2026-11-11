#pragma once

// The MIDI channel map, the C++ copy of tools/sound/channels.py (MIDI v2, ADR 0019,
// docs/midi-map.md). The numbers here must equal that file's: CHANNELS, KEY_BASE, DRUM_RANGE,
// ONE_KEY_ROLES, ROLE_LANE and TRACKS. A change goes through ADR 0019's owner and lands in both.
//
// Channels here are 1-based, as channels.py and DAWs count them; a status byte's low nibble, and
// Note::channel, are one less. The kits' extras on channels 11-16 are kit data: kit.json's extras
// (KitInfo::extras), not this table.

#include "kit/kit.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace wubansia::engine
{
    // CHANNELS: one role per channel, the same in every kit.
    enum class ChannelRole : std::uint8_t {
        bass, // 1: the bass multisample, A or B by bass_ab
        lead, // 2
        voicing, // 3: the chord voicing for your own synth; Wubansia ignores it
        chords, // 4: the chord sample at its root
        vox, // 5: the vox synth multisample
        accents, // 6: one key per sound from 48
        fx, // 7: one key per sound from 48
        chops, // 8: vox chops, one key per sound from 48
        reserved, // 9
        drums // 10: drums and perc, 35-59
    };
    inline constexpr int kChannelRoles = 10;
    inline constexpr std::array<std::string_view, kChannelRoles> kChannelRoleIds {
        "bass", "lead", "voicing", "chords", "vox", "accents", "fx", "chops", "reserved", "drums"
    };
    inline constexpr std::array<std::uint8_t, kChannelRoles> kRoleChannels { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    inline constexpr std::uint8_t kFirstExtraChannel = 11; // EXTRA_CHANNELS: 11-16, the kit's own
    inline constexpr std::uint8_t kLastExtraChannel = 16;

    inline constexpr int kKeyBaseAccents = 48; // KEY_BASE
    inline constexpr int kKeyBaseFx = 48;
    inline constexpr int kKeyBaseChops = 48;
    inline constexpr int kDrumKeyLo = 35; // DRUM_RANGE, both ends included
    inline constexpr int kDrumKeyHi = 59;

    constexpr int channelOf (ChannelRole role) noexcept { return kRoleChannels[static_cast<std::size_t> (role)]; }

    // The role of a 1-based channel 1-10, or -1 (11-16 are the kit's extras; anything else is no channel).
    constexpr int roleOf (int channel) noexcept { return channel >= 1 && channel <= kChannelRoles ? channel - 1 : -1; }

    // ONE_KEY_ROLES: on these channels a key names a sound; on the others a key is a pitch.
    constexpr bool isOneKeyRole (ChannelRole role) noexcept
    {
        return role == ChannelRole::accents || role == ChannelRole::fx || role == ChannelRole::chops || role == ChannelRole::drums;
    }

    // ROLE_LANE: the lane a role's notes play on. Drums take their track's lane (kTracks), and reserved
    // has none: -1 for both.
    constexpr int roleLane (ChannelRole role) noexcept
    {
        switch (role)
        {
            case ChannelRole::bass: return static_cast<int> (Lane::bass);
            case ChannelRole::lead: return static_cast<int> (Lane::lead);
            case ChannelRole::voicing: return static_cast<int> (Lane::chords);
            case ChannelRole::chords: return static_cast<int> (Lane::chords);
            case ChannelRole::vox: return static_cast<int> (Lane::vox);
            case ChannelRole::accents: return static_cast<int> (Lane::accents);
            case ChannelRole::fx: return static_cast<int> (Lane::fx);
            case ChannelRole::chops: return static_cast<int> (Lane::vox);
            case ChannelRole::reserved: return -1;
            case ChannelRole::drums: return -1;
        }
        return -1;
    }

    // TRACKS: the track names every kit's files may hold, with their role and lane. A kit's extras add
    // their own names (KitInfo::extras).
    struct TrackRole
    {
        std::string_view name;
        ChannelRole role;
        Lane lane;
    };

    inline constexpr std::array<TrackRole, 15> kTracks { {
        { "Kick", ChannelRole::drums, Lane::kick },
        { "Rumble", ChannelRole::drums, Lane::kick }, // FUT04's rumble, drum note 35, gated by its note's length
        { "Snare", ChannelRole::drums, Lane::snare },
        { "Clap", ChannelRole::drums, Lane::snare },
        { "Snare/Clap", ChannelRole::drums, Lane::snare },
        { "Hats", ChannelRole::drums, Lane::hats },
        { "Perc", ChannelRole::drums, Lane::perc },
        { "Bass", ChannelRole::bass, Lane::bass },
        { "Lead", ChannelRole::lead, Lane::lead },
        { "Chords", ChannelRole::chords, Lane::chords },
        { "Chord voicing", ChannelRole::voicing, Lane::chords },
        { "Vox", ChannelRole::vox, Lane::vox },
        { "Vox chops", ChannelRole::chops, Lane::vox },
        { "Accents", ChannelRole::accents, Lane::accents },
        { "FX", ChannelRole::fx, Lane::fx },
    } };

    // The lane a track of a kit's files plays on (channels.py track_lane), or -1 for a name no file
    // may carry.
    inline int trackLane (const KitInfo& kit, std::string_view name) noexcept
    {
        for (const auto& t : kTracks)
            if (t.name == name)
                return static_cast<int> (t.lane);
        for (const auto& e : kit.extras)
            if (e.name == name)
                return static_cast<int> (e.lane);
        return -1;
    }
}
