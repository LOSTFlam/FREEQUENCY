#pragma once

#include "UI/UIContext.h"
#include "Models/Track.h"
#include "Models/Mixer.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace freequency::ui
{
    /**
        ChannelStrip — one vertical mixer channel.

        A single component serves three roles, chosen at construction:
          - a track strip (fader, pan, mute/solo, meter, aux sends)
          - a bus strip   (fader + meter)
          - the master strip (fader + meter)

        All edits write the model and re-sync the engine; meters poll the engine's
        per-strip level via a timer. Keeping one class for all three keeps the
        mixer layout uniform and the routing model obvious.
    */
    class ChannelStrip final : public juce::Component,
                               private juce::Timer
    {
    public:
        enum class Role { track, bus, master };

        ChannelStrip (UIContext& ctx, Role role,
                      models::Track* track, models::Bus* bus);
        ~ChannelStrip() override;

        /** Notify the host that routing changed (a send was created). */
        std::function<void()> onRoutingChanged;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        float getVolume() const;
        void  setVolume (float);
        float getLevel() const;
        models::Track::Send* findSend (const juce::String& busId) const;

        UIContext& context;
        Role role;
        models::Track* track { nullptr };
        models::Bus* bus { nullptr };

        void showFxMenu();

        juce::Label titleLabel;
        juce::Slider fader;
        juce::Slider panKnob;
        juce::TextButton muteButton { "M" };
        juce::TextButton soloButton { "S" };
        juce::TextButton fxButton { "FX +" };
        juce::OwnedArray<juce::TextButton> insertButtons; // one per insert slot

        juce::OwnedArray<juce::Slider> sendKnobs;   // one per FX bus (track role)
        juce::OwnedArray<juce::Label> sendLabels;
        juce::Array<juce::String> sendBusIds;

        juce::Rectangle<int> meterArea; // computed in resized(), used in paint()
        float meterLevel { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
    };
} // namespace freequency::ui
