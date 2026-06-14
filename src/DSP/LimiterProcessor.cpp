#include "DSP/LimiterProcessor.h"

#include <cmath>

namespace freequency::dsp
{
    LimiterProcessor::LimiterProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void LimiterProcessor::prepareToPlay (double sampleRate, int)
    {
        // ~80 ms release; instant attack. No makeup gain, so the limiter is
        // transparent until the signal actually reaches the ceiling.
        releaseCoeff = (float) std::exp (-1.0 / (0.08 * juce::jmax (1.0, sampleRate)));
        currentGain = 1.0f;
    }

    bool LimiterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;
        return layouts.getMainInputChannelSet() == out;
    }

    void LimiterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        if (! enabled.load (std::memory_order_relaxed))
            return; // bypass

        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            // Peak across channels drives a shared gain so the stereo image is
            // preserved.
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            const float desired = peak > ceiling ? ceiling / peak : 1.0f;

            // Instant attack (clamp down immediately), exponential release back up.
            if (desired < currentGain) currentGain = desired;
            else                       currentGain = desired + (currentGain - desired) * releaseCoeff;

            for (int ch = 0; ch < numCh; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * currentGain);
        }
    }
} // namespace freequency::dsp
