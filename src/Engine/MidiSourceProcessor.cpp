#include "Engine/MidiSourceProcessor.h"

namespace omnidaw::engine
{
    MidiSourceProcessor::MidiSourceProcessor (Transport& sharedTransport)
        : AudioProcessor (BusesProperties()), // no audio buses: MIDI only
          transport (sharedTransport)
    {
    }

    void MidiSourceProcessor::prepareToPlay (double, int)
    {
        wasPlaying = false;
    }

    void MidiSourceProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
    {
        midi.clear();

        const bool playing = transport.isPlaying();

        // When playback stops, emit an all-notes-off so hosted instruments don't
        // drone. Done exactly once on the playing->stopped edge.
        if (! playing)
        {
            if (wasPlaying)
            {
                for (int ch = 1; ch <= 16; ++ch)
                    midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
                wasPlaying = false;
            }
            return;
        }

        wasPlaying = true;

        const auto snapshot = holder.getForAudio();
        if (snapshot == nullptr)
            return;

        auto& seq = snapshot->events;
        const int numEvents = seq.getNumEvents();
        if (numEvents == 0)
            return;

        const auto blockStart = (double) transport.getPositionSamples();
        const auto blockEnd   = blockStart + (double) getBlockSize();
        const auto numSamples = getBlockSize();

        // Events are pre-sorted by absolute sample time; jump straight to the
        // first one at/after the block start, then walk forward.
        for (int i = seq.getNextIndexAtTime (blockStart); i < numEvents; ++i)
        {
            auto* meh = seq.getEventPointer (i);
            const auto t = meh->message.getTimeStamp();

            if (t >= blockEnd)
                break;

            auto offset = (int) (t - blockStart);
            offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);
            midi.addEvent (meh->message, offset);
        }
    }
} // namespace omnidaw::engine
