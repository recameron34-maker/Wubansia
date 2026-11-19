#include "editor/RepaintLedger.h"

namespace wubansia::editor
{
    void RepaintLedger::start() {}
    std::vector<RepaintRequest> RepaintLedger::stop() { return {}; }
    void RepaintLedger::note (const juce::Component&, juce::Rectangle<int>) {}
    void repaintArea (juce::Component& c, juce::Rectangle<int> area)
    {
        if (area.isEmpty())
            c.repaint();
        else
            c.repaint (area);
    }
}
