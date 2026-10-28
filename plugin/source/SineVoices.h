#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace wubansia
{
    // A fixed bank of sine voices: the M0 placeholder sound, replaced by the engine in M3.
    // It doesn't use juce::Synthesiser, whose processNextBlock takes a lock (juce_Synthesiser.cpp),
    // and every method is safe to call on the audio thread: no allocation, locks or I/O.
    class SineVoices
    {
    public:
        static constexpr int numVoices = 8;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // Raw MIDI bytes. MidiMessageMetadata::getMessage() would allocate for long messages.
        void handleMidi (const juce::uint8* data, int numBytes) noexcept;

        // Adds the voices into the buffer's channels for [startSample, startSample + numSamples).
        void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

        int getNumActiveVoices() const noexcept;

    private:
        struct Voice
        {
            int note = -1; // -1 means free
            bool releasing = false;
            double phase = 0.0; // radians
            double phaseDelta = 0.0;
            float level = 0.0f; // velocity, scaled so all voices together can't clip
            float envelope = 0.0f; // linear attack and release, 0..1
        };

        void noteOn (int note, float velocity) noexcept;
        void noteOff (int note) noexcept;

        std::array<Voice, numVoices> voices {};
        double sampleRate = 44100.0;
        float envelopeStep = 0.0f; // per sample, for 5 ms ramps
    };
}
