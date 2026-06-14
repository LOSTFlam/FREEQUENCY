#include "Models/Clip.h"

namespace freequency::models
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

    PatternClip::PatternClip()
        : Clip (ClipType::pattern)
    {
        name = "Pattern Clip";
    }
} // namespace freequency::models
