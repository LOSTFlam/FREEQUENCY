#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cmath>

namespace omnidaw::engine
{
    /**
        Transport — the shared, lock-free playback clock.

        A single Transport instance is owned by the AudioEngine and referenced by
        every node that needs to know "where are we?" (the MIDI sequencer source,
        audio-clip players, automation readers, the playhead UI).

        Threading model
        ---------------
        - The audio thread is the ONLY writer of `positionSamples`, advancing it
          once per processed block via advance().
        - The message thread may relocate the playhead (setPositionSeconds) and
          toggle play/stop/loop. These are rare, single-word atomic stores.
        - Everyone reads through relaxed atomics. There is never a lock, so the
          audio thread can never be blocked by the UI.

        Position is stored in samples (the engine's native unit). Seconds/beats
        conversions are derived from the current sample rate and tempo.
    */
    class Transport
    {
    public:
        Transport() = default;

        /** Called from the audio device thread when the device (re)starts. */
        void prepare (double newSampleRate) noexcept
        {
            sampleRate.store (newSampleRate, std::memory_order_relaxed);
        }

        [[nodiscard]] double getSampleRate() const noexcept
        {
            return sampleRate.load (std::memory_order_relaxed);
        }

        // ── Play state ────────────────────────────────────────────────────────
        void play()  noexcept { playing.store (true,  std::memory_order_relaxed); }
        void stop()  noexcept { playing.store (false, std::memory_order_relaxed); }
        void togglePlay() noexcept { playing.store (! isPlaying(), std::memory_order_relaxed); }
        [[nodiscard]] bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }

        // ── Position ──────────────────────────────────────────────────────────
        void setPositionSamples (juce::int64 s) noexcept
        {
            positionSamples.store (juce::jmax ((juce::int64) 0, s), std::memory_order_relaxed);
        }

        [[nodiscard]] juce::int64 getPositionSamples() const noexcept
        {
            return positionSamples.load (std::memory_order_relaxed);
        }

        void setPositionSeconds (double t) noexcept
        {
            const auto sr = getSampleRate();
            setPositionSamples (sr > 0.0 ? (juce::int64) std::llround (t * sr) : 0);
        }

        [[nodiscard]] double getPositionSeconds() const noexcept
        {
            const auto sr = getSampleRate();
            return sr > 0.0 ? (double) getPositionSamples() / sr : 0.0;
        }

        // ── Tempo ─────────────────────────────────────────────────────────────
        void setTempo (double bpm) noexcept { tempoBpm.store (juce::jmax (1.0, bpm), std::memory_order_relaxed); }
        [[nodiscard]] double getTempo() const noexcept { return tempoBpm.load (std::memory_order_relaxed); }

        [[nodiscard]] double getPositionBeats() const noexcept
        {
            return getPositionSeconds() * (getTempo() / 60.0);
        }

        // ── Looping ───────────────────────────────────────────────────────────
        void setLooping (bool shouldLoop) noexcept { looping.store (shouldLoop, std::memory_order_relaxed); }
        [[nodiscard]] bool isLooping() const noexcept { return looping.load (std::memory_order_relaxed); }

        void setLoopRangeSeconds (double startSec, double endSec) noexcept
        {
            const auto sr = getSampleRate();
            loopStart.store (sr > 0.0 ? (juce::int64) std::llround (startSec * sr) : 0, std::memory_order_relaxed);
            loopEnd.store   (sr > 0.0 ? (juce::int64) std::llround (endSec   * sr) : 0, std::memory_order_relaxed);
        }

        [[nodiscard]] juce::int64 getLoopStartSamples() const noexcept { return loopStart.load (std::memory_order_relaxed); }
        [[nodiscard]] juce::int64 getLoopEndSamples()   const noexcept { return loopEnd.load   (std::memory_order_relaxed); }

        /** Advance the playhead by a processed block. Audio thread only.

            Returns true if a loop wrap occurred within this block, so callers
            (e.g. the sequencer) can flush hanging notes if they care.
        */
        bool advance (int numSamples) noexcept
        {
            if (! isPlaying())
                return false;

            auto pos = positionSamples.load (std::memory_order_relaxed) + numSamples;
            bool wrapped = false;

            if (isLooping())
            {
                const auto s = loopStart.load (std::memory_order_relaxed);
                const auto e = loopEnd.load (std::memory_order_relaxed);

                if (e > s && pos >= e)
                {
                    pos = s + ((pos - s) % (e - s));
                    wrapped = true;
                }
            }

            positionSamples.store (pos, std::memory_order_relaxed);
            return wrapped;
        }

    private:
        std::atomic<double>      sampleRate     { 44100.0 };
        std::atomic<double>      tempoBpm       { 120.0 };
        std::atomic<bool>        playing        { false };
        std::atomic<bool>        looping        { false };
        std::atomic<juce::int64> positionSamples { 0 };
        std::atomic<juce::int64> loopStart      { 0 };
        std::atomic<juce::int64> loopEnd        { 0 };
    };
} // namespace omnidaw::engine
