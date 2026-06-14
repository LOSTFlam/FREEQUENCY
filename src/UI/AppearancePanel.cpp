#include "UI/AppearancePanel.h"

namespace freequency::ui
{
    AppearancePanel::AppearancePanel()
    {
        for (int i = 0; i < themePresets().size(); ++i)
        {
            const auto preset = themePresets()[i];
            auto* b = themeButtons.add (new juce::TextButton (preset.name));
            b->onClick = [this, preset] { if (onPick) onPick (preset); repaint(); };
            addAndMakeVisible (b);
        }

        animToggle.onClick = [this] { notifyGuideChanged(); };
        closeToggle.onClick = [this] { notifyGuideChanged(); };
        addAndMakeVisible (animToggle);
        addAndMakeVisible (closeToggle);

        replayTourBtn.onClick = [this] { if (onReplayTour) onReplayTour(); };
        addAndMakeVisible (replayTourBtn);

        for (auto* btn : { &pin0Btn, &pin1Btn, &pin2Btn })
        {
            addAndMakeVisible (*btn);
            btn->onClick = [this, btn]
            {
                const int idx = (&pin0Btn == btn ? 0 : (&pin1Btn == btn ? 1 : 2));
                if (juce::ModifierKeys::getCurrentModifiers().isShiftDown())
                {
                    if (onPreviewPin) onPreviewPin (idx);
                }
                else if (onPlacePin)
                {
                    onPlacePin (idx);
                }
            };
        }
    }

    void AppearancePanel::setGuideSettings (const GuideSettings& s)
    {
        guideSettings = s;
        syncFromSettings();
    }

    void AppearancePanel::syncFromSettings()
    {
        animToggle.setToggleState (guideSettings.animationsEnabled, juce::dontSendNotification);
        closeToggle.setToggleState (guideSettings.panelCloseHints, juce::dontSendNotification);
    }

    void AppearancePanel::notifyGuideChanged()
    {
        guideSettings.animationsEnabled = animToggle.getToggleState();
        guideSettings.panelCloseHints   = closeToggle.getToggleState();
        if (onGuideSettingsChanged) onGuideSettingsChanged (guideSettings);
    }

    int AppearancePanel::preferredHeight() const
    {
        return 24 + themePresets().size() * 40 + 24 + 160;
    }

    void AppearancePanel::paint (juce::Graphics& g)
    {
        g.fillAll (theme().panel);

        g.setColour (theme().textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("THEME", getLocalBounds().removeFromTop (24).reduced (12, 0),
                    juce::Justification::centredLeft, false);

        const int themeRows = themePresets().size() * 40;
        g.drawText ("UI GUIDE", juce::Rectangle<int> (12, 24 + themeRows + 8, getWidth() - 24, 20),
                    juce::Justification::centredLeft, false);

        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("Shift+click a pin to preview. Click then tap the workspace to place.",
                    juce::Rectangle<int> (12, getHeight() - 28, getWidth() - 24, 24),
                    juce::Justification::centredLeft, true);
    }

    void AppearancePanel::resized()
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (24);

        for (int i = 0; i < themeButtons.size(); ++i)
        {
            auto row = r.removeFromTop (40).reduced (0, 4);
            themeButtons[i]->setBounds (row.removeFromLeft (180));
            swatchAreas.set (i, row.reduced (6, 4));
        }

        r.removeFromTop (28); // "UI GUIDE" label
        animToggle.setBounds (r.removeFromTop (26));
        closeToggle.setBounds (r.removeFromTop (26));
        r.removeFromTop (6);
        replayTourBtn.setBounds (r.removeFromTop (32).reduced (0, 2));
        r.removeFromTop (6);

        auto pinRow = r.removeFromTop (32);
        const int pw = (pinRow.getWidth() - 8) / 3;
        pin0Btn.setBounds (pinRow.removeFromLeft (pw));
        pinRow.removeFromLeft (4);
        pin1Btn.setBounds (pinRow.removeFromLeft (pw));
        pinRow.removeFromLeft (4);
        pin2Btn.setBounds (pinRow);

        repaint();
    }

    void AppearancePanel::paintOverChildren (juce::Graphics& g)
    {
        for (int i = 0; i < themeButtons.size(); ++i)
        {
            const auto t = themePresets()[i];
            auto area = swatchAreas[i];
            if (area.isEmpty()) continue;
            const juce::Colour cols[] = { t.background, t.panelLight, t.accent, t.accentWarm };
            const int n = 4;
            const float w = area.getWidth() / (float) n;
            for (int c = 0; c < n; ++c)
            {
                g.setColour (cols[c]);
                g.fillRect ((float) area.getX() + c * w, (float) area.getY(), w, (float) area.getHeight());
            }
        }
    }
} // namespace freequency::ui
