#pragma once

#include "Models/Clip.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace freequency::engine
{
    /** Expands `PortamentoSlide` curves into pitch-bend events (±12 semitone range). */
    class PortamentoExpander
    {
    public:
        static void expandSlides (const models::MidiClip& clip,
                                  juce::MidiMessageSequence& sequenceOut,
                                  double tempoBpm,
                                  int pitchBendSteps = 24);

        /** Rebuild `portamentoSlides` from consecutive slide-flagged notes. */
        static void rebuildSlidesFromNotes (const juce::MidiMessageSequence& sequence,
                                            double tempoBpm,
                                            std::vector<models::PortamentoSlide>& slidesOut);
    };
} // namespace freequency::engine
