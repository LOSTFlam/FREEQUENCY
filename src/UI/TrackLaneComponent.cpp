#include "UI/TrackLaneComponent.h"
#include "UI/FreequencyLookAndFeel.h"
#include "Models/MidiTrack.h"
#include "Models/PatternExpander.h"

#include "Models/AudioTrack.h"
#include "Models/MidiTrack.h"

namespace freequency::ui
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

    void TrackLaneComponent::importAudioFile (const juce::File& file, double startSeconds)
    {
        auto* audioTrack = dynamic_cast<models::AudioTrack*> (&trackRef);
        if (audioTrack == nullptr || ! file.existsAsFile())
            return;

        if (context.pushUndo) context.pushUndo();
        auto* clip = audioTrack->addClip();
        clip->sourceFile = file;
        clip->takeFiles.add (file.getFullPathName());
        clip->activeTake = 0;
        clip->startTime = juce::jmax (0.0, startSeconds);
        clip->name = file.getFileNameWithoutExtension();

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                context.engine.getFormatManager().createReaderFor (file)))
            if (reader->sampleRate > 0)
                clip->length = (double) reader->lengthInSamples / reader->sampleRate;

        context.engine.rebuildSequences();
        refreshClips();
        if (onClipsChanged) onClipsChanged();
    }

    bool TrackLaneComponent::isInterestedInDragSource (const SourceDetails& d)
    {
        return dynamic_cast<models::AudioTrack*> (&trackRef) != nullptr
               && d.description.toString() == "media-file";
    }

    void TrackLaneComponent::itemDropped (const SourceDetails& d)
    {
        if (! context.getBrowserSelectedFile)
            return;
        const auto file = context.getBrowserSelectedFile();
        if (file.existsAsFile())
            importAudioFile (file, context.snapTime (context.xToSeconds (d.localPosition.x)));
    }

    juce::Rectangle<int> TrackLaneComponent::clipBoundsFor (models::Clip& clip) const
    {
        const auto x = context.secondsToX (clip.startTime);
        auto length = clip.length > 0.0 ? clip.length : 2.0;
        const auto w = juce::jmax (4, context.secondsToX (length));
        return { x, 2, w, getHeight() - 6 };
    }

    models::Clip* TrackLaneComponent::clipAtTime (double seconds) const
    {
        for (int i = 0; i < trackRef.getNumClips(); ++i)
        {
            auto* clip = trackRef.getClip (i);
            const double len = clip->length > 0.0 ? clip->length : 2.0;
            if (seconds >= clip->startTime && seconds < clip->startTime + len)
                return clip;
        }
        return nullptr;
    }

    TrackLaneComponent::ClipDragMode TrackLaneComponent::hitTestClip (models::Clip& clip,
                                                                      juce::Point<int> pos) const
    {
        const auto bounds = clipBoundsFor (clip);
        if (! bounds.contains (pos))
            return ClipDragMode::none;

        if (pos.x - bounds.getX() <= trimHandleW)
            return ClipDragMode::trimLeft;
        if (bounds.getRight() - pos.x <= trimHandleW)
            return ClipDragMode::trimRight;
        return ClipDragMode::move;
    }

    double TrackLaneComponent::audioSourceDuration (models::AudioClip& clip) const
    {
        for (int i = 0; i < trackRef.getNumClips(); ++i)
        {
            if (auto* ac = dynamic_cast<models::AudioClip*> (trackRef.getClip (i)))
            {
                if (ac == &clip)
                {
                    if (i < thumbnails.size() && thumbnails[i] != nullptr)
                    {
                        const auto len = thumbnails[i]->getTotalLength();
                        if (len > 0.0)
                            return len;
                    }
                    break;
                }
            }
        }

        if (clip.sourceFile.existsAsFile())
        {
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                    context.engine.getFormatManager().createReaderFor (clip.sourceFile)))
                if (reader->sampleRate > 0)
                    return (double) reader->lengthInSamples / reader->sampleRate;
        }

        return clip.length > 0.0 ? clip.length : 2.0;
    }

    void TrackLaneComponent::drawTrimHandles (juce::Graphics& g, juce::Rectangle<int> clipBounds,
                                              bool selected)
    {
        if (! selected)
            return;

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        const int h = clipBounds.getHeight();
        g.fillRect (clipBounds.getX(), clipBounds.getY(), 2, h);
        g.fillRect (clipBounds.getRight() - 2, clipBounds.getY(), 2, h);
    }

    void TrackLaneComponent::mouseMove (const juce::MouseEvent& e)
    {
        if (trackRef.volumeAutomationEnabled)
            return;

        const double t = context.xToSeconds (e.x);
        ClipDragMode mode = ClipDragMode::none;

        if (auto* clip = clipAtTime (t))
            mode = hitTestClip (*clip, e.getPosition());

        if (mode != hoverMode)
        {
            hoverMode = mode;
            switch (mode)
            {
                case ClipDragMode::trimLeft:
                case ClipDragMode::trimRight: setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
                case ClipDragMode::move:      setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
                default:                      setMouseCursor (juce::MouseCursor::NormalCursor); break;
            }
        }
    }

    void TrackLaneComponent::paint (juce::Graphics& g)
    {
        // Alternating lane background for readability.
        g.fillAll ((laneIndex % 2 == 0 ? theme().background : theme().panel).withAlpha (0.6f));

        g.setColour (theme().outline.withAlpha (0.5f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        // Bar grid lines.
        const auto& timeline = context.project.getTimeline();
        const auto secsPerBeat = 60.0 / juce::jmax (1.0, timeline.getTempoBpm());
        const auto secsPerBar = secsPerBeat * juce::jmax (1, timeline.getTimeSigNumerator());

        for (double t = 0.0; context.secondsToX (t) < getWidth(); t += secsPerBar)
        {
            const auto x = context.secondsToX (t);
            g.setColour (theme().outline.withAlpha (0.4f));
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

            // Clip body (selected clip is highlighted with a brighter, thicker border).
            const bool selected = (clip == context.selectedClip);
            g.setColour (trackRef.colour.withAlpha (selected ? 0.45f : 0.30f));
            g.fillRoundedRectangle (clipBounds.toFloat(), 4.0f);
            g.setColour (selected ? juce::Colours::white : trackRef.colour.withAlpha (0.9f));
            g.drawRoundedRectangle (clipBounds.toFloat().reduced (0.5f), 4.0f, selected ? 2.2f : 1.2f);

            drawTrimHandles (g, clipBounds, selected);

            // Title bar.
            auto titleBar = clipBounds.removeFromTop (14);
            g.setColour (trackRef.colour.withAlpha (0.55f));
            g.fillRect (titleBar);
            g.setColour (theme().textPrimary);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (clip->name, titleBar.reduced (4, 0), juce::Justification::centredLeft, true);

            if (auto* midiClip = dynamic_cast<models::MidiClip*> (clip))
                drawMidiClip (g, *midiClip, clipBounds);
            else if (auto* patClip = dynamic_cast<models::PatternClip*> (clip))
                drawPatternClip (g, *patClip, clipBounds);
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
        g.setColour (theme().background.withAlpha (0.35f));
        g.fillRect (getLocalBounds());

        juce::Path path;
        const auto accent = theme().accentWarm;

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
        {
            const double t = context.xToSeconds (e.x);
            models::Clip* hit = clipAtTime (t);

            context.selectedTrack = &trackRef;
            context.selectedClip = hit;

            dragClip = hit;
            dragOrigStart = hit != nullptr ? hit->startTime : 0.0;
            dragOrigLength = hit != nullptr && hit->length > 0.0 ? hit->length : 2.0;
            dragOrigSourceOffset = 0.0;
            if (auto* ac = dynamic_cast<models::AudioClip*> (hit))
                dragOrigSourceOffset = ac->sourceOffset;

            dragMode = hit != nullptr ? hitTestClip (*hit, e.getPosition()) : ClipDragMode::none;
            if (e.mods.isAltDown() && hit != nullptr)
                dragMode = ClipDragMode::slip;

            dragStartX = e.x;
            didDrag = false;

            if (context.repaintArrange) context.repaintArrange();
            return;
        }

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
        // Clip editing (arrange mode).
        if (! trackRef.volumeAutomationEnabled && dragClip != nullptr)
        {
            if (! didDrag)
            {
                if (context.pushUndo) context.pushUndo();
                didDrag = true;
            }

            const double deltaSec = context.xToSeconds (e.x) - context.xToSeconds (dragStartX);
            const double minLen = context.snapTime (0.0625); // ~1/16 at 120 BPM

            switch (dragMode)
            {
                case ClipDragMode::move:
                    dragClip->startTime = context.snapTime (juce::jmax (0.0, dragOrigStart + deltaSec));
                    break;

                case ClipDragMode::trimLeft:
                {
                    const double newStart = context.snapTime (juce::jmax (0.0, dragOrigStart + deltaSec));
                    const double trimDelta = newStart - dragOrigStart;
                    dragClip->startTime = newStart;
                    dragClip->length = juce::jmax (minLen, dragOrigLength - trimDelta);

                    if (auto* ac = dynamic_cast<models::AudioClip*> (dragClip))
                    {
                        const double maxOffset = juce::jmax (0.0, audioSourceDuration (*ac) - dragOrigLength);
                        ac->sourceOffset = juce::jlimit (0.0, maxOffset, dragOrigSourceOffset + trimDelta);
                    }
                    break;
                }

                case ClipDragMode::trimRight:
                    dragClip->length = context.snapTime (juce::jmax (minLen, dragOrigLength + deltaSec));
                    break;

                case ClipDragMode::slip:
                    if (auto* ac = dynamic_cast<models::AudioClip*> (dragClip))
                    {
                        const double maxOffset = juce::jmax (0.0, audioSourceDuration (*ac) - dragOrigLength);
                        ac->sourceOffset = juce::jlimit (0.0, maxOffset, dragOrigSourceOffset + deltaSec);
                    }
                    break;

                default: break;
            }

            repaint();
            return;
        }

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
        if (dragClip != nullptr)
        {
            if (didDrag)
                context.engine.rebuildSequences();
            dragClip = nullptr;
            dragMode = ClipDragMode::none;
            didDrag = false;
        }

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

        g.setColour (theme().accent);

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

    void TrackLaneComponent::drawPatternClip (juce::Graphics& g, models::PatternClip& clip,
                                              juce::Rectangle<int> clipBounds)
    {
        auto* pattern = context.project.findPattern (clip.patternId);
        if (pattern == nullptr || clip.length <= 0.0)
            return;

        const double tempo = context.project.getTimeline().getTempoBpm();
        const double clipLen = clip.length;

        // Step-grid backdrop (FL-style pattern block).
        const int stepCount = juce::jmax (1, pattern->getStepCount());
        const double secPerBeat = 60.0 / juce::jmax (1.0, tempo);
        const double patternSec = pattern->lengthInBeats * secPerBeat;
        const int loops = juce::jmax (1, (int) std::ceil (clipLen / patternSec));
        const int totalSteps = stepCount * loops;
        const float stepW = (float) clipBounds.getWidth() / (float) juce::jmax (1, totalSteps);

        g.setColour (theme().background.withAlpha (0.35f));
        g.fillRect (clipBounds);

        for (int s = 0; s < totalSteps; ++s)
        {
            const bool barStep = (s % pattern->stepsPerBar) == 0;
            const float x = (float) clipBounds.getX() + s * stepW;
            g.setColour (barStep ? theme().outline.withAlpha (0.55f)
                                 : theme().outline.withAlpha (0.2f));
            g.drawVerticalLine ((int) x, (float) clipBounds.getY(),
                                (float) clipBounds.getBottom());
        }

        // Mini note preview from expanded pattern.
        juce::Array<models::PatternExpander::PreviewNote> preview;
        models::PatternExpander::collectPreviewNotes (*pattern, preview, clipLen, tempo);

        int lowNote = 127, highNote = 0;
        for (const auto& n : preview)
        {
            lowNote = juce::jmin (lowNote, n.pitch);
            highNote = juce::jmax (highNote, n.pitch);
        }
        if (preview.isEmpty())
        {
            lowNote = 36; highNote = 48;
        }
        const auto span = juce::jmax (1, highNote - lowNote);
        const auto pxPerSecInClip = (double) clipBounds.getWidth() / clipLen;

        g.setColour (pattern->colour.withAlpha (0.85f));
        for (const auto& n : preview)
        {
            const double startSec = n.startBeats * secPerBeat;
            const int nx = clipBounds.getX() + (int) (startSec * pxPerSecInClip);
            const int nw = juce::jmax (2, (int) (n.lengthBeats * secPerBeat * pxPerSecInClip));
            const float t = 1.0f - (float) (n.pitch - lowNote) / (float) span;
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
            g.setColour (theme().textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (clip.sourceFile.getFileName(), clipBounds, juce::Justification::centred, true);
            return;
        }

        g.setColour (trackRef.colour.brighter (0.4f));
        const auto length = clip.length > 0.0 ? clip.length : thumb->getTotalLength();
        thumb->drawChannels (g, clipBounds, clip.sourceOffset, clip.sourceOffset + length, 0.9f);

        // Comping indicator (active take / total).
        if (clip.getNumTakes() > 1)
        {
            g.setColour (theme().accentWarm);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("T" + juce::String (clip.activeTake + 1) + "/" + juce::String (clip.getNumTakes()),
                        clipBounds.removeFromBottom (12).reduced (3, 0),
                        juce::Justification::bottomRight, false);
        }
    }

    void TrackLaneComponent::mouseDoubleClick (const juce::MouseEvent& e)
    {
        // In automation mode, clicks edit the curve rather than create clips.
        if (trackRef.volumeAutomationEnabled)
            return;

        // Double-clicking an existing MIDI clip opens the piano-roll editor.
        {
            const double t = context.xToSeconds (e.x);
            for (int i = 0; i < trackRef.getNumClips(); ++i)
            {
                auto* clip = trackRef.getClip (i);
                const double len = clip->length > 0.0 ? clip->length : 2.0;
                if (t >= clip->startTime && t < clip->startTime + len)
                {
                    if (auto* midiClip = dynamic_cast<models::MidiClip*> (clip))
                    {
                        if (context.openPianoRoll) context.openPianoRoll (*midiClip, trackRef);
                        return;
                    }
                }
            }
        }

        const auto clickedSeconds = context.snapTime (context.xToSeconds (e.x));

        if (auto* midiTrack = dynamic_cast<models::MidiTrack*> (&trackRef))
        {
            if (context.pushUndo) context.pushUndo();
            const auto& timeline = context.project.getTimeline();
            const auto secsPerBar = (60.0 / juce::jmax (1.0, timeline.getTempoBpm()))
                                    * juce::jmax (1, timeline.getTimeSigNumerator());

            auto* clip = midiTrack->addClip();
            clip->startTime = clickedSeconds;
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
                    if (file.existsAsFile())
                        importAudioFile (file, clickedSeconds);
                });
        }
    }
} // namespace freequency::ui
