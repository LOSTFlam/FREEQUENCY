#include "Models/MidiTrack.h"

namespace omnidaw::models
{
    MidiTrack::MidiTrack()
        : Track (TrackType::midi)
    {
        name = "MIDI Track";
    }

    MidiClip* MidiTrack::addClip()
    {
        auto clip = std::make_unique<MidiClip>();
        auto* raw = static_cast<MidiClip*> (addClipInternal (std::move (clip)));
        return raw;
    }
} // namespace omnidaw::models
