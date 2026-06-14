#pragma once

#include "Models/Track.h"

#include <vector>

namespace freequency::models
{
    /**
        VCATrack — a Cubase-style VCA fader.

        A VCA track carries no audio. Its fader scales the *effective* gain of
        every track assigned to it, letting an engineer ride a whole group's
        balance from one fader without altering the members' relative levels or
        their own automation. VCAs can be nested (a VCA can control another VCA).

        The model stores the ids of controlled tracks (as dashed UUID strings so
        the relationship survives serialisation). The engine multiplies each
        controlled track's gain by the product of its controlling VCAs (Phase 2
        routing logic).
    */
    class VCATrack final : public Track
    {
    public:
        VCATrack() : Track (TrackType::vca) { name = "VCA"; }

        [[nodiscard]] juce::String getTypeName() const override { return "VCA"; }

        void addControlledTrack (const juce::String& trackId)
        {
            if (! controlledTrackIds.contains (trackId))
            {
                controlledTrackIds.add (trackId);
                sendChangeMessage();
            }
        }

        void removeControlledTrack (const juce::String& trackId)
        {
            controlledTrackIds.removeString (trackId);
            sendChangeMessage();
        }

        [[nodiscard]] const juce::StringArray& getControlledTrackIds() const noexcept
        {
            return controlledTrackIds;
        }

    private:
        juce::StringArray controlledTrackIds; // dashed UUID strings
    };
} // namespace freequency::models
