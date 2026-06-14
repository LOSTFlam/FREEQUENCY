#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace freequency::engine
{
    /**
        SynthProcessor — FREEQUENCY's built-in polyphonic instrument.

        Every MIDI track gets one of these as its default instrument so the DAW
        makes sound out of the box, with zero external plugins installed. When the
        user assigns a VST3/AU instrument (Phase 2 hosting), the engine swaps this
        node for the hosted plugin node instead.

        The voice is a classic subtractive design: a polyBLEP-band-limited
        sawtooth through a one-pole low-pass, shaped by an ADSR. It is intended to
        be musically useful, not a flagship synth — that's what plugin hosting is
        for.

        Real-time safety: juce::Synthesiser renders entirely in-place; no
        allocation or locking happens in processBlock.
    */
    class SynthProcessor final : public juce::AudioProcessor
    {
    public:
        SynthProcessor();
        ~SynthProcessor() override = default;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;

        bool isBusesLayoutSupported (const BusesLayout&) const override;

        const juce::String getName() const override { return "FreequencySynth"; }

        bool acceptsMidi() const override  { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 1.0; }

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
        juce::Synthesiser synth;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthProcessor)
    };
} // namespace freequency::engine
