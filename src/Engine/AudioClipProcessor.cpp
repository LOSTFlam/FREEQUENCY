#include "Engine/AudioClipProcessor.h"

namespace omnidaw::engine
{
    AudioClipProcessor::AudioClipProcessor (Transport& sharedTransport)
        : AudioProcessor (BusesProperties()
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          transport (sharedTransport)
    {
    }

    bool AudioClipProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    }

    void AudioClipProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        buffer.clear();

        if (! transport.isPlaying())
            return;

        const auto snapshot = holder.getForAudio();
        if (snapshot == nullptr)
            return;

        const auto blockStart = transport.getPositionSamples();
        const auto numSamples = (juce::int64) buffer.getNumSamples();
        const auto blockEnd   = blockStart + numSamples;
        const int  outChannels = buffer.getNumChannels();

        for (const auto& region : snapshot->regions)
        {
            const auto regionEnd = region.timelineStartSample + region.lengthSamples;

            // Clip the [region] against this [block] on the timeline.
            const auto overlapStart = juce::jmax (blockStart, region.timelineStartSample);
            const auto overlapEnd   = juce::jmin (blockEnd, regionEnd);

            if (overlapEnd <= overlapStart)
                continue;

            auto* src = snapshot->buffers[region.bufferIndex];
            if (src == nullptr)
                continue;

            const auto count       = (int) (overlapEnd - overlapStart);
            const auto destOffset  = (int) (overlapStart - blockStart);
            const auto srcStart    = region.sourceOffsetSamples + (overlapStart - region.timelineStartSample);

            if (srcStart < 0 || srcStart >= src->getNumSamples())
                continue;

            const auto safeCount = (int) juce::jmin ((juce::int64) count,
                                                     (juce::int64) src->getNumSamples() - srcStart);
            if (safeCount <= 0)
                continue;

            const int srcChannels = src->getNumSamples() > 0 ? src->getNumChannels() : 0;

            for (int ch = 0; ch < outChannels; ++ch)
            {
                // Mono sources feed every output channel; stereo maps 1:1 and
                // wraps if the output has more channels than the source.
                const int srcCh = srcChannels > 0 ? juce::jmin (ch, srcChannels - 1) : 0;

                buffer.addFrom (ch, destOffset, *src, srcCh,
                                (int) srcStart, safeCount, region.gain);
            }
        }
    }
} // namespace omnidaw::engine
