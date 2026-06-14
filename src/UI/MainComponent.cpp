#include "UI/MainComponent.h"

namespace omnidaw::ui
{
    MainComponent::MainComponent()
    {
        // Build a tiny demo document so the engine has something to instantiate
        // graph nodes for. This proves the model -> engine path end to end.
        project = std::make_unique<models::Project>();

        auto& timeline = project->getTimeline();
        timeline.addAudioTrack()->name = "Drums";
        timeline.addMidiTrack()->name = "Bass";
        timeline.addAudioTrack()->name = "Vocals";

        audioEngine = std::make_unique<engine::AudioEngine>();
        audioEngine->setProject (project.get());

        const auto error = audioEngine->initialise();
        engineStatus = error.isEmpty()
                           ? "Audio engine running."
                           : ("Audio engine idle (no device): " + error);

        setSize (900, 560);
    }

    MainComponent::~MainComponent()
    {
        // Tear down audio before the model it references disappears.
        if (audioEngine != nullptr)
            audioEngine->shutdown();
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff1b1d23));

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
        g.drawText ("OmniDAW", getLocalBounds().removeFromTop (90),
                    juce::Justification::centred, false);

        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Phase 1 \xe2\x80\x94 Engine Core (no UI yet)",
                    getLocalBounds().withTrimmedTop (70).removeFromTop (30),
                    juce::Justification::centred, false);

        g.setColour (juce::Colours::aquamarine);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (engineStatus,
                    getLocalBounds().withTrimmedTop (110).removeFromTop (30),
                    juce::Justification::centred, false);
    }

    void MainComponent::resized()
    {
        // No child components in Phase 1.
    }
} // namespace omnidaw::ui
