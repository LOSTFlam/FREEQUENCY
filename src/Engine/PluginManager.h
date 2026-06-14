#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace freequency::engine
{
    /**
        PluginManager — discovery and instantiation of VST3 / AU plugins.

        Wraps a juce::AudioPluginFormatManager (which knows how to load each
        format) and a juce::KnownPluginList (the catalogue of plugins the user has
        scanned). The data models never hold live plugin pointers — they store a
        plugin's identifier string — so this class is the single place that turns
        an identifier back into a runnable juce::AudioPluginInstance.

        Scanning is done on the message thread; instantiation happens during a
        graph rebuild (also message thread, with rendering suspended).
    */
    class PluginManager
    {
    public:
        PluginManager();

        /** Registers VST3 (all platforms) and AudioUnit (macOS) formats. */
        void registerFormats();

        /** Scans the OS default plugin locations for every registered format and
            adds anything found to the known-plugin list. Returns the number of
            newly discovered plugins. Safe to call repeatedly.
        */
        int scanDefaultLocations();

        /** Scans a specific folder (recursively) for the given format name. */
        int scanFolder (const juce::String& formatName, const juce::File& folder);

        [[nodiscard]] juce::KnownPluginList& getKnownPluginList() noexcept { return knownPlugins; }
        [[nodiscard]] juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

        /** All scanned plugin descriptions (instruments + effects). */
        [[nodiscard]] juce::Array<juce::PluginDescription> getAllPlugins() const;

        /** Resolve an identifier string (as stored in the model) to a description. */
        [[nodiscard]] std::unique_ptr<juce::PluginDescription>
            findDescription (const juce::String& identifierString) const;

        /** Instantiate a plugin by identifier. Returns nullptr (and sets `error`)
            if the plugin is unknown or fails to load.
        */
        std::unique_ptr<juce::AudioPluginInstance>
            createInstance (const juce::String& identifierString,
                            double sampleRate, int blockSize,
                            juce::String& error) const;

        /** Persist / restore the scanned catalogue (so we don't rescan each run). */
        [[nodiscard]] std::unique_ptr<juce::XmlElement> saveCatalogue() const;
        void restoreCatalogue (const juce::XmlElement& xml);

    private:
        juce::AudioPluginFormatManager formatManager;
        juce::KnownPluginList knownPlugins;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManager)
    };
} // namespace freequency::engine
