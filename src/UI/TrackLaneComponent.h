#pragma once

#include "UI/UIContext.h"
#include "Models/Track.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace freequency::ui
{
    /**
        TrackLaneComponent — the timeline lane for one track, drawing its clips.

        - Audio clips render a waveform via juce::AudioThumbnail (computed on a
          background thread; the lane repaints when new data arrives).
        - MIDI clips render as a mini piano-roll: each note a coloured rectangle
          positioned by pitch (y) and time (x), read from the MidiClip model.

        Editing affordances (Cubase/FL-style quick content creation):
        - Double-click an empty MIDI lane: insert a 2-bar clip with a starter
          pattern.
        - Double-click an empty audio lane: import an audio file at that position.
        - Drag clip body to move; drag left/right edges to trim; Alt+drag to slip.
        After any edit the engine's sequences are rebuilt so playback matches.
    */
    class TrackLaneComponent final : public juce::Component,
                                     public juce::DragAndDropTarget,
                                     private juce::ChangeListener
    {
    public:
        TrackLaneComponent (UIContext& ctx, models::Track& track, int index);
        ~TrackLaneComponent() override;

        void paint (juce::Graphics&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        /** Rebuild waveform thumbnails after the clip list changes. */
        void refreshClips();

        std::function<void()> onClipsChanged;

        // juce::DragAndDropTarget — accept audio files dragged from the browser.
        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;

        /** Import an audio file as a clip at the given timeline position. */
        void importAudioFile (const juce::File&, double startSeconds);

    private:
        enum class ClipDragMode { none, move, trimLeft, trimRight, slip };

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void drawMidiClip (juce::Graphics&, models::MidiClip&, juce::Rectangle<int> clipBounds);
        void drawPatternClip (juce::Graphics&, models::PatternClip&, juce::Rectangle<int> clipBounds);
        void drawAudioClip (juce::Graphics&, models::AudioClip&, int clipIndex, juce::Rectangle<int> clipBounds);
        void drawAutomation (juce::Graphics&);
        void drawTrimHandles (juce::Graphics&, juce::Rectangle<int> clipBounds, bool selected);

        [[nodiscard]] models::Clip* clipAtTime (double seconds) const;
        [[nodiscard]] ClipDragMode hitTestClip (models::Clip& clip, juce::Point<int> pos) const;
        [[nodiscard]] juce::Rectangle<int> clipBoundsFor (models::Clip& clip) const;
        [[nodiscard]] double audioSourceDuration (models::AudioClip& clip) const;

        // Automation value (0..maxAutoValue) <-> lane y.
        static constexpr float maxAutoValue = 1.5f;
        static constexpr int trimHandleW = 8;
        [[nodiscard]] int valueToY (float value) const;
        [[nodiscard]] float yToValue (int y) const;
        [[nodiscard]] int findAutomationPoint (juce::Point<int> pos) const; // -1 if none

        UIContext& context;
        models::Track& trackRef;
        int laneIndex { 0 };

        juce::AudioThumbnailCache thumbnailCache { 16 };
        juce::OwnedArray<juce::AudioThumbnail> thumbnails; // one per audio clip
        std::unique_ptr<juce::FileChooser> fileChooser;

        int draggingPoint { -1 };

        // Clip editing state.
        models::Clip* dragClip { nullptr };
        ClipDragMode dragMode { ClipDragMode::none };
        double dragOrigStart { 0.0 };
        double dragOrigLength { 0.0 };
        double dragOrigSourceOffset { 0.0 };
        int    dragStartX { 0 };
        bool   didDrag { false };
        ClipDragMode hoverMode { ClipDragMode::none };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLaneComponent)
    };
} // namespace freequency::ui
