#include "Models/MidiTrack.h"

namespace freequency::models
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

    PatternClip* MidiTrack::addPatternClip()
    {
        auto clip = std::make_unique<PatternClip>();
        auto* raw = static_cast<PatternClip*> (addClipInternal (std::move (clip)));
        return raw;
    }
} // namespace freequency::models
