#include "UI/PianoRollEditor.h"
#include "UI/FreequencyLookAndFeel.h"
#include "Models/MidiTrack.h"
#include "Models/PatternExpander.h"

#include <cmath>

namespace freequency::ui
{
    namespace
    {
        bool isBlackKey (int pitch) noexcept
        {
            switch (pitch % 12) { case 1: case 3: case 6: case 8: case 10: return true; default: return false; }
        }
    }

    PianoRollEditor::PianoRollEditor (UIContext& ctx, models::MidiClip& c, models::Track& t)
        : context (ctx), clip (c), track (t)
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Piano Roll — " + clip.name, juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, theme().accent);

        addAndMakeVisible (closeButton);
        closeButton.onClick = [this] { if (onClose) onClose(); };

        addAndMakeVisible (arpButton);
        arpButton.onClick = [this] { applyArpeggiator(); };

        addAndMakeVisible (slideButton);
        slideButton.setClickingTogglesState (true);
        slideButton.setColour (juce::TextButton::buttonOnColourId, theme().accentWarm);
        slideButton.onClick = [this] { slideMode = slideButton.getToggleState(); };

        addAndMakeVisible (quantButton);
        quantButton.onClick = [this] { quantizeSelected(); };

        addAndMakeVisible (ghostButton);
        ghostButton.setClickingTogglesState (true);
        ghostButton.setToggleState (true, juce::dontSendNotification);
        ghostButton.setColour (juce::TextButton::buttonOnColourId, theme().accent.withAlpha (0.55f));
        ghostButton.onClick = [this]
        {
            ghostConfig.showOtherTracks = ghostButton.getToggleState();
            ghostConfig.showPatternClips = ghostButton.getToggleState();
            loadGhostNotes();
            gridComp.repaint();
            velLane.repaint();
        };

        addAndMakeVisible (snapBox);
        snapBox.addItemList ({ "1/4", "1/8", "1/16", "1/32", "Off" }, 1);
        snapBox.setSelectedId (3, juce::dontSendNotification); // 1/16
        snapBox.onChange = [this]
        {
            switch (snapBox.getSelectedId())
            {
                case 1: snapGrid = 1.0; break;
                case 2: snapGrid = 0.5; break;
                case 3: snapGrid = 0.25; break;
                case 4: snapGrid = 0.125; break;
                default: snapGrid = 0.0; break;
            }
        };

        keysViewport.setViewedComponent (&keysComp, false);
        keysViewport.setScrollBarsShown (false, false);
        addAndMakeVisible (keysViewport);

        gridViewport.setViewedComponent (&gridComp, false);
        gridViewport.setScrollBarsShown (true, true);
        gridViewport.onScroll = [this]
        {
            const auto pos = gridViewport.getViewPosition();
            keysViewport.setViewPosition (0, pos.y);
            velLane.setViewOffsetX (pos.x);
        };
        addAndMakeVisible (gridViewport);

        addAndMakeVisible (velLane);

        loadFromClip();
        loadGhostNotes();
        startTimerHz (30);
        setWantsKeyboardFocus (true);

        // Scroll so a useful range (around middle C) is visible initially.
        juce::MessageManager::callAsync ([this]
        {
            gridViewport.setViewPosition (0, juce::jmax (0, pitchToY (84)));
        });
    }

    PianoRollEditor::~PianoRollEditor() { stopTimer(); }

    double PianoRollEditor::clipLengthBeats() const
    {
        const double tempo = context.project.getTimeline().getTempoBpm();
        double beats = clip.length * tempo / 60.0;
        for (const auto& n : notes)
            beats = juce::jmax (beats, n.startBeats + n.lengthBeats);
        return juce::jmax (16.0, beats);
    }

    double PianoRollEditor::snapBeats (double beats) const
    {
        if (snapGrid <= 0.0) return juce::jmax (0.0, beats);
        return juce::jmax (0.0, std::round (beats / snapGrid) * snapGrid);
    }

    void PianoRollEditor::loadFromClip()
    {
        notes.clear();
        const double beatsPerSec = context.project.getTimeline().getTempoBpm() / 60.0;

        for (int i = 0; i < clip.sequence.getNumEvents(); ++i)
        {
            auto* on = clip.sequence.getEventPointer (i);
            if (! on->message.isNoteOn())
                continue;

            Note n;
            n.startBeats = on->message.getTimeStamp() * beatsPerSec;
            n.pitch = on->message.getNoteNumber();
            n.velocity = on->message.getVelocity();
            n.lengthBeats = on->noteOffObject != nullptr
                                ? (on->noteOffObject->message.getTimeStamp() - on->message.getTimeStamp()) * beatsPerSec
                                : snapGrid > 0 ? snapGrid : 0.5;

            for (const auto& slide : clip.portamentoSlides)
            {
                if (std::abs (slide.endBeat - n.startBeats) < 0.02 && slide.toPitch == n.pitch)
                {
                    n.slide = true;
                    break;
                }
            }

            notes.push_back (n);
        }
    }

    void PianoRollEditor::loadGhostNotes()
    {
        ghostNotes.clear();

        const bool anySource = ghostConfig.showOtherTracks || ghostConfig.showPatternClips
                               || ghostConfig.showSameTrackClips;
        if (! anySource)
            return;

        const double tempo = context.project.getTimeline().getTempoBpm();
        const double bps = tempo / 60.0;
        const double clipStart = clip.startTime;
        const double clipEnd = clipStart + juce::jmax (clip.length, 0.25);

        auto addFromSequence = [&] (const juce::MidiMessageSequence& seq, double sourceStart,
                                    juce::Colour col)
        {
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* on = seq.getEventPointer (i);
                if (! on->message.isNoteOn())
                    continue;

                const double absSec = sourceStart + on->message.getTimeStamp();
                if (absSec < clipStart - 0.001 || absSec >= clipEnd)
                    continue;

                GhostNote gn;
                gn.startBeats = (absSec - clipStart) * bps;
                gn.pitch = on->message.getNoteNumber();
                gn.colour = col;

                if (on->noteOffObject != nullptr)
                    gn.lengthBeats = (on->noteOffObject->message.getTimeStamp()
                                      - on->message.getTimeStamp()) * bps;
                else
                    gn.lengthBeats = 0.25;

                ghostNotes.push_back (gn);
            }
        };

        const auto& timeline = context.project.getTimeline();
        for (int ti = 0; ti < timeline.getNumTracks(); ++ti)
        {
            auto* tr = timeline.getTrack (ti);
            auto* mt = dynamic_cast<models::MidiTrack*> (tr);
            if (mt == nullptr)
                continue;

            const bool sameTrack = (tr == &track);

            for (int ci = 0; ci < mt->getNumClips(); ++ci)
            {
                auto* c = mt->getClip (ci);
                if (c == nullptr || c == &clip)
                    continue;

                const double cStart = c->startTime;
                const double cLen = c->length > 0.0 ? c->length : 4.0;
                if (cStart + cLen <= clipStart || cStart >= clipEnd)
                    continue;

                if (auto* midi = dynamic_cast<models::MidiClip*> (c))
                {
                    if (sameTrack && ! ghostConfig.showSameTrackClips)
                        continue;
                    if (! sameTrack && ! ghostConfig.showOtherTracks)
                        continue;

                    addFromSequence (midi->sequence, cStart, tr->colour);
                }
                else if (auto* patClip = dynamic_cast<models::PatternClip*> (c))
                {
                    if (! ghostConfig.showPatternClips)
                        continue;

                    auto* pattern = context.project.findPattern (patClip->patternId);
                    if (pattern == nullptr)
                        continue;

                    juce::Array<models::PatternExpander::PreviewNote> preview;
                    models::PatternExpander::collectPreviewNotes (*pattern, preview, cLen, tempo);

                    for (const auto& pn : preview)
                    {
                        const double absSec = cStart + pn.startBeats / bps;
                        if (absSec < clipStart - 0.001 || absSec >= clipEnd)
                            continue;

                        GhostNote gn;
                        gn.startBeats = (absSec - clipStart) * bps;
                        gn.pitch = pn.pitch;
                        gn.lengthBeats = pn.lengthBeats;
                        gn.colour = pattern->colour;
                        ghostNotes.push_back (gn);
                    }
                }
            }
        }
    }

    void PianoRollEditor::writeBackToClip()
    {
        const double secPerBeat = 60.0 / context.project.getTimeline().getTempoBpm();

        clip.sequence.clear();
        clip.portamentoSlides.clear();
        double maxEndSec = 0.0;

        auto sorted = notes;
        std::sort (sorted.begin(), sorted.end(),
                   [] (const Note& a, const Note& b) { return a.startBeats < b.startBeats; });

        for (size_t i = 0; i < sorted.size(); ++i)
        {
            const auto& n = sorted[i];
            const double startSec = n.startBeats * secPerBeat;
            const double endSec = (n.startBeats + n.lengthBeats) * secPerBeat;
            maxEndSec = juce::jmax (maxEndSec, endSec);

            if (n.slide && i > 0)
            {
                models::PortamentoSlide slide;
                slide.fromPitch = sorted[i - 1].pitch;
                slide.toPitch = n.pitch;
                slide.endBeat = n.startBeats;
                slide.startBeat = juce::jmax (0.0, n.startBeats - juce::jmin (0.5, n.lengthBeats * 0.6));
                slide.curve = 0.55f;
                clip.portamentoSlides.push_back (slide);
            }

            clip.sequence.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) n.velocity), startSec);
            clip.sequence.addEvent (juce::MidiMessage::noteOff (1, n.pitch), endSec);
        }

        clip.sequence.updateMatchedPairs();
        if (maxEndSec > clip.length)
            clip.length = maxEndSec;

        context.engine.rebuildSequences();
        gridComp.repaint();
        velLane.repaint();
        if (context.repaintArrange) context.repaintArrange();
    }

    void PianoRollEditor::applyArpeggiator()
    {
        // Collect the distinct pitches currently in the clip, then lay them out as
        // an ascending arpeggio at the snap resolution across the clip length.
        juce::SortedSet<int> pitches;
        for (const auto& n : notes)
            pitches.add (n.pitch);
        if (pitches.isEmpty())
            return;

        if (context.pushUndo) context.pushUndo();

        const double step = snapGrid > 0 ? snapGrid : 0.25;
        const double totalBeats = clipLengthBeats();

        notes.clear();
        int idx = 0;
        for (double b = 0.0; b + step <= totalBeats + 1.0e-6; b += step)
        {
            Note n;
            n.startBeats = b;
            n.lengthBeats = step * 0.9;
            n.pitch = pitches[idx % pitches.size()];
            n.velocity = 100;
            notes.push_back (n);
            ++idx;
        }

        writeBackToClip();
    }

    void PianoRollEditor::quantizeSelected()
    {
        if (snapGrid <= 0.0)
            return;

        bool any = false;
        for (auto& n : notes)
            if (n.selected) { any = true; break; }
        if (! any)
            return;

        if (context.pushUndo) context.pushUndo();

        for (auto& n : notes)
        {
            if (! n.selected)
                continue;
            n.startBeats = snapBeats (n.startBeats);
            n.lengthBeats = juce::jmax (snapGrid, std::round (n.lengthBeats / snapGrid) * snapGrid);
        }

        writeBackToClip();
    }

    void PianoRollEditor::copySelected()
    {
        clipboard.clear();
        for (const auto& n : notes)
            if (n.selected)
                clipboard.push_back (n);
    }

    void PianoRollEditor::pasteNotes()
    {
        if (clipboard.empty())
            return;

        if (context.pushUndo) context.pushUndo();

        const double pasteOffset = snapGrid > 0 ? snapGrid : 0.25;
        for (auto& n : notes)
            n.selected = false;

        for (const auto& src : clipboard)
        {
            Note n = src;
            n.startBeats += pasteOffset;
            n.selected = true;
            notes.push_back (n);
        }

        writeBackToClip();
    }

    void PianoRollEditor::selectAll()
    {
        for (auto& n : notes)
            n.selected = true;
        gridComp.repaint();
        velLane.repaint();
    }

    bool PianoRollEditor::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)
            || key == juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0))
        {
            copySelected();
            return true;
        }

        if (key == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0)
            || key == juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0))
        {
            pasteNotes();
            return true;
        }

        if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)
            || key == juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0))
        {
            selectAll();
            return true;
        }

        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            deleteSelected();
            return true;
        }

        return false;
    }

    void PianoRollEditor::deleteSelected()
    {
        if (context.pushUndo) context.pushUndo();
        notes.erase (std::remove_if (notes.begin(), notes.end(),
                                     [] (const Note& n) { return n.selected; }),
                     notes.end());
        writeBackToClip();
    }

    void PianoRollEditor::timerCallback()
    {
        gridComp.repaint();
    }

    // ── Layout ───────────────────────────────────────────────────────────────────
    void PianoRollEditor::paint (juce::Graphics& g)
    {
        g.fillAll (theme().background);
        g.setColour (theme().panel);
        g.fillRect (0, 0, getWidth(), toolbarH);
    }

    void PianoRollEditor::resized()
    {
        auto r = getLocalBounds();

        auto toolbar = r.removeFromTop (toolbarH).reduced (8, 6);
        closeButton.setBounds (toolbar.removeFromLeft (64));
        toolbar.removeFromLeft (10);
        titleLabel.setBounds (toolbar.removeFromLeft (260));
        toolbar.removeFromLeft (10);
        snapBox.setBounds (toolbar.removeFromLeft (70));
        toolbar.removeFromLeft (8);
        arpButton.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (6);
        slideButton.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (6);
        quantButton.setBounds (toolbar.removeFromLeft (60));
        toolbar.removeFromLeft (6);
        ghostButton.setBounds (toolbar.removeFromLeft (64));

        auto velArea = r.removeFromBottom (velLaneH);
        velArea.removeFromLeft (gutterW);
        velLane.setBounds (velArea);

        keysViewport.setBounds (r.removeFromLeft (gutterW));
        gridViewport.setBounds (r);

        const int contentW = juce::jmax (getWidth(), beatsToX (clipLengthBeats()));
        const int contentH = 128 * noteHeight;
        gridComp.setSize (contentW, contentH);
        keysComp.setSize (gutterW, contentH);

        if (gridViewport.onScroll) gridViewport.onScroll();
    }

    // ── Keys gutter ──────────────────────────────────────────────────────────────
    void PianoRollEditor::Keys::paint (juce::Graphics& g)
    {
        g.fillAll (theme().panelLight);
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const int y = owner.pitchToY (pitch);
            g.setColour (isBlackKey (pitch) ? juce::Colour (0xff15171c) : juce::Colour (0xff2a2e37));
            g.fillRect (0, y, getWidth(), owner.noteHeight - 1);
            if (pitch % 12 == 0) // C
            {
                g.setColour (theme().textDim);
                g.setFont (juce::FontOptions (9.0f));
                g.drawText ("C" + juce::String (pitch / 12 - 1), 2, y, getWidth() - 4, owner.noteHeight,
                            juce::Justification::centredLeft, false);
            }
        }
    }

    // ── Grid ─────────────────────────────────────────────────────────────────────
    int PianoRollEditor::Grid::noteAt (juce::Point<int> p) const
    {
        for (int i = (int) owner.notes.size(); --i >= 0;)
        {
            const auto& n = owner.notes[(size_t) i];
            const int x = owner.beatsToX (n.startBeats);
            const int w = juce::jmax (3, owner.beatsToX (n.lengthBeats));
            const int y = owner.pitchToY (n.pitch);
            if (juce::Rectangle<int> (x, y, w, owner.noteHeight).contains (p))
                return i;
        }
        return -1;
    }

    bool PianoRollEditor::Grid::overRightEdge (int noteIndex, juce::Point<int> p) const
    {
        const auto& n = owner.notes[(size_t) noteIndex];
        const int xEnd = owner.beatsToX (n.startBeats + n.lengthBeats);
        return std::abs (p.x - xEnd) <= 5;
    }

    void PianoRollEditor::Grid::paint (juce::Graphics& g)
    {
        g.fillAll (theme().background);

        // Row striping for black/white keys.
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            if (isBlackKey (pitch))
            {
                g.setColour (juce::Colour (0xff1b1e25));
                g.fillRect (0, owner.pitchToY (pitch), getWidth(), owner.noteHeight);
            }
            g.setColour (theme().outline.withAlpha (0.25f));
            g.drawHorizontalLine (owner.pitchToY (pitch), 0.0f, (float) getWidth());
        }

        // Beat / bar grid lines.
        const int beatsPerBar = juce::jmax (1, owner.context.project.getTimeline().getTimeSigNumerator());
        const int totalBeats = (int) std::ceil (owner.clipLengthBeats());
        for (int b = 0; b <= totalBeats; ++b)
        {
            const int x = owner.beatsToX ((double) b);
            const bool bar = (b % beatsPerBar) == 0;
            g.setColour (theme().outline.withAlpha (bar ? 0.8f : 0.35f));
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
        }

        // Ghost notes (read-only context from other clips / patterns).
        for (const auto& gn : owner.ghostNotes)
        {
            const int x = owner.beatsToX (gn.startBeats);
            const int w = juce::jmax (3, owner.beatsToX (gn.lengthBeats));
            const int y = owner.pitchToY (gn.pitch);
            auto bounds = juce::Rectangle<int> (x, y, w, owner.noteHeight - 1).toFloat();

            g.setColour (gn.colour.withAlpha (owner.ghostConfig.opacity));
            g.fillRoundedRectangle (bounds, 2.0f);
            g.setColour (gn.colour.withAlpha (owner.ghostConfig.opacity * 0.6f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 0.8f);
        }

        // Portamento slide curves.
        for (const auto& slide : owner.clip.portamentoSlides)
        {
            juce::Path path;
            const int steps = 16;
            for (int s = 0; s <= steps; ++s)
            {
                const double t = (double) s / (double) steps;
                const double curved = t * (1.0 - (double) slide.curve) + t * t * (double) slide.curve;
                const double beat = slide.startBeat + (slide.endBeat - slide.startBeat) * t;
                const int pitch = (int) std::llround ((double) slide.fromPitch
                                                      + ((double) slide.toPitch - (double) slide.fromPitch) * curved);
                const float x = (float) owner.beatsToX (beat);
                const float y = (float) owner.pitchToY (pitch) + owner.noteHeight * 0.5f;
                if (s == 0) path.startNewSubPath (x, y);
                else        path.lineTo (x, y);
            }
            g.setColour (theme().accentWarm.withAlpha (0.85f));
            g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // Notes.
        for (const auto& n : owner.notes)
        {
            const int x = owner.beatsToX (n.startBeats);
            const int w = juce::jmax (3, owner.beatsToX (n.lengthBeats));
            const int y = owner.pitchToY (n.pitch);
            auto bounds = juce::Rectangle<int> (x, y, w, owner.noteHeight - 1).toFloat();

            auto col = owner.track.colour;
            g.setColour (n.slide ? col.withRotatedHue (0.1f) : col);
            g.fillRoundedRectangle (bounds, 2.0f);
            g.setColour (n.selected ? juce::Colours::white : col.darker (0.5f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, n.selected ? 1.6f : 0.8f);
        }

        // Playhead (relative to clip start).
        const double tempo = owner.context.project.getTimeline().getTempoBpm();
        const double clipStartBeats = owner.clip.startTime * tempo / 60.0;
        const double playBeats = owner.context.engine.getTransport().getPositionSeconds() * tempo / 60.0 - clipStartBeats;
        if (playBeats >= 0.0)
        {
            const int px = owner.beatsToX (playBeats);
            g.setColour (theme().accent);
            g.drawVerticalLine (px, 0.0f, (float) getHeight());
        }
    }

    void PianoRollEditor::Grid::mouseDown (const juce::MouseEvent& e)
    {
        owner.grabKeyboardFocus();
        const int idx = noteAt (e.getPosition());

        if (e.mods.isRightButtonDown())
        {
            if (idx >= 0)
            {
                if (owner.context.pushUndo) owner.context.pushUndo();
                owner.notes.erase (owner.notes.begin() + idx);
                owner.writeBackToClip();
            }
            return;
        }

        if (idx < 0)
        {
            if (owner.context.pushUndo) owner.context.pushUndo();
            if (! e.mods.isShiftDown())
                for (auto& n : owner.notes) n.selected = false;

            Note n;
            n.startBeats = owner.snapBeats (owner.xToBeats (e.x));
            n.pitch = owner.yToPitch (e.y);
            n.lengthBeats = owner.snapGrid > 0 ? owner.snapGrid : 0.5;
            n.velocity = 100;
            n.slide = owner.slideMode;
            n.selected = true;
            owner.notes.push_back (n);
            dragIndex = (int) owner.notes.size() - 1;
            mode = Mode::move;
        }
        else
        {
            dragIndex = idx;
            mode = overRightEdge (idx, e.getPosition()) ? Mode::resize : Mode::move;

            if (! e.mods.isShiftDown())
                for (auto& n : owner.notes) n.selected = false;

            owner.notes[(size_t) idx].selected = e.mods.isShiftDown()
                                                      ? ! owner.notes[(size_t) idx].selected
                                                      : true;

            if (owner.context.pushUndo) owner.context.pushUndo();
        }

        dragStart = e.getPosition();
        if (dragIndex >= 0)
        {
            origStartBeats = owner.notes[(size_t) dragIndex].startBeats;
            origLenBeats = owner.notes[(size_t) dragIndex].lengthBeats;
            origPitch = owner.notes[(size_t) dragIndex].pitch;
        }
        repaint();
    }

    void PianoRollEditor::Grid::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragIndex < 0) return;
        auto& n = owner.notes[(size_t) dragIndex];

        if (mode == Mode::resize)
        {
            const double endBeats = owner.snapBeats (owner.xToBeats (e.x));
            n.lengthBeats = juce::jmax (owner.snapGrid > 0 ? owner.snapGrid : 0.0625, endBeats - n.startBeats);
        }
        else if (mode == Mode::move)
        {
            const double deltaBeats = owner.xToBeats (e.x) - owner.xToBeats (dragStart.x);
            n.startBeats = owner.snapBeats (origStartBeats + deltaBeats);
            n.pitch = owner.yToPitch (e.y);
        }
        repaint();
    }

    void PianoRollEditor::Grid::mouseUp (const juce::MouseEvent&)
    {
        if (dragIndex >= 0)
            owner.writeBackToClip();
        dragIndex = -1;
        mode = Mode::none;
    }

    void PianoRollEditor::Grid::mouseDoubleClick (const juce::MouseEvent& e)
    {
        // Double-click a note to delete it.
        const int idx = noteAt (e.getPosition());
        if (idx >= 0)
        {
            if (owner.context.pushUndo) owner.context.pushUndo();
            owner.notes.erase (owner.notes.begin() + idx);
            owner.writeBackToClip();
        }
    }

    // ── Velocity lane ────────────────────────────────────────────────────────────
    void PianoRollEditor::VelLane::paint (juce::Graphics& g)
    {
        g.fillAll (theme().panel);
        g.setColour (theme().outline);
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());

        for (const auto& gn : owner.ghostNotes)
        {
            const int x = owner.beatsToX (gn.startBeats) - viewOffsetX;
            if (x < -4 || x > getWidth()) continue;
            const int h = (int) (owner.ghostConfig.opacity * (getHeight() - 6));
            g.setColour (gn.colour.withAlpha (owner.ghostConfig.opacity * 0.5f));
            g.fillRect (x, getHeight() - h - 2, 3, h);
        }

        for (const auto& n : owner.notes)
        {
            const int x = owner.beatsToX (n.startBeats) - viewOffsetX;
            if (x < -4 || x > getWidth()) continue;
            const int h = (int) ((float) n.velocity / 127.0f * (getHeight() - 6));
            g.setColour (owner.track.colour.withAlpha (n.selected ? 1.0f : 0.7f));
            g.fillRect (x, getHeight() - h - 2, 4, h);
        }
    }

    void PianoRollEditor::VelLane::setVel (const juce::MouseEvent& e)
    {
        const double beats = owner.xToBeats (e.x + viewOffsetX);
        // Find the nearest note by start position.
        int best = -1; double bestDist = 0.4; // within ~half a beat
        for (int i = 0; i < (int) owner.notes.size(); ++i)
        {
            const double d = std::abs (owner.notes[(size_t) i].startBeats - beats);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best < 0) return;

        const float v = 1.0f - (float) e.y / (float) juce::jmax (1, getHeight());
        owner.notes[(size_t) best].velocity = juce::jlimit (1, 127, (int) std::round (v * 127.0f));
        owner.writeBackToClip();
    }
} // namespace freequency::ui
