#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace freequency::dsp
{
    /**
        LimiterProcessor — a brick-wall limiter on the master output.

        Sits between the master strip and the audio output so the final mix never
        clips the converter, regardless of how hot the session gets. This is an
        improvement over relying on the user to ride the master fader: it's a
        safety net that's transparent until the signal actually exceeds the
        ceiling. Toggleable (bypassable) from the UI.
    */
    class LimiterProcessor final : public juce::AudioProcessor
    {
    public:
        LimiterProcessor();

        void setEnabled (bool e) noexcept { enabled.store (e, std::memory_order_relaxed); }
        [[nodiscard]] bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;

        bool isBusesLayoutSupported (const BusesLayout&) const override;
        const juce::String getName() const override { return "Master Limiter"; }

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
        std::atomic<bool> enabled { true };

        static constexpr float ceiling = 0.97f; // ~ -0.26 dBFS
        float currentGain { 1.0f };
        float releaseCoeff { 0.999f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterProcessor)
    };
} // namespace freequency::dsp
