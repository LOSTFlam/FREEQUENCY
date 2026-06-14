#pragma once

#include "Core/Types.h"
#include "Models/HarmonicEdit.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace freequency::engine
{
    /**
        AudioClipSnapshot — immutable, audio-thread-ready view of an audio track's
        clips, with their sample data pre-loaded and resampled to the engine rate.

        Phase 1/2 deliberately PRE-LOAD clip audio into memory rather than
        streaming from disk on a reader thread. Preloading keeps the audio thread
        trivially real-time safe (it just mixes from existing buffers) and is
        plenty for typical project sizes. Disk streaming for very long files is a
        Phase 5 refinement; the snapshot abstraction here won't have to change.

        The snapshot OWNS its audio buffers, so a region simply indexes into
        `buffers`. Lifetime is managed by SnapshotHolder exactly like the MIDI
        snapshots.
    */
    class AudioClipSnapshot final : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<AudioClipSnapshot>;

        struct Region
        {
            juce::int64 timelineStartSample { 0 };
            juce::int64 lengthSamples       { 0 };
            juce::int64 sourceOffsetSamples { 0 };
            int   bufferIndex { -1 };
            float gain { 1.0f };
            double stretchRatio { 1.0 };
            bool reversed { false };
            models::ElasticMode elasticMode { models::ElasticMode::offlineOLA };
            double clipLengthSec { 0.0 };
            std::vector<models::WarpMarker> warpMarkers;
        };

        juce::OwnedArray<juce::AudioBuffer<float>> buffers; // resampled source data
        std::vector<Region> regions;
    };
} // namespace freequency::engine
