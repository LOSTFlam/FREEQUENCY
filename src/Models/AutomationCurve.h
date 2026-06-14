#pragma once

#include "Core/Types.h"

#include <vector>
#include <algorithm>

namespace omnidaw::models
{
    /**
        AutomationCurve — a time-ordered set of breakpoints driving a parameter.

        Phase 5 uses linear segments between nodes (the most common and
        predictable automation shape). Points are kept sorted by time; evaluate()
        does a linear interpolation and holds the end values beyond the range.

        This is a pure data model: the UI edits it, and the engine reads a sampled
        snapshot of it on the audio thread (never this container directly).
    */
    class AutomationCurve
    {
    public:
        struct Point
        {
            Seconds time { 0.0 };
            float   value { 1.0f };
        };

        [[nodiscard]] int getNumPoints() const noexcept { return (int) points.size(); }
        [[nodiscard]] const Point& getPoint (int i) const noexcept { return points[(size_t) i]; }
        [[nodiscard]] bool isEmpty() const noexcept { return points.empty(); }

        /** Insert a point, keeping the curve sorted by time. Returns its index. */
        int addPoint (Seconds time, float value)
        {
            Point p { time, value };
            auto it = std::lower_bound (points.begin(), points.end(), p,
                                        [] (const Point& a, const Point& b) { return a.time < b.time; });
            const auto idx = (int) std::distance (points.begin(), it);
            points.insert (it, p);
            return idx;
        }

        void movePoint (int index, Seconds time, float value)
        {
            if (index < 0 || index >= getNumPoints())
                return;

            points[(size_t) index] = { juce::jmax (0.0, time), value };
            std::sort (points.begin(), points.end(),
                       [] (const Point& a, const Point& b) { return a.time < b.time; });
        }

        /** Update a point in place WITHOUT re-sorting (cheap during a drag). Call
            sortPoints() when the drag finishes. */
        void setPointTimeValue (int index, Seconds time, float value)
        {
            if (index >= 0 && index < getNumPoints())
                points[(size_t) index] = { juce::jmax (0.0, time), value };
        }

        void sortPoints()
        {
            std::sort (points.begin(), points.end(),
                       [] (const Point& a, const Point& b) { return a.time < b.time; });
        }

        void removePoint (int index)
        {
            if (index >= 0 && index < getNumPoints())
                points.erase (points.begin() + index);
        }

        void clear() { points.clear(); }

        /** Linearly interpolate the value at `time`. */
        [[nodiscard]] float evaluate (Seconds time) const noexcept
        {
            if (points.empty())
                return 1.0f;

            if (time <= points.front().time)  return points.front().value;
            if (time >= points.back().time)   return points.back().value;

            for (size_t i = 1; i < points.size(); ++i)
            {
                if (time <= points[i].time)
                {
                    const auto& a = points[i - 1];
                    const auto& b = points[i];
                    const auto span = b.time - a.time;
                    if (span <= 0.0)
                        return b.value;
                    const auto t = (float) ((time - a.time) / span);
                    return a.value + t * (b.value - a.value);
                }
            }

            return points.back().value;
        }

    private:
        std::vector<Point> points;
    };
} // namespace omnidaw::models
