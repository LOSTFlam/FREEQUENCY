#include "Models/Timeline.h"

namespace freequency::models
{
    AudioTrack* Timeline::addAudioTrack()
    {
        auto* track = new AudioTrack();
        track->colour = defaults::nextTrackColour (tracks.size());
        tracks.add (track);
        return track;
    }

    MidiTrack* Timeline::addMidiTrack()
    {
        auto* track = new MidiTrack();
        track->colour = defaults::nextTrackColour (tracks.size());
        tracks.add (track);
        return track;
    }

    bool Timeline::removeTrack (const ObjectId& trackId)
    {
        for (int i = 0; i < tracks.size(); ++i)
        {
            if (tracks[i]->getId() == trackId)
            {
                tracks.remove (i);
                return true;
            }
        }

        return false;
    }

    Track* Timeline::findTrack (const ObjectId& trackId) const noexcept
    {
        for (auto* track : tracks)
            if (track->getId() == trackId)
                return track;

        return nullptr;
    }

    void Timeline::setTimeSignature (int numerator, int denominator) noexcept
    {
        timeSigNumerator   = juce::jmax (1, numerator);
        timeSigDenominator = juce::jmax (1, denominator);
    }
} // namespace freequency::models
