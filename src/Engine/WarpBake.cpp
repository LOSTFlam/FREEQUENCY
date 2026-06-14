#include "Engine/WarpBake.h"
#include "Engine/WarpMapper.h"

namespace freequency::engine
{
    namespace
    {
        float readAt (const juce::AudioBuffer<float>& buf, int ch, double samplePos) noexcept
        {
            const int len = buf.getNumSamples();
            if (len <= 0)
                return 0.0f;

            samplePos = juce::jlimit (0.0, (double) (len - 1), samplePos);
            const int i0 = (int) samplePos;
            const int i1 = juce::jmin (i0 + 1, len - 1);
            const float frac = (float) (samplePos - (double) i0);
            const float s0 = buf.getSample (ch, i0);
            const float s1 = buf.getSample (ch, i1);
            return s0 + frac * (s1 - s0);
        }
    } // namespace

    std::unique_ptr<juce::AudioBuffer<float>> WarpBake::timelineResample (
        const juce::AudioBuffer<float>& source,
        double sampleRate,
        double clipLengthSec,
        const std::vector<models::WarpMarker>& markers,
        double stretchRatio)
    {
        if (source.getNumSamples() <= 0 || sampleRate <= 0.0 || clipLengthSec <= 0.0)
            return std::make_unique<juce::AudioBuffer<float>> (source);

        const int channels = source.getNumChannels();
        const int outLen = juce::jmax (1, (int) std::llround (clipLengthSec * sampleRate));
        const double srcLenSec = (double) source.getNumSamples() / sampleRate;

        auto out = std::make_unique<juce::AudioBuffer<float>> (channels, outLen);
        out->clear();

        for (int i = 0; i < outLen; ++i)
        {
            const double timelineSec = (double) i / sampleRate;
            const double srcSec = WarpMapper::timelineToSource (timelineSec,
                                                                  markers,
                                                                  clipLengthSec,
                                                                  srcLenSec,
                                                                  stretchRatio);
            const double srcSample = srcSec * sampleRate;

            for (int ch = 0; ch < channels; ++ch)
                out->setSample (ch, i, readAt (source, ch, srcSample));
        }

        return out;
    }
} // namespace freequency::engine
