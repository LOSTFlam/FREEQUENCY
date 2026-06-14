#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace omnidaw::engine
{
    /**
        TrackProcessor — the audio-graph node that represents one mixer strip.

        Every Track and every Bus in the model is realised in the
        juce::AudioProcessorGraph as one of these. Conceptually it is the "channel
        strip" stage that sits AFTER any insert effects:

            [clip playback / instrument] -> [insert FX nodes] -> [TrackProcessor]
                                                                       │
                                                                 volume + pan + mute
                                                                       │
                                                                 -> [bus / master]

        Why a bespoke AudioProcessor instead of juce::dsp::Gain alone?
          - It gives us one node that owns the strip's gain, pan, mute and (later)
            send taps, so the graph topology maps 1:1 onto the mixer model.
          - It is the natural place to apply per-sample automation in Phase 5.

        ── Real-time safety ──────────────────────────────────────────────────────
        processBlock() does NO allocation, locking, file or UI work. Parameters
        arrive from the message thread purely through std::atomics; the audio
        thread reads them once per block and feeds juce::SmoothedValue ramps so
        there are no zipper-noise discontinuities. This is the pattern every node
        in OmniDAW follows.
    */
    class TrackProcessor final : public juce::AudioProcessor
    {
    public:
        explicit TrackProcessor (juce::String nodeName);
        ~TrackProcessor() override = default;

        // ── Parameter control (message thread -> audio thread, lock-free) ──────
        /** Linear gain, 1.0 == unity. */
        void setGain (float linearGain) noexcept { targetGain.store (linearGain, std::memory_order_relaxed); }
        /** Pan position in [-1, +1] (left..right), constant-power law. */
        void setPan  (float newPan)     noexcept { targetPan.store (juce::jlimit (-1.0f, 1.0f, newPan), std::memory_order_relaxed); }
        void setMuted (bool shouldMute) noexcept { muted.store (shouldMute, std::memory_order_relaxed); }

        /** Total number of audio samples this node has processed since the last
            prepareToPlay(). Read from the message thread (e.g. by a status timer)
            to confirm the engine is actually running without touching the audio
            thread's hot path beyond a relaxed atomic store.
        */
        [[nodiscard]] juce::int64 getProcessedSampleCount() const noexcept
        {
            return processedSamples.load (std::memory_order_relaxed);
        }

        /** Post-fader output peak of the last block (0..1), for metering. */
        [[nodiscard]] float getOutputLevel() const noexcept
        {
            return outputLevel.load (std::memory_order_relaxed);
        }

        // ── juce::AudioProcessor ───────────────────────────────────────────────
        void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        // We only render float; bring the double overload into scope so it isn't
        // hidden (it falls back to AudioProcessor's default float<->double bridge).
        using juce::AudioProcessor::processBlock;

        bool isBusesLayoutSupported (const BusesLayout&) const override;

        const juce::String getName() const override { return name; }

        bool acceptsMidi() const override  { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

    private:
        const juce::String name;

        // Targets written by the message thread.
        std::atomic<float> targetGain { 1.0f };
        std::atomic<float> targetPan  { 0.0f };
        std::atomic<bool>  muted      { false };

        // Audio-thread-only smoothing state (never touched by other threads).
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedLeftPanGain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedRightPanGain;

        std::atomic<juce::int64> processedSamples { 0 };
        std::atomic<float> outputLevel { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackProcessor)
    };
} // namespace omnidaw::engine
