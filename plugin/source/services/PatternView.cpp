#include "services/PatternView.h"

namespace wubansia
{
    std::vector<PatternNote> PatternView::notesIn (engine::Lane, std::int32_t, std::int32_t) const { return {}; }

    juce::String PatternView::chordAt (std::int32_t) const { return {}; }

    std::shared_ptr<const PatternView> patternOf (const KitCatalog::Entry&, engine::PlayMode, engine::Variant)
    {
        return std::make_shared<const PatternView>();
    }
}
