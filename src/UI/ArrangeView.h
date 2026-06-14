#pragma once

#include "UI/UIContext.h"
#include "UI/TimelineRuler.h"
#include "UI/PlayheadOverlay.h"
#include "UI/TrackHeaderComponent.h"
#include "UI/TrackLaneComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace freequency::ui
{
    /**
        ArrangeView — the main editing surface: ruler, track headers and clip lanes.

        Layout
        ------
            ┌────────────┬──────────────────────────────┐
            │  (corner)  │           ruler              │
            ├────────────┼──────────────────────────────┤
            │  header    │                              │
            │  column    │      lanes (clips)           │  ← both scroll
            │ (vert      │      + playhead overlay      │
            │  scroll)   │                              │
            └────────────┴──────────────────────────────┘

        The header column and lanes live in separate viewports; the lanes viewport
        is the scroll master and drives the header viewport's vertical position and
        the ruler/playhead horizontal offset, so everything stays aligned.
    */
    class ArrangeView final : public juce::Component,
                              private juce::Timer
    {
    public:
        explicit ArrangeView (UIContext& ctx);
        ~ArrangeView() override;

        /** Recreate header + lane components from the current track list. */
        void rebuildTracks();

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        /** Viewport that reports scroll changes so we can keep panes in sync. */
        struct ScrollViewport final : public juce::Viewport
        {
            std::function<void()> onScroll;
            void visibleAreaChanged (const juce::Rectangle<int>&) override
            {
                if (onScroll) onScroll();
            }
        };

        void timerCallback() override;
        void syncScroll();
        void updateContentSize();
        [[nodiscard]] double computeContentSeconds() const;

        UIContext& context;

        TimelineRuler ruler;
        juce::Viewport headerViewport;
        ScrollViewport laneViewport;
        juce::Component headerContent;
        juce::Component laneContent;
        PlayheadOverlay playhead;

        juce::OwnedArray<TrackHeaderComponent> headers;
        juce::OwnedArray<TrackLaneComponent> lanes;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangeView)
    };
} // namespace freequency::ui
