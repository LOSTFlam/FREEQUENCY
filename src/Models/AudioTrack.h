#pragma once

#include "Models/Track.h"

namespace omnidaw::models
{
    /**
        AudioTrack — a track that hosts AudioClips and a chain of audio effects.

        In Cubase terms this is a linear audio lane; in FL terms it is an audio
        clip track in the playlist. The engine represents it as a TrackProcessor
        node whose input is the summed clip playback (Phase 5) and whose insert
        chain is a series of VST3 effect nodes (Phase 2).
    */
    class AudioTrack final : public Track
    {
    public:
        AudioTrack();

        [[nodiscard]] juce::String getTypeName() const override { return "Audio"; }

        /** Creates and appends a new AudioClip, returning a non-owning pointer. */
        AudioClip* addClip();

        /** The hardware/engine input channel pair this track records from.
            -1 means "no input assigned". Used by Phase 5 disk recording.
        */
        int inputChannelIndex { -1 };

        /** When true the track is armed for recording. */
        bool recordEnabled { false };
    };
} // namespace omnidaw::models
