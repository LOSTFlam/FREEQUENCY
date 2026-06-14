#include "Engine/PortamentoExpander.h"

#include <algorithm>
#include <cmath>

namespace freequency::engine
{
    namespace
    {
        int pitchWheelForSemitones (double semisFromTarget) noexcept
        {
            // Built-in synth uses ±12 semitone pitch-wheel range.
            const double clamped = juce::jlimit (-12.0, 12.0, semisFromTarget);
            return (int) std::llround (8192.0 + (clamped / 12.0) * 8192.0);
        }

        double curveT (double t, float curve) noexcept
        {
            t = juce::jlimit (0.0, 1.0, t);
            const double lin = t;
            const double exp = t * t;
            return lin * (1.0 - (double) curve) + exp * (double) curve;
        }

        struct NoteOnEvent
        {
            double startSec { 0.0 };
            int pitch { 60 };
            bool slide { false };
        };
    } // namespace

    void PortamentoExpander::expandSlides (const models::MidiClip& clip,
                                           juce::MidiMessageSequence& sequenceOut,
                                           double tempoBpm,
                                           int pitchBendSteps)
    {
        if (clip.portamentoSlides.empty() || tempoBpm <= 0.0)
            return;

        const double secPerBeat = 60.0 / tempoBpm;
        const int steps = juce::jmax (2, pitchBendSteps);

        for (const auto& slide : clip.portamentoSlides)
        {
            const double startSec = slide.startBeat * secPerBeat;
            const double endSec = slide.endBeat * secPerBeat;
            const double span = juce::jmax (1.0e-6, endSec - startSec);
            const double semisTotal = (double) (slide.toPitch - slide.fromPitch);

            for (int s = 0; s <= steps; ++s)
            {
                const double t = (double) s / (double) steps;
                const double curved = curveT (t, slide.curve);
                const double semisFromTarget = semisTotal * (curved - 1.0);
                const double timeSec = startSec + span * t;

                sequenceOut.addEvent (juce::MidiMessage::pitchWheel (1, pitchWheelForSemitones (semisFromTarget)),
                                      timeSec);
            }
        }

        sequenceOut.sort();
    }

    void PortamentoExpander::rebuildSlidesFromNotes (const juce::MidiMessageSequence& sequence,
                                                     double tempoBpm,
                                                     std::vector<models::PortamentoSlide>& slidesOut)
    {
        slidesOut.clear();
        if (tempoBpm <= 0.0)
            return;

        const double secPerBeat = 60.0 / tempoBpm;
        std::vector<NoteOnEvent> notes;

        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto& msg = sequence.getEventPointer (i)->message;
            if (! msg.isNoteOn())
                continue;

            NoteOnEvent ev;
            ev.startSec = msg.getTimeStamp();
            ev.pitch = msg.getNoteNumber();
            notes.push_back (ev);
        }

        std::sort (notes.begin(), notes.end(),
                   [] (const NoteOnEvent& a, const NoteOnEvent& b) { return a.startSec < b.startSec; });

        for (size_t i = 1; i < notes.size(); ++i)
        {
            if (! notes[i].slide)
                continue;

            models::PortamentoSlide slide;
            slide.fromPitch = notes[i - 1].pitch;
            slide.toPitch = notes[i].pitch;
            slide.endBeat = notes[i].startSec / secPerBeat;
            slide.startBeat = juce::jmax (0.0, slide.endBeat - juce::jmin (0.5, 0.35));
            slide.curve = 0.55f;
            slidesOut.push_back (slide);
        }
    }
} // namespace freequency::engine
