#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace freequency::dsp
{
    /**
        Built-in professional effects.

        Each effect is a self-contained juce::AudioProcessor exposing its
        parameters through a juce::AudioProcessorValueTreeState (APVTS). Because
        the parameters are real AudioProcessorParameters, every effect gets a
        usable parameter UI for free via juce::GenericAudioProcessorEditor, and
        state save/restore through the APVTS.

        Effects are referenced from the model by a stable id string of the form
        "builtin:<key>" (e.g. "builtin:eq"), so they sit in a track's insert
        chain exactly like a hosted VST3 — the engine just resolves built-ins
        through BuiltinEffects::create() instead of the PluginManager.
    */
    class BuiltinEffectBase : public juce::AudioProcessor
    {
    public:
        BuiltinEffectBase (juce::String name,
                           juce::AudioProcessorValueTreeState::ParameterLayout layout);
        ~BuiltinEffectBase() override = default;

        const juce::String getName() const override { return fxName; }

        bool acceptsMidi() const override  { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        bool isBusesLayoutSupported (const BusesLayout&) const override;

        juce::AudioProcessorEditor* createEditor() override
        {
            return new juce::GenericAudioProcessorEditor (*this);
        }
        bool hasEditor() const override { return true; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& dest) override;
        void setStateInformation (const void* data, int size) override;

        using juce::AudioProcessor::processBlock;

        juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

    protected:
        juce::String fxName;
        juce::AudioProcessorValueTreeState apvts;
    };

    /** Utility: gain, pan, phase invert (per channel), mono fold, stereo width. */
    class UtilityProcessor final : public BuiltinEffectBase
    {
    public:
        UtilityProcessor();
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    };

    /** 3-band parametric EQ (low shelf, peak, high shelf) + output trim. */
    class EqualizerProcessor final : public BuiltinEffectBase
    {
    public:
        EqualizerProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        void updateCoefficients();
        using Filter = juce::dsp::IIR::Filter<float>;
        using Duplicated = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;
        juce::dsp::ProcessorChain<Duplicated, Duplicated, Duplicated> chain; // low/mid/high
        double sr { 44100.0 };
    };

    /** Dynamics compressor with makeup gain. */
    class CompressorProcessor final : public BuiltinEffectBase
    {
    public:
        CompressorProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::Compressor<float> comp;
    };

    /** Saturator: tanh drive with dry/wet and output trim. */
    class SaturatorProcessor final : public BuiltinEffectBase
    {
    public:
        SaturatorProcessor();
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    };

    /** Clipper: soft (tanh) or hard clip at a ceiling, with drive and mix. */
    class ClipperProcessor final : public BuiltinEffectBase
    {
    public:
        ClipperProcessor();
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    };

    /** Algorithmic reverb (room size, damping, width, wet/dry). */
    class ReverbProcessor final : public BuiltinEffectBase
    {
    public:
        ReverbProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::Reverb reverb;
    };

    /** Stereo delay (time, feedback, mix). */
    class DelayProcessor final : public BuiltinEffectBase
    {
    public:
        DelayProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::DelayLine<float> delayL { 192000 };
        juce::dsp::DelayLine<float> delayR { 192000 };
        double sr { 44100.0 };
    };

    /** Factory + catalogue of the built-in effects. */
    struct BuiltinEffectInfo
    {
        juce::String id;    // e.g. "builtin:eq"
        juce::String name;  // e.g. "EQ"
    };

    class BuiltinEffects
    {
    public:
        static juce::Array<BuiltinEffectInfo> list();
        static bool isBuiltin (const juce::String& identifier);
        static juce::String displayName (const juce::String& identifier);
        static std::unique_ptr<juce::AudioProcessor> create (const juce::String& identifier);
    };
} // namespace freequency::dsp
