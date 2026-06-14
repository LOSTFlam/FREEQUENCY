#include "Models/Track.h"

namespace freequency::models
{
    Track::Track (TrackType trackType)
        : type (trackType)
    {
    }

    void Track::setVolume (float linearGain) noexcept
    {
        volume.store (juce::jmax (0.0f, linearGain), std::memory_order_relaxed);
        sendChangeMessage();
    }

    void Track::setPan (float newPan) noexcept
    {
        pan.store (juce::jlimit (-1.0f, 1.0f, newPan), std::memory_order_relaxed);
        sendChangeMessage();
    }

    void Track::setMuted (bool shouldBeMuted) noexcept
    {
        mute.store (shouldBeMuted, std::memory_order_relaxed);
        sendChangeMessage();
    }

    void Track::setSoloed (bool shouldBeSoloed) noexcept
    {
        solo.store (shouldBeSoloed, std::memory_order_relaxed);
        sendChangeMessage();
    }

    Track::Send* Track::addSend (const juce::String& destBusId)
    {
        auto* send = new Send();
        send->destBusId = destBusId;
        send->level.store (0.5f, std::memory_order_relaxed);
        sends.add (send);
        sendChangeMessage();
        return send;
    }

    void Track::removeSend (int index)
    {
        if (index >= 0 && index < sends.size())
        {
            sends.remove (index);
            sendChangeMessage();
        }
    }

    void Track::removeClip (Clip* clip)
    {
        for (int i = 0; i < clips.size(); ++i)
        {
            if (clips[i] == clip)
            {
                clips.remove (i);
                sendChangeMessage();
                return;
            }
        }
    }

    Clip* Track::addClipInternal (std::unique_ptr<Clip> clip)
    {
        auto* raw = clips.add (clip.release());
        sendChangeMessage();
        return raw;
    }
} // namespace freequency::models
