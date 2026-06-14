#pragma once

#include "UI/UIContext.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace omnidaw::ui
{
    /**
        PlayheadOverlay — a transparent, click-through layer drawn over the lanes
        that renders the moving playhead line.

        A timer (owned by ArrangeView) calls refresh() each frame; the line position
        is derived from the engine Transport, offset by the lane viewport's scroll.
        It intercepts no mouse events so the lanes underneath stay interactive.
    */
    class PlayheadOverlay final : public juce::Component
    {
    public:
        explicit PlayheadOverlay (UIContext& ctx) : context (ctx)
        {
            setInterceptsMouseClicks (false, false);
        }

        void setViewOffsetX (int x) { viewOffsetX = x; }

        void refresh()
        {
            const auto seconds = context.engine.getTransport().getPositionSeconds();
            const int newX = context.secondsToX (seconds) - viewOffsetX;
            if (newX != lastX)
            {
                lastX = newX;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            if (lastX < 0 || lastX > getWidth())
                return;

            g.setColour (juce::Colour (OmniLookAndFeel_accent));
            g.drawVerticalLine (lastX, 0.0f, (float) getHeight());

            juce::Path tri;
            tri.addTriangle ((float) lastX - 5.0f, 0.0f,
                             (float) lastX + 5.0f, 0.0f,
                             (float) lastX, 8.0f);
            g.fillPath (tri);
        }

    private:
        // Avoid pulling the L&F header in; inline the accent colour value.
        static constexpr juce::uint32 OmniLookAndFeel_accent = 0xff2dd4bf;

        UIContext& context;
        int viewOffsetX { 0 };
        int lastX { -1 };
    };
} // namespace omnidaw::ui
