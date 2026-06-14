#include "UI/FrequencyFieldEditor.h"
#include "Models/MidiTrack.h"
#include "Models/PatternExpander.h"

namespace freequency::ui
{
    namespace
    {
        bool isBlackKey (int pitch) noexcept
        {
            switch (pitch % 12) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; }
        }

        juce::String elasticLabel (models::ElasticMode m)
        {
            switch (m)
            {
                case models::ElasticMode::none:            return "Elastic: off";
                case models::ElasticMode::realtimePreview: return "Elastic: RT preview";
                case models::ElasticMode::offlineOLA:
                default:                                   return "Elastic: offline OLA";
            }
        }
    }

    FrequencyFieldEditor::FrequencyFieldEditor (UIContext& ctx, models::AudioClip& c, models::Track& t)
        : context (ctx), clip (c), track (t), canvas (*this)
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Frequency Field — " + clip.name, juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, theme().accentWarm);

        addAndMakeVisible (closeButton);
        closeButton.onClick = [this] { if (onClose) onClose(); };

        addAndMakeVisible (ghostButton);
        ghostButton.setToggleState (true, juce::dontSendNotification);
        ghostButton.onClick = [this] { loadGhostNotes(); canvas.repaint(); };

        addAndMakeVisible (elasticBox);
        elasticBox.addItemList ({ "Offline OLA", "RT preview (soon)", "Off" }, 1);
        switch (clip.elasticMode)
        {
            case models::ElasticMode::none:            elasticBox.setSelectedId (3, juce::dontSendNotification); break;
            case models::ElasticMode::realtimePreview: elasticBox.setSelectedId (2, juce::dontSendNotification); break;
            case models::ElasticMode::offlineOLA:
            default:                                   elasticBox.setSelectedId (1, juce::dontSendNotification); break;
        }
        elasticBox.onChange = [this]
        {
            if (context.pushUndo) context.pushUndo();
            switch (elasticBox.getSelectedId())
            {
                case 2: clip.elasticMode = models::ElasticMode::realtimePreview; break;
                case 3: clip.elasticMode = models::ElasticMode::none; break;
                default: clip.elasticMode = models::ElasticMode::offlineOLA; break;
            }
            context.engine.rebuildSequences();
        };

        addAndMakeVisible (hintLabel);
        hintLabel.setText ("Click grid to add VariAudio anchor · Esc = close", juce::dontSendNotification);
        hintLabel.setFont (juce::FontOptions (11.0f));
        hintLabel.setColour (juce::Label::textColourId, theme().textDim);

        addAndMakeVisible (canvas);
        loadGhostNotes();
        startTimerHz (30);
        setWantsKeyboardFocus (true);
    }

    FrequencyFieldEditor::~FrequencyFieldEditor() { stopTimer(); }

    double FrequencyFieldEditor::clipLengthSec() const
    {
        return juce::jmax (0.25, clip.length > 0.0 ? clip.length : 2.0);
    }

    int FrequencyFieldEditor::pitchToY (int pitch) const
    {
        return (pitchMax - pitch) * pitchRowH;
    }

    int FrequencyFieldEditor::timeToX (double relSec) const
    {
        const double len = clipLengthSec();
        return (int) std::llround ((relSec / len) * (double) canvas.getWidth());
    }

    int FrequencyFieldEditor::pitchAtY (int y) const
    {
        const int p = pitchMax - y / pitchRowH;
        return juce::jlimit (pitchMin, pitchMax, p);
    }

    double FrequencyFieldEditor::timeAtX (int x) const
    {
        const double len = clipLengthSec();
        return juce::jlimit (0.0, len, (double) x / (double) juce::jmax (1, canvas.getWidth()) * len);
    }

    void FrequencyFieldEditor::loadGhostNotes()
    {
        ghostNotes.clear();
        if (! ghostButton.getToggleState())
            return;

        const double clipStart = clip.startTime;
        const double clipEnd = clipStart + clipLengthSec();
        const auto& timeline = context.project.getTimeline();

        auto addFromSequence = [&] (const juce::MidiMessageSequence& seq, double sourceStart,
                                    juce::Colour col)
        {
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* on = seq.getEventPointer (i);
                if (! on->message.isNoteOn()) continue;

                const double absSec = sourceStart + on->message.getTimeStamp();
                if (absSec < clipStart - 0.001 || absSec >= clipEnd) continue;

                GhostNote gn;
                gn.relSeconds = absSec - clipStart;
                gn.pitch = on->message.getNoteNumber();
                gn.colour = col.withAlpha (ghostConfig.opacity);

                if (on->noteOffObject != nullptr)
                    gn.lengthSec = on->noteOffObject->message.getTimeStamp() - on->message.getTimeStamp();
                else
                    gn.lengthSec = 0.12;

                ghostNotes.push_back (gn);
            }
        };

        for (int ti = 0; ti < timeline.getNumTracks(); ++ti)
        {
            auto* tr = timeline.getTrack (ti);
            if (tr == &track) continue;

            for (int ci = 0; ci < tr->getNumClips(); ++ci)
            {
                if (auto* mc = dynamic_cast<models::MidiClip*> (tr->getClip (ci)))
                    addFromSequence (mc->sequence, mc->startTime, mc->colour);
                else if (auto* patClip = dynamic_cast<models::PatternClip*> (tr->getClip (ci)))
                {
                    auto* pattern = context.project.findPattern (patClip->patternId);
                    if (pattern == nullptr) continue;

                    const double cLen = patClip->length > 0.0 ? patClip->length : 4.0;
                    juce::Array<models::PatternExpander::PreviewNote> preview;
                    models::PatternExpander::collectPreviewNotes (*pattern, preview, cLen,
                                                                timeline.getTempoBpm());
                    const double bps = timeline.getTempoBpm() / 60.0;

                    for (const auto& pn : preview)
                    {
                        const double absSec = patClip->startTime + pn.startBeats / bps;
                        if (absSec < clipStart - 0.001 || absSec >= clipEnd) continue;

                        GhostNote gn;
                        gn.relSeconds = absSec - clipStart;
                        gn.pitch = pn.pitch;
                        gn.lengthSec = pn.lengthBeats / bps;
                        gn.colour = patClip->colour.withAlpha (ghostConfig.opacity);
                        ghostNotes.push_back (gn);
                    }
                }
            }
        }
    }

    void FrequencyFieldEditor::timerCallback()
    {
        const double pos = context.engine.getTransport().getPositionSeconds();
        const int x = timeToX (pos - clip.startTime);
        if (x != playheadX)
        {
            playheadX = x;
            canvas.repaint();
        }
    }

    void FrequencyFieldEditor::resized()
    {
        auto r = getLocalBounds();
        auto bar = r.removeFromTop (40).reduced (10, 6);
        titleLabel.setBounds (bar.removeFromLeft (320));
        closeButton.setBounds (bar.removeFromRight (72));
        bar.removeFromRight (8);
        elasticBox.setBounds (bar.removeFromRight (150).reduced (0, 4));
        bar.removeFromRight (8);
        ghostButton.setBounds (bar.removeFromRight (120));
        hintLabel.setBounds (r.removeFromBottom (22).reduced (12, 0));
        canvas.setBounds (r.reduced (8));
    }

    bool FrequencyFieldEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        return false;
    }

    void FrequencyFieldEditor::paint (juce::Graphics& g)
    {
        g.fillAll (theme().background);
        g.setColour (theme().outline);
        g.drawHorizontalLine (40, 0.0f, (float) getWidth());

        g.setColour (theme().textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (elasticLabel (clip.elasticMode),
                    getLocalBounds().removeFromTop (40).removeFromRight (320).reduced (10, 0),
                    juce::Justification::centredRight, true);
    }

    void FrequencyFieldEditor::FieldCanvas::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isRightButtonDown())
            return;

        models::VariAudioSegment seg;
        seg.time = owner.timeAtX (e.x);
        seg.pitchCents = (owner.pitchAtY (e.y) - 60) * 100;
        if (owner.context.pushUndo) owner.context.pushUndo();
        owner.clip.variAudioSegments.push_back (seg);
        owner.context.engine.rebuildSequences();
        repaint();
    }

    void FrequencyFieldEditor::FieldCanvas::paint (juce::Graphics& g)
    {
        const auto& th = theme();
        const int w = getWidth();
        const int h = (pitchMax - pitchMin + 1) * pitchRowH;

        g.fillAll (th.background.darker (0.15f));

        // Pitch rows
        for (int p = pitchMin; p <= pitchMax; ++p)
        {
            const int y = owner.pitchToY (p);
            g.setColour (isBlackKey (p) ? th.panel.darker (0.2f) : th.panelLight.withAlpha (0.35f));
            g.fillRect (0, y, w, pitchRowH);
            if (p % 12 == 0)
            {
                g.setColour (th.textDim.withAlpha (0.5f));
                g.setFont (juce::FontOptions (9.0f));
                g.drawText (juce::MidiMessage::getMidiNoteName (p, true, true, 3),
                            juce::Rectangle<int> (2, y, 36, pitchRowH),
                            juce::Justification::centredLeft, false);
            }
        }

        // Comp swipe regions
        for (const auto& region : owner.clip.compSwipeRegions)
        {
            const int x0 = owner.timeToX (region.startTime);
            const int x1 = owner.timeToX (region.startTime + region.length);
            g.setColour (th.accentWarm.withAlpha (0.12f));
            g.fillRect (x0, 0, x1 - x0, h);
            const int sx = owner.timeToX (region.startTime + region.length * region.crossfadePosition);
            g.setColour (th.accentWarm);
            g.drawVerticalLine (sx, 0.0f, (float) h);
        }

        // Ghost notes
        for (const auto& gn : owner.ghostNotes)
        {
            const int x = owner.timeToX (gn.relSeconds);
            const int y = owner.pitchToY (gn.pitch);
            const int nw = juce::jmax (4, owner.timeToX (gn.relSeconds + gn.lengthSec) - x);
            g.setColour (gn.colour);
            g.fillRoundedRectangle ((float) x, (float) y + 1, (float) nw, (float) pitchRowH - 2, 2.0f);
        }

        // VariAudio anchors
        for (const auto& seg : owner.clip.variAudioSegments)
        {
            const int x = owner.timeToX (seg.time);
            const int y = owner.pitchToY (60 + seg.pitchCents / 100);
            g.setColour (th.accent);
            g.fillEllipse ((float) x - 5.0f, (float) y + 1.0f, 10.0f, (float) pitchRowH - 2.0f);
            g.setColour (th.accent.withAlpha (0.35f));
            g.drawEllipse ((float) x - 10.0f, (float) y - 4.0f, 20.0f, (float) pitchRowH + 6.0f, 1.5f);
        }

        // Spectral frequency contour placeholder (centre line)
        juce::Path contour;
        const double len = owner.clipLengthSec();
        for (int xi = 0; xi < w; xi += 3)
        {
            const double t = (double) xi / (double) w * len;
            const float y = (float) owner.pitchToY (62)
                            + std::sin ((float) t * 8.0f) * 18.0f
                            + std::sin ((float) t * 19.0f) * 6.0f;
            if (xi == 0) contour.startNewSubPath ((float) xi, y);
            else         contour.lineTo ((float) xi, y);
        }
        g.setColour (th.accent.withAlpha (0.55f));
        g.strokePath (contour, juce::PathStrokeType (2.0f));

        // Playhead
        if (owner.playheadX >= 0 && owner.playheadX <= w)
        {
            g.setColour (th.accent);
            g.drawVerticalLine (owner.playheadX, 0.0f, (float) h);
        }

        g.setColour (th.outline);
        g.drawRect (getLocalBounds());
    }
} // namespace freequency::ui
