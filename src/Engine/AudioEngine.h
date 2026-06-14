#pragma once

#include "Models/Project.h"
#include "Engine/TrackProcessor.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>  // juce::AudioProcessorPlayer

#include <string>
#include <unordered_map>

namespace omnidaw::engine
{
    /**
        AudioEngine — owns the real-time audio infrastructure of OmniDAW.

        Responsibilities
        ----------------
          - Owns the juce::AudioDeviceManager (the bridge to the soundcard).
          - Owns the juce::AudioProcessorGraph (the signal flow) and drives it via
            a juce::AudioProcessorPlayer.
          - Translates the *model* (a Project's tracks & mixer) into *graph nodes*
            and connections.

        Why an AudioProcessorGraph?
        ---------------------------
        A DAW's signal flow is a directed graph: many tracks feed buses, buses feed
        the master, sends create extra edges. juce::AudioProcessorGraph models
        exactly this — nodes are AudioProcessors, edges are channel-to-channel
        connections — and it handles render ordering, latency compensation and
        buffer management for us. We give every track/bus its own TrackProcessor
        node so the graph topology mirrors the mixer one-to-one.

        Graph layout built in Phase 1
        -----------------------------
                 ┌─────────────┐     ┌──────────────┐     ┌───────────────────┐
            ...  │ Track node  │ ──▶ │ Master node  │ ──▶ │ Audio output (IO) │ ─▶ soundcard
                 │(TrackProc.) │     │ (TrackProc.) │     │  (graph IO proc)  │
                 └─────────────┘     └──────────────┘     └───────────────────┘

        Insert FX, instruments, submix/FX buses and sends slot into this same
        structure in later phases without changing the ownership model here.

        Thread-safety contract
        -----------------------
        All public methods here are called from the message thread. The graph is
        only ever mutated while audio rendering is suspended (we detach the player
        during a rebuild), so we never race the audio thread. The audio thread,
        once running, only reads atomics inside the nodes.
    */
    class AudioEngine : private juce::Timer
    {
    public:
        AudioEngine();
        ~AudioEngine() override;

        /** Opens the default audio device (0 inputs, 2 outputs) and starts the
            graph. Returns an empty string on success, or a human-readable error
            (e.g. when no audio hardware is present, as in a headless CI box).
        */
        juce::String initialise();

        /** Stops audio and releases the device. Safe to call multiple times. */
        void shutdown();

        /** Points the engine at a project and (re)builds the audio graph from it.
            Pass nullptr to tear the graph down to just master -> output.
        */
        void setProject (models::Project* project);

        /** Rebuilds graph nodes/connections from the current project. Call after
            structurally editing the model (adding/removing tracks). Cheap enough
            for Phase 1; later phases will offer incremental updates.
        */
        void rebuildGraph();

        /** Pushes the current model mixer values (volume/pan/mute) into the live
            graph nodes. Real-time safe: only writes node atomics.
        */
        void syncParametersFromModel();

        [[nodiscard]] bool isRunning() const noexcept { return running; }
        [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

        /** Total samples processed by the master node since the last device start.
            Handy for tests/diagnostics to confirm the engine is alive.
        */
        [[nodiscard]] juce::int64 getMasterProcessedSampleCount() const noexcept;

    private:
        using Graph  = juce::AudioProcessorGraph;
        using Node   = Graph::Node;
        using NodeID = Graph::NodeID;

        void buildBaseGraph();
        TrackProcessor* getProcessorFor (NodeID nodeId) const noexcept;
        void connectStereo (NodeID source, NodeID destination);

        // juce::Timer — message-thread heartbeat that logs engine activity.
        // This is how Phase 1 "prints when audio is processed" WITHOUT ever doing
        // I/O on the audio thread: the node counts samples via an atomic, and we
        // sample that counter here, off the audio thread.
        void timerCallback() override;

        juce::AudioDeviceManager deviceManager;
        Graph graph;
        juce::AudioProcessorPlayer player;

        models::Project* currentProject { nullptr };

        Node::Ptr audioOutputNode;
        Node::Ptr masterNode;

        // Maps model object ids (tracks/buses) to their graph node ids.
        std::unordered_map<std::string, NodeID> modelToNode;

        bool running { false };
        juce::int64 lastReportedSamples { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
} // namespace omnidaw::engine
