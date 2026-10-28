#include "SineVoices.h"

namespace wubansia
{
    void SineVoices::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void SineVoices::reset() noexcept
    {
        voices.fill (Voice {});
    }

    void SineVoices::handleMidi (const juce::uint8*, int) noexcept {}

    void SineVoices::render (juce::AudioBuffer<float>&, int, int) noexcept {}

    int SineVoices::getNumActiveVoices() const noexcept
    {
        return 0;
    }

    void SineVoices::noteOn (int, float) noexcept {}

    void SineVoices::noteOff (int) noexcept {}
}
