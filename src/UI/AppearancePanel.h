#pragma once

#include "UI/Theme.h"
#include "UI/UiGuideTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace freequency::ui
{
    /**
        AppearancePanel — theme picker + UI Guide / coach-mark settings.
    */
    class AppearancePanel final : public juce::Component
    {
    public:
        std::function<void (const Theme&)> onPick;
        std::function<void (const GuideSettings&)> onGuideSettingsChanged;
        std::function<void()> onReplayTour;
        std::function<void (int pinIndex)> onPlacePin;
        std::function<void (int pinIndex)> onPreviewPin;

        AppearancePanel();

        void setGuideSettings (const GuideSettings& s);

        void paint (juce::Graphics&) override;
        void resized() override;
        void paintOverChildren (juce::Graphics&) override;

        [[nodiscard]] int preferredHeight() const;

    private:
        void syncFromSettings();
        void notifyGuideChanged();

        GuideSettings guideSettings;
        juce::OwnedArray<juce::TextButton> themeButtons;
        juce::Array<juce::Rectangle<int>> swatchAreas;

        juce::ToggleButton animToggle { "Animated spectral effects" };
        juce::ToggleButton closeToggle { "Hints when panels close" };
        juce::TextButton replayTourBtn { "Replay workspace tour" };
        juce::TextButton pin0Btn { "Pin 1 — place" };
        juce::TextButton pin1Btn { "Pin 2 — place" };
        juce::TextButton pin2Btn { "Pin 3 — place" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppearancePanel)
    };
} // namespace freequency::ui
