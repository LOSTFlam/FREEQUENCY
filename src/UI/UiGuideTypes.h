#pragma once

#include <juce_graphics/juce_graphics.h>

namespace freequency::ui
{
    /** Named UI regions the guide can point at. */
    enum class GuideAnchor
    {
        transportPlay,
        transportMixer,
        transportBrowser,
        transportTheme,
        transportAudio,
        transportKeys,
        transportAddTrack,
        arrangeView,
        statusBar,
        customPin0,
        customPin1,
        customPin2
    };

    /** Visual treatment for a hint. */
    enum class GuideEffect
    {
        pulseRing,
        auroraGlow,
        spotlight,
        flyBeam,
        ripple,
        shimmer
    };

    inline juce::String guideAnchorName (GuideAnchor a)
    {
        switch (a)
        {
            case GuideAnchor::transportPlay:      return "transportPlay";
            case GuideAnchor::transportMixer:     return "transportMixer";
            case GuideAnchor::transportBrowser:   return "transportBrowser";
            case GuideAnchor::transportTheme:     return "transportTheme";
            case GuideAnchor::transportAudio:     return "transportAudio";
            case GuideAnchor::transportKeys:      return "transportKeys";
            case GuideAnchor::transportAddTrack:  return "transportAddTrack";
            case GuideAnchor::arrangeView:        return "arrangeView";
            case GuideAnchor::statusBar:          return "statusBar";
            case GuideAnchor::customPin0:         return "customPin0";
            case GuideAnchor::customPin1:         return "customPin1";
            case GuideAnchor::customPin2:         return "customPin2";
            default:                              return {};
        }
    }

    inline GuideAnchor guideAnchorFromName (const juce::String& name)
    {
        for (int i = 0; i <= (int) GuideAnchor::customPin2; ++i)
        {
            const auto a = (GuideAnchor) i;
            if (guideAnchorName (a) == name)
                return a;
        }
        return GuideAnchor::arrangeView;
    }

    /** One coach-mark card. */
    struct GuideHint
    {
        GuideAnchor  anchor { GuideAnchor::arrangeView };
        GuideEffect  effect { GuideEffect::auroraGlow };
        juce::String title;
        juce::String body;
        /** Normalised position within MainComponent when anchor is customPin*. */
        float normX { 0.5f };
        float normY { 0.5f };
        int   holdMs { 4200 };
    };

    /** User preferences for the guide system. */
    struct GuideSettings
    {
        bool animationsEnabled { true };
        bool panelCloseHints   { true };
        bool welcomeCompleted  { false };

        struct CustomPin
        {
            float normX { 0.5f };
            float normY { 0.5f };
            juce::String title { "Custom hint" };
            juce::String body  { "Drag pins in Appearance to reposition." };
            GuideEffect effect { GuideEffect::shimmer };
        };

        CustomPin customPins[3];
    };

    void saveGuideSettings (const GuideSettings&);
    void loadGuideSettings (GuideSettings&);
    juce::Array<GuideHint> defaultWelcomeTour();

} // namespace freequency::ui
