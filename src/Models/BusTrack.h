#pragma once

#include "Models/Track.h"

namespace freequency::models
{
    /**
        BusTrack — a summing/group track (Cubase "Group Channel").

        Other tracks route their output to a BusTrack instead of straight to the
        master, so a whole stem (e.g. all drums) can be processed and faded
        together. It behaves like any other channel strip (volume/pan/mute/solo +
        insert FX) but takes audio from its members rather than from clips.

        The model just declares the bus; the engine realises the summing in the
        routing matrix (Phase 2). Buses also exist in `Mixer` for the master/FX
        topology — BusTrack is the *arrangement-visible* group track that can hold
        clips of automation and appear in the timeline.
    */
    class BusTrack final : public Track
    {
    public:
        BusTrack() : Track (TrackType::bus) { name = "Group"; }

        [[nodiscard]] juce::String getTypeName() const override { return "Bus"; }
    };
} // namespace freequency::models
