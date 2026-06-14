#include "UI/TrackHeaderComponent.h"
#include "UI/OmniLookAndFeel.h"

namespace omnidaw::ui
{
    TrackHeaderComponent::TrackHeaderComponent (UIContext& ctx, models::Track& track)
        : context (ctx), trackRef (track)
    {
        addAndMakeVisible (nameLabel);
        nameLabel.setText (track.name, juce::dontSendNotification);
        nameLabel.setEditable (true);
        nameLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        nameLabel.onTextChange = [this] { trackRef.name = nameLabel.getText(); };

        addAndMakeVisible (typeLabel);
        typeLabel.setText (track.getTypeName(), juce::dontSendNotification);
        typeLabel.setFont (juce::FontOptions (10.0f));
        typeLabel.setColour (juce::Label::textColourId, juce::Colour (OmniLookAndFeel::textDim));

        addAndMakeVisible (muteButton);
        muteButton.setClickingTogglesState (true);
        muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (OmniLookAndFeel::danger));
        muteButton.onClick = [this]
        {
            trackRef.setMuted (muteButton.getToggleState());
            context.engine.syncParametersFromModel();
        };

        addAndMakeVisible (soloButton);
        soloButton.setClickingTogglesState (true);
        soloButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (OmniLookAndFeel::accentWarm));
        soloButton.onClick = [this]
        {
            trackRef.setSoloed (soloButton.getToggleState());
            context.engine.syncParametersFromModel();
        };

        addAndMakeVisible (volumeSlider);
        volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volumeSlider.setRange (0.0, 1.5, 0.001);
        volumeSlider.setValue (track.getVolume(), juce::dontSendNotification);
        volumeSlider.onValueChange = [this]
        {
            trackRef.setVolume ((float) volumeSlider.getValue());
            context.engine.syncParametersFromModel();
        };

        addAndMakeVisible (panSlider);
        panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        panSlider.setRange (-1.0, 1.0, 0.01);
        panSlider.setValue (track.getPan(), juce::dontSendNotification);
        panSlider.onValueChange = [this]
        {
            trackRef.setPan ((float) panSlider.getValue());
            context.engine.syncParametersFromModel();
        };
    }

    void TrackHeaderComponent::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (OmniLookAndFeel::panel));

        // Colour swatch down the left edge identifies the track at a glance.
        g.setColour (trackRef.colour);
        g.fillRect (0, 0, 5, getHeight());

        g.setColour (juce::Colour (OmniLookAndFeel::outline));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void TrackHeaderComponent::resized()
    {
        auto r = getLocalBounds().reduced (10, 6);
        r.removeFromLeft (4);

        auto topRow = r.removeFromTop (20);
        typeLabel.setBounds (topRow.removeFromRight (40));
        nameLabel.setBounds (topRow);

        r.removeFromTop (4);
        auto midRow = r.removeFromTop (22);
        muteButton.setBounds (midRow.removeFromLeft (28).reduced (1));
        midRow.removeFromLeft (4);
        soloButton.setBounds (midRow.removeFromLeft (28).reduced (1));
        midRow.removeFromLeft (8);
        panSlider.setBounds (midRow.removeFromRight (30));

        r.removeFromTop (4);
        volumeSlider.setBounds (r.removeFromTop (16));
    }
} // namespace omnidaw::ui
