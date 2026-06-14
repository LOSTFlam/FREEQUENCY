#include "DSP/TimeStretch.h"

#include <cmath>
#include <vector>

namespace freequency::dsp
{
    std::unique_ptr<juce::AudioBuffer<float>> TimeStretch::olaStretch (const juce::AudioBuffer<float>& in,
                                                                       double stretch)
    {
        if (std::abs (stretch - 1.0) < 1.0e-4 || in.getNumSamples() < 4)
            return std::make_unique<juce::AudioBuffer<float>> (in);

        const int channels = in.getNumChannels();
        const int inLen = in.getNumSamples();
        const int grain = 2048;
        const int synHop = grain / 4; // 75% overlap
        const int anaHop = juce::jmax (1, (int) std::llround (synHop / stretch));
        const int outLen = (int) std::ceil (inLen * stretch) + grain;

        juce::dsp::WindowingFunction<float> win (grain, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> window ((size_t) grain, 1.0f);
        win.multiplyWithWindowingTable (window.data(), (size_t) grain);

        auto out = std::make_unique<juce::AudioBuffer<float>> (channels, outLen);
        out->clear();
        std::vector<float> norm ((size_t) outLen, 0.0f);

        for (int ana = 0, syn = 0; ana + grain <= inLen; ana += anaHop, syn += synHop)
        {
            for (int i = 0; i < grain; ++i)
            {
                const float w = window[(size_t) i];
                for (int ch = 0; ch < channels; ++ch)
                    out->addSample (ch, syn + i, in.getSample (ch, ana + i) * w);
                norm[(size_t) (syn + i)] += w;
            }
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = out->getWritePointer (ch);
            for (int i = 0; i < outLen; ++i)
                d[i] /= juce::jmax (1.0e-4f, norm[(size_t) i]);
        }

        return out;
    }

    std::unique_ptr<juce::AudioBuffer<float>> TimeStretch::resampleByRatio (const juce::AudioBuffer<float>& in,
                                                                            double ratio)
    {
        if (std::abs (ratio - 1.0) < 1.0e-4)
            return std::make_unique<juce::AudioBuffer<float>> (in);

        const int channels = in.getNumChannels();
        const int outLen = juce::jmax (1, (int) std::ceil (in.getNumSamples() / ratio));
        auto out = std::make_unique<juce::AudioBuffer<float>> (channels, outLen);

        for (int ch = 0; ch < channels; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, in.getReadPointer (ch), out->getWritePointer (ch), outLen);
        }

        return out;
    }

    std::unique_ptr<juce::AudioBuffer<float>> TimeStretch::applyWarp (std::unique_ptr<juce::AudioBuffer<float>> buf,
                                                                      double stretchRatio,
                                                                      int pitchSemitones)
    {
        if (buf == nullptr)
            return buf;

        const double pitchFactor = std::pow (2.0, pitchSemitones / 12.0);

        // Stretch first (OLA, pitch-preserving), then pitch via resample.
        if (std::abs (stretchRatio - 1.0) > 1.0e-4)
            buf = olaStretch (*buf, stretchRatio);

        if (std::abs (pitchFactor - 1.0) > 1.0e-4)
            buf = resampleByRatio (*buf, pitchFactor);

        return buf;
    }
} // namespace freequency::dsp
