#pragma once

#include <array>
#include <cstddef>

namespace wubansia::ParameterIds
{
    // Parameter IDs are PERMANENT once released (CLAUDE.md, docs/decisions/0002-plugin-identity.md):
    // rename the label, never the ID, and append new IDs at the end. plugin/tests/ParameterTests.cpp
    // lists every ID; Parameters.cpp defines each parameter's name, range and default.

    // M0, version hint 1: the final output trim.
    inline constexpr const char* masterGain = "master_gain";

    // v0.1, version hint 2, in the host's order after master_gain (ADR 0018).
    inline constexpr const char* playMode = "play_mode"; // Off, Loop, Song: engine::PlayMode
    inline constexpr const char* voxSynth = "vox_synth"; // on: engine::Variant::vox
    inline constexpr const char* kickAb = "kick_ab"; // A or B, by engine::AbSwitch
    inline constexpr const char* bassAb = "bass_ab";
    inline constexpr const char* stabAb = "stab_ab";

    inline constexpr std::size_t numLanes = 10; // engine::kLanes; Parameters.cpp checks they agree

    // By engine::Lane: kick, snare, hats, perc, bass, lead, chords, vox, accents, fx.
    inline constexpr std::array<const char*, numLanes> laneLevel {
        "kick_level", "snare_level", "hats_level", "perc_level", "bass_level",
        "lead_level", "chords_level", "vox_level", "accents_level", "fx_level"
    };
    inline constexpr std::array<const char*, numLanes> laneMute {
        "kick_mute", "snare_mute", "hats_mute", "perc_mute", "bass_mute",
        "lead_mute", "chords_mute", "vox_mute", "accents_mute", "fx_mute"
    };

    inline constexpr const char* space = "space"; // the macros, 0-200 %
    inline constexpr const char* echo = "echo";
    inline constexpr const char* pump = "pump";
    inline constexpr const char* clubMaster = "club_master";

    // By engine::AbSwitch: kick, bass, stab.
    inline constexpr std::array<const char*, 3> ab { kickAb, bassAb, stabAb };

    // Every ID, in the host's order: master_gain, play_mode, vox_synth, the three A/B switches, each
    // lane's level and mute (lane by lane), the three macros and club_master.
    inline constexpr std::array<const char*, 30> all {
        masterGain,
        playMode,
        voxSynth,
        kickAb, bassAb, stabAb,
        laneLevel[0], laneMute[0],
        laneLevel[1], laneMute[1],
        laneLevel[2], laneMute[2],
        laneLevel[3], laneMute[3],
        laneLevel[4], laneMute[4],
        laneLevel[5], laneMute[5],
        laneLevel[6], laneMute[6],
        laneLevel[7], laneMute[7],
        laneLevel[8], laneMute[8],
        laneLevel[9], laneMute[9],
        space, echo, pump,
        clubMaster
    };
}
