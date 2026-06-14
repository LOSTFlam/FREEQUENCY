#include "Engine/AudioEngine.h"

#include "Models/AudioTrack.h"
#include "Models/MidiTrack.h"
#include "Models/PatternExpander.h"
#include "Engine/CompSwipeMixer.h"
#include "DSP/BuiltinEffects.h"
#include "DSP/TimeStretch.h"

namespace freequency::engine
{
    using AudioGraphIOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;

    namespace
    {
        /** Load an audio file into memory, resampled to `targetSampleRate`.

            Done on the message thread (during snapshot building) so the audio
            thread only ever mixes from ready-to-use buffers. Returns nullptr if
            the file can't be read.
        */
        std::unique_ptr<juce::AudioBuffer<float>> loadFileResampled (juce::AudioFormatManager& fm,
                                                                     const juce::File& file,
                                                                     double targetSampleRate)
        {
            if (! file.existsAsFile())
                return {};

            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples <= 0)
                return {};

            const int numChannels = (int) juce::jmax ((juce::uint32) 1, reader->numChannels);
            const auto srcLength   = (int) reader->lengthInSamples;

            juce::AudioBuffer<float> source (numChannels, srcLength);
            reader->read (&source, 0, srcLength, 0, true, true);

            const double srcRate = reader->sampleRate > 0 ? reader->sampleRate : targetSampleRate;

            if (std::abs (srcRate - targetSampleRate) < 1.0)
                return std::make_unique<juce::AudioBuffer<float>> (std::move (source));

            // Resample each channel to the engine rate with a Lagrange interpolator.
            const double ratio = srcRate / targetSampleRate;
            const int dstLength = (int) std::ceil ((double) srcLength / ratio);

            auto dst = std::make_unique<juce::AudioBuffer<float>> (numChannels, dstLength);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.process (ratio,
                                source.getReadPointer (ch),
                                dst->getWritePointer (ch),
                                dstLength);
            }

            return dst;
        }

    } // namespace

    AudioEngine::AudioEngine()
    {
        formatManager.registerBasicFormats();
        buildBaseGraph();
    }

    AudioEngine::~AudioEngine()
    {
        shutdown();
    }

    juce::String AudioEngine::initialise()
    {
        // Restore the user's saved audio device / sample-rate / buffer choice if
        // present, otherwise open sensible defaults (2 in / 2 out).
        const auto settingsFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                      .getChildFile ("FREEQUENCY").getChildFile ("audio.xml");
        std::unique_ptr<juce::XmlElement> saved;
        if (settingsFile.existsAsFile())
            saved = juce::XmlDocument::parse (settingsFile);

        const auto error = deviceManager.initialise (2, 2, saved.get(), true);

        if (error.isNotEmpty())
        {
            DBG ("FREEQUENCY AudioEngine: audio device init failed: " << error);
            return error;
        }

        deviceManager.addAudioCallback (this);

        // Enable every available hardware MIDI input and route it here for live
        // monitoring + recording.
        for (const auto& d : juce::MidiInput::getAvailableDevices())
        {
            deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            deviceManager.addMidiInputDeviceCallback (d.identifier, this);
            enabledMidiInputs.add (d.identifier);
        }

        running = true;
        lastReportedSamples = 0;
        startTimer (250);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            juce::ignoreUnused (device);
            DBG ("FREEQUENCY AudioEngine started on '" << device->getName()
                 << "' @ " << device->getCurrentSampleRate() << " Hz, buffer "
                 << device->getCurrentBufferSizeSamples() << " samples");
        }

        return {};
    }

    void AudioEngine::shutdown()
    {
        stopRecording();

        if (recordThread.isThreadRunning())
            recordThread.stopThread (2000);

        // Persist the current audio device configuration for next launch.
        if (auto state = deviceManager.createStateXml())
        {
            const auto settingsFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                          .getChildFile ("FREEQUENCY").getChildFile ("audio.xml");
            settingsFile.getParentDirectory().createDirectory();
            state->writeTo (settingsFile);
        }

        if (! running && ! deviceManager.getCurrentAudioDevice())
            return;

        stopTimer();
        for (const auto& id : enabledMidiInputs)
            deviceManager.removeMidiInputDeviceCallback (id, this);
        enabledMidiInputs.clear();
        deviceManager.removeAudioCallback (this);
        deviceManager.closeAudioDevice();
        running = false;
    }

    // ── Graph construction ──────────────────────────────────────────────────────

    void AudioEngine::buildBaseGraph()
    {
        graph.clear();
        trackChains.clear();
        busNodes.clear();
        busInsertNodes.clear();

        // CRITICAL: establish the graph's IO channel count *before* adding and
        // connecting the IO node. An AudioGraphIOProcessor derives its channel
        // count from the graph's play config; if we connect to it while the graph
        // still reports 0 output channels, addConnection silently fails and the
        // master never reaches the soundcard. We seed a sensible default here and
        // refine the rate/block size when the device starts (or an offline render
        // begins).
        graph.setPlayConfigDetails (0, 2, currentSampleRate, currentBlockSize);

        audioOutputNode = graph.addNode (
            std::make_unique<AudioGraphIOProcessor> (AudioGraphIOProcessor::audioOutputNode));

        masterNode = graph.addNode (std::make_unique<TrackProcessor> ("Master"));

        // Master bus -> brick-wall limiter -> output. The limiter is a safety net
        // that keeps the final mix from clipping the converter.
        limiterNode = graph.addNode (std::make_unique<dsp::LimiterProcessor>());
        connectStereo (masterNode->nodeID, limiterNode->nodeID);
        connectStereo (limiterNode->nodeID, audioOutputNode->nodeID);

        // Metronome click is summed in after the master fader (into the limiter),
        // so it's always audible but still protected by the limiter.
        metronomeNode = graph.addNode (std::make_unique<MetronomeProcessor> (transport));
        connectStereo (metronomeNode->nodeID, limiterNode->nodeID);

        // Media-browser preview also sums into the limiter path.
        previewNode = graph.addNode (std::make_unique<PreviewProcessor>());
        connectStereo (previewNode->nodeID, limiterNode->nodeID);
    }

    std::unique_ptr<juce::AudioProcessor> AudioEngine::createInsertProcessor (const juce::String& identifier)
    {
        if (dsp::BuiltinEffects::isBuiltin (identifier))
            return dsp::BuiltinEffects::create (identifier);

        juce::String error;
        auto fx = pluginManager.createInstance (identifier, currentSampleRate, currentBlockSize, error);
        if (fx == nullptr)
        {
            DBG ("FREEQUENCY: insert FX load failed (" << error << ")");
        }
        return fx;
    }

    void AudioEngine::buildBuses()
    {
        if (currentProject == nullptr)
            return;

        busInsertNodes.clear();

        // Each submix/FX bus becomes a TrackProcessor strip; tracks connect to the
        // strip, then the strip runs through the bus insert FX chain to master.
        auto& mixer = currentProject->getMixer();

        for (int i = 0; i < mixer.getNumBuses(); ++i)
        {
            auto* bus = mixer.getBus (i);
            if (bus == nullptr || bus->getKind() == models::Bus::Kind::master)
                continue;

            const auto busId = bus->getId().toDashedString().toStdString();
            auto busNode = graph.addNode (std::make_unique<TrackProcessor> (bus->name));
            busNodes[busId] = busNode->nodeID;

            NodeID last = busNode->nodeID;
            for (const auto& identifier : bus->insertPluginIdentifiers)
            {
                auto fx = createInsertProcessor (identifier);
                if (fx == nullptr)
                    continue;
                auto fxNode = graph.addNode (std::move (fx));
                connectStereo (last, fxNode->nodeID);
                last = fxNode->nodeID;
                busInsertNodes[busId].push_back (fxNode->nodeID);
            }

            connectStereo (last, masterNode->nodeID);
        }
    }

    void AudioEngine::connectStereo (NodeID source, NodeID destination)
    {
        graph.addConnection ({ { source, 0 }, { destination, 0 } });
        graph.addConnection ({ { source, 1 }, { destination, 1 } });
    }

    void AudioEngine::connectMidi (NodeID source, NodeID destination)
    {
        graph.addConnection ({ { source, Graph::midiChannelIndex },
                               { destination, Graph::midiChannelIndex } });
    }

    AudioEngine::NodeID AudioEngine::makeInstrumentNode (models::Track& track)
    {
        // A MIDI track plays a hosted VST3/AU instrument if one is assigned and
        // resolvable, otherwise it falls back to the built-in FreequencySynth so the
        // track is always audible.
        if (auto* midiTrack = dynamic_cast<models::MidiTrack*> (&track))
        {
            if (midiTrack->instrumentPluginIdentifier.isNotEmpty())
            {
                juce::String error;
                auto instance = pluginManager.createInstance (midiTrack->instrumentPluginIdentifier,
                                                              currentSampleRate, currentBlockSize, error);
                if (instance != nullptr)
                    return graph.addNode (std::move (instance))->nodeID;

                DBG ("FREEQUENCY: instrument load failed (" << error << "), using built-in synth");
            }
        }

        return graph.addNode (std::make_unique<dsp::SynthProcessor>())->nodeID;
    }

    void AudioEngine::addTrackChain (models::Track& track)
    {
        TrackChain chain;

        // Channel strip is common to every track type and sits at the chain tail.
        auto stripNode = graph.addNode (std::make_unique<TrackProcessor> (track.name));
        chain.strip = stripNode->nodeID;

        // Route the strip to its assigned output bus (if any & resolvable), else
        // straight to master.
        NodeID destination = masterNode->nodeID;
        if (track.outputBusId.isNotEmpty())
        {
            const auto busIt = busNodes.find (track.outputBusId.toStdString());
            if (busIt != busNodes.end())
                destination = busIt->second;
        }
        connectStereo (chain.strip, destination);

        // Head of the audio portion of the chain: instrument output (MIDI) or
        // clip-player output (audio). Bus/VCA tracks have no clip source in this
        // phase (their summing/control routing is built in Phase 2), so they get
        // only a strip.
        NodeID lastAudioNode;
        bool hasSource = false;

        if (track.getType() == models::TrackType::midi)
        {
            auto sourceNode = graph.addNode (std::make_unique<MidiSourceProcessor> (transport));
            chain.source = sourceNode->nodeID;

            chain.instrument = makeInstrumentNode (track);
            connectMidi (chain.source, chain.instrument);

            lastAudioNode = chain.instrument;
            hasSource = true;
        }
        else if (track.getType() == models::TrackType::audio)
        {
            auto sourceNode = graph.addNode (std::make_unique<AudioClipProcessor> (transport));
            chain.source = sourceNode->nodeID;
            lastAudioNode = chain.source;
            hasSource = true;
        }

        if (hasSource)
        {
            // Insert FX chain: a series of hosted effect plugins between the
            // source/instrument and the channel strip.
            for (const auto& identifier : track.insertPluginIdentifiers)
            {
                auto fx = createInsertProcessor (identifier);
                if (fx == nullptr)
                    continue;

                auto fxNode = graph.addNode (std::move (fx));
                if (dynamic_cast<dsp::SidechainCompressor*> (fxNode->getProcessor()) != nullptr)
                    chain.sidechainNodes.push_back (fxNode->nodeID);
                connectStereo (lastAudioNode, fxNode->nodeID);
                lastAudioNode = fxNode->nodeID;
                chain.inserts.push_back (fxNode->nodeID);
            }

            connectStereo (lastAudioNode, chain.strip);
        }

        // Sends: post-fader taps from the strip to destination buses. Each send is
        // its own gain node (a TrackProcessor used purely as a gain stage) so its
        // level is independent of the channel fader.
        for (int s = 0; s < track.getNumSends(); ++s)
        {
            auto* send = track.getSend (s);
            if (send == nullptr)
                continue;

            const auto busIt = busNodes.find (send->destBusId.toStdString());
            if (busIt == busNodes.end())
            {
                chain.sends.push_back ({}); // keep index alignment with model
                continue;
            }

            auto sendNode = graph.addNode (std::make_unique<TrackProcessor> ("Send"));
            connectStereo (chain.strip, sendNode->nodeID);
            connectStereo (sendNode->nodeID, busIt->second);
            chain.sends.push_back (sendNode->nodeID);
        }

        trackChains[track.getId().toDashedString().toStdString()] = chain;
    }

    void AudioEngine::setProject (models::Project* project)
    {
        currentProject = project;
        rebuildGraph();
    }

    void AudioEngine::rebuildGraph()
    {
        // Suspend rendering for the structural edit by detaching the callback.
        const bool wasRunning = running;

        if (wasRunning)
            deviceManager.removeAudioCallback (this);

        buildBaseGraph();

        if (currentProject != nullptr)
        {
            buildBuses();

            auto& timeline = currentProject->getTimeline();

            for (int i = 0; i < timeline.getNumTracks(); ++i)
                if (auto* track = timeline.getTrack (i))
                    addTrackChain (*track);

            // Sidechain wiring (second pass: all strips now exist). Connect each
            // keyed track's source strip to its sidechain-comp inserts' SC bus.
            for (int i = 0; i < timeline.getNumTracks(); ++i)
            {
                auto* track = timeline.getTrack (i);
                if (track == nullptr || track->sidechainSourceId.isEmpty())
                    continue;

                const auto dstIt = trackChains.find (track->getId().toDashedString().toStdString());
                const auto srcIt = trackChains.find (track->sidechainSourceId.toStdString());
                if (dstIt == trackChains.end() || srcIt == trackChains.end())
                    continue;

                const auto srcStrip = srcIt->second.strip;
                for (const auto scNode : dstIt->second.sidechainNodes)
                {
                    graph.addConnection ({ { srcStrip, 0 }, { scNode, 2 } });
                    graph.addConnection ({ { srcStrip, 1 }, { scNode, 3 } });
                }
            }
        }

        syncParametersFromModel();
        rebuildSequences();
        rebuildAutomation();

        if (wasRunning)
            deviceManager.addAudioCallback (this);
    }

    void AudioEngine::rebuildAutomation()
    {
        if (currentProject == nullptr)
            return;

        const double sr = transport.getSampleRate() > 0 ? transport.getSampleRate() : 44100.0;
        auto& timeline = currentProject->getTimeline();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            const auto it = trackChains.find (track->getId().toDashedString().toStdString());
            if (it == trackChains.end())
                continue;

            auto* strip = getStripProcessor (it->second.strip);
            if (strip == nullptr)
                continue;

            strip->setTransport (&transport);
            strip->setAutomationEnabled (track->volumeAutomationEnabled);

            if (track->volumeAutomationEnabled && ! track->volumeAutomation.isEmpty())
            {
                auto snap = new AutomationSnapshot();
                snap->nodes.reserve ((size_t) track->volumeAutomation.getNumPoints());

                for (int p = 0; p < track->volumeAutomation.getNumPoints(); ++p)
                {
                    const auto& pt = track->volumeAutomation.getPoint (p);
                    snap->nodes.push_back ({ (juce::int64) (pt.time * sr), pt.value });
                }

                strip->setAutomationSnapshot (snap);
            }
        }
    }

    void AudioEngine::rebuildSequences()
    {
        if (currentProject == nullptr)
            return;

        const double sr = transport.getSampleRate() > 0 ? transport.getSampleRate() : 44100.0;
        auto& timeline = currentProject->getTimeline();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            const auto it = trackChains.find (track->getId().toDashedString().toStdString());
            if (it == trackChains.end())
                continue;

            if (auto* midiTrack = dynamic_cast<models::MidiTrack*> (track))
            {
                auto snapshot = new MidiSequenceSnapshot();

                for (int c = 0; c < midiTrack->getNumClips(); ++c)
                {
                    auto* clip = midiTrack->getClip (c);
                    if (clip == nullptr)
                        continue;

                    const auto clipStartSamples = (juce::int64) (clip->startTime * sr);
                    const double clipLenSec = clip->length > 0.0 ? clip->length : 4.0;
                    const double tempo = currentProject->getTimeline().getTempoBpm();

                    if (auto* midiClip = dynamic_cast<models::MidiClip*> (clip))
                    {
                        for (int e = 0; e < midiClip->sequence.getNumEvents(); ++e)
                        {
                            auto m = midiClip->sequence.getEventPointer (e)->message;
                            m.setTimeStamp (clipStartSamples + m.getTimeStamp() * sr);
                            snapshot->events.addEvent (m);
                        }
                    }
                    else if (auto* patClip = dynamic_cast<models::PatternClip*> (clip))
                    {
                        if (currentProject == nullptr)
                            continue;

                        auto* pattern = currentProject->findPattern (patClip->patternId);
                        if (pattern == nullptr)
                            continue;

                        juce::MidiMessageSequence expanded;
                        models::PatternExpander::expandToSequence (*pattern, expanded, clipLenSec, tempo);

                        for (int e = 0; e < expanded.getNumEvents(); ++e)
                        {
                            auto m = expanded.getEventPointer (e)->message;
                            m.setTimeStamp (clipStartSamples + m.getTimeStamp() * sr);
                            snapshot->events.addEvent (m);
                        }
                    }
                }

                snapshot->events.updateMatchedPairs();
                snapshot->events.sort();
                if (snapshot->events.getNumEvents() > 0)
                    snapshot->endSample = (juce::int64) snapshot->events.getEndTime();

                if (auto* src = getMidiSource (it->second.source))
                    src->setSnapshot (snapshot);
            }
            else if (auto* audioTrack = dynamic_cast<models::AudioTrack*> (track))
            {
                auto snapshot = new AudioClipSnapshot();

                for (int c = 0; c < audioTrack->getNumClips(); ++c)
                {
                    auto* clip = dynamic_cast<models::AudioClip*> (audioTrack->getClip (c));
                    if (clip == nullptr)
                        continue;

                    const bool compMix = clip->getNumTakes() >= 2 && ! clip->compSwipeRegions.empty();

                    std::unique_ptr<juce::AudioBuffer<float>> buffer;

                    if (compMix)
                    {
                        std::vector<std::unique_ptr<juce::AudioBuffer<float>>> takeOwned;
                        std::vector<const juce::AudioBuffer<float>*> takePtrs;
                        takeOwned.reserve ((size_t) clip->getNumTakes());
                        takePtrs.reserve ((size_t) clip->getNumTakes());

                        for (int t = 0; t < clip->getNumTakes(); ++t)
                        {
                            auto takeBuf = loadFileResampled (formatManager,
                                                              juce::File (clip->takeFiles[t]),
                                                              sr);
                            if (takeBuf == nullptr)
                                continue;

                            takeBuf = dsp::TimeStretch::applyWarp (std::move (takeBuf),
                                                                   clip->stretchRatio,
                                                                   clip->pitchSemitones);

                            if (clip->reversed)
                                for (int ch = 0; ch < takeBuf->getNumChannels(); ++ch)
                                    takeBuf->reverse (ch, 0, takeBuf->getNumSamples());

                            takePtrs.push_back (takeBuf.get());
                            takeOwned.push_back (std::move (takeBuf));
                        }

                        if (! takePtrs.empty())
                        {
                            const auto srcOffset = (juce::int64) (clip->sourceOffset * sr);
                            buffer = CompSwipeMixer::mixTakes (*clip, takePtrs, sr, srcOffset);
                        }
                    }
                    else
                    {
                        buffer = loadFileResampled (formatManager, clip->sourceFile, sr);
                        if (buffer == nullptr)
                            continue;

                        buffer = dsp::TimeStretch::applyWarp (std::move (buffer),
                                                              clip->stretchRatio,
                                                              clip->pitchSemitones);

                        if (clip->reversed)
                            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
                                buffer->reverse (ch, 0, buffer->getNumSamples());
                    }

                    if (buffer == nullptr)
                        continue;

                    AudioClipSnapshot::Region region;
                    region.bufferIndex          = snapshot->buffers.size();
                    region.timelineStartSample  = (juce::int64) (clip->startTime * sr);
                    region.sourceOffsetSamples  = (juce::int64) (clip->sourceOffset * sr);
                    region.gain                 = compMix ? 1.0f : clip->gain;

                    const auto available = (juce::int64) buffer->getNumSamples() - region.sourceOffsetSamples;
                    region.lengthSamples = clip->length > 0.0
                                               ? (juce::int64) (clip->length * sr)
                                               : juce::jmax ((juce::int64) 0, available);
                    region.lengthSamples = juce::jmin (region.lengthSamples, juce::jmax ((juce::int64) 0, available));

                    snapshot->buffers.add (buffer.release());
                    snapshot->regions.push_back (region);
                }

                if (auto* src = getAudioSource (it->second.source))
                    src->setSnapshot (snapshot);
            }
        }
    }

    void AudioEngine::syncParametersFromModel()
    {
        if (currentProject == nullptr)
            return;

        auto& timeline = currentProject->getTimeline();

        // Solo is exclusive: if ANY track is soloed, every non-soloed track is
        // silenced. Computed once here so the audio thread just sees a mute flag.
        bool anySolo = false;
        for (int i = 0; i < timeline.getNumTracks(); ++i)
            if (auto* track = timeline.getTrack (i))
                anySolo = anySolo || track->isSoloed();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            const auto it = trackChains.find (track->getId().toDashedString().toStdString());
            if (it == trackChains.end())
                continue;

            if (auto* strip = getStripProcessor (it->second.strip))
            {
                const bool silencedBySolo = anySolo && ! track->isSoloed();
                strip->setGain (track->getVolume());
                strip->setPan (track->getPan());
                strip->setMuted (track->isMuted() || silencedBySolo);
            }

            // Send-tap levels (aligned by index with the model's sends).
            const auto& chain = it->second;
            for (int s = 0; s < track->getNumSends() && s < (int) chain.sends.size(); ++s)
                if (auto* send = track->getSend (s))
                    if (auto* sendProc = getStripProcessor (chain.sends[(size_t) s]))
                        sendProc->setGain (send->level.load (std::memory_order_relaxed));
        }

        // Bus + master volumes.
        auto& mixer = currentProject->getMixer();
        for (int i = 0; i < mixer.getNumBuses(); ++i)
        {
            auto* bus = mixer.getBus (i);
            if (bus == nullptr || bus->getKind() == models::Bus::Kind::master)
                continue;

            const auto busIt = busNodes.find (bus->getId().toDashedString().toStdString());
            if (busIt != busNodes.end())
                if (auto* busProc = getStripProcessor (busIt->second))
                    busProc->setGain (bus->getVolume());
        }

        if (auto* master = getStripProcessor (masterNode->nodeID))
            master->setGain (mixer.getMasterBus().getVolume());

        // Re-apply global processor state (these nodes are recreated on rebuild).
        if (metronomeNode != nullptr)
            if (auto* node = graph.getNodeForId (metronomeNode->nodeID))
                if (auto* m = dynamic_cast<MetronomeProcessor*> (node->getProcessor()))
                {
                    m->setEnabled (metronomeOn);
                    m->setBeatsPerBar (currentProject->getTimeline().getTimeSigNumerator());
                }

        if (limiterNode != nullptr)
            if (auto* node = graph.getNodeForId (limiterNode->nodeID))
                if (auto* l = dynamic_cast<dsp::LimiterProcessor*> (node->getProcessor()))
                    l->setEnabled (limiterOn);
    }

    void AudioEngine::previewFile (const juce::File& file)
    {
        const double sr = transport.getSampleRate() > 0 ? transport.getSampleRate() : 44100.0;
        auto buffer = loadFileResampled (formatManager, file, sr);
        if (buffer == nullptr)
            return;

        auto clip = new PreviewClip();
        clip->buffer = std::move (*buffer);

        if (auto* node = graph.getNodeForId (previewNode->nodeID))
            if (auto* p = dynamic_cast<PreviewProcessor*> (node->getProcessor()))
                p->play (clip);
    }

    void AudioEngine::stopPreview()
    {
        if (previewNode != nullptr)
            if (auto* node = graph.getNodeForId (previewNode->nodeID))
                if (auto* p = dynamic_cast<PreviewProcessor*> (node->getProcessor()))
                    p->stop();
    }

    void AudioEngine::setMetronomeEnabled (bool e)
    {
        metronomeOn = e;
        if (auto* node = graph.getNodeForId (metronomeNode->nodeID))
            if (auto* m = dynamic_cast<MetronomeProcessor*> (node->getProcessor()))
                m->setEnabled (e);
    }

    bool AudioEngine::isMetronomeEnabled() const noexcept
    {
        if (metronomeNode != nullptr)
            if (auto* node = graph.getNodeForId (metronomeNode->nodeID))
                if (auto* m = dynamic_cast<MetronomeProcessor*> (node->getProcessor()))
                    return m->isEnabled();
        return false;
    }

    void AudioEngine::setLimiterEnabled (bool e)
    {
        limiterOn = e;
        if (auto* node = graph.getNodeForId (limiterNode->nodeID))
            if (auto* l = dynamic_cast<dsp::LimiterProcessor*> (node->getProcessor()))
                l->setEnabled (e);
    }

    bool AudioEngine::isLimiterEnabled() const noexcept
    {
        if (limiterNode != nullptr)
            if (auto* node = graph.getNodeForId (limiterNode->nodeID))
                if (auto* l = dynamic_cast<dsp::LimiterProcessor*> (node->getProcessor()))
                    return l->isEnabled();
        return false;
    }

    float AudioEngine::getTrackLevel (const models::Track& track) const noexcept
    {
        const auto it = trackChains.find (track.getId().toDashedString().toStdString());
        if (it == trackChains.end())
            return 0.0f;

        if (auto* strip = getStripProcessor (it->second.strip))
            return strip->getOutputLevel();

        return 0.0f;
    }

    void AudioEngine::sendLiveNote (const models::Track& track, int noteNumber, float velocity, bool noteOn) noexcept
    {
        const auto it = trackChains.find (track.getId().toDashedString().toStdString());
        if (it == trackChains.end())
            return;

        if (auto* src = getMidiSource (it->second.source))
        {
            const auto m = noteOn ? juce::MidiMessage::noteOn (1, noteNumber, velocity)
                                  : juce::MidiMessage::noteOff (1, noteNumber);
            src->pushLiveMessage (m); // live monitoring

            // Capture into the recording buffer if armed & rolling.
            if (midiRecording.load (std::memory_order_relaxed) && transport.isPlaying())
            {
                auto rec = m;
                rec.setTimeStamp (transport.getPositionSeconds());
                const juce::ScopedLock sl (midiRecLock);
                recordedMidi.addEvent (rec);
            }
        }
    }

    void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
    {
        if (liveTargetTrack == nullptr)
            return;

        if (message.isNoteOnOrOff())
            sendLiveNote (*liveTargetTrack, message.getNoteNumber(),
                          message.getFloatVelocity(), message.isNoteOn());
    }

    void AudioEngine::startMidiRecording() noexcept
    {
        {
            const juce::ScopedLock sl (midiRecLock);
            recordedMidi.clear();
        }
        midiRecStartSeconds = transport.getPositionSeconds();
        midiRecording.store (true, std::memory_order_relaxed);
    }

    void AudioEngine::stopMidiRecording()
    {
        midiRecording.store (false, std::memory_order_relaxed);

        const juce::ScopedLock sl (midiRecLock);
        if (recordedMidi.getNumEvents() == 0)
            return;

        auto* midiTrack = dynamic_cast<models::MidiTrack*> (liveTargetTrack);
        if (midiTrack == nullptr)
            return;

        auto* clip = midiTrack->addClip();
        clip->name = "Recorded";
        clip->startTime = midiRecStartSeconds;

        double maxEnd = 0.0;
        for (int i = 0; i < recordedMidi.getNumEvents(); ++i)
        {
            auto m = recordedMidi.getEventPointer (i)->message;
            const double t = juce::jmax (0.0, m.getTimeStamp() - midiRecStartSeconds);
            m.setTimeStamp (t);
            clip->sequence.addEvent (m);
            maxEnd = juce::jmax (maxEnd, t);
        }
        clip->sequence.updateMatchedPairs();
        clip->length = maxEnd + 0.5;

        rebuildSequences();
    }

    juce::AudioProcessor* AudioEngine::getInsertProcessor (const models::Track& track, int slot) const noexcept
    {
        const auto it = trackChains.find (track.getId().toDashedString().toStdString());
        if (it == trackChains.end())
            return nullptr;

        const auto& inserts = it->second.inserts;
        if (slot < 0 || slot >= (int) inserts.size())
            return nullptr;

        if (auto* node = graph.getNodeForId (inserts[(size_t) slot]))
            return node->getProcessor();

        return nullptr;
    }

    juce::AudioProcessor* AudioEngine::getBusInsertProcessor (const models::Bus& bus, int slot) const noexcept
    {
        const auto it = busInsertNodes.find (bus.getId().toDashedString().toStdString());
        if (it == busInsertNodes.end() || slot < 0 || slot >= (int) it->second.size())
            return nullptr;

        if (auto* node = graph.getNodeForId (it->second[(size_t) slot]))
            return node->getProcessor();

        return nullptr;
    }

    float AudioEngine::getBusLevel (const models::Bus& bus) const noexcept
    {
        if (bus.getKind() == models::Bus::Kind::master)
            return getMasterLevel();

        const auto it = busNodes.find (bus.getId().toDashedString().toStdString());
        if (it == busNodes.end())
            return 0.0f;

        if (auto* strip = getStripProcessor (it->second))
            return strip->getOutputLevel();

        return 0.0f;
    }

    // ── Node lookup helpers ─────────────────────────────────────────────────────

    TrackProcessor* AudioEngine::getStripProcessor (NodeID nodeId) const noexcept
    {
        if (auto* node = graph.getNodeForId (nodeId))
            return dynamic_cast<TrackProcessor*> (node->getProcessor());
        return nullptr;
    }

    MidiSourceProcessor* AudioEngine::getMidiSource (NodeID nodeId) const noexcept
    {
        if (auto* node = graph.getNodeForId (nodeId))
            return dynamic_cast<MidiSourceProcessor*> (node->getProcessor());
        return nullptr;
    }

    AudioClipProcessor* AudioEngine::getAudioSource (NodeID nodeId) const noexcept
    {
        if (auto* node = graph.getNodeForId (nodeId))
            return dynamic_cast<AudioClipProcessor*> (node->getProcessor());
        return nullptr;
    }

    juce::int64 AudioEngine::getMasterProcessedSampleCount() const noexcept
    {
        if (auto* master = getStripProcessor (masterNode->nodeID))
            return master->getProcessedSampleCount();
        return 0;
    }

    float AudioEngine::renderOfflinePeak (double sampleRate, double seconds, int blockSize,
                                          double measureFromSeconds)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        transport.prepare (sampleRate);
        graph.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        graph.prepareToPlay (sampleRate, blockSize);

        rebuildSequences();
        rebuildAutomation();

        transport.setPositionSamples (0);
        transport.play();

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        const auto totalSamples = (juce::int64) std::llround (seconds * sampleRate);
        const auto measureFrom  = (juce::int64) std::llround (measureFromSeconds * sampleRate);
        juce::int64 done = 0;
        float peak = 0.0f;

        while (done < totalSamples)
        {
            const int n = (int) juce::jmin ((juce::int64) blockSize, totalSamples - done);
            buffer.setSize (2, n, false, false, true);
            buffer.clear();
            midi.clear();

            graph.processBlock (buffer, midi);
            transport.advance (n);

            // Skip the warm-up region so gain ramps don't pollute a steady-state
            // level measurement.
            if (done >= measureFrom)
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, n));

            done += n;
        }

        transport.stop();
        transport.setPositionSamples (0);
        graph.releaseResources();

        return peak;
    }

    // ── Disk recording ──────────────────────────────────────────────────────────

    bool AudioEngine::startRecording (const juce::File& file)
    {
        stopRecording();

        if (currentSampleRate <= 0.0)
            return false;

        file.getParentDirectory().createDirectory();
        file.deleteFile();

        std::unique_ptr<juce::OutputStream> fileStream = file.createOutputStream();

        if (fileStream != nullptr)
        {
            juce::WavAudioFormat wav;
            const int channels = juce::jmax (1, currentInputChannels);

            const auto options = juce::AudioFormatWriterOptions()
                                     .withSampleRate (currentSampleRate)
                                     .withNumChannels (channels)
                                     .withBitsPerSample (24);

            // On success this moves ownership of the stream into the writer.
            if (auto writer = wav.createWriterFor (fileStream, options))
            {
                threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (
                    writer.release(), recordThread, 32768));

                recordFile = file;
                recordStartSeconds = transport.getPositionSeconds();

                const juce::ScopedLock sl (writerLock);
                activeWriter.store (threadedWriter.get(), std::memory_order_relaxed);
                recording.store (true, std::memory_order_relaxed);
                return true;
            }
        }

        return false;
    }

    void AudioEngine::stopRecording()
    {
        {
            const juce::ScopedLock sl (writerLock);
            activeWriter.store (nullptr, std::memory_order_relaxed);
        }

        const bool wasRecording = recording.exchange (false, std::memory_order_relaxed);
        threadedWriter.reset(); // flushes remaining buffered audio to disk

        if (! wasRecording || currentProject == nullptr || ! recordFile.existsAsFile())
            return;

        // Punch-in: drop the captured file onto a record-armed audio track (or the
        // first audio track) as a clip at the position recording started.
        auto& timeline = currentProject->getTimeline();
        models::AudioTrack* target = nullptr;

        for (int i = 0; i < timeline.getNumTracks() && target == nullptr; ++i)
            if (auto* at = dynamic_cast<models::AudioTrack*> (timeline.getTrack (i)))
                if (at->recordEnabled)
                    target = at;

        if (target == nullptr)
            for (int i = 0; i < timeline.getNumTracks() && target == nullptr; ++i)
                target = dynamic_cast<models::AudioTrack*> (timeline.getTrack (i));

        if (target != nullptr)
        {
            auto* clip = target->addClip();
            clip->sourceFile = recordFile;
            clip->startTime = recordStartSeconds;
            clip->name = recordFile.getFileNameWithoutExtension();

            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                    formatManager.createReaderFor (recordFile)))
                if (reader->sampleRate > 0)
                    clip->length = (double) reader->lengthInSamples / reader->sampleRate;

            rebuildSequences();
        }

        if (onRecordingFinished)
            onRecordingFinished();
    }

    // ── Audio device callback ───────────────────────────────────────────────────

    void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        const auto sr = device->getCurrentSampleRate();
        const auto blockSize = device->getCurrentBufferSizeSamples();

        currentSampleRate = sr;
        currentBlockSize = blockSize;
        currentInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
        if (! recordThread.isThreadRunning())
            recordThread.startThread();
        transport.prepare (sr);

        // The graph is output-only (2 ch). Prepare it and a scratch buffer big
        // enough for the largest expected block so the callback never allocates.
        graph.setPlayConfigDetails (0, 2, sr, blockSize);
        graph.prepareToPlay (sr, blockSize);

        renderBuffer.setSize (2, juce::jmax (16, blockSize));
        scratchMidi.ensureSize (2048);

        // Sample-rate-dependent data (clip resampling, MIDI sample times,
        // automation breakpoints) must be rebuilt now that we know the device rate.
        rebuildSequences();
        rebuildAutomation();
    }

    void AudioEngine::audioDeviceStopped()
    {
        graph.releaseResources();
    }

    void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                        int numInputChannels,
                                                        float* const* outputChannelData,
                                                        int numOutputChannels,
                                                        int numSamples,
                                                        const juce::AudioIODeviceCallbackContext&)
    {
        juce::ScopedNoDenormals noDenormals;

        // Stream the live input to disk if recording. write() is non-blocking: the
        // ThreadedWriter buffers into a lock-free FIFO drained by recordThread.
        if (numInputChannels > 0 && transport.isPlaying())
        {
            const juce::ScopedLock sl (writerLock);
            if (auto* writer = activeWriter.load (std::memory_order_relaxed))
                writer->write (inputChannelData, numSamples);
        }

        // Render the whole graph into our stereo scratch buffer.
        renderBuffer.setSize (2, numSamples, false, false, true);
        renderBuffer.clear();
        scratchMidi.clear();

        graph.processBlock (renderBuffer, scratchMidi);

        // Advance the playhead AFTER processing so every node saw the same
        // block-start position.
        transport.advance (numSamples);

        // Copy to the device and compute a simple master peak for metering.
        float peak = 0.0f;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] == nullptr)
                continue;

            const int srcCh = juce::jmin (ch, renderBuffer.getNumChannels() - 1);
            const auto* src = renderBuffer.getReadPointer (srcCh);
            juce::FloatVectorOperations::copy (outputChannelData[ch], src, numSamples);
        }

        for (int ch = 0; ch < renderBuffer.getNumChannels(); ++ch)
            peak = juce::jmax (peak, renderBuffer.getMagnitude (ch, 0, numSamples));

        masterLevel.store (peak, std::memory_order_relaxed);

        // Feed the spectrum-analyzer ring with the mono mix.
        {
            const auto* l = renderBuffer.getReadPointer (0);
            const auto* r = renderBuffer.getNumChannels() > 1 ? renderBuffer.getReadPointer (1) : l;
            int w = scopeWrite.load (std::memory_order_relaxed);
            for (int i = 0; i < numSamples; ++i)
            {
                scopeRing[w] = 0.5f * (l[i] + r[i]);
                w = (w + 1) & (scopeSize - 1);
            }
            scopeWrite.store (w, std::memory_order_relaxed);
        }
    }

    void AudioEngine::readScope (float* dest, int numSamples) const noexcept
    {
        const int w = scopeWrite.load (std::memory_order_relaxed);
        const int count = juce::jmin (numSamples, scopeSize);
        for (int i = 0; i < count; ++i)
            dest[i] = scopeRing[(w - count + i + scopeSize) & (scopeSize - 1)];
    }

    // ── Heartbeat / housekeeping ────────────────────────────────────────────────

    void AudioEngine::timerCallback()
    {
        // Free retired snapshots on the message thread (never the audio thread).
        for (auto& [id, chain] : trackChains)
        {
            if (auto* src = getMidiSource (chain.source))
                src->collectGarbage();
            if (auto* src = getAudioSource (chain.source))
                src->collectGarbage();
            if (auto* strip = getStripProcessor (chain.strip))
                strip->collectGarbage();
        }

        if (previewNode != nullptr)
            if (auto* node = graph.getNodeForId (previewNode->nodeID))
                if (auto* p = dynamic_cast<PreviewProcessor*> (node->getProcessor()))
                    p->collectGarbage();

        const auto total = getMasterProcessedSampleCount();
        const auto delta = total - lastReportedSamples;

        if (delta > 0)
        {
            DBG ("FREEQUENCY AudioEngine: processed " << delta
                 << " samples (pos " << transport.getPositionSeconds() << "s)");
            lastReportedSamples = total;
        }
    }
} // namespace freequency::engine
