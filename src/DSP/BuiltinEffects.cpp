#include "DSP/BuiltinEffects.h"

#include <cmath>

namespace freequency::dsp
{
    using APVTS = juce::AudioProcessorValueTreeState;
    using FloatParam = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using BoolParam = juce::AudioParameterBool;

    namespace
    {
        juce::NormalisableRange<float> freqRange()
        {
            juce::NormalisableRange<float> r (20.0f, 20000.0f, 1.0f);
            r.setSkewForCentre (1000.0f);
            return r;
        }

        std::unique_ptr<FloatParam> fparam (juce::String id, juce::String name,
                                            juce::NormalisableRange<float> range, float def)
        {
            return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, range, def);
        }
    } // namespace

    // ── Base ─────────────────────────────────────────────────────────────────────
    BuiltinEffectBase::BuiltinEffectBase (juce::String name, APVTS::ParameterLayout layout)
        : AudioProcessor (BusesProperties()
                              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          fxName (std::move (name)),
          apvts (*this, nullptr, "PARAMS", std::move (layout))
    {
    }

    bool BuiltinEffectBase::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;
        return layouts.getMainInputChannelSet() == out;
    }

    void BuiltinEffectBase::getStateInformation (juce::MemoryBlock& dest)
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, dest);
    }

    void BuiltinEffectBase::setStateInformation (const void* data, int size)
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    // ── Utility ──────────────────────────────────────────────────────────────────
    UtilityProcessor::UtilityProcessor()
        : BuiltinEffectBase ("Utility", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("gain", "Gain", { -60.0f, 12.0f, 0.01f }, 0.0f));
              l.add (fparam ("pan", "Pan", { -1.0f, 1.0f, 0.001f }, 0.0f));
              l.add (fparam ("width", "Width", { 0.0f, 2.0f, 0.001f }, 1.0f));
              l.add (std::make_unique<BoolParam> (juce::ParameterID { "phaseL", 1 }, "Phase L", false));
              l.add (std::make_unique<BoolParam> (juce::ParameterID { "phaseR", 1 }, "Phase R", false));
              l.add (std::make_unique<BoolParam> (juce::ParameterID { "mono", 1 }, "Mono", false));
              return l;
          }())
    {
    }

    void UtilityProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("gain")->load());
        const float pan = apvts.getRawParameterValue ("pan")->load();
        const float width = apvts.getRawParameterValue ("width")->load();
        const bool phaseL = apvts.getRawParameterValue ("phaseL")->load() > 0.5f;
        const bool phaseR = apvts.getRawParameterValue ("phaseR")->load() > 0.5f;
        const bool mono = apvts.getRawParameterValue ("mono")->load() > 0.5f;

        const float angle = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
        const float panL = std::cos (angle), panR = std::sin (angle);

        const int n = buffer.getNumSamples();

        if (buffer.getNumChannels() >= 2)
        {
            auto* L = buffer.getWritePointer (0);
            auto* R = buffer.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                float l = phaseL ? -L[i] : L[i];
                float r = phaseR ? -R[i] : R[i];

                const float mid = (l + r) * 0.5f;
                const float side = (l - r) * 0.5f * width;
                l = mid + side;
                r = mid - side;

                if (mono) { const float m = (l + r) * 0.5f; l = m; r = m; }

                L[i] = l * panL * gain;
                R[i] = r * panR * gain;
            }
        }
        else if (buffer.getNumChannels() == 1)
        {
            auto* m = buffer.getWritePointer (0);
            for (int i = 0; i < n; ++i)
                m[i] = (phaseL ? -m[i] : m[i]) * gain;
        }
    }

    // ── EQ ───────────────────────────────────────────────────────────────────────
    EqualizerProcessor::EqualizerProcessor()
        : BuiltinEffectBase ("EQ", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("loFreq", "Low Freq", freqRange(), 120.0f));
              l.add (fparam ("loGain", "Low Gain", { -18.0f, 18.0f, 0.01f }, 0.0f));
              l.add (fparam ("midFreq", "Mid Freq", freqRange(), 1000.0f));
              l.add (fparam ("midGain", "Mid Gain", { -18.0f, 18.0f, 0.01f }, 0.0f));
              l.add (fparam ("midQ", "Mid Q", { 0.1f, 8.0f, 0.001f }, 0.8f));
              l.add (fparam ("hiFreq", "High Freq", freqRange(), 6000.0f));
              l.add (fparam ("hiGain", "High Gain", { -18.0f, 18.0f, 0.01f }, 0.0f));
              l.add (fparam ("out", "Output", { -18.0f, 18.0f, 0.01f }, 0.0f));
              return l;
          }())
    {
    }

    void EqualizerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                      (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
        chain.prepare (spec);
        updateCoefficients();
    }

    void EqualizerProcessor::updateCoefficients()
    {
        using Coeff = juce::dsp::IIR::Coefficients<float>;
        const float loF = apvts.getRawParameterValue ("loFreq")->load();
        const float loG = apvts.getRawParameterValue ("loGain")->load();
        const float mF  = apvts.getRawParameterValue ("midFreq")->load();
        const float mG  = apvts.getRawParameterValue ("midGain")->load();
        const float mQ  = apvts.getRawParameterValue ("midQ")->load();
        const float hiF = apvts.getRawParameterValue ("hiFreq")->load();
        const float hiG = apvts.getRawParameterValue ("hiGain")->load();

        chain.get<0>().state = Coeff::makeLowShelf  (sr, loF, 0.7f, juce::Decibels::decibelsToGain (loG));
        chain.get<1>().state = Coeff::makePeakFilter (sr, mF, mQ, juce::Decibels::decibelsToGain (mG));
        chain.get<2>().state = Coeff::makeHighShelf (sr, hiF, 0.7f, juce::Decibels::decibelsToGain (hiG));
    }

    void EqualizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        updateCoefficients();

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chain.process (ctx);

        buffer.applyGain (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    }

    // ── Compressor ───────────────────────────────────────────────────────────────
    CompressorProcessor::CompressorProcessor()
        : BuiltinEffectBase ("Compressor", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("threshold", "Threshold", { -60.0f, 0.0f, 0.01f }, -18.0f));
              l.add (fparam ("ratio", "Ratio", { 1.0f, 20.0f, 0.01f }, 3.0f));
              l.add (fparam ("attack", "Attack", { 0.1f, 200.0f, 0.01f }, 10.0f));
              l.add (fparam ("release", "Release", { 5.0f, 1000.0f, 0.01f }, 120.0f));
              l.add (fparam ("makeup", "Makeup", { 0.0f, 24.0f, 0.01f }, 0.0f));
              return l;
          }())
    {
    }

    void CompressorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                      (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
        comp.prepare (spec);
    }

    void CompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        comp.setThreshold (apvts.getRawParameterValue ("threshold")->load());
        comp.setRatio (apvts.getRawParameterValue ("ratio")->load());
        comp.setAttack (apvts.getRawParameterValue ("attack")->load());
        comp.setRelease (apvts.getRawParameterValue ("release")->load());

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        comp.process (ctx);

        buffer.applyGain (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("makeup")->load()));
    }

    // ── Saturator ────────────────────────────────────────────────────────────────
    SaturatorProcessor::SaturatorProcessor()
        : BuiltinEffectBase ("Saturator", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("drive", "Drive", { 0.0f, 36.0f, 0.01f }, 6.0f));
              l.add (fparam ("mix", "Mix", { 0.0f, 1.0f, 0.001f }, 1.0f));
              l.add (fparam ("out", "Output", { -24.0f, 12.0f, 0.01f }, 0.0f));
              return l;
          }())
    {
    }

    void SaturatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const float drive = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("drive")->load());
        const float mix = apvts.getRawParameterValue ("mix")->load();
        const float out = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load());

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float dry = d[i];
                const float wet = std::tanh (dry * drive);
                d[i] = (wet * mix + dry * (1.0f - mix)) * out;
            }
        }
    }

    // ── Clipper ──────────────────────────────────────────────────────────────────
    ClipperProcessor::ClipperProcessor()
        : BuiltinEffectBase ("Clipper", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("drive", "Drive", { 0.0f, 36.0f, 0.01f }, 0.0f));
              l.add (fparam ("ceiling", "Ceiling", { 0.05f, 1.0f, 0.001f }, 0.9f));
              l.add (fparam ("mix", "Mix", { 0.0f, 1.0f, 0.001f }, 1.0f));
              l.add (std::make_unique<ChoiceParam> (juce::ParameterID { "mode", 1 }, "Mode",
                                                    juce::StringArray { "Soft", "Hard" }, 0));
              return l;
          }())
    {
    }

    void ClipperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const float drive = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("drive")->load());
        const float ceiling = apvts.getRawParameterValue ("ceiling")->load();
        const float mix = apvts.getRawParameterValue ("mix")->load();
        const bool hard = apvts.getRawParameterValue ("mode")->load() > 0.5f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float dry = d[i];
                const float x = dry * drive;
                const float clipped = hard ? juce::jlimit (-ceiling, ceiling, x)
                                           : ceiling * std::tanh (x / juce::jmax (1.0e-6f, ceiling));
                d[i] = clipped * mix + dry * (1.0f - mix);
            }
        }
    }

    // ── Reverb ───────────────────────────────────────────────────────────────────
    ReverbProcessor::ReverbProcessor()
        : BuiltinEffectBase ("Reverb", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("size", "Size", { 0.0f, 1.0f, 0.001f }, 0.5f));
              l.add (fparam ("damping", "Damping", { 0.0f, 1.0f, 0.001f }, 0.5f));
              l.add (fparam ("width", "Width", { 0.0f, 1.0f, 0.001f }, 1.0f));
              l.add (fparam ("wet", "Wet", { 0.0f, 1.0f, 0.001f }, 0.33f));
              l.add (fparam ("dry", "Dry", { 0.0f, 1.0f, 0.001f }, 0.7f));
              return l;
          }())
    {
    }

    void ReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                      (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
        reverb.prepare (spec);
    }

    void ReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        juce::dsp::Reverb::Parameters p;
        p.roomSize = apvts.getRawParameterValue ("size")->load();
        p.damping  = apvts.getRawParameterValue ("damping")->load();
        p.width    = apvts.getRawParameterValue ("width")->load();
        p.wetLevel = apvts.getRawParameterValue ("wet")->load();
        p.dryLevel = apvts.getRawParameterValue ("dry")->load();
        reverb.setParameters (p);

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    // ── Delay ────────────────────────────────────────────────────────────────────
    DelayProcessor::DelayProcessor()
        : BuiltinEffectBase ("Delay", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("time", "Time", { 1.0f, 2000.0f, 0.1f }, 350.0f));
              l.add (fparam ("feedback", "Feedback", { 0.0f, 0.95f, 0.001f }, 0.35f));
              l.add (fparam ("mix", "Mix", { 0.0f, 1.0f, 0.001f }, 0.3f));
              return l;
          }())
    {
    }

    void DelayProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 1 };
        delayL.prepare (spec);
        delayR.prepare (spec);
        delayL.reset();
        delayR.reset();
    }

    void DelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const float timeMs = apvts.getRawParameterValue ("time")->load();
        const float fb = apvts.getRawParameterValue ("feedback")->load();
        const float mix = apvts.getRawParameterValue ("mix")->load();
        const float delaySamples = juce::jlimit (1.0f, 191000.0f, (float) (timeMs * 0.001 * sr));

        delayL.setDelay (delaySamples);
        delayR.setDelay (delaySamples);

        const int n = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            const float dl = delayL.popSample (0);
            delayL.pushSample (0, L[i] + dl * fb);
            L[i] = L[i] * (1.0f - mix) + dl * mix;

            if (R != nullptr)
            {
                const float dr = delayR.popSample (0);
                delayR.pushSample (0, R[i] + dr * fb);
                R[i] = R[i] * (1.0f - mix) + dr * mix;
            }
        }
    }

    // ── Filter ───────────────────────────────────────────────────────────────────
    FilterProcessor::FilterProcessor()
        : BuiltinEffectBase ("Filter", [] {
              APVTS::ParameterLayout l;
              l.add (std::make_unique<ChoiceParam> (juce::ParameterID { "type", 1 }, "Type",
                                                    juce::StringArray { "Low-pass", "High-pass", "Band-pass" }, 0));
              l.add (fparam ("cutoff", "Cutoff", freqRange(), 1200.0f));
              l.add (fparam ("res", "Resonance", { 0.1f, 10.0f, 0.001f }, 0.707f));
              return l;
          }())
    {
    }

    void FilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                      (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
        filter.prepare (spec);
    }

    void FilterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const int type = (int) apvts.getRawParameterValue ("type")->load();
        filter.setType (type == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                       : type == 2 ? juce::dsp::StateVariableTPTFilterType::bandpass
                                   : juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency (apvts.getRawParameterValue ("cutoff")->load());
        filter.setResonance (apvts.getRawParameterValue ("res")->load());

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filter.process (ctx);
    }

    // ── Gate ─────────────────────────────────────────────────────────────────────
    GateProcessor::GateProcessor()
        : BuiltinEffectBase ("Gate", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("threshold", "Threshold", { -80.0f, 0.0f, 0.01f }, -40.0f));
              l.add (fparam ("attack", "Attack", { 0.1f, 100.0f, 0.01f }, 2.0f));
              l.add (fparam ("release", "Release", { 5.0f, 1000.0f, 0.01f }, 120.0f));
              l.add (fparam ("range", "Range", { -90.0f, 0.0f, 0.01f }, -60.0f));
              return l;
          }())
    {
    }

    void GateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const float thresh = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("threshold")->load());
        const float floorGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("range")->load());
        const float atkMs = apvts.getRawParameterValue ("attack")->load();
        const float relMs = apvts.getRawParameterValue ("release")->load();
        const float atk = std::exp (-1.0f / (float) (atkMs * 0.001 * sr));
        const float rel = std::exp (-1.0f / (float) (relMs * 0.001 * sr));

        const int n = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int c = 0; c < ch; ++c) peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));
            const float target = peak > thresh ? 1.0f : floorGain;
            env = target + (env - target) * (target > env ? atk : rel);
            for (int c = 0; c < ch; ++c) buffer.setSample (c, i, buffer.getSample (c, i) * env);
        }
    }

    // ── Chorus ───────────────────────────────────────────────────────────────────
    ChorusProcessor::ChorusProcessor()
        : BuiltinEffectBase ("Chorus", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("rate", "Rate", { 0.01f, 10.0f, 0.001f }, 1.0f));
              l.add (fparam ("depth", "Depth", { 0.0f, 1.0f, 0.001f }, 0.25f));
              l.add (fparam ("delay", "Centre Delay", { 1.0f, 50.0f, 0.01f }, 7.0f));
              l.add (fparam ("feedback", "Feedback", { -0.95f, 0.95f, 0.001f }, 0.0f));
              l.add (fparam ("mix", "Mix", { 0.0f, 1.0f, 0.001f }, 0.5f));
              return l;
          }())
    {
    }

    void ChorusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                      (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
        chorus.prepare (spec);
    }

    void ChorusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        chorus.setRate (apvts.getRawParameterValue ("rate")->load());
        chorus.setDepth (apvts.getRawParameterValue ("depth")->load());
        chorus.setCentreDelay (apvts.getRawParameterValue ("delay")->load());
        chorus.setFeedback (apvts.getRawParameterValue ("feedback")->load());
        chorus.setMix (apvts.getRawParameterValue ("mix")->load());

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

    // ── Phaser ───────────────────────────────────────────────────────────────────
    PhaserProcessor::PhaserProcessor()
        : BuiltinEffectBase ("Phaser", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("rate", "Rate", { 0.01f, 10.0f, 0.001f }, 1.0f));
              l.add (fparam ("depth", "Depth", { 0.0f, 1.0f, 0.001f }, 0.5f));
              l.add (fparam ("centre", "Centre Freq", freqRange(), 1300.0f));
              l.add (fparam ("feedback", "Feedback", { -0.95f, 0.95f, 0.001f }, 0.0f));
              l.add (fparam ("mix", "Mix", { 0.0f, 1.0f, 0.001f }, 0.5f));
              return l;
          }())
    {
    }

    void PhaserProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                      (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
        phaser.prepare (spec);
    }

    void PhaserProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        phaser.setRate (apvts.getRawParameterValue ("rate")->load());
        phaser.setDepth (apvts.getRawParameterValue ("depth")->load());
        phaser.setCentreFrequency (apvts.getRawParameterValue ("centre")->load());
        phaser.setFeedback (apvts.getRawParameterValue ("feedback")->load());
        phaser.setMix (apvts.getRawParameterValue ("mix")->load());

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        phaser.process (ctx);
    }

    // ── Sidechain compressor ─────────────────────────────────────────────────────
    SidechainCompressor::SidechainCompressor()
        : AudioProcessor (BusesProperties()
                              .withInput  ("Main",      juce::AudioChannelSet::stereo(), true)
                              .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", [] {
              APVTS::ParameterLayout l;
              l.add (fparam ("threshold", "Threshold", { -60.0f, 0.0f, 0.01f }, -24.0f));
              l.add (fparam ("ratio", "Ratio", { 1.0f, 20.0f, 0.01f }, 4.0f));
              l.add (fparam ("attack", "Attack", { 0.1f, 100.0f, 0.01f }, 5.0f));
              l.add (fparam ("release", "Release", { 5.0f, 1000.0f, 0.01f }, 150.0f));
              l.add (fparam ("makeup", "Makeup", { 0.0f, 24.0f, 0.01f }, 0.0f));
              return l;
          }())
    {
    }

    bool SidechainCompressor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void SidechainCompressor::getStateInformation (juce::MemoryBlock& dest)
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, dest);
    }

    void SidechainCompressor::setStateInformation (const void* data, int size)
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    void SidechainCompressor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        auto main = getBusBuffer (buffer, true, 0);
        auto side = getBusBuffer (buffer, true, 1);

        const float thresh = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("threshold")->load());
        const float ratio = apvts.getRawParameterValue ("ratio")->load();
        const float makeup = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("makeup")->load());
        const float atk = std::exp (-1.0f / (float) (apvts.getRawParameterValue ("attack")->load() * 0.001 * sr));
        const float rel = std::exp (-1.0f / (float) (apvts.getRawParameterValue ("release")->load() * 0.001 * sr));

        const int n = main.getNumSamples();
        const int mainCh = main.getNumChannels();
        const int sideCh = side.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            // Key from the sidechain bus (fall back to main if SC is silent).
            float key = 0.0f;
            for (int c = 0; c < sideCh; ++c) key = juce::jmax (key, std::abs (side.getSample (c, i)));
            if (sideCh == 0 || key < 1.0e-6f)
                for (int c = 0; c < mainCh; ++c) key = juce::jmax (key, std::abs (main.getSample (c, i)));

            env = key + (env - key) * (key > env ? atk : rel);

            float gain = 1.0f;
            if (env > thresh && env > 0.0f)
            {
                const float over = env / thresh;                  // > 1
                const float compressed = std::pow (over, 1.0f / ratio);
                gain = compressed / over;                          // < 1 => reduction
            }

            for (int c = 0; c < mainCh; ++c)
                main.setSample (c, i, main.getSample (c, i) * gain * makeup);
        }
    }

    // ── Factory ──────────────────────────────────────────────────────────────────
    juce::Array<BuiltinEffectInfo> BuiltinEffects::list()
    {
        juce::Array<BuiltinEffectInfo> items;
        items.add ({ "builtin:utility",    "Utility" });
        items.add ({ "builtin:eq",         "EQ" });
        items.add ({ "builtin:compressor", "Compressor" });
        items.add ({ "builtin:saturator",  "Saturator" });
        items.add ({ "builtin:clipper",    "Clipper" });
        items.add ({ "builtin:reverb",     "Reverb" });
        items.add ({ "builtin:delay",      "Delay" });
        items.add ({ "builtin:filter",     "Filter" });
        items.add ({ "builtin:gate",       "Gate" });
        items.add ({ "builtin:chorus",     "Chorus" });
        items.add ({ "builtin:phaser",     "Phaser" });
        items.add ({ "builtin:sccomp",     "Sidechain Comp" });
        return items;
    }

    bool BuiltinEffects::isBuiltin (const juce::String& identifier)
    {
        return identifier.startsWith ("builtin:");
    }

    juce::String BuiltinEffects::displayName (const juce::String& identifier)
    {
        for (const auto& e : list())
            if (e.id == identifier)
                return e.name;
        return identifier;
    }

    std::unique_ptr<juce::AudioProcessor> BuiltinEffects::create (const juce::String& identifier)
    {
        if (identifier == "builtin:utility")    return std::make_unique<UtilityProcessor>();
        if (identifier == "builtin:eq")         return std::make_unique<EqualizerProcessor>();
        if (identifier == "builtin:compressor") return std::make_unique<CompressorProcessor>();
        if (identifier == "builtin:saturator")  return std::make_unique<SaturatorProcessor>();
        if (identifier == "builtin:clipper")    return std::make_unique<ClipperProcessor>();
        if (identifier == "builtin:reverb")     return std::make_unique<ReverbProcessor>();
        if (identifier == "builtin:delay")      return std::make_unique<DelayProcessor>();
        if (identifier == "builtin:filter")     return std::make_unique<FilterProcessor>();
        if (identifier == "builtin:gate")       return std::make_unique<GateProcessor>();
        if (identifier == "builtin:chorus")     return std::make_unique<ChorusProcessor>();
        if (identifier == "builtin:phaser")     return std::make_unique<PhaserProcessor>();
        if (identifier == "builtin:sccomp")     return std::make_unique<SidechainCompressor>();
        return {};
    }
} // namespace freequency::dsp
