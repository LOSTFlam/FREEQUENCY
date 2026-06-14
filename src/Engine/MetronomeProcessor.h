#pragma once

#include "Engine/Transport.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace freequency::engine
{
    /**
        MetronomeProcessor — a Cubase-style click track.

        Generates a short blip on every beat (an accented, higher blip on each
        bar's downbeat) derived from the shared Transport position and tempo. It
        is a graph node summed into the master output, so it is heard but not
        recorded into tracks.

        Real-time safe: a tiny per-sample oscillator + envelope, no allocation.
    */
    class MetronomeProcessor final : public juce::AudioProcessor
    {
    public:
        explicit MetronomeProcessor (Transport& sharedTransport);

        void setEnabled (bool e) noexcept { enabled.store (e, std::memory_order_relaxed); }
        [[nodiscard]] bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }
        void setBeatsPerBar (int n) noexcept { beatsPerBar.store (juce::jmax (1, n), std::memory_order_relaxed); }

        void prepareToPlay (double, int) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;

        bool isBusesLayoutSupported (const BusesLayout&) const override;
        const juce::String getName() const override { return "Metronome"; }

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
        Transport& transport;
        std::atomic<bool> enabled { false };
        std::atomic<int>  beatsPerBar { 4 };

        // Audio-thread-only click envelope state.
        double clickPhase { 0.0 };
        double clickFreq { 1000.0 };
        int    clickRemaining { 0 };
        int    clickTotal { 1 };
        float  clickAmp { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetronomeProcessor)
    };
} // namespace freequency::engine
