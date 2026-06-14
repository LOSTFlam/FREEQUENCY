#pragma once

#include "Models/Track.h"

namespace omnidaw::models
{
    /**
        MidiTrack — a track that hosts MidiClips driving a VST3 instrument.

        This is where OmniDAW's "hybrid" nature shows: a MidiTrack can be used in
        a Cubase-style linear arrangement (clips on the timeline) or fed from an
        FL-style pattern (Phase 6). The engine represents it as an instrument
        TrackProcessor node: MIDI events flow in from the sequencer FIFO, the
        hosted instrument renders audio, and the result passes through the same
        volume/pan/sends stage as an audio track.
    */
    class MidiTrack final : public Track
    {
    public:
        MidiTrack();

        [[nodiscard]] juce::String getTypeName() const override { return "MIDI"; }

        /** Creates and appends a new MidiClip, returning a non-owning pointer. */
        MidiClip* addClip();

        /** Identifier of the instrument plugin assigned to this track.
            Empty until an instrument is loaded in Phase 2. We store the plugin's
            file/identifier string rather than a live pointer so the model stays
            decoupled from the engine and is trivially serialisable.
        */
        juce::String instrumentPluginIdentifier;

        /** MIDI input channel the track listens to (1-16, or 0 for "omni"). */
        int midiInputChannel { 0 };
    };
} // namespace omnidaw::models
