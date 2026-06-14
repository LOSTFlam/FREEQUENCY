#pragma once

#include "UI/UIContext.h"
#include "Models/Track.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace omnidaw::ui
{
    /**
        TrackHeaderComponent — the control panel for one track, shown in the fixed
        left column of the arrange view (the "track header" in Cubase terms).

        Edits here write straight to the Track model and then ask the engine to
        re-sync parameters into the live graph node, thread-safely (the engine only
        writes node atomics). Solo handling is centralised in the engine.
    */
    class TrackHeaderComponent final : public juce::Component
    {
    public:
        TrackHeaderComponent (UIContext& ctx, models::Track& track);

        void paint (juce::Graphics&) override;
        void resized() override;

        [[nodiscard]] const models::Track& getTrack() const noexcept { return trackRef; }

    private:
        UIContext& context;
        models::Track& trackRef;

        juce::Label nameLabel;
        juce::Label typeLabel;
        juce::TextButton muteButton { "M" };
        juce::TextButton soloButton { "S" };
        juce::Slider volumeSlider;
        juce::Slider panSlider;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeaderComponent)
    };
} // namespace omnidaw::ui
