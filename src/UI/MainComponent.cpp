#include "UI/MainComponent.h"

#include "Models/MidiTrack.h"
#include "Models/AudioTrack.h"
#include "Models/ProjectSerializer.h"

namespace freequency::ui
{
    // Command identifiers for the remappable hotkey system.
    namespace CommandIDs
    {
        enum
        {
            playStop = 0x2001, recordToggle, returnToStart, toggleLoop, toggleMetronome,
            toggleMixerView, addAudioTrack, addMidiTrack, saveProject, openProject,
            openKeyMappings, duplicateClip, splitClip, deleteClip, reverseClip, nudgeLeft, nudgeRight,
            playheadLeft, playheadRight, tempoUp, tempoDown, toggleKeyboardPiano,
            octaveUp, octaveDown, undo, redo, openAudioSettings
        };
    }

    namespace
    {
        // Drop a simple musical pattern into a MIDI clip so the project is
        // audible/visible the moment the app opens.
        void fillPattern (models::MidiClip& clip, double tempoBpm, int bars,
                          const int* notes, int numNotes)
        {
            const double beat = 60.0 / juce::jmax (1.0, tempoBpm);
            clip.length = beat * 4.0 * bars;

            for (int bar = 0; bar < bars; ++bar)
            {
                for (int i = 0; i < numNotes; ++i)
                {
                    const double t = (bar * 4.0 + i * (4.0 / numNotes)) * beat;
                    clip.sequence.addEvent (juce::MidiMessage::noteOn (1, notes[i], (juce::uint8) 96), t);
                    clip.sequence.addEvent (juce::MidiMessage::noteOff (1, notes[i]), t + beat * 0.45);
                }
            }

            clip.sequence.updateMatchedPairs();
        }
    } // namespace

    MainComponent::MainComponent()
    {
        juce::Desktop::getInstance().setDefaultLookAndFeel (&lookAndFeel);

        buildDemoProject();

        // Open / close insert-effect editor windows. Windows are closed before any
        // graph rebuild because the live insert processors they wrap get recreated.
        context.openInsertEditor = [this] (models::Track& track, int slot)
        {
            if (auto* proc = audioEngine.getInsertProcessor (track, slot))
            {
                auto* w = new PluginEditorWindow (*proc, track.name + " — " + proc->getName());
                w->onClose = [this] (PluginEditorWindow* win) { pluginWindows.removeObject (win); };
                pluginWindows.add (w);
            }
        };
        context.closePluginWindows = [this] { pluginWindows.clear(); };
        context.pushUndo = [this] { pushUndo(); };
        context.openPianoRoll = [this] (models::MidiClip& mc, models::Track& tr)
        {
            pianoRoll = std::make_unique<PianoRollEditor> (context, mc, tr);
            pianoRoll->onClose = [this] { pianoRoll = nullptr; resized(); };
            addAndMakeVisible (*pianoRoll);
            pianoRoll->setBounds (getLocalBounds().withTrimmedTop (56));
        };

        audioEngine.setProject (&project);
        audioEngine.onRecordingFinished = [this] { if (arrangeView) arrangeView->rebuildTracks(); };
        const auto error = audioEngine.initialise();
        engineStatus = error.isEmpty() ? "Engine running" : ("Engine idle: " + error);

        transportBar = std::make_unique<TransportBar> (context);
        transportBar->onProjectStructureChanged = [this]
        {
            pluginWindows.clear(); // their insert processors are about to be recreated
            if (arrangeView) arrangeView->rebuildTracks();
            if (mixerView)   mixerView->rebuild();
        };
        transportBar->onToggleMixer = [this] { toggleMixer(); };
        transportBar->onOpenSettings = [this] { openKeyMappings(); };
        transportBar->onOpenAudioSettings = [this] { openAudioSettings(); };
        addAndMakeVisible (*transportBar);

        arrangeView = std::make_unique<ArrangeView> (context);
        addAndMakeVisible (*arrangeView);

        mixerView = std::make_unique<MixerView> (context);
        addChildComponent (*mixerView); // hidden until toggled

        statusBar = std::make_unique<StatusBar> (context);
        addAndMakeVisible (*statusBar);

        // ── Remappable hotkey system ────────────────────────────────────────────
        commandManager.registerAllCommandsForTarget (this);
        if (auto xml = juce::XmlDocument::parse (keyMappingsFile()))
            commandManager.getKeyMappings()->restoreFromXml (*xml);
        addKeyListener (commandManager.getKeyMappings());

        // Receive keyboard shortcuts + computer-keyboard piano.
        setWantsKeyboardFocus (true);

        setSize (1460, 820);
    }

    MainComponent::~MainComponent()
    {
        saveKeyMappings();
        keyMappingWindow = nullptr;
        audioSettingsWindow = nullptr;
        audioEngine.shutdown();
        juce::Desktop::getInstance().setDefaultLookAndFeel (nullptr);
    }

    void MainComponent::buildDemoProject()
    {
        project.name = "Demo Session";
        auto& timeline = project.getTimeline();
        timeline.setTempoBpm (120.0);

        auto* lead = timeline.addMidiTrack();
        lead->name = "Lead";
        {
            auto* clip = lead->addClip();
            clip->name = "Melody";
            clip->startTime = 0.0;
            const int notes[] = { 72, 76, 79, 76, 74, 77, 81, 79 };
            fillPattern (*clip, 120.0, 2, notes, 8);
        }

        auto* bass = timeline.addMidiTrack();
        bass->name = "Bass";
        {
            auto* clip = bass->addClip();
            clip->name = "Bassline";
            clip->startTime = 0.0;
            const int notes[] = { 36, 36, 43, 36 };
            fillPattern (*clip, 120.0, 2, notes, 4);
        }

        auto* audio = timeline.addAudioTrack();
        audio->name = "Audio";
    }

    bool MainComponent::openProjectFile (const juce::File& file)
    {
        if (! models::ProjectSerializer::loadFromFile (project, file))
            return false;

        audioEngine.rebuildGraph();
        if (arrangeView) arrangeView->rebuildTracks();
        if (mixerView)   mixerView->rebuild();
        return true;
    }

    void MainComponent::toggleMixer()
    {
        mixerVisible = ! mixerVisible;

        if (mixerVisible && mixerView != nullptr)
            mixerView->rebuild(); // reflect any track/routing changes

        if (arrangeView != nullptr) arrangeView->setVisible (! mixerVisible);
        if (mixerView != nullptr)   mixerView->setVisible (mixerVisible);
    }

    // ── ApplicationCommandTarget ────────────────────────────────────────────────

    void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
    {
        c.addArray ({
            CommandIDs::playStop, CommandIDs::recordToggle, CommandIDs::returnToStart,
            CommandIDs::toggleLoop, CommandIDs::toggleMetronome, CommandIDs::toggleMixerView,
            CommandIDs::addAudioTrack, CommandIDs::addMidiTrack, CommandIDs::saveProject,
            CommandIDs::openProject, CommandIDs::openKeyMappings, CommandIDs::duplicateClip,
            CommandIDs::splitClip, CommandIDs::deleteClip, CommandIDs::reverseClip,
            CommandIDs::nudgeLeft, CommandIDs::nudgeRight, CommandIDs::playheadLeft,
            CommandIDs::playheadRight, CommandIDs::tempoUp, CommandIDs::tempoDown,
            CommandIDs::toggleKeyboardPiano, CommandIDs::octaveUp, CommandIDs::octaveDown,
            CommandIDs::undo, CommandIDs::redo, CommandIDs::openAudioSettings });
    }

    void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& r)
    {
        using namespace juce;
        const String tx ("Transport"), ed ("Edit"), vw ("View"), in ("Input");

        switch (id)
        {
            case CommandIDs::playStop:        r.setInfo ("Play / Stop", "Toggle playback", tx, 0); r.addDefaultKeypress (KeyPress::spaceKey, 0); break;
            case CommandIDs::recordToggle:    r.setInfo ("Record", "Toggle recording", tx, 0); r.addDefaultKeypress ('r', 0); break;
            case CommandIDs::returnToStart:   r.setInfo ("Return to Start", "Move playhead to 0", tx, 0); r.addDefaultKeypress (KeyPress::homeKey, 0); break;
            case CommandIDs::toggleLoop:      r.setInfo ("Toggle Loop", "Enable/disable looping", tx, 0); r.addDefaultKeypress ('l', 0); break;
            case CommandIDs::toggleMetronome: r.setInfo ("Toggle Metronome", "Enable/disable click", tx, 0); r.addDefaultKeypress ('m', 0); break;
            case CommandIDs::playheadLeft:    r.setInfo ("Playhead Back 1 Bar", "", tx, 0); r.addDefaultKeypress (KeyPress::leftKey, 0); break;
            case CommandIDs::playheadRight:   r.setInfo ("Playhead Forward 1 Bar", "", tx, 0); r.addDefaultKeypress (KeyPress::rightKey, 0); break;
            case CommandIDs::tempoUp:         r.setInfo ("Tempo +1", "", tx, 0); r.addDefaultKeypress ('=', ModifierKeys::shiftModifier); break;
            case CommandIDs::tempoDown:       r.setInfo ("Tempo -1", "", tx, 0); r.addDefaultKeypress ('-', ModifierKeys::shiftModifier); break;

            case CommandIDs::toggleMixerView: r.setInfo ("Toggle Mixer", "Arrange/Mixer view", vw, 0); r.addDefaultKeypress ('b', 0); break;
            case CommandIDs::addAudioTrack:   r.setInfo ("Add Audio Track", "", vw, 0); r.addDefaultKeypress ('t', ModifierKeys::commandModifier); break;
            case CommandIDs::addMidiTrack:    r.setInfo ("Add MIDI Track", "", vw, 0); r.addDefaultKeypress ('t', ModifierKeys::commandModifier | ModifierKeys::shiftModifier); break;
            case CommandIDs::saveProject:     r.setInfo ("Save Project", "", vw, 0); r.addDefaultKeypress ('s', ModifierKeys::commandModifier); break;
            case CommandIDs::openProject:     r.setInfo ("Open Project", "", vw, 0); r.addDefaultKeypress ('o', ModifierKeys::commandModifier); break;
            case CommandIDs::openKeyMappings: r.setInfo ("Keyboard Shortcuts…", "Customise hotkeys", vw, 0); r.addDefaultKeypress (',', ModifierKeys::commandModifier); break;

            case CommandIDs::duplicateClip:   r.setInfo ("Duplicate Clip", "", ed, 0); r.addDefaultKeypress ('d', ModifierKeys::commandModifier); break;
            case CommandIDs::splitClip:       r.setInfo ("Split Clip at Playhead", "", ed, 0); r.addDefaultKeypress ('e', ModifierKeys::commandModifier); break;
            case CommandIDs::deleteClip:      r.setInfo ("Delete Clip", "", ed, 0); r.addDefaultKeypress (KeyPress::deleteKey, 0); r.addDefaultKeypress (KeyPress::backspaceKey, 0); break;
            case CommandIDs::reverseClip:     r.setInfo ("Reverse Audio Clip", "", ed, 0); r.addDefaultKeypress ('r', ModifierKeys::commandModifier); break;
            case CommandIDs::nudgeLeft:       r.setInfo ("Nudge Clip Left", "", ed, 0); r.addDefaultKeypress (KeyPress::leftKey, ModifierKeys::commandModifier); break;
            case CommandIDs::nudgeRight:      r.setInfo ("Nudge Clip Right", "", ed, 0); r.addDefaultKeypress (KeyPress::rightKey, ModifierKeys::commandModifier); break;
            case CommandIDs::undo:            r.setInfo ("Undo", "", ed, 0); r.addDefaultKeypress ('z', ModifierKeys::commandModifier); break;
            case CommandIDs::redo:            r.setInfo ("Redo", "", ed, 0); r.addDefaultKeypress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier); break;
            case CommandIDs::openAudioSettings: r.setInfo ("Audio Settings…", "Choose device / sample rate / buffer", vw, 0); r.addDefaultKeypress (KeyPress::F1Key, 0); break;

            case CommandIDs::toggleKeyboardPiano: r.setInfo ("Computer-Keyboard Piano", "Play instruments via QWERTY", in, 0); r.addDefaultKeypress (KeyPress::tabKey, 0); break;
            case CommandIDs::octaveUp:        r.setInfo ("Piano Octave +", "", in, 0); r.addDefaultKeypress ('x', 0); break;
            case CommandIDs::octaveDown:      r.setInfo ("Piano Octave -", "", in, 0); r.addDefaultKeypress ('z', 0); break;
            default: break;
        }
    }

    bool MainComponent::perform (const InvocationInfo& info)
    {
        auto& t = audioEngine.getTransport();

        switch (info.commandID)
        {
            case CommandIDs::playStop:        t.togglePlay(); break;
            case CommandIDs::recordToggle:    toggleRecord(); break;
            case CommandIDs::returnToStart:   t.setPositionSeconds (0.0); break;
            case CommandIDs::toggleLoop:      t.setLooping (! t.isLooping()); break;
            case CommandIDs::toggleMetronome: audioEngine.setMetronomeEnabled (! audioEngine.isMetronomeEnabled()); break;
            case CommandIDs::playheadLeft:    movePlayheadByBar (-1); break;
            case CommandIDs::playheadRight:   movePlayheadByBar (1); break;
            case CommandIDs::tempoUp:         changeTempo (1.0); break;
            case CommandIDs::tempoDown:       changeTempo (-1.0); break;

            case CommandIDs::toggleMixerView: toggleMixer(); break;
            case CommandIDs::addAudioTrack:   pushUndo(); project.getTimeline().addAudioTrack(); afterTrackChange(); break;
            case CommandIDs::addMidiTrack:    pushUndo(); project.getTimeline().addMidiTrack();  afterTrackChange(); break;
            case CommandIDs::saveProject:     saveProjectAs(); break;
            case CommandIDs::openProject:     openProjectDialog(); break;
            case CommandIDs::openKeyMappings: openKeyMappings(); break;

            case CommandIDs::duplicateClip:   duplicateSelectedClip(); break;
            case CommandIDs::splitClip:       splitSelectedClipAtPlayhead(); break;
            case CommandIDs::deleteClip:      deleteSelectedClip(); break;
            case CommandIDs::reverseClip:     reverseSelectedClip(); break;
            case CommandIDs::nudgeLeft:       nudgeSelectedClip (-1); break;
            case CommandIDs::nudgeRight:      nudgeSelectedClip (1); break;
            case CommandIDs::undo:            performUndo(); break;
            case CommandIDs::redo:            performRedo(); break;
            case CommandIDs::openAudioSettings: openAudioSettings(); break;

            case CommandIDs::toggleKeyboardPiano: pianoEnabled = ! pianoEnabled; if (! pianoEnabled) allPianoNotesOff(); break;
            case CommandIDs::octaveUp:        allPianoNotesOff(); pianoOctave = juce::jmin (9, pianoOctave + 1); break;
            case CommandIDs::octaveDown:      allPianoNotesOff(); pianoOctave = juce::jmax (0, pianoOctave - 1); break;
            default: return false;
        }
        return true;
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (FreequencyLookAndFeel::background));
    }

    void MainComponent::resized()
    {
        auto r = getLocalBounds();

        if (transportBar != nullptr)
            transportBar->setBounds (r.removeFromTop (56));

        if (statusBar != nullptr)
            statusBar->setBounds (r.removeFromBottom (24));

        if (arrangeView != nullptr)
            arrangeView->setBounds (r);

        if (mixerView != nullptr)
            mixerView->setBounds (r);

        if (pianoRoll != nullptr)
            pianoRoll->setBounds (r);
    }

    // ── Command actions ─────────────────────────────────────────────────────────

    void MainComponent::afterTrackChange()
    {
        pluginWindows.clear();
        audioEngine.rebuildGraph();
        if (arrangeView) arrangeView->rebuildTracks();
        if (mixerView)   mixerView->rebuild();
    }

    void MainComponent::afterClipChange()
    {
        audioEngine.rebuildSequences();
        if (arrangeView) arrangeView->rebuildTracks();
    }

    void MainComponent::toggleRecord()
    {
        auto& t = audioEngine.getTransport();
        if (audioEngine.isRecording())
        {
            audioEngine.stopRecording();
            t.stop();
        }
        else
        {
            const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                 .getChildFile ("FREEQUENCY Recordings");
            const auto file = dir.getChildFile ("rec_" + juce::Time::getCurrentTime()
                                 .formatted ("%Y%m%d_%H%M%S") + ".wav");
            if (audioEngine.startRecording (file))
                t.play();
        }
    }

    void MainComponent::saveProjectAs()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save project", juce::File(), "*.freq");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                if (file.getFileExtension().isEmpty()) file = file.withFileExtension ("freq");
                models::ProjectSerializer::saveToFile (project, file);
            });
    }

    void MainComponent::openProjectDialog()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Open project", juce::File(), "*.freq");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile()) openProjectFile (file);
            });
    }

    void MainComponent::openKeyMappings()
    {
        if (keyMappingWindow == nullptr)
        {
            keyMappingWindow = std::make_unique<juce::DocumentWindow> (
                "FREEQUENCY — Keyboard Shortcuts",
                juce::Colour (FreequencyLookAndFeel::panel), juce::DocumentWindow::closeButton);

            auto* editor = new juce::KeyMappingEditorComponent (*commandManager.getKeyMappings(), true);
            editor->setSize (620, 560);
            keyMappingWindow->setUsingNativeTitleBar (true);
            keyMappingWindow->setContentOwned (editor, true);
            keyMappingWindow->setResizable (true, false);
            keyMappingWindow->centreWithSize (640, 580);
        }

        keyMappingWindow->setVisible (true);
        keyMappingWindow->toFront (true);
    }

    void MainComponent::reverseSelectedClip()
    {
        auto* clip = dynamic_cast<models::AudioClip*> (context.selectedClip);
        if (clip == nullptr) return;
        pushUndo();
        clip->reversed = ! clip->reversed;
        afterClipChange();
    }

    void MainComponent::openAudioSettings()
    {
        if (audioSettingsWindow == nullptr)
        {
            audioSettingsWindow = std::make_unique<juce::DocumentWindow> (
                "FREEQUENCY — Audio Settings",
                juce::Colour (FreequencyLookAndFeel::panel), juce::DocumentWindow::closeButton);

            // 0..2 inputs (recording), 2 outputs; full device/rate/buffer control.
            auto* selector = new juce::AudioDeviceSelectorComponent (
                audioEngine.getDeviceManager(), 0, 2, 2, 2, false, false, true, false);
            selector->setSize (520, 440);
            audioSettingsWindow->setUsingNativeTitleBar (true);
            audioSettingsWindow->setContentOwned (selector, true);
            audioSettingsWindow->setResizable (true, false);
            audioSettingsWindow->centreWithSize (540, 460);
        }

        audioSettingsWindow->setVisible (true);
        audioSettingsWindow->toFront (true);
    }

    void MainComponent::pushUndo()
    {
        undoStack.push_back (models::ProjectSerializer::toValueTree (project));
        if ((int) undoStack.size() > maxUndo)
            undoStack.erase (undoStack.begin());
        redoStack.clear();
    }

    void MainComponent::performUndo()
    {
        if (undoStack.empty()) return;
        redoStack.push_back (models::ProjectSerializer::toValueTree (project));
        const auto snap = undoStack.back();
        undoStack.pop_back();
        restoreSnapshot (snap);
    }

    void MainComponent::performRedo()
    {
        if (redoStack.empty()) return;
        undoStack.push_back (models::ProjectSerializer::toValueTree (project));
        const auto snap = redoStack.back();
        redoStack.pop_back();
        restoreSnapshot (snap);
    }

    void MainComponent::restoreSnapshot (const juce::ValueTree& tree)
    {
        models::ProjectSerializer::fromValueTree (tree, project);
        context.selectedClip = nullptr;
        context.selectedTrack = nullptr;
        afterTrackChange(); // rebuild graph + arrange + mixer
    }

    void MainComponent::duplicateSelectedClip()
    {
        auto* track = context.selectedTrack;
        auto* clip = context.selectedClip;
        if (track == nullptr || clip == nullptr) return;
        pushUndo();

        const double len = clip->length > 0.0 ? clip->length : 2.0;

        if (auto* mc = dynamic_cast<models::MidiClip*> (clip))
        {
            if (auto* mt = dynamic_cast<models::MidiTrack*> (track))
            {
                auto* n = mt->addClip();
                n->name = mc->name; n->colour = mc->colour;
                n->startTime = clip->startTime + len; n->length = mc->length;
                n->sequence = mc->sequence;
            }
        }
        else if (auto* ac = dynamic_cast<models::AudioClip*> (clip))
        {
            if (auto* at = dynamic_cast<models::AudioTrack*> (track))
            {
                auto* n = at->addClip();
                n->name = ac->name; n->colour = ac->colour;
                n->startTime = clip->startTime + len; n->length = ac->length;
                n->sourceFile = ac->sourceFile; n->sourceOffset = ac->sourceOffset; n->gain = ac->gain;
            }
        }

        afterClipChange();
    }

    void MainComponent::splitSelectedClipAtPlayhead()
    {
        auto* track = context.selectedTrack;
        auto* clip = context.selectedClip;
        if (track == nullptr || clip == nullptr) return;

        const double len = clip->length > 0.0 ? clip->length : 2.0;
        const double pos = audioEngine.getTransport().getPositionSeconds();
        if (pos <= clip->startTime + 1.0e-4 || pos >= clip->startTime + len - 1.0e-4) return;
        pushUndo();

        const double cut = pos - clip->startTime; // seconds into the clip

        if (auto* mc = dynamic_cast<models::MidiClip*> (clip))
        {
            if (auto* mt = dynamic_cast<models::MidiTrack*> (track))
            {
                auto* n = mt->addClip();
                n->name = mc->name; n->colour = mc->colour;
                n->startTime = pos; n->length = len - cut;

                juce::MidiMessageSequence left, right;
                for (int i = 0; i < mc->sequence.getNumEvents(); ++i)
                {
                    auto m = mc->sequence.getEventPointer (i)->message;
                    if (m.getTimeStamp() < cut) left.addEvent (m);
                    else { m.setTimeStamp (m.getTimeStamp() - cut); right.addEvent (m); }
                }
                left.updateMatchedPairs(); right.updateMatchedPairs();
                mc->sequence = left; mc->length = cut;
                n->sequence = right;
            }
        }
        else if (auto* ac = dynamic_cast<models::AudioClip*> (clip))
        {
            if (auto* at = dynamic_cast<models::AudioTrack*> (track))
            {
                auto* n = at->addClip();
                n->name = ac->name; n->colour = ac->colour;
                n->sourceFile = ac->sourceFile; n->gain = ac->gain;
                n->startTime = pos; n->sourceOffset = ac->sourceOffset + cut; n->length = len - cut;
                ac->length = cut;
            }
        }

        afterClipChange();
    }

    void MainComponent::deleteSelectedClip()
    {
        auto* track = context.selectedTrack;
        auto* clip = context.selectedClip;
        if (track == nullptr || clip == nullptr) return;
        pushUndo();
        track->removeClip (clip);
        context.selectedClip = nullptr;
        afterClipChange();
    }

    void MainComponent::nudgeSelectedClip (int direction)
    {
        auto* clip = context.selectedClip;
        if (clip == nullptr) return;
        pushUndo();

        const auto& tl = project.getTimeline();
        const double beat = 60.0 / juce::jmax (1.0, tl.getTempoBpm());
        clip->startTime = juce::jmax (0.0, clip->startTime + direction * beat);

        audioEngine.rebuildSequences();
        if (context.repaintArrange) context.repaintArrange();
    }

    void MainComponent::movePlayheadByBar (int direction)
    {
        const auto& tl = project.getTimeline();
        const double secsPerBar = (60.0 / juce::jmax (1.0, tl.getTempoBpm()))
                                  * juce::jmax (1, tl.getTimeSigNumerator());
        auto& t = audioEngine.getTransport();
        t.setPositionSeconds (juce::jmax (0.0, t.getPositionSeconds() + direction * secsPerBar));
    }

    void MainComponent::changeTempo (double delta)
    {
        const double bpm = juce::jlimit (20.0, 999.0, project.getTimeline().getTempoBpm() + delta);
        project.getTimeline().setTempoBpm (bpm);
        audioEngine.getTransport().setTempo (bpm);
        audioEngine.rebuildSequences();
    }

    // ── Computer-keyboard piano ─────────────────────────────────────────────────

    models::MidiTrack* MainComponent::pianoTargetTrack() const
    {
        if (auto* mt = dynamic_cast<models::MidiTrack*> (context.selectedTrack))
            return mt;

        auto& tl = project.getTimeline();
        for (int i = 0; i < tl.getNumTracks(); ++i)
            if (auto* mt = dynamic_cast<models::MidiTrack*> (tl.getTrack (i)))
                return mt;

        return nullptr;
    }

    void MainComponent::allPianoNotesOff()
    {
        if (auto* mt = pianoTargetTrack())
            for (int i = 0; i < 13; ++i)
                if (pianoKeyDown[i])
                    audioEngine.sendLiveNote (*mt, pianoOctave * 12 + i, 0.0f, false);

        for (auto& d : pianoKeyDown) d = false;
    }

    bool MainComponent::keyStateChanged (bool)
    {
        if (! pianoEnabled)
            return false;

        // Ableton/FL-style QWERTY layout: a w s e d f t g y h u j k => C..C.
        static const char* layout = "AWSEDFTGYHUJK";
        auto* mt = pianoTargetTrack();
        bool handled = false;

        for (int i = 0; i < 13; ++i)
        {
            const bool down = juce::KeyPress::isKeyCurrentlyDown (layout[i]);
            if (down != pianoKeyDown[i])
            {
                pianoKeyDown[i] = down;
                handled = true;
                if (mt != nullptr)
                    audioEngine.sendLiveNote (*mt, pianoOctave * 12 + i, 0.8f, down);
            }
        }

        return handled;
    }

    juce::File MainComponent::keyMappingsFile() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("FREEQUENCY").getChildFile ("keymappings.xml");
    }

    void MainComponent::saveKeyMappings()
    {
        if (auto xml = commandManager.getKeyMappings()->createXml (true))
        {
            const auto file = keyMappingsFile();
            file.getParentDirectory().createDirectory();
            xml->writeTo (file);
        }
    }
} // namespace freequency::ui
