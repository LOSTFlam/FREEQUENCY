#include "UI/MainComponent.h"
#include "Models/Project.h"
#include "Models/MidiTrack.h"
#include "Engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

/*
    Application entry point for OmniDAW.

    This is the only place that knows about both JUCEApplication and the top-level
    window. It hosts a single MainComponent which, for Phase 1, owns the Project
    and the AudioEngine. Keeping app-lifecycle concerns here leaves the rest of the
    codebase free of framework bootstrapping.
*/
namespace omnidaw
{
    class Application final : public juce::JUCEApplication
    {
    public:
        Application() = default;

        const juce::String getApplicationName() override    { return "OmniDAW"; }
        const juce::String getApplicationVersion() override { return "0.1.0"; }
        bool moreThanOneInstanceAllowed() override          { return true; }

        void initialise (const juce::String& commandLine) override
        {
            // Headless self-test: render a known MIDI note offline and report the
            // output peak, then quit. Lets CI verify the engine makes sound with
            // no soundcard. Usage: OmniDAW --selftest
            if (commandLine.contains ("--selftest"))
            {
                runSelfTest();
                quit();
                return;
            }

            mainWindow = std::make_unique<MainWindow> (getApplicationName());
        }

        void shutdown() override
        {
            mainWindow = nullptr;
        }

        void systemRequestedQuit() override { quit(); }

    private:
        /** Builds a tiny project (one MIDI note on the built-in synth) and renders
            it offline, printing the peak level so a non-zero result proves the
            signal path Transport -> MidiSource -> Synth -> strip -> master works.
        */
        static void runSelfTest()
        {
            models::Project project;

            auto* midiTrack = project.getTimeline().addMidiTrack();
            auto* clip = midiTrack->addClip();
            clip->startTime = 0.0;
            clip->length = 1.0;

            // A 0.5s middle-C on channel 1, velocity 100.
            clip->sequence.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0.0);
            clip->sequence.addEvent (juce::MidiMessage::noteOff (1, 60), 0.5);
            clip->sequence.updateMatchedPairs();

            engine::AudioEngine audioEngine;
            audioEngine.setProject (&project);

            const float peak = audioEngine.renderOfflinePeak (44100.0, 1.0);

            std::cout << "OmniDAW self-test: rendered peak = " << peak
                      << (peak > 0.0001f ? "  [PASS]" : "  [FAIL: silence]")
                      << std::endl;
        }

        /** A standard resizable document window hosting the MainComponent. */
        class MainWindow final : public juce::DocumentWindow
        {
        public:
            explicit MainWindow (juce::String name)
                : DocumentWindow (std::move (name),
                                  juce::Desktop::getInstance().getDefaultLookAndFeel()
                                      .findColour (juce::ResizableWindow::backgroundColourId),
                                  DocumentWindow::allButtons)
            {
                setUsingNativeTitleBar (true);
                setContentOwned (new ui::MainComponent(), true);
                setResizable (true, true);
                centreWithSize (getWidth(), getHeight());
                setVisible (true);
            }

            void closeButtonPressed() override
            {
                JUCEApplication::getInstance()->systemRequestedQuit();
            }

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
        };

        std::unique_ptr<MainWindow> mainWindow;
    };
} // namespace omnidaw

// Wires omnidaw::Application into JUCE's platform-specific main().
START_JUCE_APPLICATION (omnidaw::Application)
