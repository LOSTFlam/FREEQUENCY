#pragma once

#include "Models/HarmonicEdit.h"

#include <vector>

namespace freequency::engine
{
    /**
        Piecewise-linear warp map between clip timeline and source audio time.
    */
    class WarpMapper
    {
    public:
        /** Timeline seconds (clip-relative) → source seconds. */
        static double timelineToSource (double timelineSec,
                                        const std::vector<models::WarpMarker>& markers,
                                        double clipLengthSec,
                                        double sourceLengthSec,
                                        double stretchRatio = 1.0);

        /** Source seconds → timeline seconds. */
        static double sourceToTimeline (double sourceSec,
                                        const std::vector<models::WarpMarker>& markers,
                                        double clipLengthSec,
                                        double sourceLengthSec,
                                        double stretchRatio = 1.0);

        /** Ensure start/end anchors exist for editing. */
        static void ensureEndpoints (std::vector<models::WarpMarker>& markers,
                                     double clipLengthSec,
                                     double sourceLengthSec);
    };
} // namespace freequency::engine
