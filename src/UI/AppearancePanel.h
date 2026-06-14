#pragma once

#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace freequency::ui
{
    /**
        AppearancePanel — the theme picker shown in the Appearance settings window.
        Lists the built-in theme presets as preview swatches; clicking one applies
        it live (via onPick).
    */
    class AppearancePanel final : public juce::Component
    {
    public:
        std::function<void (const Theme&)> onPick;

        AppearancePanel()
        {
            for (int i = 0; i < themePresets().size(); ++i)
            {
                const auto preset = themePresets()[i];
                auto* b = buttons.add (new juce::TextButton (preset.name));
                b->onClick = [this, preset] { if (onPick) onPick (preset); repaint(); };
                addAndMakeVisible (b);
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme().panel);
            g.setColour (theme().textDim);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("THEME", getLocalBounds().removeFromTop (24).reduced (12, 0),
                        juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12);
            r.removeFromTop (24);
            for (int i = 0; i < buttons.size(); ++i)
            {
                auto row = r.removeFromTop (40).reduced (0, 4);
                buttons[i]->setBounds (row.removeFromLeft (180));

                // Swatch preview to the right of each button.
                swatchAreas.set (i, row.reduced (6, 4));
            }
            repaint();
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            for (int i = 0; i < buttons.size(); ++i)
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

    private:
        juce::OwnedArray<juce::TextButton> buttons;
        juce::Array<juce::Rectangle<int>> swatchAreas;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppearancePanel)
    };
} // namespace freequency::ui
