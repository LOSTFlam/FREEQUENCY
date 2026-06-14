#include "DSP/RtElasticStretch.h"

#include <cmath>

namespace freequency::dsp
{
    void RtElasticStretch::mixRegion (juce::AudioBuffer<float>& dest,
                                      int destOffset,
                                      int numSamples,
                                      const juce::AudioBuffer<float>& source,
                                      double sourceStartSample,
                                      double stretchRatio,
                                      float gain,
                                      bool reversed)
    {
        if (numSamples <= 0 || dest.getNumSamples() <= 0 || source.getNumSamples() <= 0)
            return;

        const double ratio = juce::jmax (1.0e-4, stretchRatio);
        const int outChannels = dest.getNumChannels();
        const int srcChannels = source.getNumChannels();
        const int srcLen = source.getNumSamples();

        auto readSrc = [&] (int ch, double pos) -> float
        {
            if (reversed)
                pos = (double) (srcLen - 1) - pos;

            if (pos < 0.0 || pos >= (double) (srcLen - 1))
                return 0.0f;

            const int i0 = (int) pos;
            const int i1 = juce::jmin (i0 + 1, srcLen - 1);
            const float frac = (float) (pos - (double) i0);
            const int sc = juce::jmin (ch, srcChannels - 1);
            const float s0 = source.getSample (sc, i0);
            const float s1 = source.getSample (sc, i1);
            return s0 + frac * (s1 - s0);
        };

        for (int i = 0; i < numSamples; ++i)
        {
            const double srcPos = sourceStartSample + (double) i * ratio;

            for (int ch = 0; ch < outChannels; ++ch)
            {
                const float s = readSrc (ch, srcPos);
                dest.addSample (ch, destOffset + i, s * gain);
            }
        }
    }
} // namespace freequency::dsp
