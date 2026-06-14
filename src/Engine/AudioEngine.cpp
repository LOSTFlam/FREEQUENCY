#include "Engine/AudioEngine.h"

#include "Models/AudioTrack.h"
#include "Models/MidiTrack.h"

namespace omnidaw::engine
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
        // 2 inputs (for Phase 5 recording) / 2 outputs.
        const auto error = deviceManager.initialiseWithDefaultDevices (2, 2);

        if (error.isNotEmpty())
        {
            DBG ("OmniDAW AudioEngine: audio device init failed: " << error);
            return error;
        }

        deviceManager.addAudioCallback (this);
        running = true;
        lastReportedSamples = 0;
        startTimer (250);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            juce::ignoreUnused (device);
            DBG ("OmniDAW AudioEngine started on '" << device->getName()
                 << "' @ " << device->getCurrentSampleRate() << " Hz, buffer "
                 << device->getCurrentBufferSizeSamples() << " samples");
        }

        return {};
    }

    void AudioEngine::shutdown()
    {
        if (! running && ! deviceManager.getCurrentAudioDevice())
            return;

        stopTimer();
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

        connectStereo (masterNode->nodeID, audioOutputNode->nodeID);
    }

    void AudioEngine::buildBuses()
    {
        if (currentProject == nullptr)
            return;

        // Each submix/FX bus becomes a TrackProcessor strip feeding the master.
        // (The master itself is created in buildBaseGraph.)
        auto& mixer = currentProject->getMixer();

        for (int i = 0; i < mixer.getNumBuses(); ++i)
        {
            auto* bus = mixer.getBus (i);
            if (bus == nullptr || bus->getKind() == models::Bus::Kind::master)
                continue;

            auto busNode = graph.addNode (std::make_unique<TrackProcessor> (bus->name));
            connectStereo (busNode->nodeID, masterNode->nodeID);
            busNodes[bus->getId().toDashedString().toStdString()] = busNode->nodeID;
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
        // resolvable, otherwise it falls back to the built-in OmniSynth so the
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

                DBG ("OmniDAW: instrument load failed (" << error << "), using built-in synth");
            }
        }

        return graph.addNode (std::make_unique<SynthProcessor>())->nodeID;
    }

    void AudioEngine::addTrackChain (models::Track& track)
    {
        TrackChain chain;

        // Channel strip is common to every track type and sits at the chain tail.
        auto stripNode = graph.addNode (std::make_unique<TrackProcessor> (track.name));
        chain.strip = stripNode->nodeID;
        connectStereo (chain.strip, masterNode->nodeID);

        // Head of the audio portion of the chain: instrument output (MIDI) or
        // clip-player output (audio).
        NodeID lastAudioNode;

        if (track.getType() == models::TrackType::midi)
        {
            auto sourceNode = graph.addNode (std::make_unique<MidiSourceProcessor> (transport));
            chain.source = sourceNode->nodeID;

            chain.instrument = makeInstrumentNode (track);
            connectMidi (chain.source, chain.instrument);

            lastAudioNode = chain.instrument;
        }
        else
        {
            auto sourceNode = graph.addNode (std::make_unique<AudioClipProcessor> (transport));
            chain.source = sourceNode->nodeID;
            lastAudioNode = chain.source;
        }

        // Insert FX chain: a series of hosted effect plugins between the source/
        // instrument and the channel strip.
        for (const auto& identifier : track.insertPluginIdentifiers)
        {
            juce::String error;
            auto fx = pluginManager.createInstance (identifier, currentSampleRate, currentBlockSize, error);
            if (fx == nullptr)
            {
                DBG ("OmniDAW: insert FX load failed (" << error << ")");
                continue;
            }

            auto fxNode = graph.addNode (std::move (fx));
            connectStereo (lastAudioNode, fxNode->nodeID);
            lastAudioNode = fxNode->nodeID;
            chain.inserts.push_back (fxNode->nodeID);
        }

        connectStereo (lastAudioNode, chain.strip);

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
        }

        syncParametersFromModel();
        rebuildSequences();

        if (wasRunning)
            deviceManager.addAudioCallback (this);
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
                    auto* clip = dynamic_cast<models::MidiClip*> (midiTrack->getClip (c));
                    if (clip == nullptr)
                        continue;

                    const auto clipStartSamples = clip->startTime * sr;

                    for (int e = 0; e < clip->sequence.getNumEvents(); ++e)
                    {
                        auto m = clip->sequence.getEventPointer (e)->message;
                        m.setTimeStamp (clipStartSamples + m.getTimeStamp() * sr);
                        snapshot->events.addEvent (m);
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

                    auto buffer = loadFileResampled (formatManager, clip->sourceFile, sr);
                    if (buffer == nullptr)
                        continue;

                    AudioClipSnapshot::Region region;
                    region.bufferIndex          = snapshot->buffers.size();
                    region.timelineStartSample  = (juce::int64) (clip->startTime * sr);
                    region.sourceOffsetSamples  = (juce::int64) (clip->sourceOffset * sr);
                    region.gain                 = clip->gain;

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

    float AudioEngine::renderOfflinePeak (double sampleRate, double seconds, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        transport.prepare (sampleRate);
        graph.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        graph.prepareToPlay (sampleRate, blockSize);

        rebuildSequences();

        transport.setPositionSamples (0);
        transport.play();

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        const auto totalSamples = (juce::int64) std::llround (seconds * sampleRate);
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

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, n));

            done += n;
        }

        transport.stop();
        transport.setPositionSamples (0);
        graph.releaseResources();

        return peak;
    }

    // ── Audio device callback ───────────────────────────────────────────────────

    void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        const auto sr = device->getCurrentSampleRate();
        const auto blockSize = device->getCurrentBufferSizeSamples();

        currentSampleRate = sr;
        currentBlockSize = blockSize;
        transport.prepare (sr);

        // The graph is output-only (2 ch). Prepare it and a scratch buffer big
        // enough for the largest expected block so the callback never allocates.
        graph.setPlayConfigDetails (0, 2, sr, blockSize);
        graph.prepareToPlay (sr, blockSize);

        renderBuffer.setSize (2, juce::jmax (16, blockSize));
        scratchMidi.ensureSize (2048);

        // Sample-rate-dependent data (clip resampling, MIDI sample times) must be
        // rebuilt now that we know the device rate.
        rebuildSequences();
    }

    void AudioEngine::audioDeviceStopped()
    {
        graph.releaseResources();
    }

    void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*,
                                                        int,
                                                        float* const* outputChannelData,
                                                        int numOutputChannels,
                                                        int numSamples,
                                                        const juce::AudioIODeviceCallbackContext&)
    {
        juce::ScopedNoDenormals noDenormals;

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
        }

        const auto total = getMasterProcessedSampleCount();
        const auto delta = total - lastReportedSamples;

        if (delta > 0)
        {
            DBG ("OmniDAW AudioEngine: processed " << delta
                 << " samples (pos " << transport.getPositionSeconds() << "s)");
            lastReportedSamples = total;
        }
    }
} // namespace omnidaw::engine
