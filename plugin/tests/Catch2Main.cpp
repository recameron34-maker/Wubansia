// Every .cpp in plugin/tests is compiled into WubansiaPluginTests (see plugin/CMakeLists.txt).

#include <catch2/catch_session.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char* argv[])
{
    // JUCE's MessageManager, which the APVTS, editors and timers need.
    juce::ScopedJuceInitialiser_GUI gui;

    return Catch::Session().run (argc, argv);
}
