#pragma once

#include "Models/HarmonicEdit.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <vector>

namespace freequency::engine
{
    /** Offline resample: clip timeline → source using warp markers. */
    class WarpBake
    {
    public:
        static std::unique_ptr<juce::AudioBuffer<float>> timelineResample (
            const juce::AudioBuffer<float>& source,
            double sampleRate,
            double clipLengthSec,
            const std::vector<models::WarpMarker>& markers,
            double stretchRatio);
    };
} // namespace freequency::engine
