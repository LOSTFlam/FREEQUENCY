#include "Models/Project.h"

namespace freequency::models
{
    Project::Project()
    {
        // Nothing to do beyond constructing the members: a fresh project starts
        // with an empty timeline and a mixer that already contains a master bus.
    }

    Pattern& Project::addPattern()
    {
        auto* pattern = new Pattern();
        pattern->name = "Pattern " + juce::String (patterns.size() + 1);
        patterns.add (pattern);
        return *pattern;
    }

    Pattern* Project::findPattern (const juce::String& dashedId) const noexcept
    {
        for (auto* p : patterns)
            if (p->getId().toDashedString() == dashedId)
                return p;
        return nullptr;
    }
} // namespace freequency::models
