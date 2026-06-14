#pragma once

#include "UI/UiGuideTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

namespace freequency::ui
{
    /**
        UiGuideOverlay — full-screen spectral coach layer.

        Draws dimmed glass, animated spotlight rings, aurora shimmer, fly-beams
        and a sliding callout. Sits above the whole workspace; only the callout
        buttons intercept clicks.
    */
    class UiGuideOverlay final : public juce::Component,
                                 private juce::Timer
    {
    public:
        std::function<void()> onDismiss;
        std::function<void()> onNext;

        UiGuideOverlay();

        void showHint (const GuideHint& hint,
                       juce::Rectangle<int> targetGlobal,
                       juce::Point<int> flyFromGlobal = {});

        void dismiss();
        [[nodiscard]] bool isShowing() const noexcept { return showing; }

        void setAnimationsEnabled (bool e) { animationsEnabled = e; }

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        void layoutCallout();
        void drawDimWithCutout (juce::Graphics&, juce::Rectangle<float> hole);
        void drawEffect (juce::Graphics&, juce::Rectangle<float> target);
        void drawFlyBeam (juce::Graphics&);
        void drawCallout (juce::Graphics&);

        bool showing { false };
        bool animationsEnabled { true };
        GuideHint current {};
        juce::Rectangle<int> targetRect;
        juce::Point<int> flyFrom;
        bool hasFlyFrom { false };

        float phase { 0.0f };
        float flyT { 0.0f };
        float calloutT { 0.0f };
        int holdCounter { 0 };

        juce::TextButton gotIt { "Got it" };
        juce::TextButton nextBtn { "Next" };
        juce::Rectangle<int> calloutBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UiGuideOverlay)
    };
} // namespace freequency::ui
