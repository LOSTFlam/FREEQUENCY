#pragma once

#include "Models/Project.h"
#include "Engine/AudioEngine.h"
#include "UI/UIContext.h"
#include "UI/FreequencyLookAndFeel.h"
#include "UI/TransportBar.h"
#include "UI/ArrangeView.h"
#include "UI/MixerView.h"
#include "UI/StatusBar.h"
#include "UI/PluginEditorWindow.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace freequency::ui
{
    /**
        MainComponent — the application root: owns the document (Project), the
        AudioEngine, and the top-level views (transport bar + arrange view, with a
        toggleable mixer in Phase 4).

        Strict layering still holds: this UI owns and drives the model/engine, but
        the model and engine never depend on it.
    */
    class MainComponent final : public juce::Component,
                                public juce::ApplicationCommandTarget
    {
    public:
        MainComponent();
        ~MainComponent() override;

        /** Loads a .freq project file, rebuilding the engine graph and views. */
        bool openProjectFile (const juce::File&);

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyStateChanged (bool isKeyDown) override; // computer-keyboard piano
        void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

        // ── ApplicationCommandTarget ────────────────────────────────────────────
        ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
        void getAllCommands (juce::Array<juce::CommandID>&) override;
        void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
        bool perform (const InvocationInfo&) override;

        [[nodiscard]] juce::ApplicationCommandManager& getCommandManager() noexcept { return commandManager; }

    private:
        void buildDemoProject();
        void toggleMixer();

        // Command actions.
        void afterTrackChange();
        void afterClipChange();
        void toggleRecord();
        void saveProjectAs();
        void openProjectDialog();
        void openKeyMappings();
        void duplicateSelectedClip();
        void splitSelectedClipAtPlayhead();
        void deleteSelectedClip();
        void nudgeSelectedClip (int direction);
        void movePlayheadByBar (int direction);
        void changeTempo (double delta);

        // Computer-keyboard piano.
        [[nodiscard]] models::MidiTrack* pianoTargetTrack() const;
        void allPianoNotesOff();

        juce::File keyMappingsFile() const;
        void saveKeyMappings();

        FreequencyLookAndFeel lookAndFeel;

        models::Project project;
        engine::AudioEngine audioEngine;
        UIContext context { project, audioEngine };

        std::unique_ptr<TransportBar> transportBar;
        std::unique_ptr<ArrangeView> arrangeView;
        std::unique_ptr<MixerView> mixerView;
        std::unique_ptr<StatusBar> statusBar;
        juce::OwnedArray<PluginEditorWindow> pluginWindows;

        juce::ApplicationCommandManager commandManager;
        std::unique_ptr<juce::DocumentWindow> keyMappingWindow;
        std::unique_ptr<juce::FileChooser> fileChooser;

        // Computer-keyboard piano state.
        bool pianoEnabled { false };
        int  pianoOctave { 5 };          // octave 5 => 'a' = C5 = MIDI 60
        bool pianoKeyDown[13] { false };  // C..C across one octave + 1

        bool mixerVisible { false };
        juce::String engineStatus;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
} // namespace freequency::ui
