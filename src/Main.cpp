#include "UI/MainComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>

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

        void initialise (const juce::String&) override
        {
            mainWindow = std::make_unique<MainWindow> (getApplicationName());
        }

        void shutdown() override
        {
            mainWindow = nullptr;
        }

        void systemRequestedQuit() override { quit(); }

    private:
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
