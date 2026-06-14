#pragma once

#include "Models/Project.h"
#include "Engine/Transport.h"
#include "Engine/TrackProcessor.h"
#include "Engine/MidiSourceProcessor.h"
#include "Engine/AudioClipProcessor.h"
#include "Engine/SynthProcessor.h"
#include "Engine/PluginManager.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace omnidaw::engine
{

    /**
        AudioEngine — owns the real-time audio infrastructure of OmniDAW.

        It is the AudioIODeviceCallback itself (rather than delegating to a
        juce::AudioProcessorPlayer) so that it can own the Transport: advancing the
        playhead, handling looping and (Phase 5) recording all happen around the
        single graph.processBlock() per device callback.

        Graph topology built from the model
        -----------------------------------
        Per audio track:
            [AudioClipProcessor] ─audio─▶ [insert FX…] ─▶ [TrackProcessor strip] ─▶ master
        Per MIDI track:
            [MidiSourceProcessor] ─midi─▶ [instrument] ─audio─▶ [TrackProcessor strip] ─▶ master
            (instrument = built-in SynthProcessor, or a hosted VST3/AU in Phase 2b)
        And finally:
            [master TrackProcessor] ─▶ [audio output IO] ─▶ soundcard

        Thread-safety contract
        ----------------------
        Public methods run on the message thread. Graph topology is only mutated
        while the device callback is detached (rebuilds suspend rendering), so the
        audio thread never races structural edits. Steady-state parameter and
        sequence changes reach the audio thread through atomics / lock-free
        snapshot publication — never a lock in processBlock.
    */
    class AudioEngine : private juce::Timer,
                        public juce::AudioIODeviceCallback
    {
    public:
        AudioEngine();
        ~AudioEngine() override;

        /** Opens the default device (2 in / 2 out) and starts rendering.
            Returns "" on success or an error string (e.g. headless CI box).
        */
        juce::String initialise();
        void shutdown();

        /** Point the engine at a project and (re)build everything from it. */
        void setProject (models::Project* project);

        /** Rebuild graph nodes/connections from the current model structure. */
        void rebuildGraph();

        /** Rebuild the per-track MIDI / audio-clip snapshots (after clip edits). */
        void rebuildSequences();

        /** Push current mixer values (volume/pan/mute) into the live nodes. */
        void syncParametersFromModel();

        [[nodiscard]] Transport& getTransport() noexcept { return transport; }
        [[nodiscard]] PluginManager& getPluginManager() noexcept { return pluginManager; }
        [[nodiscard]] models::Project* getProject() const noexcept { return currentProject; }
        [[nodiscard]] bool isRunning() const noexcept { return running; }
        [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
        [[nodiscard]] juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

        [[nodiscard]] juce::int64 getMasterProcessedSampleCount() const noexcept;

        /** Post-fader output level (0..1) of a track's channel strip, for the
            mixer meters. Returns 0 if the track has no live node. */
        [[nodiscard]] float getTrackLevel (const models::Track& track) const noexcept;

        /** Post-fader output level (0..1) of a bus strip, for the mixer meters. */
        [[nodiscard]] float getBusLevel (const models::Bus& bus) const noexcept;

        /** Offline (non-realtime) render of the current project for `seconds`,
            starting from timeline position 0 with the transport playing. Returns
            the peak magnitude of the rendered output. Used by the headless
            self-test to verify the engine actually produces audio without any
            soundcard present. Must only be called while NOT attached to a device.
        */
        float renderOfflinePeak (double sampleRate, double seconds, int blockSize = 512);

        /** Peek the master output level (0..1, post-fader) for metering. */
        [[nodiscard]] float getMasterLevel() const noexcept { return masterLevel.load (std::memory_order_relaxed); }

        // ── juce::AudioIODeviceCallback ─────────────────────────────────────────
        void audioDeviceAboutToStart (juce::AudioIODevice*) override;
        void audioDeviceStopped() override;
        void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                               int numInputChannels,
                                               float* const* outputChannelData,
                                               int numOutputChannels,
                                               int numSamples,
                                               const juce::AudioIODeviceCallbackContext& context) override;

    private:
        using Graph  = juce::AudioProcessorGraph;
        using Node   = Graph::Node;
        using NodeID = Graph::NodeID;

        struct TrackChain
        {
            NodeID strip;       // TrackProcessor (channel strip) — always present
            NodeID source;      // AudioClipProcessor or MidiSourceProcessor
            NodeID instrument;  // instrument node (MIDI tracks only), else invalid
            std::vector<NodeID> inserts; // insert FX nodes, in series
            std::vector<NodeID> sends;   // send-tap gain nodes, aligned with model sends
        };

        void buildBaseGraph();
        void buildBuses();
        void addTrackChain (models::Track& track);
        NodeID makeInstrumentNode (models::Track& track); // hosted plugin or built-in synth
        void connectStereo (NodeID source, NodeID destination);
        void connectMidi (NodeID source, NodeID destination);

        TrackProcessor*      getStripProcessor (NodeID) const noexcept;
        MidiSourceProcessor* getMidiSource (NodeID) const noexcept;
        AudioClipProcessor*  getAudioSource (NodeID) const noexcept;

        void timerCallback() override;

        Transport transport;
        PluginManager pluginManager;
        juce::AudioFormatManager formatManager;
        juce::AudioDeviceManager deviceManager;
        Graph graph;

        models::Project* currentProject { nullptr };

        Node::Ptr audioOutputNode;
        Node::Ptr masterNode;

        std::unordered_map<std::string, TrackChain> trackChains;
        std::unordered_map<std::string, NodeID> busNodes; // bus id -> strip node

        juce::AudioBuffer<float> renderBuffer; // scratch for the device callback
        juce::MidiBuffer scratchMidi;

        double currentSampleRate { 44100.0 };
        int    currentBlockSize  { 512 };

        std::atomic<float> masterLevel { 0.0f };
        bool running { false };
        juce::int64 lastReportedSamples { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
} // namespace omnidaw::engine
