#pragma once

#include "Models/Project.h"
#include "Models/Track.h"
#include "Engine/AudioEngine.h"

#include <functional>

namespace freequency::ui
{
    /**
        UIContext — the small bundle of shared state every arrange-view component
        needs: the document, the engine, and the current horizontal zoom.

        Passed by reference so all components agree on the same timebase. Keeping
        the zoom here (rather than duplicated per component) means the ruler,
        lanes and playhead always convert seconds<->pixels identically.
    */
    struct UIContext
    {
        UIContext (models::Project& p, engine::AudioEngine& e) : project (p), engine (e) {}

        models::Project& project;
        engine::AudioEngine& engine;

        // Opens a floating editor window for any processor (set by MainComponent).
        std::function<void (juce::AudioProcessor*, juce::String)> openProcessorEditor;
        // Closes all open plugin editor windows (call before a graph rebuild, which
        // invalidates the live insert processors those windows reference).
        std::function<void()> closePluginWindows;

        // Selection (drives keyboard clip-editing commands).
        models::Track* selectedTrack { nullptr };
        models::Clip*  selectedClip  { nullptr };

        std::function<void()> repaintArrange; // lightweight: repaint lanes
        std::function<void()> rebuildArrange; // structural: recreate lanes after add/remove
        std::function<void()> pushUndo;       // snapshot the project before an edit
        std::function<void (models::MidiClip&, models::Track&)> openPianoRoll;
        std::function<juce::File()> getBrowserSelectedFile; // for drag-and-drop from the browser

        double pixelsPerSecond { 90.0 };

        // Grid snap (FL/Cubase-style). Clip placement snaps to this division.
        enum class Snap { off, bar, half, quarter, eighth, sixteenth };
        Snap snap { Snap::bar };

        /** Snap a time (seconds) to the current grid division. */
        [[nodiscard]] double snapTime (double seconds) const noexcept
        {
            if (snap == Snap::off)
                return juce::jmax (0.0, seconds);

            const auto& timeline = project.getTimeline();
            const double secsPerBeat = 60.0 / juce::jmax (1.0, timeline.getTempoBpm());
            const double secsPerBar  = secsPerBeat * juce::jmax (1, timeline.getTimeSigNumerator());

            double grid = secsPerBar;
            switch (snap)
            {
                case Snap::off:       return juce::jmax (0.0, seconds);
                case Snap::bar:       grid = secsPerBar;        break;
                case Snap::half:      grid = secsPerBar * 0.5;  break;
                case Snap::quarter:   grid = secsPerBeat;       break;
                case Snap::eighth:    grid = secsPerBeat * 0.5; break;
                case Snap::sixteenth: grid = secsPerBeat * 0.25;break;
            }

            return juce::jmax (0.0, std::round (seconds / grid) * grid);
        }

        // Layout constants shared across the arrange view.
        static constexpr int headerWidth = 220;
        static constexpr int rulerHeight = 26;
        static constexpr int laneHeight  = 78;

        [[nodiscard]] int secondsToX (double seconds) const noexcept
        {
            return (int) std::llround (seconds * pixelsPerSecond);
        }

        [[nodiscard]] double xToSeconds (int x) const noexcept
        {
            return pixelsPerSecond > 0.0 ? (double) x / pixelsPerSecond : 0.0;
        }
    };
} // namespace freequency::ui
