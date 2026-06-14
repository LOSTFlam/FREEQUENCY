#pragma once

#include <juce_graphics/juce_graphics.h>

namespace freequency::ui
{
    /**
        Theme — the runtime colour palette for the whole UI.

        Every component draws from the *current* Theme (via theme()), so switching
        themes in the Appearance settings re-skins the entire app live. Themes are
        named presets; the selection persists across launches.
    */
    struct Theme
    {
        juce::String name { "Freequency Teal" };
        juce::Colour background  { 0xff15171c };
        juce::Colour panel       { 0xff1d2027 };
        juce::Colour panelLight  { 0xff262a33 };
        juce::Colour outline     { 0xff31363f };
        juce::Colour textPrimary { 0xffe8eaed };
        juce::Colour textDim     { 0xff9aa0aa };
        juce::Colour accent      { 0xff2dd4bf };
        juce::Colour accentWarm  { 0xfff59e0b };
        juce::Colour danger      { 0xffef4444 };
    };

    /** The current, live theme. */
    Theme& theme();

    /** Replace the current theme (does not repaint — callers refresh the UI). */
    void setTheme (const Theme&);

    /** Built-in presets shown in the Appearance settings. */
    const juce::Array<Theme>& themePresets();

    /** Persist / restore the chosen theme by name. */
    void saveThemeSelection();
    void loadThemeSelection();
} // namespace freequency::ui
