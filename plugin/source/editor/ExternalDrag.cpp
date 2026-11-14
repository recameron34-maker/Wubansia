#include "ExternalDrag.h"

#include "dragout/DragFiles.h"

namespace wubansia::editor
{
    bool handOverDrag (const juce::File& file, juce::Component& source, PerformExternalDrag perform)
    {
        return perform != nullptr && perform ({ dragout::externalDragPath (file) }, false, &source, nullptr);
    }

    bool startExternalDrag (const juce::File& file, juce::Component& source)
    {
        return handOverDrag (file, source, &juce::DragAndDropContainer::performExternalDragDropOfFiles);
    }
}
