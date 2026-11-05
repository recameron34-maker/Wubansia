#include "Parameters.h"

#include <cmath>
#include <string_view>

namespace wubansia::Parameters
{
    namespace
    {
        // The lane parameters follow engine::Lane, and the A/B switches engine::AbSwitch: the IDs are
        // "<lane>_level", "<lane>_mute" and "<switch>_ab".
        constexpr bool idIs (const char* id, std::string_view stem, std::string_view suffix)
        {
            const std::string_view s (id);
            return s.size() == stem.size() + suffix.size() && s.substr (0, stem.size()) == stem && s.substr (stem.size()) == suffix;
        }

        constexpr bool idsFollowTheEngine()
        {
            for (std::size_t i = 0; i < ParameterIds::numLanes; ++i)
                if (! idIs (ParameterIds::laneLevel[i], engine::kLaneIds[i], "_level") || ! idIs (ParameterIds::laneMute[i], engine::kLaneIds[i], "_mute"))
                    return false;
            for (std::size_t i = 0; i < ParameterIds::ab.size(); ++i)
                if (! idIs (ParameterIds::ab[i], engine::kAbSwitchIds[i], "_ab"))
                    return false;
            return true;
        }

        static_assert (ParameterIds::numLanes == engine::kLanes);
        static_assert (ParameterIds::ab.size() == engine::kAbSwitches);
        static_assert (idsFollowTheEngine());

        // The names hosts show. Free to change; the IDs are not.
        constexpr std::array<const char*, ParameterIds::numLanes> laneNames {
            "Kick", "Snare", "Hats", "Perc", "Bass", "Lead", "Chords", "Vox", "Accents", "FX"
        };
        constexpr std::array<const char*, ParameterIds::ab.size()> abNames { "Kick A/B", "Bass A/B", "Stab A/B" };
        constexpr std::array<int, ParameterIds::ab.size()> abDefaults { 1, 1, 0 }; // B, B, A: what the club loops play

        juce::String limited (const juce::String& text, int maximumLength)
        {
            return maximumLength > 0 ? text.substring (0, maximumLength) : text;
        }

        juce::AudioParameterFloatAttributes levelAttributes()
        {
            return juce::AudioParameterFloatAttributes()
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float db, int length) { return levelText (db, length); })
                .withValueFromStringFunction ([] (const juce::String& text) { return levelFromText (text); });
        }

        juce::AudioParameterFloatAttributes percentAttributes()
        {
            return juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction ([] (float percent, int length) { return percentText (percent, length); })
                .withValueFromStringFunction ([] (const juce::String& text) { return percentFromText (text); });
        }

        std::atomic<float>* resolve (juce::AudioProcessorValueTreeState& state, const char* id)
        {
            if (auto* raw = state.getRawParameterValue (id))
                return raw;

            // createLayout () always has it. Never hand the audio thread a null pointer, though.
            jassertfalse;
            static std::atomic<float> zero { 0.0f };
            return &zero;
        }

        bool on (const std::atomic<float>* raw) noexcept { return raw->load (std::memory_order_relaxed) >= 0.5f; }

        float macro (const std::atomic<float>* raw) noexcept
        {
            const auto percent = raw->load (std::memory_order_relaxed);
            return percent > 0.0f ? std::min (percent, maxMacroPercent) / 100.0f : 0.0f; // NaN gives 0
        }
    }

    juce::String levelText (float db, int maximumLength)
    {
        const auto tenths = std::floor (db * 10.0f + 0.5f);
        if (! (tenths > minDb * 10.0f)) // the bottom of the range, and NaN
            return limited ("-inf", maximumLength);
        if (juce::exactlyEqual (tenths, 0.0f))
            return limited ("0.0", maximumLength);
        return limited ((tenths > 0.0f ? "+" : "") + juce::String (tenths / 10.0f, 1), maximumLength);
    }

    float levelFromText (const juce::String& text)
    {
        const auto trimmed = text.trim().toLowerCase();
        if (trimmed.startsWith ("-inf") || trimmed.startsWith ("off"))
            return minDb;
        return juce::jlimit (minDb, maxDb, trimmed.getFloatValue());
    }

    juce::String percentText (float percent, int maximumLength)
    {
        const auto rounded = std::floor (percent + 0.5f);
        return limited (juce::String (rounded > 0.0f ? static_cast<int> (rounded) : 0), maximumLength);
    }

    float percentFromText (const juce::String& text)
    {
        return juce::jlimit (0.0f, maxMacroPercent, text.trim().getFloatValue());
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using juce::ParameterID;
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // M0's, unchanged (version hint 1, JUCE's default text).
        layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { ParameterIds::masterGain, 1 },
            "Gain",
            juce::NormalisableRange<float> (minDb, maxDb),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { ParameterIds::playMode, 2 },
            "Play Mode",
            juce::StringArray { "Off", "Loop", "Song" }, // engine::PlayMode's order
            1));

        layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { ParameterIds::voxSynth, 2 }, "Vox Synth", true));

        for (std::size_t i = 0; i < ParameterIds::ab.size(); ++i)
            layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { ParameterIds::ab[i], 2 },
                abNames[i],
                juce::StringArray { "A", "B" },
                abDefaults[i]));

        for (std::size_t lane = 0; lane < ParameterIds::numLanes; ++lane)
        {
            const juce::String name (laneNames[lane]);
            layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { ParameterIds::laneLevel[lane], 2 },
                name + " Level",
                juce::NormalisableRange<float> (minDb, maxDb),
                0.0f,
                levelAttributes()));
            layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { ParameterIds::laneMute[lane], 2 }, name + " Mute", false));
        }

        for (const auto& [id, name] : { std::pair { ParameterIds::space, "Space" }, std::pair { ParameterIds::echo, "Echo" }, std::pair { ParameterIds::pump, "Pump" } })
            layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { id, 2 },
                name,
                juce::NormalisableRange<float> (0.0f, maxMacroPercent),
                defaultMacroPercent,
                percentAttributes()));

        layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { ParameterIds::clubMaster, 2 }, "Club Master", true));
        return layout;
    }

    RawParameters::RawParameters (juce::AudioProcessorValueTreeState& state)
        : masterGain (resolve (state, ParameterIds::masterGain)),
          playMode (resolve (state, ParameterIds::playMode)),
          voxSynth (resolve (state, ParameterIds::voxSynth)),
          space (resolve (state, ParameterIds::space)),
          echo (resolve (state, ParameterIds::echo)),
          pump (resolve (state, ParameterIds::pump)),
          clubMaster (resolve (state, ParameterIds::clubMaster))
    {
        for (std::size_t i = 0; i < ab.size(); ++i)
            ab[i] = resolve (state, ParameterIds::ab[i]);
        for (std::size_t lane = 0; lane < ParameterIds::numLanes; ++lane)
        {
            laneLevel[lane] = resolve (state, ParameterIds::laneLevel[lane]);
            laneMute[lane] = resolve (state, ParameterIds::laneMute[lane]);
        }
    }

    void RawParameters::read (engine::Controls& controls) const noexcept
    {
        const auto mode = playMode->load (std::memory_order_relaxed);
        controls.mode = mode >= 1.5f ? engine::PlayMode::song : (mode >= 0.5f ? engine::PlayMode::loop : engine::PlayMode::off);
        controls.variant = on (voxSynth) ? engine::Variant::vox : engine::Variant::novox;

        for (std::size_t i = 0; i < ab.size(); ++i)
            controls.ab[i] = on (ab[i]) ? 1 : 0;

        for (std::size_t lane = 0; lane < ParameterIds::numLanes; ++lane)
            controls.laneGain[lane] = on (laneMute[lane]) ? 0.0f : juce::Decibels::decibelsToGain (laneLevel[lane]->load (std::memory_order_relaxed), minDb);

        controls.space = macro (space);
        controls.echo = macro (echo);
        controls.pump = macro (pump);
        controls.club = on (clubMaster);
    }
}
