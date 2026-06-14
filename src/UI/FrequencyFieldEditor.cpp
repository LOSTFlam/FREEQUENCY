#include "UI/FrequencyFieldEditor.h"
#include "Engine/VariAudioResynth.h"
#include "Engine/WarpMapper.h"
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

        void paintSpectralBackdrop (juce::Graphics& g, juce::Rectangle<int> area, const Theme& th)
        {
            juce::ColourGradient grad (th.accent.withAlpha (0.18f), (float) area.getX(), (float) area.getY(),
                                       th.accentWarm.withAlpha (0.10f), (float) area.getRight(), (float) area.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillRect (area);

            g.setColour (th.accent.withAlpha (0.04f));
            for (int i = 0; i < 5; ++i)
            {
                const float y = (float) area.getY() + (float) area.getHeight() * (0.15f + 0.17f * (float) i);
                g.drawHorizontalLine ((int) y, (float) area.getX(), (float) area.getRight());
            }
        }
    }

    FrequencyFieldEditor::FrequencyFieldEditor (UIContext& ctx, models::AudioClip& c, models::Track& t)
        : context (ctx), clip (c), track (t), canvas (*this), warpStrip (*this)
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Frequency Field — " + clip.name, juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, theme().accentWarm);

        addAndMakeVisible (closeButton);
        closeButton.onClick = [this] { if (onClose) onClose(); };

        addAndMakeVisible (ghostButton);
        ghostButton.setToggleState (true, juce::dontSendNotification);
        ghostButton.setColour (juce::TextButton::buttonOnColourId, theme().accent.withAlpha (0.55f));
        ghostButton.onClick = [this] { loadGhostNotes(); canvas.repaint(); };

        addAndMakeVisible (elasticBox);
        elasticBox.addItemList ({ "Offline OLA", "RT preview", "Off" }, 1);
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
            statusLabel.setText ("Elastic mode updated", juce::dontSendNotification);
        };

        addAndMakeVisible (bakeButton);
        bakeButton.setColour (juce::TextButton::buttonColourId, theme().accent.withAlpha (0.35f));
        bakeButton.onClick = [this] { bakeToSnapshot(); };

        addAndMakeVisible (warpModeButton);
        warpModeButton.setColour (juce::TextButton::buttonOnColourId, theme().accentWarm.withAlpha (0.6f));

        addAndMakeVisible (formantLabel);
        formantLabel.setText ("Formant", juce::dontSendNotification);
        formantLabel.setFont (juce::FontOptions (10.0f));
        formantLabel.setColour (juce::Label::textColourId, theme().textDim);

        addAndMakeVisible (formantSlider);
        formantSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        formantSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        formantSlider.setRange (-6.0, 6.0, 0.1);
        formantSlider.setValue (0.0, juce::dontSendNotification);
        formantSlider.onValueChange = [this]
        {
            if (dragAnchorIndex < 0 || dragAnchorIndex >= (int) clip.variAudioSegments.size())
                return;
            if (context.pushUndo) context.pushUndo();
            clip.variAudioSegments[(size_t) dragAnchorIndex].formantShift = (float) formantSlider.getValue();
            context.engine.rebuildSequences();
        };

        addAndMakeVisible (statusLabel);
        statusLabel.setFont (juce::FontOptions (11.0f));
        statusLabel.setColour (juce::Label::textColourId, theme().accent);

        addAndMakeVisible (hintLabel);
        hintLabel.setText ("Anchor: click/drag · Warp: toggle + strip · Formant on selected anchor",
                           juce::dontSendNotification);
        hintLabel.setFont (juce::FontOptions (11.0f));
        hintLabel.setColour (juce::Label::textColourId, theme().textDim);

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&canvas, false);
        viewport.setScrollBarsShown (true, false);

        addAndMakeVisible (warpStrip);

        loadPreviewAudio();
        ensureWarpEndpoints();
        loadGhostNotes();
        refreshContour();
        syncFormantSlider();
        startTimerHz (30);
        setWantsKeyboardFocus (true);
    }

    FrequencyFieldEditor::~FrequencyFieldEditor() { stopTimer(); }

    void FrequencyFieldEditor::loadPreviewAudio()
    {
        previewBuffer.setSize (0, 0);
        previewSampleRate = 44100.0;

        if (! clip.sourceFile.existsAsFile())
            return;

        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (clip.sourceFile));
        if (reader == nullptr)
            return;

        previewSampleRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        const int len = (int) reader->lengthInSamples;
        const int ch = (int) juce::jmax ((juce::uint32) 1, reader->numChannels);
        previewBuffer.setSize (ch, len);
        reader->read (&previewBuffer, 0, len, 0, true, true);
    }

    void FrequencyFieldEditor::ensureWarpEndpoints()
    {
        engine::WarpMapper::ensureEndpoints (clip.warpMarkers, clipLengthSec(), sourceLengthSec());
    }

    double FrequencyFieldEditor::sourceLengthSec() const
    {
        if (previewBuffer.getNumSamples() > 0 && previewSampleRate > 0.0)
            return (double) previewBuffer.getNumSamples() / previewSampleRate;
        return clipLengthSec() * clip.stretchRatio;
    }

    int FrequencyFieldEditor::timeToXInStrip (double relSec, int stripWidth) const
    {
        const double len = clipLengthSec();
        return (int) std::llround ((relSec / len) * (double) stripWidth);
    }

    double FrequencyFieldEditor::timeAtXInStrip (int x, int stripWidth) const
    {
        const double len = clipLengthSec();
        return juce::jlimit (0.0, len, (double) x / (double) juce::jmax (1, stripWidth) * len);
    }

    int FrequencyFieldEditor::findWarpAt (int x) const
    {
        const int w = warpStrip.getWidth();
        for (int i = 0; i < (int) clip.warpMarkers.size(); ++i)
        {
            const int wx = timeToXInStrip (clip.warpMarkers[(size_t) i].timelineTime, w);
            if (std::abs (x - wx) <= 7)
                return i;
        }
        return -1;
    }

    void FrequencyFieldEditor::removeWarp (int index)
    {
        if (index < 0 || index >= (int) clip.warpMarkers.size())
            return;
        if (context.pushUndo) context.pushUndo();
        clip.warpMarkers.erase (clip.warpMarkers.begin() + index);
        ensureWarpEndpoints();
        context.engine.rebuildSequences();
        warpStrip.repaint();
        statusLabel.setText ("Warp marker removed", juce::dontSendNotification);
    }

    void FrequencyFieldEditor::syncFormantSlider()
    {
        const bool show = dragAnchorIndex >= 0 && dragAnchorIndex < (int) clip.variAudioSegments.size();
        formantSlider.setVisible (show);
        formantLabel.setVisible (show);
        if (show)
            formantSlider.setValue (clip.variAudioSegments[(size_t) dragAnchorIndex].formantShift,
                                    juce::dontSendNotification);
    }

    void FrequencyFieldEditor::refreshContour()
    {
        contourTimes.clear();
        contourCents.clear();

        if (previewBuffer.getNumSamples() <= 0)
            return;

        engine::VariAudioResynth::buildDetectedContour (previewBuffer,
                                                         previewSampleRate,
                                                         clipLengthSec(),
                                                         juce::jmax (32, canvas.getWidth() / 4),
                                                         contourTimes,
                                                         contourCents);
    }

    void FrequencyFieldEditor::bakeToSnapshot()
    {
        context.engine.rebuildSequences();
        bakePulse = true;
        bakeButton.setColour (juce::TextButton::buttonColourId, theme().accent.withAlpha (0.75f));
        statusLabel.setText ("Baked VariAudio + elastic into snapshot", juce::dontSendNotification);
        canvas.repaint();
    }

    double FrequencyFieldEditor::clipLengthSec() const
    {
        return juce::jmax (0.25, clip.length > 0.0 ? clip.length : 2.0);
    }

    double FrequencyFieldEditor::targetCentsAtY (int y) const
    {
        return (double) (pitchAtY (y) - 60) * 100.0;
    }

    int FrequencyFieldEditor::pitchToY (int pitch) const
    {
        return (pitchMax - pitch) * pitchRowH;
    }

    int FrequencyFieldEditor::centsToY (double cents) const
    {
        return pitchToY (60) - (int) std::llround (cents / 100.0) * pitchRowH;
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

    int FrequencyFieldEditor::findAnchorAt (int x, int y) const
    {
        for (int i = 0; i < (int) clip.variAudioSegments.size(); ++i)
        {
            const auto& seg = clip.variAudioSegments[(size_t) i];
            const int ax = timeToX (seg.time);
            const double detected = engine::VariAudioResynth::estimatePitchCentsAt (previewBuffer,
                                                                                      previewSampleRate,
                                                                                      seg.time);
            const int ay = centsToY (detected + (double) seg.pitchCents);
            if (std::abs (x - ax) <= 8 && std::abs (y - ay) <= pitchRowH)
                return i;
        }
        return -1;
    }

    void FrequencyFieldEditor::removeAnchor (int index)
    {
        if (index < 0 || index >= (int) clip.variAudioSegments.size())
            return;

        if (context.pushUndo) context.pushUndo();
        clip.variAudioSegments.erase (clip.variAudioSegments.begin() + index);
        context.engine.rebuildSequences();
        statusLabel.setText ("Anchor removed", juce::dontSendNotification);
        canvas.repaint();
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

        if (bakePulse)
        {
            bakePulse = false;
            bakeButton.setColour (juce::TextButton::buttonColourId, theme().accent.withAlpha (0.35f));
        }
    }

    void FrequencyFieldEditor::resized()
    {
        auto r = getLocalBounds();
        auto bar = r.removeFromTop (40).reduced (10, 6);
        titleLabel.setBounds (bar.removeFromLeft (280));
        closeButton.setBounds (bar.removeFromRight (72));
        bar.removeFromRight (6);
        bakeButton.setBounds (bar.removeFromRight (64).reduced (0, 4));
        bar.removeFromRight (6);
        formantSlider.setBounds (bar.removeFromRight (150).reduced (0, 8));
        formantLabel.setBounds (bar.removeFromRight (52));
        warpModeButton.setBounds (bar.removeFromRight (58).reduced (0, 4));
        bar.removeFromRight (6);
        elasticBox.setBounds (bar.removeFromRight (120).reduced (0, 4));
        bar.removeFromRight (8);
        ghostButton.setBounds (bar.removeFromRight (120));

        auto footer = r.removeFromBottom (22).reduced (12, 0);
        hintLabel.setBounds (footer.removeFromLeft (getWidth() * 2 / 3));
        statusLabel.setBounds (footer);

        warpStrip.setBounds (r.removeFromBottom (warpStripH).reduced (8, 0));
        viewport.setBounds (r.reduced (8));

        const int canvasH = (pitchMax - pitchMin + 1) * pitchRowH;
        const int canvasW = juce::jmax (viewport.getWidth(), (int) (clipLengthSec() * 120.0));
        canvas.setSize (canvasW, canvasH);
        refreshContour();
    }

    bool FrequencyFieldEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }

        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (dragWarpIndex >= 0)
            {
                removeWarp (dragWarpIndex);
                dragWarpIndex = -1;
                return true;
            }
            if (dragAnchorIndex >= 0)
            {
                removeAnchor (dragAnchorIndex);
                dragAnchorIndex = -1;
                syncFormantSlider();
                return true;
            }
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
                    getLocalBounds().removeFromTop (40).removeFromRight (300).reduced (10, 0),
                    juce::Justification::centredRight, true);
    }

    void FrequencyFieldEditor::FieldCanvas::mouseDown (const juce::MouseEvent& e)
    {
        if (owner.warpModeButton.getToggleState())
            return;

        if (e.mods.isRightButtonDown())
        {
            const int idx = owner.findAnchorAt (e.x, e.y);
            if (idx >= 0)
                owner.removeAnchor (idx);
            return;
        }

        owner.dragAnchorIndex = owner.findAnchorAt (e.x, e.y);
        if (owner.dragAnchorIndex >= 0)
            return;

        models::VariAudioSegment seg;
        seg.time = owner.timeAtX (e.x);
        const double detected = engine::VariAudioResynth::estimatePitchCentsAt (owner.previewBuffer,
                                                                                owner.previewSampleRate,
                                                                                seg.time);
        const double target = owner.targetCentsAtY (e.y);
        seg.pitchCents = (int) std::llround (target - detected);

        if (owner.context.pushUndo) owner.context.pushUndo();
        owner.clip.variAudioSegments.push_back (seg);
        owner.dragAnchorIndex = (int) owner.clip.variAudioSegments.size() - 1;
        owner.syncFormantSlider();
        owner.context.engine.rebuildSequences();
        owner.statusLabel.setText ("Anchor added — drag to refine", juce::dontSendNotification);
        repaint();
    }

    void FrequencyFieldEditor::FieldCanvas::mouseDrag (const juce::MouseEvent& e)
    {
        if (owner.dragAnchorIndex < 0
            || owner.dragAnchorIndex >= (int) owner.clip.variAudioSegments.size())
            return;

        auto& seg = owner.clip.variAudioSegments[(size_t) owner.dragAnchorIndex];
        seg.time = owner.timeAtX (e.x);
        const double detected = engine::VariAudioResynth::estimatePitchCentsAt (owner.previewBuffer,
                                                                                owner.previewSampleRate,
                                                                                seg.time);
        seg.pitchCents = (int) std::llround (owner.targetCentsAtY (e.y) - detected);
        owner.context.engine.rebuildSequences();
        repaint();
    }

    void FrequencyFieldEditor::FieldCanvas::mouseUp (const juce::MouseEvent&)
    {
        if (owner.dragAnchorIndex >= 0)
            owner.statusLabel.setText ("Pitch correction updated", juce::dontSendNotification);
        owner.syncFormantSlider();
        owner.dragAnchorIndex = -1;
    }

    void FrequencyFieldEditor::WarpStrip::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isRightButtonDown())
        {
            const int idx = owner.findWarpAt (e.x);
            if (idx >= 0)
                owner.removeWarp (idx);
            return;
        }

        owner.dragWarpIndex = owner.findWarpAt (e.x);
        if (owner.dragWarpIndex >= 0)
            return;

        if (owner.context.pushUndo) owner.context.pushUndo();

        models::WarpMarker wm;
        wm.timelineTime = owner.timeAtXInStrip (e.x, getWidth());
        wm.sourceTime = engine::WarpMapper::timelineToSource (wm.timelineTime,
                                                              owner.clip.warpMarkers,
                                                              owner.clipLengthSec(),
                                                              owner.sourceLengthSec(),
                                                              owner.clip.stretchRatio);
        owner.clip.warpMarkers.push_back (wm);
        owner.dragWarpIndex = (int) owner.clip.warpMarkers.size() - 1;
        owner.context.engine.rebuildSequences();
        owner.statusLabel.setText ("Warp marker added", juce::dontSendNotification);
        repaint();
    }

    void FrequencyFieldEditor::WarpStrip::mouseDrag (const juce::MouseEvent& e)
    {
        if (owner.dragWarpIndex < 0 || owner.dragWarpIndex >= (int) owner.clip.warpMarkers.size())
            return;

        auto& wm = owner.clip.warpMarkers[(size_t) owner.dragWarpIndex];
        wm.timelineTime = owner.timeAtXInStrip (e.x, getWidth());
        const double yNorm = 1.0 - juce::jlimit (0.0, 1.0, (double) e.y / (double) juce::jmax (1, getHeight()));
        wm.sourceTime = yNorm * owner.sourceLengthSec();
        owner.context.engine.rebuildSequences();
        repaint();
    }

    void FrequencyFieldEditor::WarpStrip::mouseUp (const juce::MouseEvent&)
    {
        owner.ensureWarpEndpoints();
        owner.dragWarpIndex = -1;
    }

    void FrequencyFieldEditor::WarpStrip::paint (juce::Graphics& g)
    {
        const auto& th = theme();
        const int w = getWidth();
        const int h = getHeight();

        juce::ColourGradient grad (th.panel.darker (0.1f), 0.0f, 0.0f,
                                   th.panelLight.withAlpha (0.35f), (float) w, (float) h, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

        juce::Path mapPath;
        const int steps = juce::jmax (16, w / 6);
        for (int i = 0; i <= steps; ++i)
        {
            const double t = owner.clipLengthSec() * (double) i / (double) steps;
            const double src = engine::WarpMapper::timelineToSource (t,
                                                                     owner.clip.warpMarkers,
                                                                     owner.clipLengthSec(),
                                                                     owner.sourceLengthSec(),
                                                                     owner.clip.stretchRatio);
            const float x = (float) owner.timeToXInStrip (t, w);
            const float y = (float) h - (float) (src / owner.sourceLengthSec()) * (float) (h - 8) - 4.0f;
            if (i == 0) mapPath.startNewSubPath (x, y);
            else        mapPath.lineTo (x, y);
        }
        g.setColour (th.accent.withAlpha (0.75f));
        g.strokePath (mapPath, juce::PathStrokeType (2.0f));

        for (int i = 0; i < (int) owner.clip.warpMarkers.size(); ++i)
        {
            const auto& wm = owner.clip.warpMarkers[(size_t) i];
            const float x = (float) owner.timeToXInStrip (wm.timelineTime, w);
            const float y = (float) h - (float) (wm.sourceTime / owner.sourceLengthSec()) * (float) (h - 8) - 4.0f;
            const bool sel = i == owner.dragWarpIndex;
            g.setColour (sel ? th.accentWarm : th.accent);
            g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
            g.drawVerticalLine ((int) x, 0.0f, (float) h);
        }

        g.setColour (th.outline.withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (th.textDim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("Warp map — drag marker vertically for source time", getLocalBounds().reduced (8, 0),
                    juce::Justification::centredLeft, false);
    }

    void FrequencyFieldEditor::FieldCanvas::paint (juce::Graphics& g)
    {
        const auto& th = theme();
        const int w = getWidth();
        const int h = (pitchMax - pitchMin + 1) * pitchRowH;

        paintSpectralBackdrop (g, getLocalBounds(), th);

        for (int p = pitchMin; p <= pitchMax; ++p)
        {
            const int y = owner.pitchToY (p);
            g.setColour (isBlackKey (p) ? th.panel.darker (0.25f).withAlpha (0.85f)
                                        : th.panelLight.withAlpha (0.30f));
            g.fillRect (0, y, w, pitchRowH);
            if (p % 12 == 0)
            {
                g.setColour (th.textDim.withAlpha (0.55f));
                g.setFont (juce::FontOptions (9.0f));
                g.drawText (juce::MidiMessage::getMidiNoteName (p, true, true, 3),
                            juce::Rectangle<int> (2, y, 36, pitchRowH),
                            juce::Justification::centredLeft, false);
            }
        }

        for (const auto& region : owner.clip.compSwipeRegions)
        {
            const int x0 = owner.timeToX (region.startTime);
            const int x1 = owner.timeToX (region.startTime + region.length);
            g.setColour (th.accentWarm.withAlpha (0.14f));
            g.fillRect (x0, 0, x1 - x0, h);
            const int sx = owner.timeToX (region.startTime + region.length * region.crossfadePosition);
            g.setColour (th.accentWarm.withAlpha (0.85f));
            g.drawVerticalLine (sx, 0.0f, (float) h);
        }

        for (const auto& gn : owner.ghostNotes)
        {
            const int x = owner.timeToX (gn.relSeconds);
            const int y = owner.pitchToY (gn.pitch);
            const int nw = juce::jmax (4, owner.timeToX (gn.relSeconds + gn.lengthSec) - x);
            g.setColour (gn.colour);
            g.fillRoundedRectangle ((float) x, (float) y + 1, (float) nw, (float) pitchRowH - 2, 2.0f);
        }

        // Detected pitch contour
        if (owner.contourTimes.size() == owner.contourCents.size() && owner.contourTimes.size() > 1)
        {
            juce::Path detected;
            for (size_t i = 0; i < owner.contourTimes.size(); ++i)
            {
                const float x = (float) owner.timeToX (owner.contourTimes[i]);
                const float y = (float) owner.centsToY (owner.contourCents[i]);
                if (i == 0) detected.startNewSubPath (x, y);
                else        detected.lineTo (x, y);
            }
            g.setColour (th.accentWarm.withAlpha (0.70f));
            g.strokePath (detected, juce::PathStrokeType (1.8f));
        }

        // Target correction curve
        if (! owner.clip.variAudioSegments.empty())
        {
            juce::Path corrected;
            const int steps = juce::jmax (32, w / 3);
            const double len = owner.clipLengthSec();
            for (int i = 0; i <= steps; ++i)
            {
                const double t = len * (double) i / (double) steps;
                const double corr = engine::VariAudioResynth::correctionCentsAt (owner.clip.variAudioSegments,
                                                                                 t, len);
                double base = 0.0;
                if (! owner.contourCents.empty())
                {
                    const double norm = t / len;
                    const size_t idx = (size_t) juce::jlimit (0, (int) owner.contourTimes.size() - 1,
                                                              (int) std::llround (norm * (owner.contourTimes.size() - 1)));
                    base = owner.contourCents[idx];
                }
                const float x = (float) owner.timeToX (t);
                const float y = (float) owner.centsToY (base + corr);
                if (i == 0) corrected.startNewSubPath (x, y);
                else        corrected.lineTo (x, y);
            }
            g.setColour (th.accent.withAlpha (0.85f));
            g.strokePath (corrected, juce::PathStrokeType (2.2f));
        }

        // VariAudio anchors
        for (int i = 0; i < (int) owner.clip.variAudioSegments.size(); ++i)
        {
            const auto& seg = owner.clip.variAudioSegments[(size_t) i];
            const int x = owner.timeToX (seg.time);
            const double detected = engine::VariAudioResynth::estimatePitchCentsAt (owner.previewBuffer,
                                                                                    owner.previewSampleRate,
                                                                                    seg.time);
            const int y = owner.centsToY (detected + (double) seg.pitchCents);
            const bool selected = i == owner.dragAnchorIndex;

            g.setColour (th.accent.withAlpha (selected ? 0.55f : 0.28f));
            g.fillEllipse ((float) x - 12.0f, (float) y - 6.0f, 24.0f, (float) pitchRowH + 10.0f);
            g.setColour (selected ? th.accent.brighter (0.35f) : th.accent);
            g.fillEllipse ((float) x - 5.0f, (float) y + 1.0f, 10.0f, (float) pitchRowH - 2.0f);
        }

        if (owner.playheadX >= 0 && owner.playheadX <= w)
        {
            g.setColour (th.accent.withAlpha (0.9f));
            g.drawVerticalLine (owner.playheadX, 0.0f, (float) h);
            g.setColour (th.accent.withAlpha (0.25f));
            g.fillRect (owner.playheadX - 1, 0, 3, h);
        }

        g.setColour (th.outline.withAlpha (0.6f));
        g.drawRect (getLocalBounds());
    }
} // namespace freequency::ui
