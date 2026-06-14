#pragma once

#include "Models/Project.h"
#include "Engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace omnidaw::ui
{
    /**
        MainComponent — the application's root component.

        Phase 1 keeps this deliberately bare: there is no timeline, mixer or
        transport UI yet. Its only jobs are to OWN the document (Project) and the
        AudioEngine, wire them together, and start audio. The real views arrive in
        Phase 3+.

        Note the strict layering: this UI component depends on the model and the
        engine, but neither of those depends on it. That one-way dependency is the
        whole point of the MVVM/Model-View separation mandated for OmniDAW — the
        audio engine could run in a headless render tool with this file deleted.
    */
    class MainComponent final : public juce::Component
    {
    public:
        MainComponent();
        ~MainComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        // The document and the engine are owned here for Phase 1. As the app
        // grows these will likely move up into the JUCEApplication / a controller.
        std::unique_ptr<models::Project> project;
        std::unique_ptr<engine::AudioEngine> audioEngine;

        juce::String engineStatus;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
} // namespace omnidaw::ui
