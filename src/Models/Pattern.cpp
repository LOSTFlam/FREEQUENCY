#include "Models/Pattern.h"

namespace freequency::models
{
    Pattern::Pattern() = default;

    PatternChannel& Pattern::addStepChannel (const juce::String& channelName, int rootNote)
    {
        auto channel = std::make_unique<PatternChannel> (channelName);

        StepSequence seq;
        seq.rootNote = rootNote;
        seq.steps.resize ((size_t) getStepCount());
        channel->content = std::move (seq);

        channels.push_back (std::move (channel));
        return *channels.back();
    }

    PatternChannel& Pattern::addNoteChannel (const juce::String& channelName)
    {
        auto channel = std::make_unique<PatternChannel> (channelName);
        channel->content = NoteSequence {};
        channels.push_back (std::move (channel));
        return *channels.back();
    }
} // namespace freequency::models
