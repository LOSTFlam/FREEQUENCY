#pragma once

#include "UI/UIContext.h"
#include "Models/Track.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace omnidaw::ui
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
        After any edit the engine's sequences are rebuilt so playback matches.
    */
    class TrackLaneComponent final : public juce::Component,
                                     private juce::ChangeListener
    {
    public:
        TrackLaneComponent (UIContext& ctx, models::Track& track, int index);
        ~TrackLaneComponent() override;

        void paint (juce::Graphics&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        /** Rebuild waveform thumbnails after the clip list changes. */
        void refreshClips();

        std::function<void()> onClipsChanged;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void drawMidiClip (juce::Graphics&, models::MidiClip&, juce::Rectangle<int> clipBounds);
        void drawAudioClip (juce::Graphics&, models::AudioClip&, int clipIndex, juce::Rectangle<int> clipBounds);

        UIContext& context;
        models::Track& trackRef;
        int laneIndex { 0 };

        juce::AudioThumbnailCache thumbnailCache { 16 };
        juce::OwnedArray<juce::AudioThumbnail> thumbnails; // one per audio clip
        std::unique_ptr<juce::FileChooser> fileChooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLaneComponent)
    };
} // namespace omnidaw::ui
