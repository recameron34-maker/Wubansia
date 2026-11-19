#pragma once

// Where the current kit's samples stand, and exactly what to do about it: install them, pull them
// with Git LFS, reinstall the right version, or choose the folder that holds them. It covers the
// lanes while the kit can't play, and opens on demand to change the folder.

#include "Controls.h"
#include "EditorModel.h"

#include <functional>

namespace wubansia::editor
{
    class ContentPanel : public juce::Component, public DrawnTextSource
    {
    public:
        std::vector<DrawnText> drawnText() const override; // ADR 0032
        enum class Platform { macOS, windowsOS, linuxOS };

        explicit ContentPanel (Controller&);
        ~ContentPanel() override;

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;

        const juce::String& headline() const noexcept { return headlineText; }
        const juce::String& message() const noexcept { return messageText; }

        // Whose instructions to give; the build's platform unless a test says otherwise.
        Platform platform;

        // Opens the folder chooser and calls back with the folder picked; tests replace it.
        std::function<void (std::function<void (const juce::File&)>)> chooseFolder;
        std::function<void()> onClose;

        // What "Choose content folder..." does, here or in the Samples drawer's banner: chooseFolder, then the
        // folder picked goes to the Controller.
        void chooseContentFolder();

        juce::TextButton chooseButton;
        IconButton closeButton { "Close", IconButton::Icon::close };

    private:
        Controller& controller;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::String headlineText, messageText;
        juce::StringArray searched;
        ContentState state = ContentState::missing;
        juce::Colour accent;
        juce::Rectangle<int> textArea, listArea;
    };
}
