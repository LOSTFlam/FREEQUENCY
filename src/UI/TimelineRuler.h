#pragma once

#include "UI/UIContext.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace freequency::ui
{
    /**
        TimelineRuler — the bar/beat ruler above the lanes.

        Draws bar numbers and beat ticks, offset by the lane viewport's horizontal
        scroll so it stays aligned with the clips. Clicking (or dragging) on it
        relocates the transport playhead.
    */
    class TimelineRuler final : public juce::Component
    {
    public:
        explicit TimelineRuler (UIContext& ctx) : context (ctx) {}

        /** Horizontal scroll offset (pixels) of the lane viewport. */
        void setViewOffsetX (int x) { viewOffsetX = x; repaint(); }

        std::function<void (double)> onSeek; // seconds

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override { seek (e); }
        void mouseDrag (const juce::MouseEvent& e) override { seek (e); }

    private:
        void seek (const juce::MouseEvent& e)
        {
            if (onSeek)
                onSeek (juce::jmax (0.0, context.xToSeconds (e.x + viewOffsetX)));
        }

        UIContext& context;
        int viewOffsetX { 0 };
    };
} // namespace freequency::ui
