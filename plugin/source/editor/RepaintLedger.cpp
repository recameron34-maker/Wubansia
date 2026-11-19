#include "editor/RepaintLedger.h"

#include <utility>

namespace wubansia::editor
{
    namespace
    {
        bool recording = false;
        std::vector<RepaintRequest>& requests()
        {
            static std::vector<RepaintRequest> r;
            return r;
        }
    }

    void RepaintLedger::start()
    {
        requests().clear();
        recording = true;
    }

    std::vector<RepaintRequest> RepaintLedger::stop()
    {
        recording = false;
        return std::exchange (requests(), {});
    }

    void RepaintLedger::note (const juce::Component& component, juce::Rectangle<int> area)
    {
        if (recording)
            requests().push_back ({ &component, area });
    }

    void repaintArea (juce::Component& component, juce::Rectangle<int> area)
    {
        if (area.isEmpty())
            area = component.getLocalBounds();
        area = area.getIntersection (component.getLocalBounds());
        if (area.isEmpty())
            return;
        RepaintLedger::note (component, area);
        component.repaint (area);
    }
}
