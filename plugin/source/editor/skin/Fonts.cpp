#include "editor/skin/Fonts.h"

namespace wubansia::editor
{
    juce::Typeface::Ptr typefaceFor (FaceId) { return nullptr; }

    juce::String familyOf (FaceId) { return {}; }

    juce::Font fontFor (FaceId, float px, float, bool) { return juce::Font (juce::FontOptions (px)); }
}
