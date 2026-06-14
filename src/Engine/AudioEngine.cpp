#include "Engine/AudioEngine.h"

namespace omnidaw::engine
{
    using AudioGraphIOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;

    AudioEngine::AudioEngine()
    {
        // Build the minimal master->output skeleton immediately so the graph is
        // always in a valid, renderable state even before a project is loaded.
        buildBaseGraph();
    }

    AudioEngine::~AudioEngine()
    {
        shutdown();
    }

    juce::String AudioEngine::initialise()
    {
        // 0 input channels, 2 output channels: Phase 1 only needs playback.
        // Recording inputs are enabled in Phase 5.
        const auto error = deviceManager.initialiseWithDefaultDevices (0, 2);

        if (error.isNotEmpty())
        {
            // No hardware (common in headless/CI). The engine object is still
            // valid and the graph is built; we simply have nothing to render to.
            DBG ("OmniDAW AudioEngine: audio device init failed: " << error);
            return error;
        }

        // Drive the graph from the device. The player owns the AudioIODeviceCallback
        // and forwards prepareToPlay / processBlock to the graph for us.
        player.setProcessor (&graph);
        deviceManager.addAudioCallback (&player);

        running = true;
        lastReportedSamples = 0;

        // Heartbeat: report engine activity roughly twice a second from the
        // message thread (never from processBlock).
        startTimer (500);

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
        deviceManager.removeAudioCallback (&player);
        player.setProcessor (nullptr);
        deviceManager.closeAudioDevice();
        running = false;
    }

    void AudioEngine::buildBaseGraph()
    {
        graph.clear();
        modelToNode.clear();

        // The single sink: audio leaving this node goes to the soundcard.
        audioOutputNode = graph.addNode (
            std::make_unique<AudioGraphIOProcessor> (AudioGraphIOProcessor::audioOutputNode));

        // The master strip. Everything sums here before hitting the output.
        masterNode = graph.addNode (std::make_unique<TrackProcessor> ("Master"));

        connectStereo (masterNode->nodeID, audioOutputNode->nodeID);
    }

    void AudioEngine::connectStereo (NodeID source, NodeID destination)
    {
        // Wire both stereo channels (0 -> 0, 1 -> 1). addConnection silently
        // returns false if a channel doesn't exist, which is fine for mono nodes.
        graph.addConnection ({ { source, 0 }, { destination, 0 } });
        graph.addConnection ({ { source, 1 }, { destination, 1 } });
    }

    TrackProcessor* AudioEngine::getProcessorFor (NodeID nodeId) const noexcept
    {
        if (auto* node = graph.getNodeForId (nodeId))
            return dynamic_cast<TrackProcessor*> (node->getProcessor());

        return nullptr;
    }

    void AudioEngine::setProject (models::Project* project)
    {
        currentProject = project;
        rebuildGraph();
    }

    void AudioEngine::rebuildGraph()
    {
        // Mutating the graph topology is only safe while the audio thread is not
        // rendering it. Detaching the player suspends rendering for the duration
        // of the rebuild; we re-attach afterwards. This keeps us clear of the
        // audio thread without any locks in processBlock.
        const bool wasRunning = running;

        if (wasRunning)
            player.setProcessor (nullptr);

        buildBaseGraph();

        if (currentProject != nullptr)
        {
            auto& timeline = currentProject->getTimeline();

            for (int i = 0; i < timeline.getNumTracks(); ++i)
            {
                auto* track = timeline.getTrack (i);
                if (track == nullptr)
                    continue;

                // Each model Track becomes a TrackProcessor node feeding master.
                // (Insert FX / instrument nodes are inserted upstream of this in
                // later phases.)
                auto node = graph.addNode (
                    std::make_unique<TrackProcessor> (track->name));

                modelToNode[track->getId().toDashedString().toStdString()] = node->nodeID;

                connectStereo (node->nodeID, masterNode->nodeID);
            }
        }

        syncParametersFromModel();

        if (wasRunning)
            player.setProcessor (&graph);
    }

    void AudioEngine::syncParametersFromModel()
    {
        if (currentProject == nullptr)
            return;

        auto& timeline = currentProject->getTimeline();

        for (int i = 0; i < timeline.getNumTracks(); ++i)
        {
            auto* track = timeline.getTrack (i);
            if (track == nullptr)
                continue;

            const auto it = modelToNode.find (track->getId().toDashedString().toStdString());
            if (it == modelToNode.end())
                continue;

            if (auto* proc = getProcessorFor (it->second))
            {
                proc->setGain (track->getVolume());
                proc->setPan (track->getPan());
                proc->setMuted (track->isMuted());
            }
        }

        // Master volume comes from the mixer model.
        if (auto* master = getProcessorFor (masterNode->nodeID))
            master->setGain (currentProject->getMixer().getMasterBus().getVolume());
    }

    juce::int64 AudioEngine::getMasterProcessedSampleCount() const noexcept
    {
        if (masterNode == nullptr)
            return 0;

        if (auto* master = getProcessorFor (masterNode->nodeID))
            return master->getProcessedSampleCount();

        return 0;
    }

    void AudioEngine::timerCallback()
    {
        const auto total = getMasterProcessedSampleCount();
        const auto delta = total - lastReportedSamples;

        if (delta > 0)
        {
            // Phase 1's "print to the console when audio is processed" — done the
            // real-time-safe way: read an atomic that the audio thread bumped.
            DBG ("OmniDAW AudioEngine: processed " << delta
                 << " samples (total " << total << ")");
            lastReportedSamples = total;
        }
    }
} // namespace omnidaw::engine
