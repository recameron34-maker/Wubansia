// PhraseVoice's interface with a silent body, so the tests (written first) build. P6 replaces it.

#include "sampler/phrase_voice.h"

namespace wubansia::engine
{
    bool playsSlices (const Phrase&, std::int32_t, double, PhraseMode) noexcept { return false; }

    void PhraseVoice::prepare (double sampleRate) { rate = sampleRate; }
    void PhraseVoice::reset() noexcept {}
    void PhraseVoice::start (const Kit& kit, const NoteOn& note, int) noexcept
    {
        kit_ = &kit;
        phrase_ = note.phrase;
    }
    void PhraseVoice::continueWith (const NoteOn&, int) noexcept {}
    void PhraseVoice::stop (int, int) noexcept {}
    void PhraseVoice::render (MixBus&, SendBus&, int, const float*) noexcept {}
    bool PhraseVoice::active() const noexcept { return false; }
    double PhraseVoice::readPosition() const noexcept { return -1.0; }
}
