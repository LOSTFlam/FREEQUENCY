#pragma once

#include "UI/UIContext.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace freequency::ui
{
    /**
        MediaBrowser — a dockable file browser for samples/audio.

        Shows a directory tree (pick a root folder), auditions a file on
        double-click through the engine's preview node, and supports dragging a
        file onto an audio lane to create a clip (lanes are DragAndDropTargets;
        the FileTreeComponent supplies the file path as the drag description).
    */
    class MediaBrowser final : public juce::Component,
                               private juce::FileBrowserListener
    {
    public:
        explicit MediaBrowser (UIContext& ctx);
        ~MediaBrowser() override;

        [[nodiscard]] juce::File getSelectedFile() const
        {
            return tree != nullptr ? tree->getSelectedFile (0) : juce::File();
        }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void selectionChanged() override {}
        void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
        void fileDoubleClicked (const juce::File&) override;
        void browserRootChanged (const juce::File&) override {}

        void chooseFolder();

        UIContext& context;

        juce::TextButton folderButton { "Folder…" };
        juce::TextButton stopButton { "Stop" };
        juce::Label hint;

        juce::TimeSliceThread thread { "MediaBrowser" };
        juce::WildcardFileFilter filter { "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", "*", "Audio files" };
        std::unique_ptr<juce::DirectoryContentsList> dirList;
        std::unique_ptr<juce::FileTreeComponent> tree;
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaBrowser)
    };
} // namespace freequency::ui
