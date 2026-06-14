#include "Engine/WarpMapper.h"

#include <algorithm>

namespace freequency::engine
{
    namespace
    {
        struct MapPoint
        {
            double timeline { 0.0 };
            double source { 0.0 };
        };

        std::vector<MapPoint> buildMap (const std::vector<models::WarpMarker>& markers,
                                        double clipLengthSec,
                                        double sourceLengthSec,
                                        double stretchRatio)
        {
            std::vector<MapPoint> pts;
            pts.push_back ({ 0.0, 0.0 });

            auto sorted = markers;
            std::sort (sorted.begin(), sorted.end(),
                       [] (const models::WarpMarker& a, const models::WarpMarker& b)
                       { return a.timelineTime < b.timelineTime; });

            for (const auto& m : sorted)
            {
                if (m.timelineTime <= 0.0 && m.sourceTime <= 0.0)
                    continue;
                pts.push_back ({ m.timelineTime, m.sourceTime });
            }

            const double endSource = juce::jmax (sourceLengthSec, clipLengthSec * stretchRatio);
            pts.push_back ({ clipLengthSec, endSource });

            std::sort (pts.begin(), pts.end(),
                       [] (const MapPoint& a, const MapPoint& b) { return a.timeline < b.timeline; });

            pts.erase (std::unique (pts.begin(), pts.end(),
                                    [] (const MapPoint& a, const MapPoint& b)
                                    { return std::abs (a.timeline - b.timeline) < 1.0e-9; }),
                       pts.end());
            return pts;
        }

        double interpSourceAt (const std::vector<MapPoint>& pts, double timelineSec)
        {
            if (pts.empty())
                return timelineSec;

            if (timelineSec <= pts.front().timeline)
                return pts.front().source;

            if (timelineSec >= pts.back().timeline)
                return pts.back().source;

            for (size_t i = 1; i < pts.size(); ++i)
            {
                const auto& a = pts[i - 1];
                const auto& b = pts[i];
                if (timelineSec >= a.timeline && timelineSec <= b.timeline)
                {
                    const double span = juce::jmax (1.0e-9, b.timeline - a.timeline);
                    const double t = (timelineSec - a.timeline) / span;
                    return a.source + t * (b.source - a.source);
                }
            }

            return pts.back().source;
        }

        double interpTimelineAt (const std::vector<MapPoint>& pts, double sourceSec)
        {
            if (pts.empty())
                return sourceSec;

            if (sourceSec <= pts.front().source)
                return pts.front().timeline;

            if (sourceSec >= pts.back().source)
                return pts.back().timeline;

            for (size_t i = 1; i < pts.size(); ++i)
            {
                const auto& a = pts[i - 1];
                const auto& b = pts[i];
                if (sourceSec >= a.source && sourceSec <= b.source)
                {
                    const double span = juce::jmax (1.0e-9, b.source - a.source);
                    const double t = (sourceSec - a.source) / span;
                    return a.timeline + t * (b.timeline - a.timeline);
                }
            }

            return pts.back().timeline;
        }
    } // namespace

    void WarpMapper::ensureEndpoints (std::vector<models::WarpMarker>& markers,
                                      double clipLengthSec,
                                      double sourceLengthSec)
    {
        auto hasStart = std::any_of (markers.begin(), markers.end(),
                                     [] (const models::WarpMarker& m) { return m.timelineTime <= 1.0e-6; });
        auto hasEnd = std::any_of (markers.begin(), markers.end(),
                                   [&] (const models::WarpMarker& m)
                                   { return std::abs (m.timelineTime - clipLengthSec) < 1.0e-3; });

        if (! hasStart)
            markers.insert (markers.begin(), { 0.0, 0.0 });
        if (! hasEnd)
            markers.push_back ({ clipLengthSec, sourceLengthSec });
    }

    double WarpMapper::timelineToSource (double timelineSec,
                                         const std::vector<models::WarpMarker>& markers,
                                         double clipLengthSec,
                                         double sourceLengthSec,
                                         double stretchRatio)
    {
        if (markers.empty())
            return timelineSec * stretchRatio;

        const auto pts = buildMap (markers, clipLengthSec, sourceLengthSec, stretchRatio);
        return interpSourceAt (pts, timelineSec);
    }

    double WarpMapper::sourceToTimeline (double sourceSec,
                                         const std::vector<models::WarpMarker>& markers,
                                         double clipLengthSec,
                                         double sourceLengthSec,
                                         double stretchRatio)
    {
        if (markers.empty())
            return sourceSec / juce::jmax (1.0e-4, stretchRatio);

        const auto pts = buildMap (markers, clipLengthSec, sourceLengthSec, stretchRatio);
        return interpTimelineAt (pts, sourceSec);
    }
} // namespace freequency::engine
