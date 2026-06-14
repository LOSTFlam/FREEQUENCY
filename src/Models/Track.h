#pragma once

#include "Core/Types.h"
#include "Models/Clip.h"
#include "Models/AutomationCurve.h"

#include <juce_data_structures/juce_data_structures.h>

#include <atomic>

namespace freequency::models
{
    enum class TrackType
    {
        audio,
        midi,
        bus,   // group / summing track (Cubase Group Channel)
        vca    // VCA control track (no audio; scales member faders)
    };

    /**
        Track — polymorphic base for every lane in the timeline.

        A Track owns:
          - identity & presentation (name, colour)
          - mixer state (volume, pan, mute, solo)
          - a list of Clips (concrete subclasses constrain the clip type)

        Why the mixer state lives here AND mirrored in the engine
        --------------------------------------------------------
        The model is the single source of truth edited on the message thread.
        The audio thread must never read these std::atomics through a model
        pointer that the user might be deleting; instead the engine's
        TrackProcessor keeps its own atomic mirror. We still store atomics here so
        that any non-audio reader (UI, automation writer, undo system) gets a
        consistent value without locking. The setters below are the funnel the UI
        uses; later phases route them through the engine so model and graph stay
        in sync thread-safely.

        This class is intentionally free of any juce::Component dependency — it is
        a model, observed by the view, consumed by the engine.
    */
    class Track : public juce::ChangeBroadcaster
    {
    public:
        explicit Track (TrackType trackType);
        ~Track() override = default;

        Track (const Track&) = delete;
        Track& operator= (const Track&) = delete;

        [[nodiscard]] TrackType getType() const noexcept { return type; }
        [[nodiscard]] const ObjectId& getId() const noexcept { return id; }

        /** A short label for the concrete track type, e.g. "Audio" / "MIDI". */
        [[nodiscard]] virtual juce::String getTypeName() const = 0;

        // ── Presentation ──────────────────────────────────────────────────────
        juce::String name;
        juce::Colour colour { juce::Colours::grey };

        /** Destination bus id (dashed UUID) for this track's main output; empty
            means route directly to the master bus. Lets a track be "thrown" onto
            any mixer bus/group. */
        juce::String outputBusId;

        // ── Mixer state (lock-free readable) ──────────────────────────────────
        // Stored as linear gain (1.0 == unity) and pan in [-1, +1].
        [[nodiscard]] float getVolume() const noexcept { return volume.load (std::memory_order_relaxed); }
        [[nodiscard]] float getPan()    const noexcept { return pan.load    (std::memory_order_relaxed); }
        [[nodiscard]] bool  isMuted()   const noexcept { return mute.load   (std::memory_order_relaxed); }
        [[nodiscard]] bool  isSoloed()  const noexcept { return solo.load   (std::memory_order_relaxed); }

        void setVolume (float linearGain) noexcept;
        void setPan    (float newPan) noexcept;     // clamped to [-1, +1]
        void setMuted  (bool shouldBeMuted) noexcept;
        void setSoloed (bool shouldBeSoloed) noexcept;

        // ── Insert FX chain ───────────────────────────────────────────────────
        // Ordered list of effect plugins applied in series, post-instrument /
        // post-clip and pre-fader. We store identifier strings (not live
        // pointers) so the model stays decoupled from the engine and is trivially
        // serialisable; the engine resolves them via the PluginManager.
        juce::StringArray insertPluginIdentifiers;

        // ── Sends (aux routing) ─────────────────────────────────────────────────
        /** A post-fader tap that copies this track's signal to a destination bus
            (typically an FX/aux bus) at an independent level. */
        struct Send
        {
            juce::String destBusId;          // Bus id (dashed string)
            std::atomic<float> level { 0.0f }; // linear gain of the send tap
        };

        [[nodiscard]] int getNumSends() const noexcept { return sends.size(); }
        [[nodiscard]] Send* getSend (int index) const noexcept { return sends[index]; }
        Send* addSend (const juce::String& destBusId);
        void removeSend (int index);

        // ── Volume automation ───────────────────────────────────────────────────
        // When enabled, this curve drives the channel-strip gain over time instead
        // of the static fader value. Edited on the message thread; the engine
        // publishes a sampled snapshot to the audio thread.
        AutomationCurve volumeAutomation;
        bool volumeAutomationEnabled { false };

        // ── Clips ─────────────────────────────────────────────────────────────
        [[nodiscard]] int getNumClips() const noexcept { return clips.size(); }
        [[nodiscard]] Clip* getClip (int index) const noexcept { return clips[index]; }
        void removeClip (Clip* clip);

    protected:
        /** Subclasses add clips through here so they can enforce the clip type. */
        Clip* addClipInternal (std::unique_ptr<Clip> clip);

        juce::OwnedArray<Clip> clips;
        juce::OwnedArray<Send> sends;

    private:
        const ObjectId id;
        const TrackType type;

        std::atomic<float> volume { 1.0f };
        std::atomic<float> pan    { 0.0f };
        std::atomic<bool>  mute   { false };
        std::atomic<bool>  solo   { false };
    };
} // namespace freequency::models
