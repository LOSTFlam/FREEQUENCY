#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

/*
    Core types shared across the whole application.

    Everything in FREEQUENCY lives under the `freequency` namespace. Layer-specific
    code is further namespaced (freequency::models, freequency::engine, freequency::ui)
    so that, for example, a model `Track` and an engine `TrackProcessor` never
    collide and the dependency direction stays obvious at the call site.
*/
namespace freequency
{
    /** Musical time / transport units used throughout the models.

        We deliberately keep two notions of time:
          - seconds       : wall-clock position, what the audio engine cares about.
          - beats (PPQ)    : musical position, what the pattern/sequencer cares about.
        Phase 1 only needs seconds; beats are introduced properly with the
        sequencer in a later phase.
    */
    using Seconds = double;
    using Beats   = double;
    using SampleIndex = juce::int64;

    /** A stable, unique identifier for any model object (track, clip, bus...).

        Using juce::Uuid means IDs survive serialisation and can be referenced
        across the model graph (e.g. an automation lane targeting a track) without
        relying on raw pointers or array indices that shift as the user edits.
    */
    using ObjectId = juce::Uuid;

    /** Convenience: a sensible default palette so freshly created tracks/clips
        don't all show up as the same colour in the UI later on.
    */
    namespace defaults
    {
        inline juce::Colour nextTrackColour (int index) noexcept
        {
            // Spread hues evenly around the wheel; deterministic per index.
            const auto hue = std::fmod (0.13f * static_cast<float> (index), 1.0f);
            return juce::Colour::fromHSV (hue, 0.55f, 0.85f, 1.0f);
        }
    } // namespace defaults
} // namespace freequency
