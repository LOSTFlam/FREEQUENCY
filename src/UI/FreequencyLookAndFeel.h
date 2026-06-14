#pragma once

#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace freequency::ui
{
    /**
        FreequencyLookAndFeel — the application's JUCE LookAndFeel, driven by the
        runtime Theme. Call applyTheme() after the theme changes to re-skin all
        standard widgets; custom components read theme() directly when painting.
    */
    class FreequencyLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        FreequencyLookAndFeel();

        /** Re-read the current theme into the LookAndFeel colour scheme. */
        void applyTheme();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;

        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&,
                                   const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;
    };
} // namespace freequency::ui
