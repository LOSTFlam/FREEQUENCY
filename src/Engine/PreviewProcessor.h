#pragma once

#include "Engine/SnapshotHolder.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace freequency::engine
{
    /** Immutable, ref-counted audio buffer for lock-free preview publication. */
    class PreviewClip final : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<PreviewClip>;
        juce::AudioBuffer<float> buffer;
    };

    /**
        PreviewProcessor — plays a one-shot preview buffer (media-browser audition)
        mixed into the master output, so previewing works even during playback.
        Real-time safe: reads an immutable snapshot + atomic position.
    */
    class PreviewProcessor final : public juce::AudioProcessor
    {
    public:
        PreviewProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

        void play (PreviewClip::Ptr clip)
        {
            holder.publish (std::move (clip));
            position.store (0, std::memory_order_relaxed);
            playing.store (true, std::memory_order_relaxed);
        }
        void stop() noexcept { playing.store (false, std::memory_order_relaxed); }
        void collectGarbage() { holder.collectGarbage(); }

        void prepareToPlay (double, int) override {}
        void releaseResources() override {}

        void processBlock (juce::AudioBuffer<float>& out, juce::MidiBuffer&) override
        {
            juce::ScopedNoDenormals noDenormals;
            out.clear();
            if (! playing.load (std::memory_order_relaxed))
                return;

            auto clip = holder.getForAudio();
            if (clip == nullptr || clip->buffer.getNumSamples() == 0)
                return;

            const auto& src = clip->buffer;
            const auto len = (juce::int64) src.getNumSamples();
            auto pos = position.load (std::memory_order_relaxed);
            const int n = out.getNumSamples();

            for (int ch = 0; ch < out.getNumChannels(); ++ch)
            {
                const int srcCh = juce::jmin (ch, src.getNumChannels() - 1);
                if (srcCh < 0) break;
                auto p = pos;
                for (int i = 0; i < n && p < len; ++i, ++p)
                    out.addSample (ch, i, src.getSample (srcCh, (int) p));
            }

            pos += n;
            if (pos >= len) { playing.store (false, std::memory_order_relaxed); pos = 0; }
            position.store (pos, std::memory_order_relaxed);
        }
        using juce::AudioProcessor::processBlock;

        const juce::String getName() const override { return "Preview"; }
        bool acceptsMidi() const override { return false; }
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
        SnapshotHolder<PreviewClip> holder;
        std::atomic<juce::int64> position { 0 };
        std::atomic<bool> playing { false };
    };
} // namespace freequency::engine
