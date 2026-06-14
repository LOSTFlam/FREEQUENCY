#include "UI/MediaBrowser.h"
#include "UI/FreequencyLookAndFeel.h"

namespace freequency::ui
{
    MediaBrowser::MediaBrowser (UIContext& ctx)
        : context (ctx)
    {
        thread.startThread();

        dirList = std::make_unique<juce::DirectoryContentsList> (&filter, thread);
        tree = std::make_unique<juce::FileTreeComponent> (*dirList);
        tree->addListener (this);
        tree->setDragAndDropDescription ("media-file"); // enables internal drag
        addAndMakeVisible (*tree);

        addAndMakeVisible (folderButton);
        folderButton.onClick = [this] { chooseFolder(); };

        addAndMakeVisible (stopButton);
        stopButton.onClick = [this] { context.engine.stopPreview(); };

        addAndMakeVisible (hint);
        hint.setText ("Double-click to preview · drag onto an audio track",
                      juce::dontSendNotification);
        hint.setFont (juce::FontOptions (10.0f));
        hint.setColour (juce::Label::textColourId, juce::Colour (FreequencyLookAndFeel::textDim));

        // Default root: the user's Music folder (or home).
        auto root = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (! root.isDirectory())
            root = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        dirList->setDirectory (root, true, true);
    }

    MediaBrowser::~MediaBrowser()
    {
        if (tree != nullptr)
            tree->removeListener (this);
        thread.stopThread (2000);
    }

    void MediaBrowser::fileDoubleClicked (const juce::File& file)
    {
        if (file.existsAsFile())
            context.engine.previewFile (file);
    }

    void MediaBrowser::chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a samples folder",
                                                       dirList->getDirectory());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (dir.isDirectory())
                    dirList->setDirectory (dir, true, true);
            });
    }

    void MediaBrowser::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (FreequencyLookAndFeel::panel));
        g.setColour (juce::Colour (FreequencyLookAndFeel::outline));
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

        g.setColour (juce::Colour (FreequencyLookAndFeel::textDim));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("BROWSER", getLocalBounds().removeFromTop (22).reduced (10, 0),
                    juce::Justification::centredLeft, false);
    }

    void MediaBrowser::resized()
    {
        auto r = getLocalBounds();
        r.removeFromTop (22);
        auto top = r.removeFromTop (28).reduced (6, 2);
        folderButton.setBounds (top.removeFromLeft (78));
        top.removeFromLeft (4);
        stopButton.setBounds (top.removeFromLeft (54));
        hint.setBounds (r.removeFromBottom (18).reduced (6, 0));
        if (tree != nullptr)
            tree->setBounds (r.reduced (4));
    }
} // namespace freequency::ui
