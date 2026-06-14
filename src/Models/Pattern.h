#pragma once

#include "Core/Types.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <variant>
#include <vector>

namespace freequency::models
{
    /**
        Pattern — FL Studio's "Channel Rack" content: a reusable bundle of
        per-instrument sequences that can be placed many times on the timeline as
        PatternClips. Editing the pattern updates every placement.

        Each channel is EITHER:
          - a StepSequence (the classic FL step grid: on/off cells at a fixed
            resolution, all triggering one note), or
          - a NoteSequence (free piano-roll notes),
        modelled with a std::variant so a channel is exhaustively one-of-two and
        type-safe — no parallel enum/union, no invalid states.
    */
    struct Step
    {
        bool  on { false };
        float velocity { 0.8f };
    };

    /** Classic step grid: `steps` cells, each triggering `rootNote`. */
    struct StepSequence
    {
        int rootNote { 60 };           // note triggered by an active step
        std::vector<Step> steps;       // size == pattern step count
    };

    /** Free piano-roll content (timestamps in beats, relative to pattern start). */
    struct NoteSequence
    {
        juce::MidiMessageSequence notes;
    };

    using ChannelContent = std::variant<StepSequence, NoteSequence>;

    class PatternChannel
    {
    public:
        explicit PatternChannel (juce::String channelName) : name (std::move (channelName)) {}

        juce::String name;
        /** Instrument plugin identifier; empty => use the built-in synth. */
        juce::String instrumentPluginIdentifier;

        ChannelContent content { StepSequence {} };

        [[nodiscard]] bool isStepChannel() const noexcept
        {
            return std::holds_alternative<StepSequence> (content);
        }
    };

    /**
        Pattern — owns its channels and the step grid resolution. `lengthInBeats`
        gives the musical length so a PatternClip knows how wide it is and the
        sequencer knows when it loops.
    */
    class Pattern
    {
    public:
        Pattern();

        [[nodiscard]] const ObjectId& getId() const noexcept { return id; }

        juce::String name { "Pattern" };
        juce::Colour colour { juce::Colours::mediumpurple };

        /** Step grid resolution and length. 16 steps per bar = 1/16 notes. */
        int stepsPerBar { 16 };
        double lengthInBeats { 4.0 }; // one 4/4 bar by default

        [[nodiscard]] int getNumChannels() const noexcept { return (int) channels.size(); }
        [[nodiscard]] PatternChannel& getChannel (int i) noexcept { return *channels[(size_t) i]; }
        [[nodiscard]] const PatternChannel& getChannel (int i) const noexcept { return *channels[(size_t) i]; }

        /** Adds a step-grid channel sized to the current grid; returns it. */
        PatternChannel& addStepChannel (const juce::String& channelName, int rootNote);

        /** Adds an (empty) piano-roll channel; returns it. */
        PatternChannel& addNoteChannel (const juce::String& channelName);

        [[nodiscard]] int getStepCount() const noexcept
        {
            return juce::jmax (1, (int) std::llround (lengthInBeats / 4.0 * stepsPerBar));
        }

    private:
        const ObjectId id;
        std::vector<std::unique_ptr<PatternChannel>> channels;
    };
} // namespace freequency::models
