#include "UI/ChannelStrip.h"
#include "UI/OmniLookAndFeel.h"

namespace omnidaw::ui
{
    ChannelStrip::ChannelStrip (UIContext& ctx, Role r, models::Track* t, models::Bus* b)
        : context (ctx), role (r), track (t), bus (b)
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));

        if (role == Role::track && track != nullptr)
            titleLabel.setText (track->name, juce::dontSendNotification);
        else if (role == Role::bus && bus != nullptr)
            titleLabel.setText (bus->name, juce::dontSendNotification);
        else
            titleLabel.setText ("MASTER", juce::dontSendNotification);

        addAndMakeVisible (fader);
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (0.0, 1.5, 0.001);
        fader.setValue (getVolume(), juce::dontSendNotification);
        fader.onValueChange = [this] { setVolume ((float) fader.getValue()); };

        if (role == Role::track && track != nullptr)
        {
            addAndMakeVisible (panKnob);
            panKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            panKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            panKnob.setRange (-1.0, 1.0, 0.01);
            panKnob.setValue (track->getPan(), juce::dontSendNotification);
            panKnob.onValueChange = [this]
            {
                track->setPan ((float) panKnob.getValue());
                context.engine.syncParametersFromModel();
            };

            addAndMakeVisible (muteButton);
            muteButton.setClickingTogglesState (true);
            muteButton.setToggleState (track->isMuted(), juce::dontSendNotification);
            muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (OmniLookAndFeel::danger));
            muteButton.onClick = [this]
            {
                track->setMuted (muteButton.getToggleState());
                context.engine.syncParametersFromModel();
            };

            addAndMakeVisible (soloButton);
            soloButton.setClickingTogglesState (true);
            soloButton.setToggleState (track->isSoloed(), juce::dontSendNotification);
            soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (OmniLookAndFeel::accentWarm));
            soloButton.onClick = [this]
            {
                track->setSoloed (soloButton.getToggleState());
                context.engine.syncParametersFromModel();
            };

            // One send knob per FX bus in the project.
            auto& mixer = context.project.getMixer();
            for (int i = 0; i < mixer.getNumBuses(); ++i)
            {
                auto* fxBus = mixer.getBus (i);
                if (fxBus == nullptr || fxBus->getKind() != models::Bus::Kind::fx)
                    continue;

                const auto busId = fxBus->getId().toDashedString();
                sendBusIds.add (busId);

                auto* lbl = sendLabels.add (new juce::Label());
                lbl->setText (fxBus->name, juce::dontSendNotification);
                lbl->setFont (juce::FontOptions (9.0f));
                lbl->setJustificationType (juce::Justification::centred);
                lbl->setColour (juce::Label::textColourId, juce::Colour (OmniLookAndFeel::textDim));
                addAndMakeVisible (lbl);

                auto* knob = sendKnobs.add (new juce::Slider());
                knob->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                knob->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                knob->setRange (0.0, 1.0, 0.01);
                auto* existing = findSend (busId);
                knob->setValue (existing != nullptr ? existing->level.load() : 0.0,
                                juce::dontSendNotification);
                knob->onValueChange = [this, busId, knob]
                {
                    auto* send = findSend (busId);
                    if (send == nullptr)
                    {
                        track->addSend (busId);
                        context.engine.rebuildGraph();
                        send = findSend (busId);
                        if (onRoutingChanged) onRoutingChanged();
                    }
                    if (send != nullptr)
                    {
                        send->level.store ((float) knob->getValue(), std::memory_order_relaxed);
                        context.engine.syncParametersFromModel();
                    }
                };
                addAndMakeVisible (knob);
            }
        }

        startTimerHz (24);
    }

    ChannelStrip::~ChannelStrip()
    {
        stopTimer();
    }

    models::Track::Send* ChannelStrip::findSend (const juce::String& busId) const
    {
        if (track == nullptr)
            return nullptr;

        for (int i = 0; i < track->getNumSends(); ++i)
            if (auto* s = track->getSend (i))
                if (s->destBusId == busId)
                    return s;

        return nullptr;
    }

    float ChannelStrip::getVolume() const
    {
        if (role == Role::track && track != nullptr) return track->getVolume();
        if (role == Role::bus && bus != nullptr)     return bus->getVolume();
        return context.project.getMixer().getMasterBus().getVolume();
    }

    void ChannelStrip::setVolume (float v)
    {
        if (role == Role::track && track != nullptr) track->setVolume (v);
        else if (role == Role::bus && bus != nullptr) bus->setVolume (v);
        else context.project.getMixer().getMasterBus().setVolume (v);

        context.engine.syncParametersFromModel();
    }

    float ChannelStrip::getLevel() const
    {
        if (role == Role::track && track != nullptr) return context.engine.getTrackLevel (*track);
        if (role == Role::bus && bus != nullptr)     return context.engine.getBusLevel (*bus);
        return context.engine.getMasterLevel();
    }

    void ChannelStrip::timerCallback()
    {
        const auto level = getLevel();
        if (std::abs (level - meterLevel) > 0.005f)
        {
            meterLevel = level;
            repaint();
        }
    }

    void ChannelStrip::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (role == Role::master ? OmniLookAndFeel::panelLight
                                                      : OmniLookAndFeel::panel));

        // Colour band at the top for tracks/buses.
        if (role != Role::master)
        {
            auto c = (role == Role::track && track != nullptr) ? track->colour
                   : (bus != nullptr ? bus->colour : juce::Colours::grey);
            g.setColour (c);
            g.fillRect (0, 0, getWidth(), 4);
        }

        g.setColour (juce::Colour (OmniLookAndFeel::outline));
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

        // Meter beside the fader (drawn in the reserved strip on the right of the
        // fader area). Bounds computed to match resized().
        auto meterBounds = meterArea;
        g.setColour (juce::Colour (OmniLookAndFeel::background));
        g.fillRect (meterBounds);

        const auto level = juce::jlimit (0.0f, 1.0f, meterLevel);
        auto filled = meterBounds.withTop (meterBounds.getBottom() - (int) (meterBounds.getHeight() * level));
        g.setColour (level > 0.9f ? juce::Colour (OmniLookAndFeel::danger)
                                  : juce::Colour (OmniLookAndFeel::accent));
        g.fillRect (filled);
    }

    void ChannelStrip::resized()
    {
        auto r = getLocalBounds().reduced (6, 6);
        r.removeFromTop (2);

        titleLabel.setBounds (r.removeFromTop (18));
        r.removeFromTop (4);

        if (role == Role::track)
        {
            panKnob.setBounds (r.removeFromTop (34).withSizeKeepingCentre (34, 34));
            r.removeFromTop (4);

            auto msRow = r.removeFromTop (20);
            muteButton.setBounds (msRow.removeFromLeft (msRow.getWidth() / 2).reduced (1));
            soloButton.setBounds (msRow.reduced (1));
            r.removeFromTop (4);
        }

        // Sends at the bottom (track role).
        if (! sendKnobs.isEmpty())
        {
            auto sendsArea = r.removeFromBottom (54);
            const int n = sendKnobs.size();
            const int w = juce::jmax (1, sendsArea.getWidth() / n);
            for (int i = 0; i < n; ++i)
            {
                auto col = sendsArea.removeFromLeft (w);
                sendLabels[i]->setBounds (col.removeFromBottom (12));
                sendKnobs[i]->setBounds (col.withSizeKeepingCentre (juce::jmin (col.getWidth(), 34), juce::jmin (col.getHeight(), 34)));
            }
            r.removeFromBottom (4);
        }

        // Remaining area: fader on the left, meter strip on the right.
        auto faderArea = r;
        meterArea = faderArea.removeFromRight (10);
        faderArea.removeFromRight (4);
        fader.setBounds (faderArea);
    }
} // namespace omnidaw::ui
