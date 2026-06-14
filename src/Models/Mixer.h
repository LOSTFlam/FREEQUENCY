#pragma once

#include "Core/Types.h"

#include <atomic>

namespace omnidaw::models
{
    /**
        Bus — a summing destination in the mixer.

        Tracks route their main output (and optional sends) to buses; buses route
        to other buses or, ultimately, to the master bus. This is the model side
        of the routing matrix; the engine mirrors it as TrackProcessor nodes and
        AudioProcessorGraph connections.
    */
    class Bus
    {
    public:
        enum class Kind
        {
            master,  // exactly one per project; the final output
            submix,  // a group/sum of tracks
            fx       // an aux/return bus fed by sends
        };

        explicit Bus (Kind busKind);

        [[nodiscard]] Kind getKind() const noexcept { return kind; }
        [[nodiscard]] const ObjectId& getId() const noexcept { return id; }

        juce::String name;
        juce::Colour colour { juce::Colours::darkgrey };

        [[nodiscard]] float getVolume() const noexcept { return volume.load (std::memory_order_relaxed); }
        void setVolume (float linearGain) noexcept { volume.store (juce::jmax (0.0f, linearGain), std::memory_order_relaxed); }

    private:
        const ObjectId id;
        const Kind kind;
        std::atomic<float> volume { 1.0f };
    };

    /**
        Mixer — owns the bus topology of the project.

        Every project has exactly one master bus, created on construction. Submix
        and FX buses are added on demand. The Mixer is pure data; the engine reads
        it to build the corresponding graph nodes and connections in Phase 4.
    */
    class Mixer
    {
    public:
        Mixer();

        [[nodiscard]] Bus& getMasterBus() noexcept { return *masterBus; }
        [[nodiscard]] const Bus& getMasterBus() const noexcept { return *masterBus; }

        Bus* addSubmixBus (const juce::String& name);
        Bus* addFxBus (const juce::String& name);

        /** Removes all submix/FX buses, keeping the master (for project load). */
        void clearNonMasterBuses();

        [[nodiscard]] int  getNumBuses() const noexcept { return buses.size(); }
        [[nodiscard]] Bus* getBus (int index) const noexcept { return buses[index]; }

    private:
        juce::OwnedArray<Bus> buses; // owns master + all submix/fx buses
        Bus* masterBus { nullptr };  // non-owning convenience pointer into `buses`
    };
} // namespace omnidaw::models
