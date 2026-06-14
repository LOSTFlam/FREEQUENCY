#include "Engine/MetronomeProcessor.h"

#include <cmath>

namespace freequency::engine
{
    MetronomeProcessor::MetronomeProcessor (Transport& sharedTransport)
        : AudioProcessor (BusesProperties()
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          transport (sharedTransport)
    {
    }

    void MetronomeProcessor::prepareToPlay (double, int)
    {
        clickRemaining = 0;
        clickPhase = 0.0;
    }

    bool MetronomeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    }

    void MetronomeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        if (! enabled.load (std::memory_order_relaxed) || ! transport.isPlaying())
        {
            clickRemaining = 0;
            return;
        }

        const double sr = getSampleRate();
        const double tempo = transport.getTempo();
        if (sr <= 0.0 || tempo <= 0.0)
            return;

        const auto beatLen = (juce::int64) std::llround (sr * 60.0 / tempo);
        if (beatLen <= 0)
            return;

        const int bpb = beatsPerBar.load (std::memory_order_relaxed);
        const auto pos = transport.getPositionSamples();
        const int numSamples = buffer.getNumSamples();
        const int clickLen = juce::jmax (1, (int) (sr * 0.035)); // 35 ms blip

        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto absSample = pos + i;

            // Trigger a new click exactly on a beat boundary.
            if (absSample % beatLen == 0)
            {
                const auto beatNumber = absSample / beatLen;
                const bool downbeat = (beatNumber % bpb) == 0;
                clickFreq = downbeat ? 1568.0 : 988.0; // G6 / B5
                clickAmp  = downbeat ? 0.55f : 0.38f;
                clickRemaining = clickLen;
                clickTotal = clickLen;
                clickPhase = 0.0;
            }

            if (clickRemaining > 0)
            {
                const float env = (float) clickRemaining / (float) clickTotal;
                const float s = std::sin ((float) clickPhase) * clickAmp * env * env;
                left[i] += s;
                if (right != nullptr) right[i] += s;

                clickPhase += 2.0 * juce::MathConstants<double>::pi * clickFreq / sr;
                --clickRemaining;
            }
        }
    }
} // namespace freequency::engine
