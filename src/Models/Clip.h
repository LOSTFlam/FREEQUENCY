#pragma once

#include "Core/Types.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace freequency::models
{
    /** Discriminator for the polymorphic Clip hierarchy.

        We keep an explicit enum (rather than relying solely on dynamic_cast) so
        that hot, UI-facing code can branch on clip type cheaply.
    */
    enum class ClipType
    {
        audio,
        midi,
        pattern   // an instance of a reusable Pattern (FL playlist block)
    };

    /**
        Clip / Region — the base class for anything placed on a track's timeline.

        A Clip is a *view* onto some source material positioned on the timeline.
        It owns timeline placement (where it sits and how long it is) and shared
        presentation (name, colour). Concrete subclasses own the actual content:
        AudioClip references a file on disk, MidiClip owns a MIDI sequence.

        Design notes:
          - This is a pure data model: it knows nothing about juce::Component or
            rendering. The UI observes/draws it; the engine reads it.
          - Times are stored in seconds. Sample-accurate conversion happens in the
            engine using the current sample rate, keeping the model rate-agnostic.
    */
    class Clip
    {
    public:
        explicit Clip (ClipType clipType);
        virtual ~Clip() = default;

        Clip (const Clip&) = delete;
        Clip& operator= (const Clip&) = delete;

        [[nodiscard]] ClipType getType() const noexcept { return type; }
        [[nodiscard]] const ObjectId& getId() const noexcept { return id; }

        /** Human-readable name shown in the UI. */
        juce::String name;

        /** Display colour; defaults to a neutral grey until assigned. */
        juce::Colour colour { juce::Colours::grey };

        /** Start position on the timeline, in seconds. */
        Seconds startTime { 0.0 };

        /** Duration on the timeline, in seconds. */
        Seconds length { 0.0 };

        [[nodiscard]] Seconds getEndTime() const noexcept { return startTime + length; }

        /** True if the given timeline position (seconds) falls within this clip. */
        [[nodiscard]] bool containsTime (Seconds t) const noexcept
        {
            return t >= startTime && t < getEndTime();
        }

    private:
        const ObjectId id;     // generated on construction; stable for the clip's life
        const ClipType type;
    };

    /**
        AudioClip — a region that plays back samples read from a file on disk.

        The clip does NOT hold the audio in memory; it references the file and an
        offset into it. The engine streams from disk (Phase 5) and the UI draws a
        thumbnail (Phase 3). Keeping only a reference here is what lets a project
        contain hours of audio without exhausting RAM.
    */
    class AudioClip final : public Clip
    {
    public:
        AudioClip();

        /** The source audio file this clip reads from. */
        juce::File sourceFile;

        /** Offset, in seconds, into the source file where playback begins.
            Lets a clip represent a trimmed sub-region of a longer recording.
        */
        Seconds sourceOffset { 0.0 };

        /** Per-clip linear gain applied on top of the track fader. */
        float gain { 1.0f };

        /** Play the clip's audio backwards. */
        bool reversed { false };

        /** Time-stretch ratio (1 = original; >1 = longer/slower, pitch preserved). */
        double stretchRatio { 1.0 };

        /** Pitch shift in semitones (length preserved). */
        int pitchSemitones { 0 };

        /** Comping: alternative recorded takes for this clip. The active take is
            mirrored into `sourceFile` (so playback/thumbnail follow it). */
        juce::StringArray takeFiles;
        int activeTake { 0 };

        [[nodiscard]] int getNumTakes() const noexcept { return takeFiles.size(); }
    };

    /**
        MidiClip — a region containing a sequence of MIDI events.

        Owns a juce::MidiMessageSequence with timestamps expressed relative to the
        clip start (in seconds). The sequencer reads from this and feeds events to
        the instrument node via a lock-free FIFO (Phase 2+), never touching it from
        the audio thread directly.
    */
    class MidiClip final : public Clip
    {
    public:
        MidiClip();

        /** The note/CC data for this clip, timestamped relative to startTime. */
        juce::MidiMessageSequence sequence;
    };

    /**
        PatternClip — a placement of a reusable Pattern on the linear timeline.

        This is the bridge between the FL pattern workflow and the Cubase linear
        arranger: the clip merely *references* a Pattern (by id); the pattern's
        content lives once in the Project and is shared by every placement, so
        editing the pattern updates all its clips. The engine expands the
        referenced pattern into MIDI for the clip's time range at render.
    */
    class PatternClip final : public Clip
    {
    public:
        PatternClip();

        /** Id of the Pattern this clip instantiates (dashed UUID string). */
        juce::String patternId;
    };
} // namespace freequency::models
