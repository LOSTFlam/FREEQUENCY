#include "UI/ChannelStrip.h"
#include "UI/FreequencyLookAndFeel.h"

#include "DSP/BuiltinEffects.h"

namespace freequency::ui
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

        // Insert FX (available on track and bus strips).
        if (insertList() != nullptr)
        {
            addAndMakeVisible (fxButton);
            fxButton.setColour (juce::TextButton::buttonColourId, juce::Colour (FreequencyLookAndFeel::panelLight));
            fxButton.onClick = [this] { showFxMenu(); };
            buildInsertButtons();
        }

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
            muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (FreequencyLookAndFeel::danger));
            muteButton.onClick = [this]
            {
                track->setMuted (muteButton.getToggleState());
                context.engine.syncParametersFromModel();
            };

            addAndMakeVisible (soloButton);
            soloButton.setClickingTogglesState (true);
            soloButton.setToggleState (track->isSoloed(), juce::dontSendNotification);
            soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (FreequencyLookAndFeel::accentWarm));
            soloButton.onClick = [this]
            {
                track->setSoloed (soloButton.getToggleState());
                context.engine.syncParametersFromModel();
            };

            // ── Output routing (route this track onto any mixer bus) ────────────
            addAndMakeVisible (outBox);
            {
                auto& mixer = context.project.getMixer();
                outBox.addItem ("> Master", 1);
                outBusIds.add (""); // index 0 == master
                for (int i = 0; i < mixer.getNumBuses(); ++i)
                {
                    auto* ob = mixer.getBus (i);
                    if (ob == nullptr || ob->getKind() == models::Bus::Kind::master)
                        continue;
                    outBusIds.add (ob->getId().toDashedString());
                    outBox.addItem ("> " + ob->name, outBox.getNumItems() + 1);
                }
                const int sel = juce::jmax (0, outBusIds.indexOf (track->outputBusId));
                outBox.setSelectedId (sel + 1, juce::dontSendNotification);
                outBox.onChange = [this]
                {
                    const int idx = outBox.getSelectedId() - 1;
                    if (idx >= 0 && idx < outBusIds.size())
                    {
                        if (context.pushUndo) context.pushUndo();
                        track->outputBusId = outBusIds[idx];
                        if (context.closePluginWindows) context.closePluginWindows();
                        context.engine.rebuildGraph();
                        if (onRoutingChanged) onRoutingChanged();
                    }
                };
            }

            // ── Sidechain key source (feeds any Sidechain Comp inserts) ─────────
            addAndMakeVisible (scBox);
            {
                scBox.addItem ("SC: none", 1);
                scTrackIds.add ("");
                auto& tl = context.project.getTimeline();
                for (int i = 0; i < tl.getNumTracks(); ++i)
                {
                    auto* other = tl.getTrack (i);
                    if (other == nullptr || other == track)
                        continue;
                    scTrackIds.add (other->getId().toDashedString());
                    scBox.addItem ("SC: " + other->name, scBox.getNumItems() + 1);
                }
                const int sel = juce::jmax (0, scTrackIds.indexOf (track->sidechainSourceId));
                scBox.setSelectedId (sel + 1, juce::dontSendNotification);
                scBox.onChange = [this]
                {
                    const int idx = scBox.getSelectedId() - 1;
                    if (idx >= 0 && idx < scTrackIds.size())
                    {
                        if (context.pushUndo) context.pushUndo();
                        track->sidechainSourceId = scTrackIds[idx];
                        if (context.closePluginWindows) context.closePluginWindows();
                        context.engine.rebuildGraph();
                        if (onRoutingChanged) onRoutingChanged();
                    }
                };
            }

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
                lbl->setColour (juce::Label::textColourId, juce::Colour (FreequencyLookAndFeel::textDim));
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

    juce::StringArray* ChannelStrip::insertList() const
    {
        if (role == Role::track && track != nullptr) return &track->insertPluginIdentifiers;
        if (role == Role::bus && bus != nullptr)     return &bus->insertPluginIdentifiers;
        return nullptr;
    }

    juce::AudioProcessor* ChannelStrip::insertProcessor (int slot) const
    {
        if (role == Role::track && track != nullptr) return context.engine.getInsertProcessor (*track, slot);
        if (role == Role::bus && bus != nullptr)     return context.engine.getBusInsertProcessor (*bus, slot);
        return nullptr;
    }

    juce::String ChannelStrip::stripName() const
    {
        if (role == Role::track && track != nullptr) return track->name;
        if (role == Role::bus && bus != nullptr)     return bus->name;
        return "Master";
    }

    void ChannelStrip::buildInsertButtons()
    {
        auto* list = insertList();
        if (list == nullptr) return;

        const auto col = role == Role::track && track != nullptr ? track->colour
                       : bus != nullptr ? bus->colour : juce::Colours::grey;

        for (int i = 0; i < list->size(); ++i)
        {
            const auto id = (*list)[i];
            const auto label = dsp::BuiltinEffects::isBuiltin (id)
                                   ? dsp::BuiltinEffects::displayName (id)
                                   : id.fromLastOccurrenceOf (":", false, false);

            auto* ib = insertButtons.add (new juce::TextButton (label));
            ib->setColour (juce::TextButton::buttonColourId, col.withAlpha (0.35f));
            const int slot = i;
            ib->onClick = [this, slot]
            {
                if (context.openProcessorEditor)
                    context.openProcessorEditor (insertProcessor (slot), stripName());
            };
            addAndMakeVisible (ib);
        }
    }

    void ChannelStrip::showFxMenu()
    {
        auto* list = insertList();
        if (list == nullptr)
            return;

        juce::PopupMenu menu;

        juce::PopupMenu builtin;
        const auto effects = dsp::BuiltinEffects::list();
        for (int i = 0; i < effects.size(); ++i)
            builtin.addItem (1000 + i, effects[i].name);
        menu.addSubMenu ("Add built-in", builtin);

        // Scanned VST3/AU plugins, if any.
        auto plugins = context.engine.getPluginManager().getAllPlugins();
        if (! plugins.isEmpty())
        {
            juce::PopupMenu pluginMenu;
            for (int i = 0; i < plugins.size(); ++i)
                pluginMenu.addItem (2000 + i, plugins[i].name);
            menu.addSubMenu ("Add plugin", pluginMenu);
        }

        if (list->size() > 0)
        {
            menu.addSeparator();
            juce::PopupMenu removeMenu;
            for (int i = 0; i < list->size(); ++i)
            {
                const auto id = (*list)[i];
                const auto label = dsp::BuiltinEffects::isBuiltin (id)
                                       ? dsp::BuiltinEffects::displayName (id)
                                       : id.fromLastOccurrenceOf (":", false, false);
                removeMenu.addItem (3000 + i, label);
            }
            menu.addSubMenu ("Remove", removeMenu);
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fxButton),
            [this, effects, plugins, list] (int result)
            {
                if (result == 0)
                    return;

                if (context.pushUndo) context.pushUndo();
                if (context.closePluginWindows)
                    context.closePluginWindows();

                if (result >= 1000 && result < 2000)
                    list->add (effects[result - 1000].id);
                else if (result >= 2000 && result < 3000)
                    list->add (plugins[result - 2000].createIdentifierString());
                else if (result >= 3000)
                    list->remove (result - 3000);

                context.engine.rebuildGraph();
                if (onRoutingChanged) onRoutingChanged(); // rebuilds the mixer strips
            });
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
        g.fillAll (juce::Colour (role == Role::master ? FreequencyLookAndFeel::panelLight
                                                      : FreequencyLookAndFeel::panel));

        // Colour band at the top for tracks/buses.
        if (role != Role::master)
        {
            auto c = (role == Role::track && track != nullptr) ? track->colour
                   : (bus != nullptr ? bus->colour : juce::Colours::grey);
            g.setColour (c);
            g.fillRect (0, 0, getWidth(), 4);
        }

        g.setColour (juce::Colour (FreequencyLookAndFeel::outline));
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

        // Meter beside the fader (drawn in the reserved strip on the right of the
        // fader area). Bounds computed to match resized().
        auto meterBounds = meterArea;
        g.setColour (juce::Colour (FreequencyLookAndFeel::background));
        g.fillRect (meterBounds);

        const auto level = juce::jlimit (0.0f, 1.0f, meterLevel);
        auto filled = meterBounds.withTop (meterBounds.getBottom() - (int) (meterBounds.getHeight() * level));
        g.setColour (level > 0.9f ? juce::Colour (FreequencyLookAndFeel::danger)
                                  : juce::Colour (FreequencyLookAndFeel::accent));
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
            outBox.setBounds (r.removeFromTop (18));
            r.removeFromTop (3);
            scBox.setBounds (r.removeFromTop (18));
            r.removeFromTop (3);
        }

        // Insert FX list (track + bus strips).
        if (insertList() != nullptr)
        {
            fxButton.setBounds (r.removeFromTop (18));
            for (auto* ib : insertButtons)
                ib->setBounds (r.removeFromTop (16).reduced (0, 1));
            r.removeFromTop (4);
        }

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
} // namespace freequency::ui
