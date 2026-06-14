#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace freequency::engine
{
    /**
        AutomationSnapshot — immutable, audio-thread-ready sampling of an
        AutomationCurve, with breakpoints expressed in absolute samples.

        Built on the message thread from the model curve and published via
        SnapshotHolder. evaluate() does a branch-light linear interpolation given
        the current transport sample position.
    */
    class AutomationSnapshot final : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<AutomationSnapshot>;

        struct Node
        {
            juce::int64 sample { 0 };
            float value { 1.0f };
        };

        std::vector<Node> nodes; // sorted by sample

        [[nodiscard]] float evaluate (juce::int64 pos) const noexcept
        {
            if (nodes.empty())
                return 1.0f;

            if (pos <= nodes.front().sample) return nodes.front().value;
            if (pos >= nodes.back().sample)  return nodes.back().value;

            for (size_t i = 1; i < nodes.size(); ++i)
            {
                if (pos <= nodes[i].sample)
                {
                    const auto& a = nodes[i - 1];
                    const auto& b = nodes[i];
                    const auto span = (double) (b.sample - a.sample);
                    if (span <= 0.0)
                        return b.value;
                    const auto t = (float) ((double) (pos - a.sample) / span);
                    return a.value + t * (b.value - a.value);
                }
            }

            return nodes.back().value;
        }
    };
} // namespace freequency::engine
