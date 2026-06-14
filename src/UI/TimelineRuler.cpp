#include "UI/TimelineRuler.h"
#include "UI/OmniLookAndFeel.h"

namespace omnidaw::ui
{
    void TimelineRuler::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (OmniLookAndFeel::panelLight));
        g.setColour (juce::Colour (OmniLookAndFeel::outline));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        const auto& timeline = context.project.getTimeline();
        const auto secsPerBeat = 60.0 / juce::jmax (1.0, timeline.getTempoBpm());
        const auto beatsPerBar = juce::jmax (1, timeline.getTimeSigNumerator());
        const auto secsPerBar = secsPerBeat * beatsPerBar;

        // Find the first bar visible given the scroll offset.
        const auto firstSec = context.xToSeconds (viewOffsetX);
        int bar = (int) (firstSec / secsPerBar);

        g.setFont (juce::FontOptions (11.0f));

        for (double t = bar * secsPerBar; ; t += secsPerBar, ++bar)
        {
            const int x = context.secondsToX (t) - viewOffsetX;
            if (x > getWidth())
                break;

            g.setColour (juce::Colour (OmniLookAndFeel::textPrimary));
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
            g.drawText (juce::String (bar + 1), x + 4, 2, 40, getHeight() - 4,
                        juce::Justification::topLeft, false);

            // Beat ticks within the bar.
            g.setColour (juce::Colour (OmniLookAndFeel::textDim).withAlpha (0.5f));
            for (int b = 1; b < beatsPerBar; ++b)
            {
                const int bx = context.secondsToX (t + b * secsPerBeat) - viewOffsetX;
                g.drawVerticalLine (bx, getHeight() * 0.6f, (float) getHeight());
            }
        }
    }
} // namespace omnidaw::ui
