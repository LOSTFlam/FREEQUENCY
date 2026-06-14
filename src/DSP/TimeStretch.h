#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <memory>

namespace freequency::dsp
{
    /**
        Offline audio warp utilities — OLA time-stretch (pitch-preserving) and
        resample pitch shift. These run on the message thread when building clip
        snapshots; the audio thread only streams the baked result.

        Not transient-aware (no Élastique-style locking) — a simple Hann-window
        overlap-add stretch suitable for musical material at moderate ratios.
    */
    class TimeStretch
    {
    public:
        /** OLA time-stretch: `stretch` > 1 lengthens (slower), pitch preserved. */
        static std::unique_ptr<juce::AudioBuffer<float>> olaStretch (const juce::AudioBuffer<float>& in,
                                                                      double stretch);

        /** Resample by `ratio` (output length = input / ratio). Used for pitch shift. */
        static std::unique_ptr<juce::AudioBuffer<float>> resampleByRatio (const juce::AudioBuffer<float>& in,
                                                                          double ratio);

        /** Apply stretch (OLA) then pitch (resample) independently. */
        static std::unique_ptr<juce::AudioBuffer<float>> applyWarp (std::unique_ptr<juce::AudioBuffer<float>> buf,
                                                                    double stretchRatio,
                                                                    int pitchSemitones);
    };
} // namespace freequency::dsp
