#pragma once

#include "UI/Theme.h"
#include "Engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>

namespace freequency::ui
{
    /**
        SpectrumAnalyzer — FREEQUENCY's signature flourish: a live FFT spectrum of
        the master output, rendered as log-frequency bars in the accent colour.
        It is literally the app's namesake (frequency) made visible.

        Pulls recent master samples from the engine (lock-free), windows + FFTs
        them on a timer, and draws smoothed, decaying bars.
    */
    class SpectrumAnalyzer final : public juce::Component,
                                   private juce::Timer
    {
    public:
        explicit SpectrumAnalyzer (engine::AudioEngine& e)
            : engineRef (e), fft (fftOrder),
              window (fftSize, juce::dsp::WindowingFunction<float>::hann)
        {
            startTimerHz (30);
        }

        ~SpectrumAnalyzer() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.setColour (theme().background);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

            const auto area = getLocalBounds().toFloat().reduced (3.0f);
            const float barW = area.getWidth() / (float) numBars;

            for (int b = 0; b < numBars; ++b)
            {
                const float level = bars[b];
                const float h = level * area.getHeight();
                const auto x = area.getX() + b * barW;
                const auto colour = theme().accent.interpolatedWith (theme().accentWarm, level);
                g.setColour (colour.withAlpha (0.35f + 0.65f * level));
                g.fillRoundedRectangle (x + 0.5f, area.getBottom() - h, barW - 1.0f, h, 1.0f);
            }

            g.setColour (theme().outline);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
        }

    private:
        static constexpr int fftOrder = 11;
        static constexpr int fftSize = 1 << fftOrder; // 2048
        static constexpr int numBars = 40;

        void timerCallback() override
        {
            engineRef.readScope (fifo, fftSize);
            window.multiplyWithWindowingTable (fifo, (size_t) fftSize);

            juce::zeromem (fftData, sizeof (fftData));
            std::memcpy (fftData, fifo, sizeof (float) * fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData);

            double sr = 44100.0;
            if (auto* dev = engineRef.getDeviceManager().getCurrentAudioDevice())
                sr = dev->getCurrentSampleRate();

            const double maxF = juce::jmin (sr * 0.5, 20000.0);
            const double minF = 30.0;

            for (int b = 0; b < numBars; ++b)
            {
                const double f0 = minF * std::pow (maxF / minF, (double) b / numBars);
                const double f1 = minF * std::pow (maxF / minF, (double) (b + 1) / numBars);
                int bin0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 * fftSize / sr));
                int bin1 = juce::jlimit (bin0 + 1, fftSize / 2, (int) (f1 * fftSize / sr));

                float mag = 0.0f;
                for (int i = bin0; i < bin1; ++i)
                    mag = juce::jmax (mag, fftData[i]);

                const float db = juce::Decibels::gainToDecibels (mag / (float) fftSize, -90.0f);
                const float norm = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f);

                bars[b] = juce::jmax (norm, bars[b] * 0.82f); // peak + decay
            }

            repaint();
        }

        engine::AudioEngine& engineRef;
        juce::dsp::FFT fft;
        juce::dsp::WindowingFunction<float> window;
        float fifo[fftSize] {};
        float fftData[2 * fftSize] {};
        float bars[numBars] {};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
    };
} // namespace freequency::ui
