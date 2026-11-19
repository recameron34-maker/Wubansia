#include "ContentPanel.h"
#include "KitPanel.h"
#include "LookAndFeel.h"
#include "editor/skin/Paint.h"

namespace wubansia::editor
{
    namespace
    {
        ContentPanel::Platform buildPlatform()
        {
           #if JUCE_MAC
            return ContentPanel::Platform::macOS;
           #elif JUCE_WINDOWS
            return ContentPanel::Platform::windowsOS;
           #else
            return ContentPanel::Platform::linuxOS;
           #endif
        }

        // Paths read better with the home folder as "~".
        juce::String tidy (const juce::String& path)
        {
            const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
            return home.isNotEmpty() && path.startsWith (home) ? "~" + path.substring (home.length()) : path;
        }

        juce::String withFullStop (const juce::String& text)
        {
            const auto t = text.trim();
            return t.isEmpty() || t.endsWithChar ('.') ? t : t + ".";
        }
    }

    ContentPanel::ContentPanel (Controller& c)
        : platform (buildPlatform()), controller (c)
    {
        setComponentID ("content");

        chooseFolder = [this] (std::function<void (const juce::File&)> done)
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose the folder that holds Wubansia's kits (fut01, fut02...)",
                juce::File::getSpecialLocation (juce::File::userHomeDirectory));
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                [done] (const juce::FileChooser& fc)
                {
                    const auto folder = fc.getResult();
                    if (folder.isDirectory())
                        done (folder);
                });
        };

        chooseButton.setButtonText (utf8 ("Choose content folder\xe2\x80\xa6"));
        chooseButton.setComponentID ("content_folder");
        chooseButton.setTooltip ("Point Wubansia at the folder that holds the kits' sample folders (fut01, fut02...). "
                                 "It's remembered for you, not saved in projects.");
        chooseButton.onClick = [this] { chooseContentFolder(); };
        closeButton.setComponentID ("content_close");
        closeButton.setTooltip (utf8 ("Hide this panel; Content\xe2\x80\xa6 in the Samples drawer brings it back"));
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };

        addAndMakeVisible (chooseButton);
        addAndMakeVisible (closeButton);
    }

    ContentPanel::~ContentPanel() = default;

    void ContentPanel::chooseContentFolder()
    {
        if (chooseFolder != nullptr)
            chooseFolder ([this] (const juce::File& folder) { controller.setContentFolder (folder); });
    }

    void ContentPanel::update (const EditorModel& model)
    {
        const auto& content = model.content;
        const auto kit = model.hasKit() ? model.kit.id : juce::String ("This kit");
        const auto folderName = model.kit.id.toLowerCase();
        const auto problem = withFullStop (content.problem);
        const auto version = juce::String (juce::jmax (1, model.kit.contentVersion));
        const auto mac = platform == Platform::macOS;
        const auto defaultFolder = mac ? juce::String ("~/Library/Application Support/Wubansia Audio/Wubansia/Content")
                                       : (platform == Platform::linuxOS ? juce::String ("~/.local/share/Wubansia Audio/Wubansia/Content")
                                                                        : juce::String ("the Content folder listed below"));

        // The message: a line of context, then the steps, each on its own line starting "1  ".
        juce::String headline, message;
        switch (content.state)
        {
            case ContentState::ready:
                headline = kit + "'s samples are ready";
                message = "Wubansia plays " + kit + " " + model.kit.title + " from " + tidy (content.folder.getFullPathName())
                          + ". To play it from another copy of the samples, choose that folder.";
                break;

            case ContentState::loading:
                headline = utf8 ("Loading ") + kit + utf8 ("\xe2\x80\xa6 ") + juce::String (juce::jlimit (0, 100, content.percent)) + " %";
                message = "Reading the samples from " + tidy (content.folder.getFullPathName()) + ". The kit plays as soon as they're in.";
                break;

            case ContentState::missing:
                headline = kit + "'s samples aren't installed";
                message = "The kit stays silent until they are, though its MIDI already drags out.\n";
                if (mac)
                    message << "1  Run install-macos.sh from the Wubansia download. It puts every kit's samples in " << defaultFolder << ".\n"
                            << "2  Or unzip " << kit << "'s content zip anywhere, then choose the folder that holds its '" << folderName << "' folder.";
                else
                    message << "1  Unzip " << kit << "'s content zip into " << defaultFolder << ".\n"
                            << "2  Or unzip it anywhere, then choose the folder that holds its '" << folderName << "' folder.";
                break;

            case ContentState::lfsPointers:
                headline = kit + "'s samples are Git LFS pointers, not audio";
                message = problem + "\n1  In your clone, run: git lfs pull --include \"content/samples/**\"\n"
                                    "2  Or choose a folder that holds the real samples.";
                break;

            case ContentState::mismatch:
                headline = kit + "'s samples don't match this version";
                message = problem + " This build plays " + kit + "'s content v" + version + ": 48 kHz WAV files of the listed lengths.\n"
                          + (mac ? "1  Reinstall the kit with install-macos.sh.\n" : "1  Unzip the kit's content zip again.\n")
                          + "2  Or choose the folder that holds v" + version + ".";
                break;

            case ContentState::failed:
                headline = kit + "'s samples couldn't be loaded";
                message = problem + "\n1  Reinstall the kit's content zip" + (mac ? " with install-macos.sh." : juce::String (".")) + "\n"
                          + "2  Or choose another folder.";
                break;
        }

        if (headline != headlineText || message != messageText || content.searched != searched || content.state != state || model.accent != accent)
        {
            headlineText = headline;
            messageText = message;
            searched = content.searched;
            state = content.state;
            accent = model.accent;
            repaint();
        }
    }

    void ContentPanel::resized()
    {
        closeButton.setBounds (getLocalBounds().removeFromTop (42).removeFromRight (42).reduced (7));
        auto area = getLocalBounds().reduced (24, 18);
        listArea = area.removeFromRight (juce::roundToInt ((float) area.getWidth() * 0.36f)).withTrimmedTop (24);
        area.removeFromRight (28);
        auto buttons = area.removeFromBottom (32);
        chooseButton.setBounds (buttons.removeFromLeft (236));
        area.removeFromBottom (8);
        textArea = area;
    }

    std::vector<DrawnText> ContentPanel::drawnText() const
    {
        // The headline and the folders searched (the person's paths: the path role, fallback on), as paint () draws them.
        std::vector<DrawnText> out;
        const auto& skin = skinOf (*this);
        auto area = textArea.toFloat();
        DrawnText headline;
        headline.text = headlineText;
        headline.font = bodyFont (skin, 20.0f, true);
        headline.area = area.removeFromTop (28.0f).withTrimmedLeft (22.0f);
        headline.styledPx = headline.minPx = headline.font.getHeightInPoints();
        if (juce::GlyphArrangement::getStringWidth (headline.font, headline.text) > headline.area.getWidth())
            headline.cut = true;
        out.push_back (headline);
        for (const auto& folder : searched)
        {
            DrawnText t;
            t.text = folder;
            t.font = pathFont (skin, 11.5f);
            t.area = { area.getX(), area.getY(), 1.0e5f, 16.0f }; // a path may run under the panel's edge, cut by the box
            t.styledPx = t.minPx = t.font.getHeightInPoints();
            out.push_back (t);
        }
        return out;
    }

    void ContentPanel::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();

        // The panel's frame in the look (ADR 0032), rimmed in its tone.
        paint::drawer (g, skinOf (*this), bounds);
        const auto problem = state != ContentState::ready && state != ContentState::loading;
        const auto tone = problem ? skinOf (*this).colours.warning : (state == ContentState::loading ? skinOf (*this).colours.busy : accent);
        g.setColour (tone.withAlpha (0.65f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.2f);

        // The headline.
        auto area = textArea.toFloat();
        auto top = area.removeFromTop (28.0f);
        paint::led (g, { top.getX() + 7.0f, top.getCentreY() }, 5.0f, tone, 1.0f);
        g.setFont (bodyFont (skinOf (*this), 20.0f, true));
        g.setColour (skinOf (*this).colours.text);
        g.drawText (headlineText, top.withTrimmedLeft (22.0f), juce::Justification::centredLeft, true);
        area.removeFromTop (8.0f);

        // The message, its steps numbered in lit badges.
        for (const auto& paragraph : juce::StringArray::fromLines (messageText))
        {
            if (area.getHeight() < 12.0f)
                break;
            const auto step = paragraph.length() > 3 && juce::CharacterFunctions::isDigit (paragraph[0]) && paragraph.substring (1, 3) == "  ";
            auto row = area;
            if (step)
            {
                const auto badge = juce::Rectangle<float> (18.0f, 18.0f).withPosition (row.getX(), row.getY());
                g.setColour (tone.withAlpha (0.2f));
                g.fillEllipse (badge);
                g.setColour (tone);
                g.drawEllipse (badge.reduced (0.5f), 1.0f);
                g.setFont (bodyFont (skinOf (*this), 11.0f, true));
                g.drawText (paragraph.substring (0, 1), badge, juce::Justification::centred, false);
                row.removeFromLeft (28.0f);
            }

            juce::AttributedString text;
            text.append (step ? paragraph.substring (3) : paragraph, bodyFont (skinOf (*this), 14.0f), step ? skinOf (*this).colours.text.withAlpha (0.92f) : skinOf (*this).colours.dim.brighter (0.2f));
            text.setLineSpacing (2.0f);
            juce::TextLayout layout;
            layout.createLayout (text, row.getWidth());
            layout.draw (g, row.withHeight (juce::jmin (row.getHeight(), layout.getHeight())));
            area.removeFromTop (layout.getHeight() + (step ? 7.0f : 9.0f));
        }

        // Where it looked.
        auto list = listArea.toFloat();
        paint::well (g, skinOf (*this), list, 8.0f);
        list = list.reduced (14.0f, 10.0f);
        paint::label (g, skinOf (*this), searched.isEmpty() ? "Folders searched" : "Wubansia looked in", list.removeFromTop (14.0f), juce::Justification::centredLeft, 9.5f);
        list.removeFromTop (6.0f);
        g.setFont (pathFont (skinOf (*this), 11.5f));
        if (searched.isEmpty())
        {
            g.setColour (skinOf (*this).colours.faint);
            g.drawText ("none yet", list.removeFromTop (16.0f), juce::Justification::centredLeft, true);
        }
        for (int i = 0; i < searched.size() && list.getHeight() >= 15.0f; ++i)
        {
            auto row = list.removeFromTop (17.0f);
            g.setColour (skinOf (*this).colours.faint);
            g.drawText (juce::String (i + 1) + ".", row.removeFromLeft (20.0f), juce::Justification::centredLeft, false);
            g.setColour (skinOf (*this).colours.dim);
            g.drawText (tidy (searched[i]), row, juce::Justification::centredLeft, true);
        }
    }
}
