#include "Engine/TrackProcessor.h"

#include <cmath>

namespace freequency::engine
{
    namespace
    {
        // Constant-power pan: keeps perceived loudness even as a source is panned.
        // pan in [-1, +1] maps to an angle in [0, pi/2]; left = cos, right = sin.
        void computePanGains (float pan, float& leftGain, float& rightGain) noexcept
        {
            const auto angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            leftGain  = std::cos (angle);
            rightGain = std::sin (angle);
        }
    } // namespace

    TrackProcessor::TrackProcessor (juce::String nodeName)
        : AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          name (std::move (nodeName))
    {
    }

    void TrackProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
    {
        // ~20 ms ramps remove zipper noise while still feeling instantaneous.
        constexpr double rampSeconds = 0.02;

        smoothedGain.reset (sampleRate, rampSeconds);
        smoothedLeftPanGain.reset (sampleRate, rampSeconds);
        smoothedRightPanGain.reset (sampleRate, rampSeconds);

        // Initialise ramps at the current target so the first block doesn't fade
        // in from silence.
        smoothedGain.setCurrentAndTargetValue (targetGain.load (std::memory_order_relaxed));

        float l = 1.0f, r = 1.0f;
        computePanGains (targetPan.load (std::memory_order_relaxed), l, r);
        smoothedLeftPanGain.setCurrentAndTargetValue (l);
        smoothedRightPanGain.setCurrentAndTargetValue (r);

        processedSamples.store (0, std::memory_order_relaxed);
    }

    bool TrackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        // We only support matching mono or stereo in/out; the graph runs in
        // stereo throughout FREEQUENCY.
        const auto& mainOut = layouts.getMainOutputChannelSet();

        if (mainOut != juce::AudioChannelSet::mono()
            && mainOut != juce::AudioChannelSet::stereo())
            return false;

        return layouts.getMainInputChannelSet() == mainOut;
    }

    void TrackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Pull the latest message-thread targets exactly once per block.
        // When automation is enabled, the curve value at the current playhead
        // position drives the gain instead of the static fader target.
        float gainTarget = targetGain.load (std::memory_order_relaxed);

        if (automationEnabled.load (std::memory_order_relaxed) && transport != nullptr)
        {
            if (auto snap = automationHolder.getForAudio())
                gainTarget = snap->evaluate (transport->getPositionSamples());
        }

        if (muted.load (std::memory_order_relaxed))
            smoothedGain.setTargetValue (0.0f);
        else
            smoothedGain.setTargetValue (gainTarget);

        float panLeft = 1.0f, panRight = 1.0f;
        computePanGains (targetPan.load (std::memory_order_relaxed), panLeft, panRight);
        smoothedLeftPanGain.setTargetValue (panLeft);
        smoothedRightPanGain.setTargetValue (panRight);

        // Apply per-sample gain + pan. Stereo is the common case; mono skips pan.
        if (numChannels >= 2)
        {
            auto* left  = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            for (int i = 0; i < numSamples; ++i)
            {
                const float g = smoothedGain.getNextValue();
                left[i]  *= g * smoothedLeftPanGain.getNextValue();
                right[i] *= g * smoothedRightPanGain.getNextValue();
            }
        }
        else if (numChannels == 1)
        {
            auto* mono = buffer.getWritePointer (0);

            for (int i = 0; i < numSamples; ++i)
                mono[i] *= smoothedGain.getNextValue();

            // Keep the pan ramps advancing so they stay in step if the layout
            // later changes to stereo.
            smoothedLeftPanGain.skip (numSamples);
            smoothedRightPanGain.skip (numSamples);
        }

        processedSamples.fetch_add (numSamples, std::memory_order_relaxed);

        // Post-fader peak for the mixer meters. A decaying max gives a smoother,
        // more readable meter than a raw per-block peak.
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

        const float previous = outputLevel.load (std::memory_order_relaxed) * 0.78f;
        outputLevel.store (juce::jmax (peak, previous), std::memory_order_relaxed);
    }
} // namespace freequency::engine
