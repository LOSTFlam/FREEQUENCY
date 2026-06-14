#include "UI/TransportBar.h"
#include "UI/FreequencyLookAndFeel.h"

#include "Models/ProjectSerializer.h"

namespace freequency::ui
{
    TransportBar::TransportBar (UIContext& ctx)
        : context (ctx)
    {
        auto& transport = context.engine.getTransport();

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
        recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (FreequencyLookAndFeel::danger));
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

        addAndMakeVisible (addAudioButton);
        addAudioButton.onClick = [this]
        {
            context.project.getTimeline().addAudioTrack();
            context.engine.rebuildGraph();
            if (onProjectStructureChanged) onProjectStructureChanged();
        };

        addAndMakeVisible (addMidiButton);
        addMidiButton.onClick = [this]
        {
            context.project.getTimeline().addMidiTrack();
            context.engine.rebuildGraph();
            if (onProjectStructureChanged) onProjectStructureChanged();
        };

        addAndMakeVisible (mixerButton);
        mixerButton.setClickingTogglesState (true);
        mixerButton.onClick = [this] { if (onToggleMixer) onToggleMixer(); };

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
        positionLabel.setColour (juce::Label::textColourId, juce::Colour (FreequencyLookAndFeel::accent));

        addAndMakeVisible (tempoCaption);
        tempoCaption.setText ("BPM", juce::dontSendNotification);
        tempoCaption.setJustificationType (juce::Justification::centredLeft);
        tempoCaption.setColour (juce::Label::textColourId, juce::Colour (FreequencyLookAndFeel::textDim));
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

        const auto seconds = t.getPositionSeconds();
        const auto tempo = context.project.getTimeline().getTempoBpm();
        const auto sig = context.project.getTimeline().getTimeSigNumerator();

        const auto totalBeats = seconds * (tempo / 60.0);
        const int bar  = (int) (totalBeats / juce::jmax (1, sig)) + 1;
        const int beat = (int) std::fmod (totalBeats, (double) juce::jmax (1, sig)) + 1;
        const int tick = (int) (std::fmod (totalBeats, 1.0) * 960.0);

        positionLabel.setText (juce::String::formatted ("%03d.%d.%03d", bar, beat, tick),
                               juce::dontSendNotification);

        const auto newLevel = context.engine.getMasterLevel();
        if (std::abs (newLevel - meterLevel) > 0.001f)
        {
            meterLevel = newLevel;
            repaint();
        }
    }

    void TransportBar::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (FreequencyLookAndFeel::panel));
        g.setColour (juce::Colour (FreequencyLookAndFeel::outline));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        // Master meter (right side).
        auto meterArea = getLocalBounds().removeFromRight (160).reduced (12, 16);
        g.setColour (juce::Colour (FreequencyLookAndFeel::background));
        g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);

        const auto level = juce::jlimit (0.0f, 1.0f, meterLevel);
        auto filled = meterArea.toFloat().withWidth (meterArea.getWidth() * level);
        const auto meterColour = level > 0.9f ? juce::Colour (FreequencyLookAndFeel::danger)
                                              : juce::Colour (FreequencyLookAndFeel::accent);
        g.setColour (meterColour);
        g.fillRoundedRectangle (filled, 3.0f);
    }

    void TransportBar::resized()
    {
        auto r = getLocalBounds().reduced (8, 8);

        auto button = [&r] (juce::Component& c, int w)
        {
            c.setBounds (r.removeFromLeft (w).reduced (3, 0));
        };

        button (playButton, 64);
        button (stopButton, 56);
        button (recordButton, 52);
        button (loopButton, 56);

        r.removeFromLeft (14);
        positionLabel.setBounds (r.removeFromLeft (150));

        r.removeFromLeft (10);
        tempoCaption.setBounds (r.removeFromLeft (32));
        tempoLabel.setBounds (r.removeFromLeft (60));

        r.removeFromLeft (14);
        button (addAudioButton, 76);
        button (addMidiButton, 72);
        button (mixerButton, 64);
        r.removeFromLeft (8);
        button (saveButton, 58);
        button (openButton, 58);

        // Right area is reserved for the meter (painted in paint()).
    }
} // namespace freequency::ui
