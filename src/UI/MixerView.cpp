#include "UI/MixerView.h"
#include "UI/FreequencyLookAndFeel.h"

namespace freequency::ui
{
    MixerView::MixerView (UIContext& ctx)
        : context (ctx)
    {
        addAndMakeVisible (addFxBusButton);
        addFxBusButton.onClick = [this]
        {
            auto& mixer = context.project.getMixer();
            mixer.addFxBus ("FX " + juce::String (mixer.getNumBuses()));
            context.engine.rebuildGraph();
            rebuild();
        };

        viewport.setViewedComponent (&stripContainer, false);
        viewport.setScrollBarsShown (false, true);
        addAndMakeVisible (viewport);

        rebuild();
    }

    void MixerView::rebuild()
    {
        strips.clear();

        auto& timeline = context.project.getTimeline();
        auto& mixer = context.project.getMixer();

        // Track strips.
        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            if (auto* track = timeline.getTrack (i))
            {
                auto* strip = strips.add (new ChannelStrip (context, ChannelStrip::Role::track, track, nullptr));
                strip->onRoutingChanged = [this] { rebuild(); };
                stripContainer.addAndMakeVisible (strip);
            }
        }

        // Bus strips (submix + FX), excluding master.
        for (int i = 0; i < mixer.getNumBuses(); ++i)
        {
            auto* bus = mixer.getBus (i);
            if (bus == nullptr || bus->getKind() == models::Bus::Kind::master)
                continue;

            auto* strip = strips.add (new ChannelStrip (context, ChannelStrip::Role::bus, nullptr, bus));
            strip->onRoutingChanged = [this] { rebuild(); };
            stripContainer.addAndMakeVisible (strip);
        }

        // Master strip.
        {
            auto* strip = strips.add (new ChannelStrip (context, ChannelStrip::Role::master, nullptr, nullptr));
            stripContainer.addAndMakeVisible (strip);
        }

        resized();
    }

    void MixerView::paint (juce::Graphics& g)
    {
        g.fillAll (theme().background);
    }

    void MixerView::resized()
    {
        auto r = getLocalBounds();

        auto top = r.removeFromTop (34);
        addFxBusButton.setBounds (top.removeFromLeft (110).reduced (6, 5));

        viewport.setBounds (r);

        stripContainer.setSize (juce::jmax (r.getWidth(), strips.size() * stripWidth), r.getHeight());

        for (int i = 0; i < strips.size(); ++i)
            strips[i]->setBounds (i * stripWidth, 0, stripWidth, stripContainer.getHeight());
    }
} // namespace freequency::ui
