#pragma once

#include "Models/Clip.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <vector>

namespace freequency::engine
{
    /**
        Bakes multi-take comp swipe crossfades into a single timeline buffer
        for AudioClipSnapshot playback. Times are relative to clip start.
    */
    class CompSwipeMixer
    {
    public:
        /** Ensure a default swipe region exists when two or more takes are present. */
        static void ensureDefaultRegion (models::AudioClip& clip);

        /** Per-sample take weights at `timeSec` (relative to clip start). */
        static void takeWeightsAt (const models::AudioClip& clip,
                                   double timeSec,
                                   std::vector<float>& weightsOut);

        /** Mix processed take buffers into one output buffer (clip timeline length). */
        static std::unique_ptr<juce::AudioBuffer<float>> mixTakes (
            const models::AudioClip& clip,
            const std::vector<const juce::AudioBuffer<float>*>& takes,
            double sampleRate,
            juce::int64 sourceOffsetSamples);
    };
} // namespace freequency::engine
