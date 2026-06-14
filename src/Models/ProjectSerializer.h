#pragma once

#include "Models/Project.h"

namespace omnidaw::models
{
    /**
        ProjectSerializer — converts a Project to/from a juce::ValueTree and an XML
        file on disk.

        The model is plain data, so serialisation lives outside it (here) rather
        than bloating the model classes. Bus identifiers are remapped on load so
        send routing survives a save/load round-trip even though reconstructed
        objects get fresh ObjectIds.
    */
    class ProjectSerializer
    {
    public:
        static juce::ValueTree toValueTree (const Project&);
        static void fromValueTree (const juce::ValueTree&, Project&);

        static bool saveToFile (const Project&, const juce::File&);
        static bool loadFromFile (Project&, const juce::File&);
    };
} // namespace omnidaw::models
