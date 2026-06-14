#pragma once

#include "UI/UIContext.h"
#include "UI/ChannelStrip.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace omnidaw::ui
{
    /**
        MixerView — the mixing console: a channel strip per track, then per FX/
        submix bus, and the master on the far right.

        Rebuilt whenever the routing topology changes (track added, FX bus
        created). Strips scroll horizontally when they overflow.
    */
    class MixerView final : public juce::Component
    {
    public:
        explicit MixerView (UIContext& ctx);

        /** Recreate all strips from the current project. */
        void rebuild();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        UIContext& context;

        juce::TextButton addFxBusButton { "+ FX Bus" };
        juce::Viewport viewport;
        juce::Component stripContainer;
        juce::OwnedArray<ChannelStrip> strips;

        static constexpr int stripWidth = 96;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
    };
} // namespace omnidaw::ui
