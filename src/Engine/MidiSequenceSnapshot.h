#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace omnidaw::engine
{
    /**
        MidiSequenceSnapshot — an immutable, reference-counted view of a track's
        MIDI events, ready for the audio thread.

        Why a snapshot?
        ---------------
        The model's MidiClips can be edited on the message thread at any time. The
        audio thread must never read a container that another thread is mutating.
        So whenever the model changes, the message thread *builds a fresh
        snapshot* (events flattened to absolute sample timestamps and sorted) and
        atomically swaps it into the MidiSourceProcessor. The audio thread holds a
        ref-counted pointer to whichever snapshot was current when its block
        began; old snapshots are released safely once no one references them.

        This is the lock-free "publish an immutable copy" pattern — no allocation,
        no locking and no torn reads on the audio thread.
    */
    class MidiSequenceSnapshot final : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<MidiSequenceSnapshot>;

        MidiSequenceSnapshot() { events.clear(); }

        /** Note/CC events with timestamps in ABSOLUTE samples on the timeline.
            Built and sorted on the message thread; read-only thereafter.
        */
        juce::MidiMessageSequence events;

        /** One past the last event sample; lets readers early-out cheaply. */
        juce::int64 endSample { 0 };
    };
} // namespace omnidaw::engine
