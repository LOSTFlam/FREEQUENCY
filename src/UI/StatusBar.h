#pragma once

#include "UI/UIContext.h"
#include "UI/FreequencyLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace freequency::ui
{
    /**
        StatusBar — the bottom information strip: project name, sample rate, audio
        CPU load and transport position. A lightweight timer keeps it live.
    */
    class StatusBar final : public juce::Component,
                            private juce::Timer
    {
    public:
        explicit StatusBar (UIContext& ctx) : context (ctx) { startTimerHz (8); }
        ~StatusBar() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (FreequencyLookAndFeel::panel));
            g.setColour (juce::Colour (FreequencyLookAndFeel::outline));
            g.drawHorizontalLine (0, 0.0f, (float) getWidth());

            auto r = getLocalBounds().reduced (12, 0);
            g.setColour (juce::Colour (FreequencyLookAndFeel::textDim));
            g.setFont (juce::FontOptions (12.0f));

            g.drawText (context.project.name, r.removeFromLeft (220),
                        juce::Justification::centredLeft, true);

            const auto& dm = context.engine.getDeviceManager();
            double sr = 44100.0;
            int blockSize = 0;
            if (auto* dev = dm.getCurrentAudioDevice())
            {
                sr = dev->getCurrentSampleRate();
                blockSize = dev->getCurrentBufferSizeSamples();
            }

            const int cpu = (int) std::round (context.engine.getCpuUsage() * 100.0);

            g.drawText (juce::String::formatted ("CPU %d%%", cpu),
                        r.removeFromRight (90), juce::Justification::centredRight, true);
            g.drawText (juce::String::formatted ("%.0f Hz / %d smp", sr, blockSize),
                        r.removeFromRight (160), juce::Justification::centredRight, true);
            g.drawText (juce::String (context.engine.getTransport().getPositionSeconds(), 2) + " s",
                        r.removeFromRight (90), juce::Justification::centredRight, true);
        }

    private:
        void timerCallback() override { repaint(); }
        UIContext& context;
    };
} // namespace freequency::ui
