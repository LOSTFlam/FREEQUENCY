#pragma once

#include "Models/HarmonicEdit.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <vector>

namespace freequency::engine
{
    /**
        Offline VariAudio pitch correction — time-varying resample between anchor
        points, baked into AudioClipSnapshot on the message thread.
    */
    class VariAudioResynth
    {
    public:
        /** Pitch in cents relative to MIDI note 60 at `timeSec` (clip-relative). */
        static double estimatePitchCentsAt (const juce::AudioBuffer<float>& buffer,
                                            double sampleRate,
                                            double timeSec);

        /** Interpolated formant shift (semitones) at clip time. */
        static double formantShiftAt (const std::vector<models::VariAudioSegment>& segments,
                                      double timeSec,
                                      double clipLengthSec);

        /** Interpolated pitch correction cents at clip time. */
        static double correctionCentsAt (const std::vector<models::VariAudioSegment>& segments,
                                         double timeSec,
                                         double clipLengthSec);

        /** Apply segment pitch corrections; output length matches input. */
        static std::unique_ptr<juce::AudioBuffer<float>> applySegments (
            const juce::AudioBuffer<float>& in,
            const std::vector<models::VariAudioSegment>& segments,
            double sampleRate,
            double clipLengthSec);

        /** Build detected pitch contour samples for UI (times + cents). */
        static void buildDetectedContour (const juce::AudioBuffer<float>& buffer,
                                          double sampleRate,
                                          double clipLengthSec,
                                          int numPoints,
                                          std::vector<double>& timesOut,
                                          std::vector<double>& centsOut);
    };
} // namespace freequency::engine
