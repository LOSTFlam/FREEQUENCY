#include "Engine/CompSwipeMixer.h"

namespace freequency::engine
{
    namespace
    {
        double clipLengthSec (const models::AudioClip& clip)
        {
            return clip.length > 0.0 ? clip.length : 4.0;
        }

        const models::CompSwipeRegion* regionAt (const models::AudioClip& clip, double timeSec)
        {
            for (const auto& r : clip.compSwipeRegions)
            {
                if (timeSec >= r.startTime && timeSec < r.startTime + r.length)
                    return &r;
            }
            return nullptr;
        }

        float crossfadeGainA (double timeSec, const models::CompSwipeRegion& region)
        {
            const double boundary = region.startTime + region.length * (double) region.crossfadePosition;
            const double fadeSec = juce::jmin (0.05, region.length * 0.08);
            const double half = fadeSec * 0.5;

            if (timeSec <= boundary - half)
                return 1.0f;
            if (timeSec >= boundary + half)
                return 0.0f;

            const double t = (timeSec - (boundary - half)) / juce::jmax (1.0e-9, fadeSec);
            return (float) (1.0 - juce::jlimit (0.0, 1.0, t));
        }
    } // namespace

    void CompSwipeMixer::ensureDefaultRegion (models::AudioClip& clip)
    {
        clip.ensureDefaultCompRegion();
        if (! clip.compSwipeRegions.empty())
            clip.compSwipeRegions.back().length = clipLengthSec (clip);
    }

    void CompSwipeMixer::takeWeightsAt (const models::AudioClip& clip,
                                        double timeSec,
                                        std::vector<float>& weightsOut)
    {
        const int n = clip.getNumTakes();
        weightsOut.assign ((size_t) juce::jmax (1, n), 0.0f);

        if (n <= 0)
            return;

        if (n == 1 || clip.compSwipeRegions.empty())
        {
            const int idx = juce::jlimit (0, n - 1, clip.activeTake);
            weightsOut[(size_t) idx] = 1.0f;
            return;
        }

        if (const auto* region = regionAt (clip, timeSec))
        {
            const int a = juce::jlimit (0, n - 1, region->takeA);
            const int b = juce::jlimit (0, n - 1, region->takeB);
            const float wA = crossfadeGainA (timeSec, *region);
            weightsOut[(size_t) a] = wA;
            weightsOut[(size_t) b] = 1.0f - wA;
            return;
        }

        const int idx = juce::jlimit (0, n - 1, clip.activeTake);
        weightsOut[(size_t) idx] = 1.0f;
    }

    std::unique_ptr<juce::AudioBuffer<float>> CompSwipeMixer::mixTakes (
        const models::AudioClip& clip,
        const std::vector<const juce::AudioBuffer<float>*>& takes,
        double sampleRate,
        juce::int64 sourceOffsetSamples)
    {
        if (takes.empty() || sampleRate <= 0.0)
            return {};

        const juce::int64 outLen = (juce::int64) std::llround (clipLengthSec (clip) * sampleRate);
        if (outLen <= 0)
            return {};

        int numChannels = 0;
        for (auto* t : takes)
            if (t != nullptr)
                numChannels = juce::jmax (numChannels, t->getNumChannels());
        numChannels = juce::jmax (1, numChannels);

        auto out = std::make_unique<juce::AudioBuffer<float>> (numChannels, (int) outLen);
        out->clear();

        std::vector<float> weights;
        weights.reserve (takes.size());

        for (juce::int64 i = 0; i < outLen; ++i)
        {
            const double timeSec = (double) i / sampleRate;
            takeWeightsAt (clip, timeSec, weights);

            for (size_t ti = 0; ti < takes.size(); ++ti)
            {
                const float w = ti < weights.size() ? weights[ti] : 0.0f;
                if (w <= 0.0f)
                    continue;

                auto* src = takes[ti];
                if (src == nullptr || src->getNumSamples() <= 0)
                    continue;

                const juce::int64 srcIdx = sourceOffsetSamples + i;
                if (srcIdx < 0 || srcIdx >= src->getNumSamples())
                    continue;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const int srcCh = juce::jmin (ch, src->getNumChannels() - 1);
                    out->addSample (ch, (int) i, src->getSample (srcCh, (int) srcIdx) * w * clip.gain);
                }
            }
        }

        return out;
    }
} // namespace freequency::engine
