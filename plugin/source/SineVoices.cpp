#include "SineVoices.h"

#include <algorithm>
#include <cmath>

namespace wubansia
{
    void SineVoices::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        envelopeStep = (float) (1.0 / (0.005 * sampleRate)); // 5 ms attack and release
        reset();
    }

    void SineVoices::reset() noexcept
    {
        voices.fill (Voice {});
    }

    void SineVoices::handleMidi (const juce::uint8* data, int numBytes) noexcept
    {
        if (numBytes < 3) // every message we act on has three bytes; SysEx starts with 0xf0 and is ignored
            return;

        const auto status = data[0] & 0xf0;
        const auto data1 = data[1] & 0x7f;
        const auto data2 = data[2] & 0x7f;

        if (status == 0x90 && data2 > 0)
            noteOn (data1, (float) data2 / 127.0f);
        else if (status == 0x80 || status == 0x90)
            noteOff (data1);
        else if (status == 0xb0 && data1 == 120) // all sound off: silence now
            reset();
        else if (status == 0xb0 && data1 == 123) // all notes off: release
            for (auto& v : voices)
                v.releasing = true;
    }

    void SineVoices::noteOn (int note, float velocity) noexcept
    {
        // The same note retriggers its voice; otherwise take a free voice, or steal the quietest.
        Voice* target = nullptr;

        for (auto& v : voices)
            if (v.note == note)
                target = &v;

        if (target == nullptr)
            for (auto& v : voices)
                if (v.note < 0 && target == nullptr)
                    target = &v;

        if (target == nullptr)
        {
            target = &voices.front();
            for (auto& v : voices)
                if (v.envelope < target->envelope)
                    target = &v;
        }

        const auto wasSilent = target->note < 0;
        target->note = note;
        target->releasing = false;
        target->phaseDelta = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (note) / sampleRate;
        target->targetLevel = velocity / (float) numVoices; // all voices at full level can't clip

        if (wasSilent) // its envelope starts from zero, so it can take the new level at once
            target->level = target->targetLevel;
    }

    void SineVoices::noteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.note == note)
                v.releasing = true;
    }

    void SineVoices::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        const auto numChannels = buffer.getNumChannels();
        const auto endSample = startSample + numSamples;
        const auto levelStep = envelopeStep / (float) numVoices; // full level change in 5 ms

        for (auto& v : voices)
        {
            for (int i = startSample; i < endSample && v.note >= 0; ++i)
            {
                v.envelope = v.releasing ? v.envelope - envelopeStep : std::min (1.0f, v.envelope + envelopeStep);

                if (v.envelope <= 0.0f)
                {
                    v = Voice {};
                    break;
                }

                v.level += juce::jlimit (-levelStep, levelStep, v.targetLevel - v.level);
                const auto sample = (float) std::sin (v.phase) * v.level * v.envelope;

                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.addSample (ch, i, sample);

                v.phase += v.phaseDelta;
                if (v.phase >= juce::MathConstants<double>::twoPi)
                    v.phase -= juce::MathConstants<double>::twoPi;
            }
        }
    }

    int SineVoices::getNumActiveVoices() const noexcept
    {
        return (int) std::count_if (voices.begin(), voices.end(), [] (const Voice& v) { return v.note >= 0; });
    }
}
