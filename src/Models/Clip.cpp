#include "Models/Clip.h"

namespace omnidaw::models
{
    Clip::Clip (ClipType clipType)
        : type (clipType)
    {
    }

    AudioClip::AudioClip()
        : Clip (ClipType::audio)
    {
        name = "Audio Clip";
    }

    MidiClip::MidiClip()
        : Clip (ClipType::midi)
    {
        name = "MIDI Clip";
    }
} // namespace omnidaw::models
