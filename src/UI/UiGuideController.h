#pragma once

#include "UI/UiGuideOverlay.h"
#include "UI/UiGuideTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>

namespace freequency::ui
{
    class TransportBar;

    /**
        UiGuideController — orchestrates tours, panel-close fly hints and custom pins.

        Anchor resolution is supplied by MainComponent so hints track real control
        positions as the layout changes.
    */
    class UiGuideController final
    {
    public:
        using AnchorResolver = std::function<std::optional<juce::Rectangle<int>> (GuideAnchor)>;

        explicit UiGuideController (juce::Component& host);

        void setAnchorResolver (AnchorResolver r) { anchorResolver = std::move (r); }

        GuideSettings& settings() noexcept { return guideSettings; }
        [[nodiscard]] const GuideSettings& settings() const noexcept { return guideSettings; }

        void load();
        void save();

        void attachOverlay();
        void resizeToHost();
        void applySettings();

        /** Show a single hint at an anchor. */
        void showHint (const GuideHint& hint, juce::Point<int> flyFromGlobal = {});

        /** After closing a floating panel — beam to the matching toolbar control. */
        void showPanelClosedHint (GuideAnchor anchor,
                                  const juce::String& title,
                                  const juce::String& body);

        void startWelcomeTour();
        void replayTour();
        void showCustomPin (int index);

        void setPinPlacementMode (int pinIndex, bool enabled);
        [[nodiscard]] bool isPlacingPin() const noexcept { return placingPinIndex >= 0; }

        /** Call from host mouseDown when placing a custom pin. Returns true if consumed. */
        bool handlePlacementClick (juce::Point<int> hostLocal);

    private:
        void showTourIndex (int idx);
        std::optional<juce::Rectangle<int>> resolveAnchor (GuideAnchor anchor) const;
        GuideHint hintForCustomPin (int index) const;

        juce::Component& host;
        std::unique_ptr<UiGuideOverlay> overlay;
        AnchorResolver anchorResolver;

        GuideSettings guideSettings;
        juce::Array<GuideHint> tour;
        int tourIndex { -1 };
        int placingPinIndex { -1 };
    };
} // namespace freequency::ui
