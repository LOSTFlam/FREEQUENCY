#pragma once

#include "UI/UIContext.h"
#include "Models/Clip.h"
#include "Models/Track.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace freequency::ui
{
    /**
        PianoRollEditor — a full-screen note-level MIDI editor (FL/Cubase piano
        roll) for a single MidiClip.

        Features: draw / move / resize / delete / select notes on a snapping grid,
        a velocity lane, a piano keyboard gutter, a transport-synced playhead, an
        arpeggiator that turns the clip's pitches into a rhythmic pattern, and
        per-note "slide" (pitch-glide) flags. All edits write back to the model
        and re-sync the engine, so changes are heard immediately.
    */
    class PianoRollEditor final : public juce::Component,
                                  private juce::Timer
    {
    public:
        PianoRollEditor (UIContext& ctx, models::MidiClip& clip, models::Track& track);
        ~PianoRollEditor() override;

        std::function<void()> onClose;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Note
        {
            double startBeats { 0.0 };
            double lengthBeats { 1.0 };
            int    pitch { 60 };
            int    velocity { 100 };
            bool   slide { false };
            bool   selected { false };
        };

        // The interactive grid (notes + velocity lane).
        class Grid final : public juce::Component
        {
        public:
            explicit Grid (PianoRollEditor& o) : owner (o) {}
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;
            void mouseDoubleClick (const juce::MouseEvent&) override;
        private:
            int noteAt (juce::Point<int>) const;       // index or -1
            bool overRightEdge (int noteIndex, juce::Point<int>) const;
            PianoRollEditor& owner;
            enum class Mode { none, move, resize, velocity } mode { Mode::none };
            int dragIndex { -1 };
            juce::Point<int> dragStart;
            double origStartBeats { 0.0 }, origLenBeats { 0.0 };
            int origPitch { 0 };
        };

        // The piano keyboard gutter (left).
        class Keys final : public juce::Component
        {
        public:
            explicit Keys (PianoRollEditor& o) : owner (o) {}
            void paint (juce::Graphics&) override;
        private:
            PianoRollEditor& owner;
        };

        // Velocity lane (bottom), horizontally synced to the grid.
        class VelLane final : public juce::Component
        {
        public:
            explicit VelLane (PianoRollEditor& o) : owner (o) {}
            void setViewOffsetX (int x) { viewOffsetX = x; repaint(); }
            void paint (juce::Graphics&) override;
            void mouseDown (const juce::MouseEvent& e) override { setVel (e); }
            void mouseDrag (const juce::MouseEvent& e) override { setVel (e); }
        private:
            void setVel (const juce::MouseEvent&);
            PianoRollEditor& owner;
            int viewOffsetX { 0 };
        };

        struct ScrollViewport final : public juce::Viewport
        {
            std::function<void()> onScroll;
            void visibleAreaChanged (const juce::Rectangle<int>&) override { if (onScroll) onScroll(); }
        };

        // Geometry / conversions.
        [[nodiscard]] int beatsToX (double beats) const { return (int) std::llround (beats * pxPerBeat); }
        [[nodiscard]] double xToBeats (int x) const { return pxPerBeat > 0 ? (double) x / pxPerBeat : 0.0; }
        [[nodiscard]] int pitchToY (int pitch) const { return (127 - pitch) * noteHeight; }
        [[nodiscard]] int yToPitch (int y) const { return juce::jlimit (0, 127, 127 - y / noteHeight); }
        [[nodiscard]] double snapBeats (double beats) const;
        [[nodiscard]] double clipLengthBeats() const;

        void loadFromClip();
        void writeBackToClip();
        void applyArpeggiator();
        void deleteSelected();
        void timerCallback() override;

        UIContext& context;
        models::MidiClip& clip;
        models::Track& track;

        std::vector<Note> notes;

        double pxPerBeat { 64.0 };
        int noteHeight { 12 };
        double snapGrid { 0.25 }; // beats; 0.25 == 1/16 note. 0 == off.
        bool slideMode { false };

        juce::Label titleLabel;
        juce::TextButton closeButton { "Close" };
        juce::TextButton arpButton { "Arp" };
        juce::TextButton slideButton { "Slide" };
        juce::ComboBox snapBox;

        Keys keysComp { *this };
        Grid gridComp { *this };
        VelLane velLane { *this };
        juce::Viewport keysViewport;
        ScrollViewport gridViewport;

        static constexpr int gutterW = 64;
        static constexpr int toolbarH = 40;
        static constexpr int velLaneH = 80;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollEditor)
    };
} // namespace freequency::ui
