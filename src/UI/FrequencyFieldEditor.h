#pragma once

#include "UI/UIContext.h"
#include "Models/Clip.h"
#include "Models/HarmonicEdit.h"
#include "Models/Track.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace freequency::ui
{
    /**
        FrequencyFieldEditor — flagship harmonic surface for one AudioClip.

        Pitch grid, ghost notes, VariAudio anchors (drag / delete), detected
        spectral contour, comp-swipe overlay, elastic mode, and bake-to-snapshot.
    */
    class FrequencyFieldEditor final : public juce::Component,
                                       private juce::Timer
    {
    public:
        FrequencyFieldEditor (UIContext& ctx, models::AudioClip& clip, models::Track& track);
        ~FrequencyFieldEditor() override;

        std::function<void()> onClose;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        struct GhostNote
        {
            double relSeconds { 0.0 };
            double lengthSec  { 0.1 };
            int    pitch      { 60 };
            juce::Colour colour;
        };

        class FieldCanvas final : public juce::Component
        {
        public:
            explicit FieldCanvas (FrequencyFieldEditor& o) : owner (o) {}
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
        private:
            FrequencyFieldEditor& owner;
        };

        void timerCallback() override;
        void loadGhostNotes();
        void loadPreviewAudio();
        void refreshContour();
        void bakeToSnapshot();
        int findAnchorAt (int x, int y) const;
        void removeAnchor (int index);
        [[nodiscard]] double targetCentsAtY (int y) const;
        [[nodiscard]] double clipLengthSec() const;
        [[nodiscard]] int pitchToY (int pitch) const;
        [[nodiscard]] int centsToY (double cents) const;
        [[nodiscard]] int timeToX (double relSec) const;
        [[nodiscard]] int pitchAtY (int y) const;
        [[nodiscard]] double timeAtX (int x) const;

        UIContext& context;
        models::AudioClip& clip;
        models::Track& track;

        juce::Label titleLabel;
        juce::TextButton closeButton { "Close" };
        juce::ToggleButton ghostButton { "Ghost notes" };
        juce::ComboBox elasticBox;
        juce::TextButton bakeButton { "Bake" };
        juce::Label hintLabel;
        juce::Label statusLabel;

        juce::Viewport viewport;
        FieldCanvas canvas;

        models::GhostNoteConfig ghostConfig;
        std::vector<GhostNote> ghostNotes;

        juce::AudioBuffer<float> previewBuffer;
        double previewSampleRate { 44100.0 };
        std::vector<double> contourTimes;
        std::vector<double> contourCents;

        int playheadX { -1 };
        int dragAnchorIndex { -1 };
        bool bakePulse { false };

        static constexpr int pitchMin = 36;
        static constexpr int pitchMax = 96;
        static constexpr int pitchRowH = 10;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrequencyFieldEditor)
    };
} // namespace freequency::ui
