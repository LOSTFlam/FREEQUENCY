#pragma once

#include "Models/Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace freequency::models
{
    /**
        Expands a reusable Pattern into MIDI events for PatternClip playback
        and ghost-note preview. Timestamps are seconds relative to clip start.
    */
    class PatternExpander
    {
    public:
        /** Fill `out` with note events looping the pattern to cover `clipLengthSec`. */
        static void expandToSequence (const Pattern& pattern,
                                      juce::MidiMessageSequence& out,
                                      double clipLengthSec,
                                      double tempoBpm,
                                      int midiChannel = 1);

        /** Collect note rectangles for UI (beats relative to clip start). */
        struct PreviewNote
        {
            double startBeats { 0.0 };
            double lengthBeats { 0.25 };
            int pitch { 60 };
            int velocity { 100 };
        };

        static void collectPreviewNotes (const Pattern& pattern,
                                         juce::Array<PreviewNote>& out,
                                         double clipLengthSec,
                                         double tempoBpm);
    };
} // namespace freequency::models
