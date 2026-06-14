#include "Engine/AudioClipProcessor.h"

#include "Engine/RtElasticStretch.h"

namespace freequency::engine
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
        const double sr = transport.getSampleRate() > 0 ? transport.getSampleRate() : 44100.0;

        for (const auto& region : snapshot->regions)
        {
            const auto regionEnd = region.timelineStartSample + region.lengthSamples;

            const auto overlapStart = juce::jmax (blockStart, region.timelineStartSample);
            const auto overlapEnd   = juce::jmin (blockEnd, regionEnd);

            if (overlapEnd <= overlapStart)
                continue;

            auto* src = snapshot->buffers[region.bufferIndex];
            if (src == nullptr)
                continue;

            const auto count      = (int) (overlapEnd - overlapStart);
            const auto destOffset = (int) (overlapStart - blockStart);

            const bool rtElastic = region.elasticMode == models::ElasticMode::realtimePreview;
            const bool useRtStretch = rtElastic
                                      && (std::abs (region.stretchRatio - 1.0) > 1.0e-4
                                          || ! region.warpMarkers.empty());

            if (useRtStretch)
            {
                const double timelineStartSec = (double) (overlapStart - region.timelineStartSample) / sr;
                const auto* warpPtr = region.warpMarkers.empty() ? nullptr : &region.warpMarkers;

                RtElasticStretch::mixRegion (buffer,
                                             destOffset,
                                             count,
                                             *src,
                                             timelineStartSec,
                                             sr,
                                             (double) region.sourceOffsetSamples,
                                             region.stretchRatio,
                                             region.gain,
                                             region.reversed,
                                             warpPtr,
                                             region.clipLengthSec);
                continue;
            }

            const auto srcStart = region.sourceOffsetSamples + (overlapStart - region.timelineStartSample);

            if (srcStart < 0 || srcStart >= src->getNumSamples())
                continue;

            const auto safeCount = (int) juce::jmin ((juce::int64) count,
                                                     (juce::int64) src->getNumSamples() - srcStart);
            if (safeCount <= 0)
                continue;

            const int srcChannels = src->getNumSamples() > 0 ? src->getNumChannels() : 0;

            for (int ch = 0; ch < outChannels; ++ch)
            {
                const int srcCh = srcChannels > 0 ? juce::jmin (ch, srcChannels - 1) : 0;

                buffer.addFrom (ch, destOffset, *src, srcCh,
                                (int) srcStart, safeCount, region.gain);
            }
        }
    }
} // namespace freequency::engine
