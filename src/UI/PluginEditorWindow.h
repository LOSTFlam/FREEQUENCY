#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace freequency::ui
{
    /**
        PluginEditorWindow — a floating window hosting an effect/instrument editor.

        Works for both built-in effects (which return a
        juce::GenericAudioProcessorEditor) and hosted VST3/AU plugins (which return
        their own editor). Owned by MainComponent so its lifetime is independent of
        the mixer strips that spawn it.
    */
    class PluginEditorWindow final : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow (juce::AudioProcessor& processor, const juce::String& title)
            : DocumentWindow (title, juce::Colour (0xff1d2027), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            juce::AudioProcessorEditor* editor = processor.hasEditor()
                                                     ? processor.createEditorAndMakeActive()
                                                     : nullptr;
            if (editor == nullptr)
                editor = new juce::GenericAudioProcessorEditor (processor);

            setContentOwned (editor, true);
            setResizable (false, false);
            centreWithSize (juce::jmax (320, getWidth()), juce::jmax (200, getHeight()));
            setVisible (true);
        }

        std::function<void (PluginEditorWindow*)> onClose;

        void closeButtonPressed() override { if (onClose) onClose (this); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorWindow)
    };
} // namespace freequency::ui
