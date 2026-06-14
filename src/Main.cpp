#include "UI/MainComponent.h"
#include "Models/Project.h"
#include "Models/MidiTrack.h"
#include "Models/ProjectSerializer.h"
#include "Engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

/*
    Application entry point for FREEQUENCY.

    This is the only place that knows about both JUCEApplication and the top-level
    window. It hosts a single MainComponent which, for Phase 1, owns the Project
    and the AudioEngine. Keeping app-lifecycle concerns here leaves the rest of the
    codebase free of framework bootstrapping.
*/
namespace freequency
{
    class Application final : public juce::JUCEApplication
    {
    public:
        Application() = default;

        const juce::String getApplicationName() override    { return "FREEQUENCY"; }
        const juce::String getApplicationVersion() override { return "0.1.0"; }
        bool moreThanOneInstanceAllowed() override          { return true; }

        void initialise (const juce::String& commandLine) override
        {
            // Headless self-test: render a known MIDI note offline and report the
            // output peak, then quit. Lets CI verify the engine makes sound with
            // no soundcard. Usage: FREEQUENCY --selftest
            if (commandLine.contains ("--selftest"))
            {
                runSelfTest();
                quit();
                return;
            }

            mainWindow = std::make_unique<MainWindow> (getApplicationName());
            openFileFromCommandLine (commandLine);
        }

        void anotherInstanceStarted (const juce::String& commandLine) override
        {
            openFileFromCommandLine (commandLine);
        }

        void shutdown() override
        {
            mainWindow = nullptr;
        }

        void systemRequestedQuit() override { quit(); }

    private:
        /** Opens a .freq file passed on the command line (e.g. by double-clicking
            an associated document). */
        void openFileFromCommandLine (const juce::String& commandLine)
        {
            if (mainWindow == nullptr)
                return;

            for (const auto& arg : juce::StringArray::fromTokens (commandLine, true))
            {
                juce::File file (arg.unquoted());
                if (file.existsAsFile() && file.hasFileExtension ("freq"))
                    if (auto* mc = dynamic_cast<ui::MainComponent*> (mainWindow->getContentComponent()))
                        mc->openProjectFile (file);
            }
        }

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

            std::cout << "FREEQUENCY self-test: rendered peak = " << peak
                      << (peak > 0.0001f ? "  [PASS]" : "  [FAIL: silence]")
                      << std::endl;

            // Persistence round-trip: save -> load -> render must match.
            {
                const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("freequency_selftest.freq");
                models::ProjectSerializer::saveToFile (project, tmp);

                models::Project loaded;
                const bool ok = models::ProjectSerializer::loadFromFile (loaded, tmp);

                engine::AudioEngine engine2;
                engine2.setProject (&loaded);
                const float peak2 = engine2.renderOfflinePeak (44100.0, 1.0);
                tmp.deleteFile();

                const bool match = ok && std::abs (peak2 - peak) < 0.02f;
                std::cout << "FREEQUENCY self-test: persistence reload peak = " << peak2
                          << (match ? "  [PASS]" : "  [FAIL]") << std::endl;
            }

            // Automation check: a volume curve pinned to 0 must silence the track,
            // proving automation overrides the fader on the audio thread.
            midiTrack->volumeAutomation.addPoint (0.0, 0.0f);
            midiTrack->volumeAutomation.addPoint (2.0, 0.0f);
            midiTrack->volumeAutomationEnabled = true;
            audioEngine.rebuildAutomation();

            const float autoPeak = audioEngine.renderOfflinePeak (44100.0, 1.0, 512, 0.1);
            std::cout << "FREEQUENCY self-test: automation(0) steady peak = " << autoPeak
                      << (autoPeak < 0.01f ? "  [PASS]" : "  [FAIL: not silenced]")
                      << std::endl;

            // Metronome: an empty project with the click on must still produce
            // sound (the beat blips), proving the metronome node works.
            {
                models::Project clickProject;
                clickProject.getTimeline().setTempoBpm (120.0);
                engine::AudioEngine clickEngine;
                clickEngine.setProject (&clickProject);
                clickEngine.setMetronomeEnabled (true);
                const float clickPeak = clickEngine.renderOfflinePeak (44100.0, 1.0);
                std::cout << "FREEQUENCY self-test: metronome peak = " << clickPeak
                          << (clickPeak > 0.01f ? "  [PASS]" : "  [FAIL: no click]") << std::endl;
            }
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
} // namespace freequency

// Wires freequency::Application into JUCE's platform-specific main().
START_JUCE_APPLICATION (freequency::Application)
