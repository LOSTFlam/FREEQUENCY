#include "Engine/SynthProcessor.h"

#include <cmath>

namespace omnidaw::engine
{
    namespace
    {
        constexpr int kNumVoices = 16;

        /** Sound that responds to every note/channel. */
        struct OmniSound final : public juce::SynthesiserSound
        {
            bool appliesToNote (int) override    { return true; }
            bool appliesToChannel (int) override { return true; }
        };

        /** polyBLEP correction — removes most of the aliasing from a naive saw. */
        inline float polyBlep (double t, double dt) noexcept
        {
            if (t < dt)
            {
                t /= dt;
                return (float) (t + t - t * t - 1.0);
            }

            if (t > 1.0 - dt)
            {
                t = (t - 1.0) / dt;
                return (float) (t * t + t + t + 1.0);
            }

            return 0.0f;
        }

        class OmniVoice final : public juce::SynthesiserVoice
        {
        public:
            OmniVoice()
            {
                adsr.setParameters ({ 0.005f, 0.12f, 0.8f, 0.25f });
            }

            bool canPlaySound (juce::SynthesiserSound* s) override
            {
                return dynamic_cast<OmniSound*> (s) != nullptr;
            }

            void startNote (int midiNote, float velocity,
                            juce::SynthesiserSound*, int) override
            {
                phase = 0.0;
                level = juce::jlimit (0.05f, 1.0f, velocity);
                const auto freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
                phaseIncrement = freq / getSampleRate();

                // Reset the one-pole filter and (re)trigger the envelope.
                lpState = 0.0f;
                adsr.setSampleRate (getSampleRate());
                adsr.noteOn();
            }

            void stopNote (float, bool allowTailOff) override
            {
                if (allowTailOff)
                {
                    adsr.noteOff();
                }
                else
                {
                    adsr.reset();
                    clearCurrentNote();
                }
            }

            void pitchWheelMoved (int) override {}
            void controllerMoved (int, int) override {}

            using juce::SynthesiserVoice::renderNextBlock; // keep double overload visible

            void renderNextBlock (juce::AudioBuffer<float>& output,
                                  int startSample, int numSamples) override
            {
                if (! adsr.isActive())
                    return;

                // Gentle fixed low-pass to tame the saw's high harmonics.
                constexpr float cutoffCoeff = 0.25f;

                while (--numSamples >= 0)
                {
                    // Naive saw in [-1, 1] plus polyBLEP discontinuity correction.
                    float saw = (float) (2.0 * phase - 1.0);
                    saw -= polyBlep (phase, phaseIncrement);

                    lpState += cutoffCoeff * (saw - lpState);

                    const float env = adsr.getNextSample();
                    const float sample = lpState * env * level * 0.4f;

                    for (int ch = output.getNumChannels(); --ch >= 0;)
                        output.addSample (ch, startSample, sample);

                    ++startSample;

                    phase += phaseIncrement;
                    if (phase >= 1.0)
                        phase -= 1.0;
                }

                if (! adsr.isActive())
                    clearCurrentNote();
            }

        private:
            double phase { 0.0 };
            double phaseIncrement { 0.0 };
            float  level { 0.7f };
            float  lpState { 0.0f };
            juce::ADSR adsr;
        };
    } // namespace

    SynthProcessor::SynthProcessor()
        : AudioProcessor (BusesProperties()
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        for (int i = 0; i < kNumVoices; ++i)
            synth.addVoice (new OmniVoice());

        synth.addSound (new OmniSound());
        synth.setNoteStealingEnabled (true);
    }

    void SynthProcessor::prepareToPlay (double sampleRate, int)
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    bool SynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    }

    void SynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals noDenormals;

        // Instrument nodes are sound sources: start from silence each block.
        buffer.clear();
        synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    }
} // namespace omnidaw::engine
