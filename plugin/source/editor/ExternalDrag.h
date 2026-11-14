#pragma once

// The OS drag that the MIDI chips (DragChip) and the Samples drawer (ADR 0024) start by default: one file, as
// dragout::externalDragPath gives it. Both components' startDrag is startExternalDrag itself, a plain function, so a
// test can check that each one's default is this function (std::function::target) even where no drag can start: with
// no window, JUCE refuses every OS drag before it reads the file list. handOverDrag, which it calls, takes the JUCE
// call as an argument, so a test can see the list it hands over.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace wubansia::editor
{
    // How an OS drag of files starts: juce::DragAndDropContainer::performExternalDragDropOfFiles, or a test's own.
    using PerformExternalDrag = bool (*) (const juce::StringArray& files, bool canMoveFiles, juce::Component* source, std::function<void()> callback);

    // Hands file to perform as the one file of an OS drag from source: its dragout::externalDragPath (a percent-encoded
    // file:// URL on Linux and the BSDs, the path elsewhere), never to be moved, with no callback. What perform returns.
    bool handOverDrag (const juce::File& file, juce::Component& source, PerformExternalDrag perform);

    // DragChip's and SamplesPanel's default startDrag: handOverDrag through JUCE. Message thread, inside a mouse event.
    bool startExternalDrag (const juce::File& file, juce::Component& source);
}
