#pragma once

#include "Engine/Transport.h"
#include "Engine/AudioClipSnapshot.h"
#include "Engine/SnapshotHolder.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace freequency::engine
{
    /**
        AudioClipProcessor — the per-audio-track playback source node.

        Sits at the head of an audio track's chain:

            [AudioClipProcessor] --audio--> [insert FX...] --audio--> [strip] -> master

        Each block it reads the Transport position and mixes any clip regions that
        overlap the block into the output buffer. Source data lives in an
        immutable AudioClipSnapshot published from the message thread.
    */
    class AudioClipProcessor final : public juce::AudioProcessor
    {
    public:
        explicit AudioClipProcessor (Transport& sharedTransport);
        ~AudioClipProcessor() override = default;

        void setSnapshot (AudioClipSnapshot::Ptr snapshot) { holder.publish (std::move (snapshot)); }
        void collectGarbage() { holder.collectGarbage(); }

        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;

        bool isBusesLayoutSupported (const BusesLayout&) const override;

        const juce::String getName() const override { return "Audio Clips"; }

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
        SnapshotHolder<AudioClipSnapshot> holder;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipProcessor)
    };
} // namespace freequency::engine
