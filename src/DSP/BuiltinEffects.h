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

    /** Resonant multimode filter (low-pass / high-pass / band-pass). */
    class FilterProcessor final : public BuiltinEffectBase
    {
    public:
        FilterProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::StateVariableTPTFilter<float> filter;
    };

    /** Noise gate with threshold / attack / release / range. */
    class GateProcessor final : public BuiltinEffectBase
    {
    public:
        GateProcessor();
        void prepareToPlay (double sampleRate, int) override { sr = sampleRate; env = 0.0f; }
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        double sr { 44100.0 };
        float env { 0.0f };
    };

    /** Stereo chorus (rate, depth, centre delay, feedback, mix). */
    class ChorusProcessor final : public BuiltinEffectBase
    {
    public:
        ChorusProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::Chorus<float> chorus;
    };

    /** Phaser (rate, depth, centre freq, feedback, mix). */
    class PhaserProcessor final : public BuiltinEffectBase
    {
    public:
        PhaserProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::Phaser<float> phaser;
    };

    /** Tremolo — LFO amplitude modulation. */
    class TremoloProcessor final : public BuiltinEffectBase
    {
    public:
        TremoloProcessor();
        void prepareToPlay (double sampleRate, int) override { sr = sampleRate; }
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        double sr { 44100.0 };
        float phase { 0.0f };
    };

    /** Flanger — short modulated delay with feedback. */
    class FlangerProcessor final : public BuiltinEffectBase
    {
    public:
        FlangerProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 4096 };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 4096 };
        double sr { 44100.0 };
        float phase { 0.0f };
    };

    /** De-esser — high-band compressor for sibilance reduction. */
    class DeEsserProcessor final : public BuiltinEffectBase
    {
    public:
        DeEsserProcessor();
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        juce::dsp::IIR::Filter<float> hpL, hpR;
        double sr { 44100.0 };
        float env { 0.0f };
    };

    /** Bitcrusher — sample-rate and bit-depth reduction. */
    class BitcrusherProcessor final : public BuiltinEffectBase
    {
    public:
        BitcrusherProcessor();
        void prepareToPlay (double sampleRate, int) override { sr = sampleRate; }
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    private:
        double sr { 44100.0 };
        float holdL { 0.0f }, holdR { 0.0f };
        int holdCounter { 0 };
    };

    /**
        SidechainCompressor — a compressor whose gain reduction is driven by a
        separate "key" signal on its sidechain input bus (classic ducking). The
        engine connects a chosen source track to the sidechain input; if nothing
        is connected, it falls back to keying off its own main input.

        Standalone (not BuiltinEffectBase) because it needs an extra input bus.
    */
    class SidechainCompressor final : public juce::AudioProcessor
    {
    public:
        SidechainCompressor();

        const juce::String getName() const override { return "Sidechain Comp"; }
        void prepareToPlay (double sampleRate, int) override { sr = sampleRate; env = 0.0f; }
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;
        bool isBusesLayoutSupported (const BusesLayout&) const override;

        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
        bool hasEditor() const override { return true; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

    private:
        juce::AudioProcessorValueTreeState apvts;
        double sr { 44100.0 };
        float env { 0.0f };
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
