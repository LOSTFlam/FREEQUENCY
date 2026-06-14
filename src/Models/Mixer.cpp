#include "Models/Mixer.h"

namespace omnidaw::models
{
    Bus::Bus (Kind busKind)
        : kind (busKind)
    {
    }

    Mixer::Mixer()
    {
        // A project is meaningless without somewhere for audio to end up, so the
        // master bus is created up front and kept for the Mixer's lifetime.
        auto* master = new Bus (Bus::Kind::master);
        master->name = "Master";
        master->colour = juce::Colours::white;
        buses.add (master);
        masterBus = master;
    }

    Bus* Mixer::addSubmixBus (const juce::String& name)
    {
        auto* bus = new Bus (Bus::Kind::submix);
        bus->name = name;
        buses.add (bus);
        return bus;
    }

    Bus* Mixer::addFxBus (const juce::String& name)
    {
        auto* bus = new Bus (Bus::Kind::fx);
        bus->name = name;
        buses.add (bus);
        return bus;
    }
} // namespace omnidaw::models
