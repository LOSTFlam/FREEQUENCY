#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace freequency::dsp
{
    /**
        Real-time elastic preview — maps timeline samples to source with stretch
        ratio using Hann-window overlap-add grains (pitch-preserving tape-style
        preview during playback).
    */
    class RtElasticStretch
    {
    public:
        /** Mix stretched source region into `dest` at `destOffset` (samples). */
        static void mixRegion (juce::AudioBuffer<float>& dest,
                               int destOffset,
                               int numSamples,
                               const juce::AudioBuffer<float>& source,
                               double sourceStartSample,
                               double stretchRatio,
                               float gain,
                               bool reversed);
    };
} // namespace freequency::dsp
