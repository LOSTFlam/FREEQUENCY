#pragma once

#include "Engine/Transport.h"
#include "Engine/MidiSequenceSnapshot.h"
#include "Engine/SnapshotHolder.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace freequency::engine
{
    /**
        MidiSourceProcessor — the per-MIDI-track sequencer node.

        It produces no audio; it emits the MIDI events that belong in the current
        block based on the shared Transport position, then the graph routes that
        MIDI (via a midi connection) into the track's instrument node:

            [MidiSourceProcessor] --midi--> [instrument] --audio--> [strip] -> master

        The event data arrives as an immutable MidiSequenceSnapshot published from
        the message thread, so processBlock never touches model containers.

        Real-time safety: no allocation/locking in processBlock. The only "lock"
        is a try-lock inside SnapshotHolder that, on the rare miss, simply reuses
        last block's snapshot.
    */
    class MidiSourceProcessor final : public juce::AudioProcessor
    {
    public:
        explicit MidiSourceProcessor (Transport& sharedTransport);
        ~MidiSourceProcessor() override = default;

        /** Message thread: publish freshly flattened events (absolute samples). */
        void setSnapshot (MidiSequenceSnapshot::Ptr snapshot) { holder.publish (std::move (snapshot)); }
        void collectGarbage() { holder.collectGarbage(); }

        void prepareToPlay (double, int) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;

        const juce::String getName() const override { return "MIDI Source"; }

        bool acceptsMidi() const override  { return false; }
        bool producesMidi() const override { return true; }
        bool isMidiEffect() const override { return true; }
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
        SnapshotHolder<MidiSequenceSnapshot> holder;
        bool wasPlaying { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSourceProcessor)
    };
} // namespace freequency::engine
