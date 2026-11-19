#pragma once

// The editor's repaint requests (ADR 0032): every panel asks for its repaints through repaintArea, so a test can see
// that a tick repaints only what changed and never the whole face. Message thread only; recording is off unless a
// test starts it.

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace wubansia::editor
{
    struct RepaintRequest
    {
        const juce::Component* component = nullptr;
        juce::Rectangle<int> area; // in the component's coordinates
    };

    class RepaintLedger
    {
    public:
        static void start(); // records from now on (tests)
        static std::vector<RepaintRequest> stop(); // what was asked since start, and stops recording
        static void note (const juce::Component&, juce::Rectangle<int> area);
    };

    // Repaints area of component (its whole bounds when area is empty), through the ledger.
    void repaintArea (juce::Component&, juce::Rectangle<int> area = {});
}
