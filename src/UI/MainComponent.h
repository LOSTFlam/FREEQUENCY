#pragma once

#include "Models/Project.h"
#include "Engine/AudioEngine.h"
#include "UI/UIContext.h"
#include "UI/OmniLookAndFeel.h"
#include "UI/TransportBar.h"
#include "UI/ArrangeView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace omnidaw::ui
{
    /**
        MainComponent — the application root: owns the document (Project), the
        AudioEngine, and the top-level views (transport bar + arrange view, with a
        toggleable mixer in Phase 4).

        Strict layering still holds: this UI owns and drives the model/engine, but
        the model and engine never depend on it.
    */
    class MainComponent final : public juce::Component
    {
    public:
        MainComponent();
        ~MainComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void buildDemoProject();
        void toggleMixer();

        OmniLookAndFeel lookAndFeel;

        models::Project project;
        engine::AudioEngine audioEngine;
        UIContext context { project, audioEngine };

        std::unique_ptr<TransportBar> transportBar;
        std::unique_ptr<ArrangeView> arrangeView;

        bool mixerVisible { false };
        juce::String engineStatus;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
} // namespace omnidaw::ui
