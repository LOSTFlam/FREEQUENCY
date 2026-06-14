#include "Models/ProjectSerializer.h"

#include "Models/AudioTrack.h"
#include "Models/MidiTrack.h"
#include "Models/BusTrack.h"
#include "Models/VCATrack.h"

#include <unordered_map>

namespace freequency::models
{
    namespace ids
    {
        #define OMNI_ID(name) static const juce::Identifier name { #name };
        OMNI_ID (PROJECT) OMNI_ID (MIXER) OMNI_ID (TIMELINE)
        OMNI_ID (BUS) OMNI_ID (TRACK) OMNI_ID (CLIP) OMNI_ID (EVENT)
        OMNI_ID (SEND) OMNI_ID (INSERT) OMNI_ID (POINT)
        OMNI_ID (name) OMNI_ID (tempo) OMNI_ID (tsNum) OMNI_ID (tsDen)
        OMNI_ID (kind) OMNI_ID (id) OMNI_ID (colour) OMNI_ID (volume)
        OMNI_ID (type) OMNI_ID (pan) OMNI_ID (mute) OMNI_ID (solo)
        OMNI_ID (instrument) OMNI_ID (autoEnabled)
        OMNI_ID (start) OMNI_ID (length) OMNI_ID (file) OMNI_ID (offset) OMNI_ID (gain)
        OMNI_ID (time) OMNI_ID (value) OMNI_ID (data) OMNI_ID (bus) OMNI_ID (level)
        OMNI_ID (refs) OMNI_ID (scsrc) OMNI_ID (stretch) OMNI_ID (pitch)
        #undef OMNI_ID
    }

    namespace
    {
        juce::String hexFromMidi (const juce::MidiMessage& m)
        {
            return juce::String::toHexString (m.getRawData(), m.getRawDataSize(), 0);
        }

        juce::MidiMessage midiFromHex (const juce::String& hex, double timeStamp)
        {
            juce::MemoryBlock mb;
            mb.loadFromHexString (hex);
            if (mb.getSize() == 0)
                return {};
            return juce::MidiMessage (mb.getData(), (int) mb.getSize(), timeStamp);
        }
    } // namespace

    juce::ValueTree ProjectSerializer::toValueTree (const Project& project)
    {
        using namespace ids;

        juce::ValueTree root (PROJECT);
        root.setProperty (name, project.name, nullptr);

        const auto& timeline = project.getTimeline();
        root.setProperty (tempo, timeline.getTempoBpm(), nullptr);
        root.setProperty (tsNum, timeline.getTimeSigNumerator(), nullptr);
        root.setProperty (tsDen, timeline.getTimeSigDenominator(), nullptr);

        // Mixer / buses.
        juce::ValueTree mixerTree (MIXER);
        const auto& mixer = project.getMixer();
        for (int i = 0; i < mixer.getNumBuses(); ++i)
        {
            auto* b = mixer.getBus (i);
            if (b == nullptr)
                continue;

            juce::ValueTree busTree (BUS);
            busTree.setProperty (kind, (int) b->getKind(), nullptr);
            busTree.setProperty (id, b->getId().toDashedString(), nullptr);
            busTree.setProperty (name, b->name, nullptr);
            busTree.setProperty (colour, b->colour.toString(), nullptr);
            busTree.setProperty (volume, b->getVolume(), nullptr);
            mixerTree.appendChild (busTree, nullptr);
        }
        root.appendChild (mixerTree, nullptr);

        // Timeline / tracks.
        juce::ValueTree timelineTree (TIMELINE);
        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            const char* typeStr = track->getType() == TrackType::audio ? "audio"
                                : track->getType() == TrackType::midi  ? "midi"
                                : track->getType() == TrackType::bus   ? "bus" : "vca";

            juce::ValueTree trackTree (TRACK);
            trackTree.setProperty (type, typeStr, nullptr);
            trackTree.setProperty (id, track->getId().toDashedString(), nullptr);
            trackTree.setProperty (name, track->name, nullptr);
            trackTree.setProperty (colour, track->colour.toString(), nullptr);
            trackTree.setProperty (bus, track->outputBusId, nullptr); // output routing
            trackTree.setProperty (scsrc, track->sidechainSourceId, nullptr);
            trackTree.setProperty (volume, track->getVolume(), nullptr);
            trackTree.setProperty (pan, track->getPan(), nullptr);
            trackTree.setProperty (mute, track->isMuted(), nullptr);
            trackTree.setProperty (solo, track->isSoloed(), nullptr);
            trackTree.setProperty (autoEnabled, track->volumeAutomationEnabled, nullptr);

            if (auto* midiTrack = dynamic_cast<MidiTrack*> (track))
                trackTree.setProperty (instrument, midiTrack->instrumentPluginIdentifier, nullptr);

            if (auto* vcaTrack = dynamic_cast<VCATrack*> (track))
                trackTree.setProperty (refs, vcaTrack->getControlledTrackIds().joinIntoString (";"), nullptr);

            for (const auto& ins : track->insertPluginIdentifiers)
            {
                juce::ValueTree insTree (INSERT);
                insTree.setProperty (id, ins, nullptr);
                trackTree.appendChild (insTree, nullptr);
            }

            for (int s = 0; s < track->getNumSends(); ++s)
            {
                if (auto* send = track->getSend (s))
                {
                    juce::ValueTree sendTree (SEND);
                    sendTree.setProperty (bus, send->destBusId, nullptr);
                    sendTree.setProperty (level, send->level.load(), nullptr);
                    trackTree.appendChild (sendTree, nullptr);
                }
            }

            for (int p = 0; p < track->volumeAutomation.getNumPoints(); ++p)
            {
                const auto& pt = track->volumeAutomation.getPoint (p);
                juce::ValueTree ptTree (POINT);
                ptTree.setProperty (time, pt.time, nullptr);
                ptTree.setProperty (value, pt.value, nullptr);
                trackTree.appendChild (ptTree, nullptr);
            }

            for (int c = 0; c < track->getNumClips(); ++c)
            {
                auto* clip = track->getClip (c);
                if (clip == nullptr)
                    continue;

                juce::ValueTree clipTree (CLIP);
                clipTree.setProperty (name, clip->name, nullptr);
                clipTree.setProperty (colour, clip->colour.toString(), nullptr);
                clipTree.setProperty (start, clip->startTime, nullptr);
                clipTree.setProperty (length, clip->length, nullptr);

                if (auto* audioClip = dynamic_cast<AudioClip*> (clip))
                {
                    clipTree.setProperty (kind, "audio", nullptr);
                    clipTree.setProperty (file, audioClip->sourceFile.getFullPathName(), nullptr);
                    clipTree.setProperty (offset, audioClip->sourceOffset, nullptr);
                    clipTree.setProperty (gain, audioClip->gain, nullptr);
                    clipTree.setProperty (value, audioClip->reversed, nullptr); // reuse 'value' as reversed flag
                    clipTree.setProperty (stretch, audioClip->stretchRatio, nullptr);
                    clipTree.setProperty (pitch, audioClip->pitchSemitones, nullptr);
                }
                else if (auto* midiClip = dynamic_cast<MidiClip*> (clip))
                {
                    clipTree.setProperty (kind, "midi", nullptr);
                    for (int e = 0; e < midiClip->sequence.getNumEvents(); ++e)
                    {
                        auto& msg = midiClip->sequence.getEventPointer (e)->message;
                        juce::ValueTree evTree (EVENT);
                        evTree.setProperty (time, msg.getTimeStamp(), nullptr);
                        evTree.setProperty (data, hexFromMidi (msg), nullptr);
                        clipTree.appendChild (evTree, nullptr);
                    }
                }

                trackTree.appendChild (clipTree, nullptr);
            }

            timelineTree.appendChild (trackTree, nullptr);
        }
        root.appendChild (timelineTree, nullptr);

        return root;
    }

    void ProjectSerializer::fromValueTree (const juce::ValueTree& root, Project& project)
    {
        using namespace ids;

        if (! root.hasType (PROJECT))
            return;

        project.name = root.getProperty (name, "Untitled Project");

        auto& timeline = project.getTimeline();
        timeline.setTempoBpm (root.getProperty (tempo, 120.0));
        timeline.setTimeSignature (root.getProperty (tsNum, 4), root.getProperty (tsDen, 4));

        // Buses (remap old ids -> new ids so sends keep pointing at the right bus).
        auto& mixer = project.getMixer();
        mixer.clearNonMasterBuses();
        std::unordered_map<std::string, juce::String> busIdRemap;

        auto mixerTree = root.getChildWithName (MIXER);
        for (int i = 0; i < mixerTree.getNumChildren(); ++i)
        {
            auto busTree = mixerTree.getChild (i);
            const auto k = (Bus::Kind) (int) busTree.getProperty (kind, (int) Bus::Kind::fx);
            const auto oldId = busTree.getProperty (id).toString();
            const auto busName = busTree.getProperty (name, "Bus").toString();

            Bus* bus = nullptr;
            if (k == Bus::Kind::master)      bus = &mixer.getMasterBus();
            else if (k == Bus::Kind::submix) bus = mixer.addSubmixBus (busName);
            else                             bus = mixer.addFxBus (busName);

            if (bus != nullptr)
            {
                bus->name = busName;
                bus->colour = juce::Colour::fromString (busTree.getProperty (colour, "ff808080").toString());
                bus->setVolume ((float) (double) busTree.getProperty (volume, 1.0));
                busIdRemap[oldId.toStdString()] = bus->getId().toDashedString();
            }
        }

        // Tracks. Track ids are remapped (old -> new) so VCA references survive.
        timeline.clear();
        std::unordered_map<std::string, juce::String> trackIdRemap;
        auto timelineTree = root.getChildWithName (TIMELINE);
        for (int i = 0; i < timelineTree.getNumChildren(); ++i)
        {
            auto trackTree = timelineTree.getChild (i);
            const auto typeStr = trackTree.getProperty (type, "audio").toString();

            Track* track = typeStr == "midi" ? (Track*) timeline.addMidiTrack()
                         : typeStr == "bus"  ? (Track*) timeline.addBusTrack()
                         : typeStr == "vca"  ? (Track*) timeline.addVCATrack()
                                             : (Track*) timeline.addAudioTrack();
            if (track == nullptr)
                continue;

            trackIdRemap[trackTree.getProperty (id).toString().toStdString()] = track->getId().toDashedString();

            track->name = trackTree.getProperty (name, "Track").toString();
            track->colour = juce::Colour::fromString (trackTree.getProperty (colour, "ff808080").toString());
            track->outputBusId = trackTree.getProperty (bus, "").toString();
            {
                const auto remapped = busIdRemap.find (track->outputBusId.toStdString());
                if (remapped != busIdRemap.end())
                    track->outputBusId = remapped->second;
            }
            track->sidechainSourceId = trackTree.getProperty (scsrc, "").toString(); // remapped below
            track->setVolume ((float) (double) trackTree.getProperty (volume, 1.0));
            track->setPan ((float) (double) trackTree.getProperty (pan, 0.0));
            track->setMuted ((bool) trackTree.getProperty (mute, false));
            track->setSoloed ((bool) trackTree.getProperty (solo, false));
            track->volumeAutomationEnabled = (bool) trackTree.getProperty (autoEnabled, false);

            if (auto* midiTrack = dynamic_cast<MidiTrack*> (track))
                midiTrack->instrumentPluginIdentifier = trackTree.getProperty (instrument, "").toString();

            for (int c = 0; c < trackTree.getNumChildren(); ++c)
            {
                auto child = trackTree.getChild (c);

                if (child.hasType (INSERT))
                {
                    track->insertPluginIdentifiers.add (child.getProperty (id).toString());
                }
                else if (child.hasType (SEND))
                {
                    const auto oldBus = child.getProperty (bus).toString();
                    const auto remapped = busIdRemap.find (oldBus.toStdString());
                    if (remapped != busIdRemap.end())
                    {
                        auto* send = track->addSend (remapped->second);
                        send->level.store ((float) (double) child.getProperty (level, 0.5));
                    }
                }
                else if (child.hasType (POINT))
                {
                    track->volumeAutomation.addPoint (child.getProperty (time, 0.0),
                                                      (float) (double) child.getProperty (value, 1.0));
                }
                else if (child.hasType (CLIP))
                {
                    const bool clipIsAudio = child.getProperty (kind, "audio").toString() == "audio";

                    if (clipIsAudio)
                    {
                        if (auto* at = dynamic_cast<AudioTrack*> (track))
                        {
                            auto* clip = at->addClip();
                            clip->name = child.getProperty (name, "Clip").toString();
                            clip->startTime = child.getProperty (start, 0.0);
                            clip->length = child.getProperty (length, 0.0);
                            clip->sourceFile = juce::File (child.getProperty (file, "").toString());
                            clip->sourceOffset = child.getProperty (offset, 0.0);
                            clip->gain = (float) (double) child.getProperty (gain, 1.0);
                            clip->reversed = (bool) child.getProperty (value, false);
                            clip->stretchRatio = child.getProperty (stretch, 1.0);
                            clip->pitchSemitones = (int) child.getProperty (pitch, 0);
                        }
                    }
                    else
                    {
                        if (auto* mt = dynamic_cast<MidiTrack*> (track))
                        {
                            auto* clip = mt->addClip();
                            clip->name = child.getProperty (name, "Clip").toString();
                            clip->startTime = child.getProperty (start, 0.0);
                            clip->length = child.getProperty (length, 0.0);

                            for (int e = 0; e < child.getNumChildren(); ++e)
                            {
                                auto ev = child.getChild (e);
                                if (ev.hasType (EVENT))
                                    clip->sequence.addEvent (
                                        midiFromHex (ev.getProperty (data).toString(),
                                                     ev.getProperty (time, 0.0)));
                            }
                            clip->sequence.updateMatchedPairs();
                        }
                    }
                }
            }
        }

        // Second pass: remap track-id references (sidechain sources + VCA refs).
        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr || track->sidechainSourceId.isEmpty())
                continue;
            const auto it = trackIdRemap.find (track->sidechainSourceId.toStdString());
            track->sidechainSourceId = it != trackIdRemap.end() ? it->second : juce::String();
        }

        for (int i = 0; i < timelineTree.getNumChildren(); ++i)
        {
            auto trackTree = timelineTree.getChild (i);
            if (trackTree.getProperty (type, "").toString() != "vca")
                continue;

            auto* vca = dynamic_cast<VCATrack*> (timeline.getTrack (i));
            if (vca == nullptr)
                continue;

            const auto oldRefs = juce::StringArray::fromTokens (trackTree.getProperty (refs, "").toString(), ";", "");
            for (const auto& oldId : oldRefs)
            {
                const auto it = trackIdRemap.find (oldId.toStdString());
                if (it != trackIdRemap.end())
                    vca->addControlledTrack (it->second);
            }
        }
    }

    bool ProjectSerializer::saveToFile (const Project& project, const juce::File& f)
    {
        auto tree = toValueTree (project);
        if (auto xml = tree.createXml())
            return xml->writeTo (f);
        return false;
    }

    bool ProjectSerializer::loadFromFile (Project& project, const juce::File& f)
    {
        if (! f.existsAsFile())
            return false;

        if (auto xml = juce::XmlDocument::parse (f))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            if (tree.isValid())
            {
                fromValueTree (tree, project);
                return true;
            }
        }

        return false;
    }
} // namespace freequency::models
