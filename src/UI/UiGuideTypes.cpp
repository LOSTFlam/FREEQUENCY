#include "UI/UiGuideTypes.h"

#include <juce_core/juce_core.h>

namespace freequency::ui
{
    namespace
    {
        juce::File guideSettingsFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("FREEQUENCY").getChildFile ("guide_settings.xml");
        }

        juce::String effectName (GuideEffect e)
        {
            switch (e)
            {
                case GuideEffect::pulseRing:   return "pulseRing";
                case GuideEffect::auroraGlow:  return "auroraGlow";
                case GuideEffect::spotlight:   return "spotlight";
                case GuideEffect::flyBeam:     return "flyBeam";
                case GuideEffect::ripple:      return "ripple";
                case GuideEffect::shimmer:     return "shimmer";
                default:                       return "auroraGlow";
            }
        }

        GuideEffect effectFromName (const juce::String& n)
        {
            if (n == "pulseRing")  return GuideEffect::pulseRing;
            if (n == "spotlight")  return GuideEffect::spotlight;
            if (n == "flyBeam")    return GuideEffect::flyBeam;
            if (n == "ripple")     return GuideEffect::ripple;
            if (n == "shimmer")    return GuideEffect::shimmer;
            return GuideEffect::auroraGlow;
        }
    }

    void saveGuideSettings (const GuideSettings& s)
    {
        juce::XmlElement root ("GuideSettings");
        root.setAttribute ("animations", s.animationsEnabled ? 1 : 0);
        root.setAttribute ("panelCloseHints", s.panelCloseHints ? 1 : 0);
        root.setAttribute ("welcomeCompleted", s.welcomeCompleted ? 1 : 0);

        for (int i = 0; i < 3; ++i)
        {
            const auto& p = s.customPins[(size_t) i];
            auto* pin = root.createNewChildElement ("Pin");
            pin->setAttribute ("index", i);
            pin->setAttribute ("x", p.normX);
            pin->setAttribute ("y", p.normY);
            pin->setAttribute ("title", p.title);
            pin->setAttribute ("body", p.body);
            pin->setAttribute ("effect", effectName (p.effect));
        }

        const auto f = guideSettingsFile();
        f.getParentDirectory().createDirectory();
        root.writeTo (f);
    }

    void loadGuideSettings (GuideSettings& s)
    {
        if (auto xml = juce::XmlDocument::parse (guideSettingsFile()))
        {
            s.animationsEnabled = xml->getBoolAttribute ("animations", true);
            s.panelCloseHints     = xml->getBoolAttribute ("panelCloseHints", true);
            s.welcomeCompleted    = xml->getBoolAttribute ("welcomeCompleted", false);

            for (auto* pin : xml->getChildWithTagNameIterator ("Pin"))
            {
                const int idx = pin->getIntAttribute ("index", -1);
                if (idx < 0 || idx > 2) continue;
                auto& p = s.customPins[(size_t) idx];
                p.normX   = (float) pin->getDoubleAttribute ("x", 0.5);
                p.normY   = (float) pin->getDoubleAttribute ("y", 0.5);
                p.title   = pin->getStringAttribute ("title", p.title);
                p.body    = pin->getStringAttribute ("body", p.body);
                p.effect  = effectFromName (pin->getStringAttribute ("effect", "shimmer"));
            }
        }
    }

    juce::Array<GuideHint> defaultWelcomeTour()
    {
        juce::Array<GuideHint> tour;

        {
            GuideHint h;
            h.anchor = GuideAnchor::transportPlay;
            h.effect = GuideEffect::pulseRing;
            h.title  = "Transport";
            h.body   = "Play, stop, record and loop live here. Space toggles playback.";
            h.holdMs = 4500;
            tour.add (h);
        }
        {
            GuideHint h;
            h.anchor = GuideAnchor::arrangeView;
            h.effect = GuideEffect::auroraGlow;
            h.title  = "Arrange";
            h.body   = "Clips, comping and swipe crossfades live on the timeline lanes.";
            h.holdMs = 5000;
            tour.add (h);
        }
        {
            GuideHint h;
            h.anchor = GuideAnchor::transportMixer;
            h.effect = GuideEffect::spotlight;
            h.title  = "Mixer";
            h.body   = "Press Mixer or B to swap between arrange and channel strips.";
            h.holdMs = 4500;
            tour.add (h);
        }
        {
            GuideHint h;
            h.anchor = GuideAnchor::transportBrowser;
            h.effect = GuideEffect::ripple;
            h.title  = "Browser";
            h.body   = "F2 opens samples and media on the left. Drag files onto audio tracks.";
            h.holdMs = 4500;
            tour.add (h);
        }
        {
            GuideHint h;
            h.anchor = GuideAnchor::transportTheme;
            h.effect = GuideEffect::shimmer;
            h.title  = "Spectral themes";
            h.body   = "F3 opens Appearance — pick Spectral and pin custom coach marks anywhere.";
            h.holdMs = 5000;
            tour.add (h);
        }

        return tour;
    }
} // namespace freequency::ui
