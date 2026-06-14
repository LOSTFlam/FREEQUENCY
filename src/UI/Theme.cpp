#include "UI/Theme.h"

#include <juce_core/juce_core.h>

namespace freequency::ui
{
    namespace
    {
        Theme current; // the live theme

        juce::File themeFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("FREEQUENCY").getChildFile ("theme.xml");
        }
    }

    Theme& theme() { return current; }
    void setTheme (const Theme& t) { current = t; }

    const juce::Array<Theme>& themePresets()
    {
        static const juce::Array<Theme> presets = []
        {
            juce::Array<Theme> p;

            Theme teal; // default
            p.add (teal);

            Theme magenta;
            magenta.name = "Magenta Pulse";
            magenta.background = juce::Colour (0xff17141c);
            magenta.panel = juce::Colour (0xff211b29);
            magenta.panelLight = juce::Colour (0xff2c2438);
            magenta.outline = juce::Colour (0xff3a3048);
            magenta.accent = juce::Colour (0xffe879f9);
            magenta.accentWarm = juce::Colour (0xfffb7185);
            p.add (magenta);

            Theme amber;
            amber.name = "Amber Studio";
            amber.background = juce::Colour (0xff1a1714);
            amber.panel = juce::Colour (0xff241f19);
            amber.panelLight = juce::Colour (0xff2f2820);
            amber.outline = juce::Colour (0xff43392c);
            amber.accent = juce::Colour (0xfff5a623);
            amber.accentWarm = juce::Colour (0xffffd166);
            p.add (amber);

            Theme aurora;
            aurora.name = "Aurora";
            aurora.background = juce::Colour (0xff0f1518);
            aurora.panel = juce::Colour (0xff15211f);
            aurora.panelLight = juce::Colour (0xff1c2b28);
            aurora.outline = juce::Colour (0xff294039);
            aurora.accent = juce::Colour (0xff34d399);
            aurora.accentWarm = juce::Colour (0xff60a5fa);
            p.add (aurora);

            Theme spectral;
            spectral.name = "Spectral";
            spectral.background = juce::Colour (0xff0a0e14);
            spectral.panel = juce::Colour (0xff121820);
            spectral.panelLight = juce::Colour (0xff1a2230);
            spectral.outline = juce::Colour (0xff2a3548);
            spectral.textPrimary = juce::Colour (0xffeef2ff);
            spectral.textDim = juce::Colour (0xff8b95a8);
            spectral.accent = juce::Colour (0xff2dd4bf);
            spectral.accentWarm = juce::Colour (0xffa78bfa);
            spectral.danger = juce::Colour (0xffff6b8a);
            p.add (spectral);

            Theme mono;
            mono.name = "Mono Graphite";
            mono.background = juce::Colour (0xff141416);
            mono.panel = juce::Colour (0xff1d1d20);
            mono.panelLight = juce::Colour (0xff27272b);
            mono.outline = juce::Colour (0xff35353a);
            mono.accent = juce::Colour (0xffd4d4d8);
            mono.accentWarm = juce::Colour (0xfffacc15);
            p.add (mono);

            Theme light;
            light.name = "Daylight";
            light.background = juce::Colour (0xffeceef2);
            light.panel = juce::Colour (0xfff6f7f9);
            light.panelLight = juce::Colour (0xffffffff);
            light.outline = juce::Colour (0xffd0d4da);
            light.textPrimary = juce::Colour (0xff20242b);
            light.textDim = juce::Colour (0xff6b727d);
            light.accent = juce::Colour (0xff0ea5a3);
            light.accentWarm = juce::Colour (0xffd97706);
            light.danger = juce::Colour (0xffdc2626);
            p.add (light);

            return p;
        }();

        return presets;
    }

    void saveThemeSelection()
    {
        juce::XmlElement xml ("FreequencyTheme");
        xml.setAttribute ("name", current.name);
        const auto f = themeFile();
        f.getParentDirectory().createDirectory();
        xml.writeTo (f);
    }

    void loadThemeSelection()
    {
        if (auto xml = juce::XmlDocument::parse (themeFile()))
        {
            const auto name = xml->getStringAttribute ("name");
            for (const auto& t : themePresets())
                if (t.name == name)
                {
                    current = t;
                    return;
                }
        }
    }
} // namespace freequency::ui
