#include "Engine/RtElasticStretch.h"
#include "Engine/WarpMapper.h"

#include <cmath>
#include <vector>

namespace freequency::engine
{
    void RtElasticStretch::mixRegion (juce::AudioBuffer<float>& dest,
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
                                      double clipLengthSec)
    {
        if (numSamples <= 0 || dest.getNumSamples() <= 0 || source.getNumSamples() <= 0 || sampleRate <= 0.0)
            return;

        const int outChannels = dest.getNumChannels();
        const int srcChannels = source.getNumChannels();
        const int srcLen = source.getNumSamples();
        const double srcLenSec = (double) srcLen / sampleRate;
        const double ratio = juce::jmax (1.0e-4, stretchRatio);
        const bool useWarp = warpMarkers != nullptr && ! warpMarkers->empty();

        const int grain = 256;
        juce::dsp::WindowingFunction<float> win (grain, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> window ((size_t) grain, 1.0f);
        win.multiplyWithWindowingTable (window.data(), (size_t) grain);

        auto readSrc = [&] (int ch, double pos) -> float
        {
            pos += sourceOffsetSamples;

            if (reversed)
                pos = (double) (srcLen - 1) - pos;

            if (pos < 0.0 || pos >= (double) (srcLen - 1))
                return 0.0f;

            const int i0 = (int) pos;
            const int i1 = juce::jmin (i0 + 1, srcLen - 1);
            const float frac = (float) (pos - (double) i0);
            const int sc = juce::jmin (ch, srcChannels - 1);
            return source.getSample (sc, i0) + frac * (source.getSample (sc, i1) - source.getSample (sc, i0));
        };

        for (int i = 0; i < numSamples; ++i)
        {
            const double timelineSec = timelineStartSec + (double) i / sampleRate;
            const double srcSec = useWarp
                                      ? WarpMapper::timelineToSource (timelineSec,
                                                                      *warpMarkers,
                                                                      clipLengthSec,
                                                                      srcLenSec,
                                                                      ratio)
                                      : timelineSec * ratio;
            const double srcCentre = srcSec * sampleRate;

            for (int ch = 0; ch < outChannels; ++ch)
            {
                float sum = 0.0f;
                float wSum = 0.0f;

                for (int g = 0; g < grain; ++g)
                {
                    const float w = window[(size_t) g];
                    const double srcPos = srcCentre + (double) g - (double) grain * 0.5;
                    sum += readSrc (ch, srcPos) * w;
                    wSum += w;
                }

                dest.addSample (ch, destOffset + i, (sum / juce::jmax (1.0e-4f, wSum)) * gain);
            }
        }
    }
} // namespace freequency::engine
