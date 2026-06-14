#pragma once

#include "Core/Types.h"

#include <vector>

namespace freequency::models
{
    /**
        Harmonic editing primitives — shared by VariAudio, elastic warp,
        comp swipe, portamento, and the future Frequency Field editor.

        Pure data; resynthesis and real-time preview live in FreequencyDSP /
        FreequencyEngine. Times are seconds relative to clip start unless noted.
    */

    /** One VariAudio correction point → resynthesised segment between neighbours. */
    struct VariAudioSegment
    {
        Seconds  time { 0.0 };       // anchor on clip timeline
        int      pitchCents { 0 };   // correction offset from detected pitch
        float    formantShift { 0.0f }; // semitones, future formant preservation
        bool     locked { false };   // exclude from auto-detect re-analysis
    };

    /** Swipe comp: blend two takes across a timeline region. */
    struct CompSwipeRegion
    {
        Seconds startTime { 0.0 };
        Seconds length { 0.0 };
        int     takeA { 0 };
        int     takeB { 1 };
        /** 0 = full takeA, 1 = full takeB; curve points optional later. */
        float   crossfadePosition { 0.5f };
    };

    /** Elastic warp mode for an audio clip. */
    enum class ElasticMode
    {
        none,           // no stretch
        offlineOLA,     // message-thread OLA bake (current default)
        realtimePreview // granular/OLA block preview during playback (future)
    };

    /** MIDI portamento slide — pitch path between two notes (beats relative to clip). */
    struct PortamentoSlide
    {
        double startBeat { 0.0 };
        double endBeat { 0.0 };
        int    fromPitch { 60 };
        int    toPitch { 64 };
        /** 0 = linear, 1 = exponential scoop (matches piano-roll slide mode). */
        float  curve { 0.5f };
    };

    /** Editor session: which ghost-note sources are visible (not persisted on clip). */
    struct GhostNoteConfig
    {
        bool showOtherTracks { true };
        bool showPatternClips { true };
        bool showSameTrackClips { false };
        float opacity { 0.35f };
    };

    /** Warp marker for elastic time (musical or absolute). */
    struct WarpMarker
    {
        Seconds sourceTime { 0.0 };    // position in source audio
        Seconds timelineTime { 0.0 };  // mapped position on clip timeline
    };

} // namespace freequency::models
