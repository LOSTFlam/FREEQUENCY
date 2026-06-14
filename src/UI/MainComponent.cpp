#include "UI/MainComponent.h"

#include "Models/MidiTrack.h"
#include "Models/AudioTrack.h"

namespace omnidaw::ui
{
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

        audioEngine.setProject (&project);
        audioEngine.onRecordingFinished = [this] { if (arrangeView) arrangeView->rebuildTracks(); };
        const auto error = audioEngine.initialise();
        engineStatus = error.isEmpty() ? "Engine running" : ("Engine idle: " + error);

        transportBar = std::make_unique<TransportBar> (context);
        transportBar->onProjectStructureChanged = [this]
        {
            if (arrangeView) arrangeView->rebuildTracks();
            if (mixerView)   mixerView->rebuild();
        };
        transportBar->onToggleMixer = [this] { toggleMixer(); };
        addAndMakeVisible (*transportBar);

        arrangeView = std::make_unique<ArrangeView> (context);
        addAndMakeVisible (*arrangeView);

        mixerView = std::make_unique<MixerView> (context);
        addChildComponent (*mixerView); // hidden until toggled

        setSize (1280, 760);
    }

    MainComponent::~MainComponent()
    {
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

    void MainComponent::toggleMixer()
    {
        mixerVisible = ! mixerVisible;

        if (mixerVisible && mixerView != nullptr)
            mixerView->rebuild(); // reflect any track/routing changes

        if (arrangeView != nullptr) arrangeView->setVisible (! mixerVisible);
        if (mixerView != nullptr)   mixerView->setVisible (mixerVisible);
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (OmniLookAndFeel::background));
    }

    void MainComponent::resized()
    {
        auto r = getLocalBounds();

        if (transportBar != nullptr)
            transportBar->setBounds (r.removeFromTop (56));

        if (arrangeView != nullptr)
            arrangeView->setBounds (r);

        if (mixerView != nullptr)
            mixerView->setBounds (r);
    }
} // namespace omnidaw::ui
