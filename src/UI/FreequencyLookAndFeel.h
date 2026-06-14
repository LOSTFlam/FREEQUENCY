#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace freequency::ui
{
    /**
        FreequencyLookAndFeel — the application's visual identity.

        A single dark, modern theme applied app-wide. Centralising colours here
        keeps every component consistent and makes re-skinning trivial. The
        palette favours a near-black backdrop with a teal accent so meters,
        playheads and selections pop.
    */
    class FreequencyLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        FreequencyLookAndFeel();

        // Shared palette (used by custom paint routines across the UI).
        static constexpr juce::uint32 background   = 0xff15171c;
        static constexpr juce::uint32 panel        = 0xff1d2027;
        static constexpr juce::uint32 panelLight   = 0xff262a33;
        static constexpr juce::uint32 outline      = 0xff31363f;
        static constexpr juce::uint32 textPrimary  = 0xffe8eaed;
        static constexpr juce::uint32 textDim      = 0xff9aa0aa;
        static constexpr juce::uint32 accent       = 0xff2dd4bf;  // teal
        static constexpr juce::uint32 accentWarm   = 0xfff59e0b;  // amber (record/solo)
        static constexpr juce::uint32 danger       = 0xffef4444;

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
