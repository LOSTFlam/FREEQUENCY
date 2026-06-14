#include "UI/TransportBar.h"
#include "UI/FreequencyLookAndFeel.h"

#include "Models/ProjectSerializer.h"

namespace freequency::ui
{
    TransportBar::TransportBar (UIContext& ctx)
        : context (ctx)
    {
        auto& transport = context.engine.getTransport();

        addAndMakeVisible (spectrum);

        addAndMakeVisible (logoLabel);
        logoLabel.setText ("FREEQUENCY", juce::dontSendNotification);
        logoLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        logoLabel.setColour (juce::Label::textColourId, theme().accent);

        addAndMakeVisible (playButton);
        playButton.setClickingTogglesState (true);
        playButton.onClick = [this]
        {
            auto& t = context.engine.getTransport();
            if (playButton.getToggleState()) t.play();
            else                             t.stop();
        };

        addAndMakeVisible (stopButton);
        stopButton.onClick = [this]
        {
            auto& t = context.engine.getTransport();
            t.stop();
            t.setPositionSeconds (0.0);
            playButton.setToggleState (false, juce::dontSendNotification);
        };

        addAndMakeVisible (recordButton);
        recordButton.setClickingTogglesState (true);
        recordButton.setColour (juce::TextButton::buttonOnColourId, theme().danger);
        recordButton.onClick = [this]
        {
            auto& t = context.engine.getTransport();

            if (context.engine.isRecording())
            {
                context.engine.stopRecording();
                t.stop();
                playButton.setToggleState (false, juce::dontSendNotification);
                recordButton.setToggleState (false, juce::dontSendNotification);
            }
            else
            {
                const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                     .getChildFile ("FREEQUENCY Recordings");
                const auto file = dir.getChildFile ("rec_" + juce::Time::getCurrentTime()
                                     .formatted ("%Y%m%d_%H%M%S") + ".wav");

                if (context.engine.startRecording (file))
                {
                    t.play();
                    playButton.setToggleState (true, juce::dontSendNotification);
                    recordButton.setToggleState (true, juce::dontSendNotification);
                }
                else
                {
                    recordButton.setToggleState (false, juce::dontSendNotification);
                }
            }
        };

        addAndMakeVisible (loopButton);
        loopButton.setClickingTogglesState (true);
        loopButton.onClick = [this, &transport]
        {
            context.engine.getTransport().setLooping (loopButton.getToggleState());
        };

        addAndMakeVisible (metronomeButton);
        metronomeButton.setClickingTogglesState (true);
        metronomeButton.setToggleState (context.engine.isMetronomeEnabled(), juce::dontSendNotification);
        metronomeButton.onClick = [this]
        {
            context.engine.setMetronomeEnabled (metronomeButton.getToggleState());
        };

        addAndMakeVisible (tapButton);
        tapButton.onClick = [this] { tapTempo(); };

        addAndMakeVisible (limiterButton);
        limiterButton.setClickingTogglesState (true);
        limiterButton.setToggleState (context.engine.isLimiterEnabled(), juce::dontSendNotification);
        limiterButton.onClick = [this]
        {
            context.engine.setLimiterEnabled (limiterButton.getToggleState());
        };

        addAndMakeVisible (snapCaption);
        snapCaption.setText ("Snap", juce::dontSendNotification);
        snapCaption.setFont (juce::FontOptions (11.0f));
        snapCaption.setColour (juce::Label::textColourId, theme().textDim);

        addAndMakeVisible (snapBox);
        snapBox.addItemList ({ "Off", "Bar", "1/2", "1/4", "1/8", "1/16" }, 1);
        snapBox.setSelectedId (2, juce::dontSendNotification); // Bar
        snapBox.onChange = [this]
        {
            context.snap = (UIContext::Snap) (snapBox.getSelectedId() - 1);
        };

        addAndMakeVisible (addAudioButton);
        addAudioButton.onClick = [this]
        {
            if (context.pushUndo) context.pushUndo();
            context.project.getTimeline().addAudioTrack();
            context.engine.rebuildGraph();
            if (onProjectStructureChanged) onProjectStructureChanged();
        };

        addAndMakeVisible (addMidiButton);
        addMidiButton.onClick = [this]
        {
            if (context.pushUndo) context.pushUndo();
            context.project.getTimeline().addMidiTrack();
            context.engine.rebuildGraph();
            if (onProjectStructureChanged) onProjectStructureChanged();
        };

        addAndMakeVisible (mixerButton);
        mixerButton.setClickingTogglesState (true);
        mixerButton.onClick = [this] { if (onToggleMixer) onToggleMixer(); };

        addAndMakeVisible (keysButton);
        keysButton.onClick = [this] { if (onOpenSettings) onOpenSettings(); };

        addAndMakeVisible (audioButton);
        audioButton.onClick = [this] { if (onOpenAudioSettings) onOpenAudioSettings(); };

        addAndMakeVisible (browseButton);
        browseButton.setClickingTogglesState (true);
        browseButton.onClick = [this] { if (onToggleBrowser) onToggleBrowser(); };

        addAndMakeVisible (themeButton);
        themeButton.onClick = [this] { if (onOpenAppearance) onOpenAppearance(); };

        addAndMakeVisible (saveButton);
        saveButton.onClick = [this]
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Save project", juce::File(), "*.freq");
            fileChooser->launchAsync (
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file == juce::File())
                        return;
                    if (file.getFileExtension().isEmpty())
                        file = file.withFileExtension ("freq");
                    models::ProjectSerializer::saveToFile (context.project, file);
                });
        };

        addAndMakeVisible (openButton);
        openButton.onClick = [this]
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Open project", juce::File(), "*.freq");
            fileChooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    const auto file = fc.getResult();
                    if (! file.existsAsFile())
                        return;

                    if (models::ProjectSerializer::loadFromFile (context.project, file))
                    {
                        context.engine.rebuildGraph();
                        tempoLabel.setText (juce::String (context.project.getTimeline().getTempoBpm(), 1),
                                            juce::dontSendNotification);
                        context.engine.getTransport().setTempo (context.project.getTimeline().getTempoBpm());
                        if (onProjectStructureChanged) onProjectStructureChanged();
                    }
                });
        };

        addAndMakeVisible (positionLabel);
        positionLabel.setJustificationType (juce::Justification::centred);
        positionLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::bold));
        positionLabel.setColour (juce::Label::textColourId, theme().accent);

        addAndMakeVisible (tempoCaption);
        tempoCaption.setText ("BPM", juce::dontSendNotification);
        tempoCaption.setJustificationType (juce::Justification::centredLeft);
        tempoCaption.setColour (juce::Label::textColourId, theme().textDim);
        tempoCaption.setFont (juce::FontOptions (11.0f));

        addAndMakeVisible (tempoLabel);
        tempoLabel.setEditable (true);
        tempoLabel.setJustificationType (juce::Justification::centredLeft);
        tempoLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        tempoLabel.setText (juce::String (context.project.getTimeline().getTempoBpm(), 1), juce::dontSendNotification);
        tempoLabel.onTextChange = [this] { updateTempoFromLabel(); };

        startTimerHz (30);
    }

    TransportBar::~TransportBar()
    {
        stopTimer();
    }

    void TransportBar::tapTempo()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();

        // Reset if the previous tap was long ago (a new tempo intent).
        if (! tapTimesMs.empty() && now - tapTimesMs.back() > 2000.0)
            tapTimesMs.clear();

        tapTimesMs.push_back (now);
        if (tapTimesMs.size() > 8)
            tapTimesMs.erase (tapTimesMs.begin());

        if (tapTimesMs.size() >= 2)
        {
            const double span = tapTimesMs.back() - tapTimesMs.front();
            const double avgInterval = span / (double) (tapTimesMs.size() - 1);
            if (avgInterval > 0.0)
            {
                const double bpm = juce::jlimit (20.0, 999.0, 60000.0 / avgInterval);
                context.project.getTimeline().setTempoBpm (bpm);
                context.engine.getTransport().setTempo (bpm);
                context.engine.rebuildSequences();
                tempoLabel.setText (juce::String (bpm, 1), juce::dontSendNotification);
            }
        }
    }

    void TransportBar::updateTempoFromLabel()
    {
        const auto bpm = juce::jlimit (20.0, 999.0, tempoLabel.getText().getDoubleValue());
        context.project.getTimeline().setTempoBpm (bpm);
        context.engine.getTransport().setTempo (bpm);
        context.engine.rebuildSequences(); // tempo affects beat->sample mapping
        tempoLabel.setText (juce::String (bpm, 1), juce::dontSendNotification);
    }

    void TransportBar::timerCallback()
    {
        auto& t = context.engine.getTransport();

        // Keep the play button in sync if transport state changed elsewhere.
        playButton.setToggleState (t.isPlaying(), juce::dontSendNotification);
        recordButton.setToggleState (context.engine.isRecording(), juce::dontSendNotification);
        metronomeButton.setToggleState (context.engine.isMetronomeEnabled(), juce::dontSendNotification);
        limiterButton.setToggleState (context.engine.isLimiterEnabled(), juce::dontSendNotification);
        loopButton.setToggleState (t.isLooping(), juce::dontSendNotification);

        // Keep the tempo read-out in sync with tempo changed via tap / shortcuts.
        if (! tempoLabel.isBeingEdited())
        {
            const auto bpm = juce::String (context.project.getTimeline().getTempoBpm(), 1);
            if (tempoLabel.getText() != bpm)
                tempoLabel.setText (bpm, juce::dontSendNotification);
        }

        const auto seconds = t.getPositionSeconds();
        const auto tempo = context.project.getTimeline().getTempoBpm();
        const auto sig = context.project.getTimeline().getTimeSigNumerator();

        const auto totalBeats = seconds * (tempo / 60.0);
        const int bar  = (int) (totalBeats / juce::jmax (1, sig)) + 1;
        const int beat = (int) std::fmod (totalBeats, (double) juce::jmax (1, sig)) + 1;
        const int tick = (int) (std::fmod (totalBeats, 1.0) * 960.0);

        positionLabel.setText (juce::String::formatted ("%03d.%d.%03d", bar, beat, tick),
                               juce::dontSendNotification);

    }

    void TransportBar::paint (juce::Graphics& g)
    {
        g.fillAll (theme().panel);
        g.setColour (theme().outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void TransportBar::resized()
    {
        auto r = getLocalBounds().reduced (8, 8);

        // Spectrum analyzer on the far right (the FREEQUENCY flourish).
        spectrum.setBounds (r.removeFromRight (200).reduced (4, 8));
        r.removeFromRight (6);

        auto button = [&r] (juce::Component& c, int w)
        {
            c.setBounds (r.removeFromLeft (w).reduced (3, 0));
        };

        logoLabel.setBounds (r.removeFromLeft (104));
        r.removeFromLeft (4);

        button (playButton, 56);
        button (stopButton, 52);
        button (recordButton, 48);
        button (loopButton, 58);
        button (metronomeButton, 58);

        r.removeFromLeft (10);
        positionLabel.setBounds (r.removeFromLeft (128));

        r.removeFromLeft (8);
        tempoCaption.setBounds (r.removeFromLeft (30));
        tempoLabel.setBounds (r.removeFromLeft (52));
        button (tapButton, 50);

        r.removeFromLeft (8);
        snapCaption.setBounds (r.removeFromLeft (34));
        snapBox.setBounds (r.removeFromLeft (64).reduced (0, 6));

        r.removeFromLeft (10);
        button (addAudioButton, 70);
        button (addMidiButton, 64);
        button (mixerButton, 58);
        button (limiterButton, 44);
        button (saveButton, 54);
        button (openButton, 58);
        button (keysButton, 52);
        button (audioButton, 58);
        button (browseButton, 64);
        button (themeButton, 62);
    }

    juce::Rectangle<int> TransportBar::getAnchorBounds (GuideAnchor anchor) const
    {
        switch (anchor)
        {
            case GuideAnchor::transportPlay:     return playButton.getBounds();
            case GuideAnchor::transportMixer:    return mixerButton.getBounds();
            case GuideAnchor::transportBrowser:  return browseButton.getBounds();
            case GuideAnchor::transportTheme:    return themeButton.getBounds();
            case GuideAnchor::transportAudio:    return audioButton.getBounds();
            case GuideAnchor::transportKeys:     return keysButton.getBounds();
            case GuideAnchor::transportAddTrack: return addAudioButton.getBounds().getUnion (addMidiButton.getBounds());
            case GuideAnchor::arrangeView:
            case GuideAnchor::statusBar:
            case GuideAnchor::customPin0:
            case GuideAnchor::customPin1:
            case GuideAnchor::customPin2:
            default:                             return {};
        }
    }
} // namespace freequency::ui
