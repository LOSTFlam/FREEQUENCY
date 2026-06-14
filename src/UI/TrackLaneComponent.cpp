#include "UI/TrackLaneComponent.h"
#include "UI/OmniLookAndFeel.h"

#include "Models/AudioTrack.h"
#include "Models/MidiTrack.h"

namespace omnidaw::ui
{
    TrackLaneComponent::TrackLaneComponent (UIContext& ctx, models::Track& track, int index)
        : context (ctx), trackRef (track), laneIndex (index)
    {
        refreshClips();
    }

    TrackLaneComponent::~TrackLaneComponent()
    {
        for (auto* t : thumbnails)
            t->removeChangeListener (this);
    }

    void TrackLaneComponent::refreshClips()
    {
        for (auto* t : thumbnails)
            t->removeChangeListener (this);
        thumbnails.clear();

        if (auto* audioTrack = dynamic_cast<models::AudioTrack*> (&trackRef))
        {
            for (int i = 0; i < audioTrack->getNumClips(); ++i)
            {
                auto* thumb = thumbnails.add (
                    new juce::AudioThumbnail (512, context.engine.getFormatManager(), thumbnailCache));
                thumb->addChangeListener (this);

                if (auto* clip = dynamic_cast<models::AudioClip*> (audioTrack->getClip (i)))
                    if (clip->sourceFile.existsAsFile())
                        thumb->setSource (new juce::FileInputSource (clip->sourceFile));
            }
        }

        repaint();
    }

    void TrackLaneComponent::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        repaint();
    }

    void TrackLaneComponent::paint (juce::Graphics& g)
    {
        // Alternating lane background for readability.
        g.fillAll (juce::Colour (laneIndex % 2 == 0 ? OmniLookAndFeel::background
                                                    : OmniLookAndFeel::panel).withAlpha (0.6f));

        g.setColour (juce::Colour (OmniLookAndFeel::outline).withAlpha (0.5f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        // Bar grid lines.
        const auto& timeline = context.project.getTimeline();
        const auto secsPerBeat = 60.0 / juce::jmax (1.0, timeline.getTempoBpm());
        const auto secsPerBar = secsPerBeat * juce::jmax (1, timeline.getTimeSigNumerator());

        for (double t = 0.0; context.secondsToX (t) < getWidth(); t += secsPerBar)
        {
            const auto x = context.secondsToX (t);
            g.setColour (juce::Colour (OmniLookAndFeel::outline).withAlpha (0.4f));
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
        }

        // Clips.
        int audioClipIndex = 0;
        for (int i = 0; i < trackRef.getNumClips(); ++i)
        {
            auto* clip = trackRef.getClip (i);
            if (clip == nullptr)
                continue;

            const auto x = context.secondsToX (clip->startTime);
            auto length = clip->length;
            if (length <= 0.0)
                length = 2.0;
            const auto w = juce::jmax (4, context.secondsToX (length));

            auto clipBounds = juce::Rectangle<int> (x, 2, w, getHeight() - 6);

            // Clip body.
            g.setColour (trackRef.colour.withAlpha (0.30f));
            g.fillRoundedRectangle (clipBounds.toFloat(), 4.0f);
            g.setColour (trackRef.colour.withAlpha (0.9f));
            g.drawRoundedRectangle (clipBounds.toFloat().reduced (0.5f), 4.0f, 1.2f);

            // Title bar.
            auto titleBar = clipBounds.removeFromTop (14);
            g.setColour (trackRef.colour.withAlpha (0.55f));
            g.fillRect (titleBar);
            g.setColour (juce::Colour (OmniLookAndFeel::textPrimary));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (clip->name, titleBar.reduced (4, 0), juce::Justification::centredLeft, true);

            if (auto* midiClip = dynamic_cast<models::MidiClip*> (clip))
                drawMidiClip (g, *midiClip, clipBounds);
            else if (auto* audioClip = dynamic_cast<models::AudioClip*> (clip))
                drawAudioClip (g, *audioClip, audioClipIndex++, clipBounds);
        }

        if (trackRef.volumeAutomationEnabled)
            drawAutomation (g);
    }

    int TrackLaneComponent::valueToY (float value) const
    {
        const float norm = juce::jlimit (0.0f, 1.0f, value / maxAutoValue);
        return (int) ((1.0f - norm) * (getHeight() - 4)) + 2;
    }

    float TrackLaneComponent::yToValue (int y) const
    {
        const float norm = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (y - 2) / (float) juce::jmax (1, getHeight() - 4));
        return norm * maxAutoValue;
    }

    int TrackLaneComponent::findAutomationPoint (juce::Point<int> pos) const
    {
        const auto& curve = trackRef.volumeAutomation;
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& p = curve.getPoint (i);
            const juce::Point<int> pt (context.secondsToX (p.time), valueToY (p.value));
            if (pt.getDistanceFrom (pos) <= 7)
                return i;
        }
        return -1;
    }

    void TrackLaneComponent::drawAutomation (juce::Graphics& g)
    {
        const auto& curve = trackRef.volumeAutomation;

        // Dim the lane so the automation overlay reads clearly.
        g.setColour (juce::Colour (OmniLookAndFeel::background).withAlpha (0.35f));
        g.fillRect (getLocalBounds());

        juce::Path path;
        const auto accent = juce::Colour (OmniLookAndFeel::accentWarm);

        if (curve.getNumPoints() == 0)
            return;

        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& p = curve.getPoint (i);
            const float x = (float) context.secondsToX (p.time);
            const float y = (float) valueToY (p.value);
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        g.setColour (accent);
        g.strokePath (path, juce::PathStrokeType (1.6f));

        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& p = curve.getPoint (i);
            const float x = (float) context.secondsToX (p.time);
            const float y = (float) valueToY (p.value);
            g.setColour (i == draggingPoint ? juce::Colours::white : accent);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        }
    }

    void TrackLaneComponent::mouseDown (const juce::MouseEvent& e)
    {
        if (! trackRef.volumeAutomationEnabled)
            return;

        auto& curve = trackRef.volumeAutomation;
        const int hit = findAutomationPoint (e.getPosition());

        if (e.mods.isRightButtonDown())
        {
            if (hit >= 0)
            {
                curve.removePoint (hit);
                context.engine.rebuildAutomation();
                repaint();
            }
            return;
        }

        if (hit >= 0)
        {
            draggingPoint = hit;
        }
        else
        {
            draggingPoint = curve.addPoint (juce::jmax (0.0, context.xToSeconds (e.x)), yToValue (e.y));
            context.engine.rebuildAutomation();
        }
        repaint();
    }

    void TrackLaneComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (! trackRef.volumeAutomationEnabled || draggingPoint < 0)
            return;

        trackRef.volumeAutomation.setPointTimeValue (draggingPoint,
                                                     juce::jmax (0.0, context.xToSeconds (e.x)),
                                                     yToValue (e.y));
        context.engine.rebuildAutomation();
        repaint();
    }

    void TrackLaneComponent::mouseUp (const juce::MouseEvent&)
    {
        if (draggingPoint >= 0)
        {
            trackRef.volumeAutomation.sortPoints();
            context.engine.rebuildAutomation();
            draggingPoint = -1;
            repaint();
        }
    }

    void TrackLaneComponent::drawMidiClip (juce::Graphics& g, models::MidiClip& clip,
                                           juce::Rectangle<int> clipBounds)
    {
        auto& seq = clip.sequence;
        if (seq.getNumEvents() == 0 || clip.length <= 0.0)
            return;

        // Determine pitch range so notes fill the clip vertically.
        int lowNote = 127, highNote = 0;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto& m = seq.getEventPointer (i)->message;
            if (m.isNoteOn())
            {
                lowNote  = juce::jmin (lowNote, m.getNoteNumber());
                highNote = juce::jmax (highNote, m.getNoteNumber());
            }
        }
        if (highNote < lowNote) { lowNote = 48; highNote = 72; }
        const auto span = juce::jmax (1, highNote - lowNote);

        const auto pxPerSecInClip = (double) clipBounds.getWidth() / clip.length;

        g.setColour (juce::Colour (OmniLookAndFeel::accent));

        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* on = seq.getEventPointer (i);
            if (! on->message.isNoteOn())
                continue;

            const double startSec = on->message.getTimeStamp();
            double durSec = 0.25;
            if (on->noteOffObject != nullptr)
                durSec = on->noteOffObject->message.getTimeStamp() - startSec;

            const int nx = clipBounds.getX() + (int) (startSec * pxPerSecInClip);
            const int nw = juce::jmax (2, (int) (durSec * pxPerSecInClip));
            const float t = 1.0f - (float) (on->message.getNoteNumber() - lowNote) / (float) span;
            const int ny = clipBounds.getY() + (int) (t * (clipBounds.getHeight() - 4)) + 1;

            g.fillRoundedRectangle ((float) nx, (float) ny, (float) nw, 3.0f, 1.0f);
        }
    }

    void TrackLaneComponent::drawAudioClip (juce::Graphics& g, models::AudioClip& clip,
                                            int clipIndex, juce::Rectangle<int> clipBounds)
    {
        if (clipIndex < 0 || clipIndex >= thumbnails.size())
            return;

        auto* thumb = thumbnails[clipIndex];
        if (thumb == nullptr || thumb->getTotalLength() <= 0.0)
        {
            g.setColour (juce::Colour (OmniLookAndFeel::textDim));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (clip.sourceFile.getFileName(), clipBounds, juce::Justification::centred, true);
            return;
        }

        g.setColour (trackRef.colour.brighter (0.4f));
        const auto length = clip.length > 0.0 ? clip.length : thumb->getTotalLength();
        thumb->drawChannels (g, clipBounds, clip.sourceOffset, clip.sourceOffset + length, 0.9f);
    }

    void TrackLaneComponent::mouseDoubleClick (const juce::MouseEvent& e)
    {
        // In automation mode, clicks edit the curve rather than create clips.
        if (trackRef.volumeAutomationEnabled)
            return;

        const auto clickedSeconds = juce::jmax (0.0, context.xToSeconds (e.x));

        if (auto* midiTrack = dynamic_cast<models::MidiTrack*> (&trackRef))
        {
            // Snap to the bar and drop in a 2-bar starter pattern.
            const auto& timeline = context.project.getTimeline();
            const auto secsPerBar = (60.0 / juce::jmax (1.0, timeline.getTempoBpm()))
                                    * juce::jmax (1, timeline.getTimeSigNumerator());
            const auto barStart = std::floor (clickedSeconds / secsPerBar) * secsPerBar;

            auto* clip = midiTrack->addClip();
            clip->startTime = barStart;
            clip->length = secsPerBar * 2.0;
            clip->name = "Pattern";

            const auto beat = 60.0 / juce::jmax (1.0, timeline.getTempoBpm());
            const int scale[] = { 0, 2, 4, 5, 7, 9, 11, 12 };
            for (int i = 0; i < 8; ++i)
            {
                const double t = i * beat * 0.5;
                clip->sequence.addEvent (juce::MidiMessage::noteOn (1, 60 + scale[i], (juce::uint8) 100), t);
                clip->sequence.addEvent (juce::MidiMessage::noteOff (1, 60 + scale[i]), t + beat * 0.4);
            }
            clip->sequence.updateMatchedPairs();

            context.engine.rebuildSequences();
            refreshClips();
            if (onClipsChanged) onClipsChanged();
        }
        else if (auto* audioTrack = dynamic_cast<models::AudioTrack*> (&trackRef))
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Import audio file", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

            fileChooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, audioTrack, clickedSeconds] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    if (! file.existsAsFile())
                        return;

                    auto* clip = audioTrack->addClip();
                    clip->sourceFile = file;
                    clip->startTime = clickedSeconds;
                    clip->name = file.getFileNameWithoutExtension();

                    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                            context.engine.getFormatManager().createReaderFor (file)))
                    {
                        if (reader->sampleRate > 0)
                            clip->length = (double) reader->lengthInSamples / reader->sampleRate;
                    }

                    context.engine.rebuildSequences();
                    refreshClips();
                    if (onClipsChanged) onClipsChanged();
                });
        }
    }
} // namespace omnidaw::ui
