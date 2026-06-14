#include "UI/ArrangeView.h"
#include "UI/OmniLookAndFeel.h"

namespace omnidaw::ui
{
    ArrangeView::ArrangeView (UIContext& ctx)
        : context (ctx), ruler (ctx), playhead (ctx)
    {
        addAndMakeVisible (ruler);

        headerViewport.setViewedComponent (&headerContent, false);
        headerViewport.setScrollBarsShown (false, false);
        addAndMakeVisible (headerViewport);

        laneViewport.setViewedComponent (&laneContent, false);
        laneViewport.setScrollBarsShown (true, true);
        laneViewport.onScroll = [this] { syncScroll(); };
        addAndMakeVisible (laneViewport);

        addAndMakeVisible (playhead);

        ruler.onSeek = [this] (double seconds)
        {
            context.engine.getTransport().setPositionSeconds (seconds);
            playhead.refresh();
        };

        rebuildTracks();
        startTimerHz (30);
    }

    ArrangeView::~ArrangeView()
    {
        stopTimer();
    }

    void ArrangeView::rebuildTracks()
    {
        headers.clear();
        lanes.clear();

        auto& timeline = context.project.getTimeline();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            auto* header = headers.add (new TrackHeaderComponent (context, *track));
            headerContent.addAndMakeVisible (header);

            auto* lane = lanes.add (new TrackLaneComponent (context, *track, i));
            lane->onClipsChanged = [this] { updateContentSize(); };
            laneContent.addAndMakeVisible (lane);

            header->onAutomationToggled = [lane] { lane->repaint(); };
        }

        updateContentSize();
    }

    double ArrangeView::computeContentSeconds() const
    {
        double maxEnd = 30.0; // always show at least half a minute
        auto& timeline = context.project.getTimeline();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            for (int c = 0; c < track->getNumClips(); ++c)
                if (auto* clip = track->getClip (c))
                    maxEnd = juce::jmax (maxEnd, clip->getEndTime());
        }

        return maxEnd + 16.0; // headroom to drop new clips past the end
    }

    void ArrangeView::updateContentSize()
    {
        const int contentWidth  = juce::jmax (getWidth(), context.secondsToX (computeContentSeconds()));
        const int numTracks     = lanes.size();
        const int contentHeight = juce::jmax (getHeight(), numTracks * UIContext::laneHeight);

        laneContent.setSize (contentWidth, contentHeight);
        headerContent.setSize (UIContext::headerWidth, contentHeight);

        for (int i = 0; i < lanes.size(); ++i)
        {
            lanes[i]->setBounds (0, i * UIContext::laneHeight, contentWidth, UIContext::laneHeight);
            headers[i]->setBounds (0, i * UIContext::laneHeight, UIContext::headerWidth, UIContext::laneHeight);
        }
    }

    void ArrangeView::syncScroll()
    {
        const auto pos = laneViewport.getViewPosition();
        headerViewport.setViewPosition (0, pos.y);
        ruler.setViewOffsetX (pos.x);
        playhead.setViewOffsetX (pos.x);
        playhead.refresh();
    }

    void ArrangeView::timerCallback()
    {
        playhead.refresh();
    }

    void ArrangeView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (OmniLookAndFeel::background));

        // Top-left corner block above the header column.
        g.setColour (juce::Colour (OmniLookAndFeel::panelLight));
        g.fillRect (0, 0, UIContext::headerWidth, UIContext::rulerHeight);
        g.setColour (juce::Colour (OmniLookAndFeel::textDim));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("TRACKS", juce::Rectangle<int> (0, 0, UIContext::headerWidth, UIContext::rulerHeight)
                                  .reduced (10, 0),
                    juce::Justification::centredLeft, false);
    }

    void ArrangeView::resized()
    {
        const int hw = UIContext::headerWidth;
        const int rh = UIContext::rulerHeight;

        ruler.setBounds (hw, 0, getWidth() - hw, rh);
        headerViewport.setBounds (0, rh, hw, getHeight() - rh);
        laneViewport.setBounds (hw, rh, getWidth() - hw, getHeight() - rh);
        playhead.setBounds (laneViewport.getBounds());

        updateContentSize();
        syncScroll();
    }
} // namespace omnidaw::ui
