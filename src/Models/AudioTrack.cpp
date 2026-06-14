#include "Models/AudioTrack.h"

namespace omnidaw::models
{
    AudioTrack::AudioTrack()
        : Track (TrackType::audio)
    {
        name = "Audio Track";
    }

    AudioClip* AudioTrack::addClip()
    {
        auto clip = std::make_unique<AudioClip>();
        auto* raw = static_cast<AudioClip*> (addClipInternal (std::move (clip)));
        return raw;
    }
} // namespace omnidaw::models
