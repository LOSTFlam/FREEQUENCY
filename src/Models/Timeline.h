#pragma once

#include "Core/Types.h"
#include "Models/AudioTrack.h"
#include "Models/MidiTrack.h"

namespace freequency::models
{
    /**
        Timeline — the ordered collection of tracks plus the musical grid.

        Holds tempo and time signature so musical (beats) <-> wall-clock (seconds)
        conversions have a single authority. The arrangement view (Phase 3) draws
        this; the engine walks its tracks to build the audio graph.
    */
    class Timeline
    {
    public:
        Timeline() = default;

        // ── Tracks ────────────────────────────────────────────────────────────
        AudioTrack* addAudioTrack();
        MidiTrack*  addMidiTrack();

        /** Removes a track by id. Returns true if a track was removed. */
        bool removeTrack (const ObjectId& trackId);

        /** Removes all tracks (used when loading a project). */
        void clear() { tracks.clear(); }

        [[nodiscard]] int    getNumTracks() const noexcept { return tracks.size(); }
        [[nodiscard]] Track* getTrack (int index) const noexcept { return tracks[index]; }
        [[nodiscard]] Track* findTrack (const ObjectId& trackId) const noexcept;

        // ── Musical grid ──────────────────────────────────────────────────────
        [[nodiscard]] double getTempoBpm() const noexcept { return tempoBpm; }
        void setTempoBpm (double newTempo) noexcept { tempoBpm = juce::jmax (1.0, newTempo); }

        [[nodiscard]] int getTimeSigNumerator()   const noexcept { return timeSigNumerator; }
        [[nodiscard]] int getTimeSigDenominator() const noexcept { return timeSigDenominator; }
        void setTimeSignature (int numerator, int denominator) noexcept;

        /** Convert beats to seconds using the current tempo. */
        [[nodiscard]] Seconds beatsToSeconds (Beats beats) const noexcept
        {
            return beats * (60.0 / tempoBpm);
        }

        /** Convert seconds to beats using the current tempo. */
        [[nodiscard]] Beats secondsToBeats (Seconds seconds) const noexcept
        {
            return seconds * (tempoBpm / 60.0);
        }

    private:
        juce::OwnedArray<Track> tracks;

        double tempoBpm { 120.0 };
        int    timeSigNumerator { 4 };
        int    timeSigDenominator { 4 };
    };
} // namespace freequency::models
