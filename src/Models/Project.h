#pragma once

#include "Models/Timeline.h"
#include "Models/Mixer.h"

namespace freequency::models
{
    /**
        Project — the document root of FREEQUENCY.

        Aggregates the two top-level models the rest of the app hangs off:
          - Timeline : tracks, clips and the musical grid.
          - Mixer    : the bus/routing topology.

        The Project is owned by the application; the AudioEngine takes a reference
        to it to build the audio graph, and the (future) UI observes it. There is
        deliberately no engine or UI state stored here — this is the serialisable
        document, nothing more.
    */
    class Project
    {
    public:
        Project();

        [[nodiscard]] Timeline& getTimeline() noexcept { return timeline; }
        [[nodiscard]] const Timeline& getTimeline() const noexcept { return timeline; }

        [[nodiscard]] Mixer& getMixer() noexcept { return mixer; }
        [[nodiscard]] const Mixer& getMixer() const noexcept { return mixer; }

        juce::String name { "Untitled Project" };

        /** Path the project was last saved to (empty if never saved). */
        juce::File file;

    private:
        Timeline timeline;
        Mixer mixer;
    };
} // namespace freequency::models
