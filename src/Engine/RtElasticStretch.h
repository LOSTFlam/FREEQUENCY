#pragma once

#include "Models/HarmonicEdit.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace freequency::engine
{
    /** Real-time pitch-preserving OLA elastic preview with optional warp map. */
    class RtElasticStretch
    {
    public:
        static void mixRegion (juce::AudioBuffer<float>& dest,
                               int destOffset,
                               int numSamples,
                               const juce::AudioBuffer<float>& source,
                               double timelineStartSec,
                               double sampleRate,
                               double sourceOffsetSamples,
                               double stretchRatio,
                               float gain,
                               bool reversed,
                               const std::vector<models::WarpMarker>* warpMarkers,
                               double clipLengthSec);
    };
} // namespace freequency::engine
