#pragma once

#include "Models/Project.h"
#include "Engine/AudioEngine.h"

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
        models::Project& project;
        engine::AudioEngine& engine;

        double pixelsPerSecond { 90.0 };

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
