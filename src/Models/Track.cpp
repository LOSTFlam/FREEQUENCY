#include "Models/Track.h"

namespace omnidaw::models
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

    Clip* Track::addClipInternal (std::unique_ptr<Clip> clip)
    {
        auto* raw = clips.add (clip.release());
        sendChangeMessage();
        return raw;
    }
} // namespace omnidaw::models
