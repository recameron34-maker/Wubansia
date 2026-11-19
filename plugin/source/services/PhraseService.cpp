#include "services/PhraseService.h"

#include "content/UserSettings.h"

namespace wubansia
{
    // Stubs (step `phrases`, part B, commit 1): the tests fail until commit 3.
    PhraseService::PhraseService (engine::Engine& e, std::function<const KitCatalog&()> c) : engine (e), catalog (std::move (c)) {}
    PhraseService::~PhraseService() = default;
    void PhraseService::show (const juce::String&) {}
    std::shared_ptr<const PhraseList> PhraseService::list() const { return {}; }
    bool PhraseService::more (const juce::String&, const juce::String&) { return false; }
    bool PhraseService::moreInFlight() const { return false; }
    PhraseService::MoreResult PhraseService::lastMore() const { return {}; }
    std::uint32_t PhraseService::audition (const juce::String&, const juce::String&) { return 0; }
    void PhraseService::stop() {}
    PhraseAuditionState PhraseService::status() const { return {}; }
    util::Result<PhraseService::File> PhraseService::file (const juce::String&, const juce::String&) const
    {
        return util::Result<File>::failure ("not built yet");
    }
    void PhraseService::setKept (const juce::String&, const juce::String&, bool) {}
    void PhraseService::markSeen (const juce::String&, const juce::String&) {}
}
