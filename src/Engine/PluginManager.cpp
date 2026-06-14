#include "Engine/PluginManager.h"

namespace freequency::engine
{
    PluginManager::PluginManager()
    {
        registerFormats();
    }

    void PluginManager::registerFormats()
    {
        if (formatManager.getNumFormats() > 0)
            return;

        // VST3 is available on every desktop platform JUCE targets.
       #if JUCE_PLUGINHOST_VST3
        formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());
       #endif

        // AudioUnit is macOS-only.
       #if JUCE_PLUGINHOST_AU && JUCE_MAC
        formatManager.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
       #endif
    }

    int PluginManager::scanDefaultLocations()
    {
        const int before = knownPlugins.getNumTypes();

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat (i);
            if (format == nullptr)
                continue;

            const auto paths = format->getDefaultLocationsToSearch();

            // A dead-man's-pedal file lets the scanner skip a plugin that crashed
            // it on a previous run — important for robustness with 3rd-party code.
            auto deadMansPedal = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("FREEQUENCY_deadmanspedal.txt");

            juce::PluginDirectoryScanner scanner (knownPlugins, *format, paths,
                                                  true, deadMansPedal, true);

            juce::String nameOfPluginBeingScanned;
            while (scanner.scanNextFile (true, nameOfPluginBeingScanned))
            {
            }
        }

        return knownPlugins.getNumTypes() - before;
    }

    int PluginManager::scanFolder (const juce::String& formatName, const juce::File& folder)
    {
        const int before = knownPlugins.getNumTypes();

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat (i);
            if (format == nullptr || format->getName() != formatName)
                continue;

            juce::FileSearchPath path;
            path.add (folder);

            auto deadMansPedal = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("FREEQUENCY_deadmanspedal.txt");

            juce::PluginDirectoryScanner scanner (knownPlugins, *format, path,
                                                  true, deadMansPedal, true);

            juce::String nameOfPluginBeingScanned;
            while (scanner.scanNextFile (true, nameOfPluginBeingScanned))
            {
            }
        }

        return knownPlugins.getNumTypes() - before;
    }

    juce::Array<juce::PluginDescription> PluginManager::getAllPlugins() const
    {
        return knownPlugins.getTypes();
    }

    std::unique_ptr<juce::PluginDescription>
        PluginManager::findDescription (const juce::String& identifierString) const
    {
        return knownPlugins.getTypeForIdentifierString (identifierString);
    }

    std::unique_ptr<juce::AudioPluginInstance>
        PluginManager::createInstance (const juce::String& identifierString,
                                       double sampleRate, int blockSize,
                                       juce::String& error) const
    {
        auto desc = findDescription (identifierString);

        if (desc == nullptr)
        {
            error = "Unknown plugin: " + identifierString;
            return {};
        }

        return formatManager.createPluginInstance (*desc, sampleRate, blockSize, error);
    }

    std::unique_ptr<juce::XmlElement> PluginManager::saveCatalogue() const
    {
        return knownPlugins.createXml();
    }

    void PluginManager::restoreCatalogue (const juce::XmlElement& xml)
    {
        knownPlugins.recreateFromXml (xml);
    }
} // namespace freequency::engine
