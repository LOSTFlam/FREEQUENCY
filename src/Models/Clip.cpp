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

    void AudioClip::ensureDefaultCompRegion()
    {
        if (getNumTakes() < 2 || ! compSwipeRegions.empty())
            return;

        CompSwipeRegion region;
        region.startTime = 0.0;
        region.length = length > 0.0 ? length : 4.0;
        region.takeA = 0;
        region.takeB = juce::jmin (1, getNumTakes() - 1);
        region.crossfadePosition = 0.5f;
        compSwipeRegions.push_back (region);
    }
} // namespace freequency::models
