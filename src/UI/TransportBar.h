#pragma once

#include "UI/UIContext.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace freequency::ui
{
    /**
        TransportBar — the top control strip: transport, tempo, time display,
        master meter and track-creation buttons.

        It reads/writes the engine's Transport (play/stop/loop/position) and the
        project tempo, and asks the host to rebuild the arrange view when the user
        adds a track. A lightweight timer refreshes the position read-out, play
        state and meter ~30x/second — all on the message thread.
    */
    class TransportBar final : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit TransportBar (UIContext& ctx);
        ~TransportBar() override;

        /** Called when the project's track list changes (add/remove). */
        std::function<void()> onProjectStructureChanged;

        /** Called when the user toggles the mixer view. */
        std::function<void()> onToggleMixer;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void updateTempoFromLabel();
        void tapTempo();

        UIContext& context;

        juce::Label      logoLabel;
        juce::TextButton playButton   { "Play" };
        juce::TextButton stopButton   { "Stop" };
        juce::TextButton recordButton { "Rec" };
        juce::TextButton loopButton   { "Loop" };
        juce::TextButton metronomeButton { "Click" };
        juce::TextButton tapButton    { "Tap" };
        juce::TextButton limiterButton { "Lim" };
        juce::ComboBox   snapBox;
        juce::Label      snapCaption;
        juce::TextButton addAudioButton { "+ Audio" };
        juce::TextButton addMidiButton  { "+ MIDI" };
        juce::TextButton mixerButton    { "Mixer" };
        juce::TextButton saveButton     { "Save" };
        juce::TextButton openButton     { "Open" };

        std::unique_ptr<juce::FileChooser> fileChooser;

        juce::Label positionLabel;
        juce::Label tempoLabel;
        juce::Label tempoCaption;

        float meterLevel { 0.0f };
        std::vector<double> tapTimesMs; // tap-tempo history

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
    };
} // namespace freequency::ui
