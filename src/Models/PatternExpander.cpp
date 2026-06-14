#include "Models/PatternExpander.h"

namespace freequency::models
{
    namespace
    {
        double beatsPerSecond (double tempoBpm) { return tempoBpm / 60.0; }

        void addNote (juce::MidiMessageSequence& out, int channel, int pitch, int velocity,
                      double startSec, double endSec)
        {
            out.addEvent (juce::MidiMessage::noteOn (channel, pitch, (juce::uint8) velocity), startSec);
            out.addEvent (juce::MidiMessage::noteOff (channel, pitch), endSec);
        }
    } // namespace

    void PatternExpander::expandToSequence (const Pattern& pattern,
                                            juce::MidiMessageSequence& out,
                                            double clipLengthSec,
                                            double tempoBpm,
                                            int midiChannel)
    {
        if (clipLengthSec <= 0.0 || tempoBpm <= 0.0)
            return;

        const double secPerBeat = 60.0 / tempoBpm;
        const double patternSec = pattern.lengthInBeats * secPerBeat;
        if (patternSec <= 0.0)
            return;

        const int loops = (int) std::ceil (clipLengthSec / patternSec);

        for (int loop = 0; loop < loops; ++loop)
        {
            const double loopOffsetSec = loop * patternSec;

            for (int ch = 0; ch < pattern.getNumChannels(); ++ch)
            {
                const auto& channel = pattern.getChannel (ch);

                if (auto* steps = std::get_if<StepSequence> (&channel.content))
                {
                    const int stepCount = juce::jmax (1, pattern.getStepCount());
                    const double beatPerStep = pattern.lengthInBeats / (double) stepCount;
                    const double stepSec = beatPerStep * secPerBeat;

                    for (int i = 0; i < stepCount; ++i)
                    {
                        if (i >= (int) steps->steps.size() || ! steps->steps[(size_t) i].on)
                            continue;

                        const double startSec = loopOffsetSec + i * stepSec;
                        if (startSec >= clipLengthSec)
                            break;

                        const int vel = juce::jlimit (1, 127, (int) std::round (steps->steps[(size_t) i].velocity * 127.0f));
                        addNote (out, midiChannel, steps->rootNote, vel, startSec,
                                 juce::jmin (clipLengthSec, startSec + stepSec * 0.9));
                    }
                }
                else if (auto* notes = std::get_if<NoteSequence> (&channel.content))
                {
                    for (int e = 0; e < notes->notes.getNumEvents(); ++e)
                    {
                        auto* on = notes->notes.getEventPointer (e);
                        if (! on->message.isNoteOn())
                            continue;

                        const double startSec = loopOffsetSec + on->message.getTimeStamp();
                        if (startSec >= clipLengthSec)
                            continue;

                        double endSec = startSec + 0.1;
                        if (on->noteOffObject != nullptr)
                            endSec = loopOffsetSec + on->noteOffObject->message.getTimeStamp();

                        endSec = juce::jmin (clipLengthSec, endSec);
                        addNote (out, midiChannel, on->message.getNoteNumber(), on->message.getVelocity(),
                                 startSec, endSec);
                    }
                }
            }
        }

        out.updateMatchedPairs();
    }

    void PatternExpander::collectPreviewNotes (const Pattern& pattern,
                                               juce::Array<PreviewNote>& out,
                                               double clipLengthSec,
                                               double tempoBpm)
    {
        juce::MidiMessageSequence seq;
        expandToSequence (pattern, seq, clipLengthSec, tempoBpm);

        const double bps = beatsPerSecond (tempoBpm);
        if (bps <= 0.0)
            return;

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* on = seq.getEventPointer (i);
            if (! on->message.isNoteOn())
                continue;

            PreviewNote n;
            n.startBeats = on->message.getTimeStamp() * bps;
            n.pitch = on->message.getNoteNumber();
            n.velocity = on->message.getVelocity();

            if (on->noteOffObject != nullptr)
                n.lengthBeats = (on->noteOffObject->message.getTimeStamp() - on->message.getTimeStamp()) * bps;
            else
                n.lengthBeats = 0.25;

            out.add (n);
        }
    }
} // namespace freequency::models
