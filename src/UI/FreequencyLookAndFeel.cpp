#include "UI/FreequencyLookAndFeel.h"

namespace freequency::ui
{
    FreequencyLookAndFeel::FreequencyLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (background));
        setColour (juce::DocumentWindow::textColourId,        juce::Colour (textPrimary));

        setColour (juce::Label::textColourId,                 juce::Colour (textPrimary));

        setColour (juce::TextButton::buttonColourId,          juce::Colour (panelLight));
        setColour (juce::TextButton::buttonOnColourId,        juce::Colour (accent));
        setColour (juce::TextButton::textColourOffId,         juce::Colour (textPrimary));
        setColour (juce::TextButton::textColourOnId,          juce::Colour (background));

        setColour (juce::Slider::backgroundColourId,          juce::Colour (panel));
        setColour (juce::Slider::trackColourId,               juce::Colour (accent));
        setColour (juce::Slider::thumbColourId,               juce::Colour (textPrimary));

        setColour (juce::ComboBox::backgroundColourId,        juce::Colour (panelLight));
        setColour (juce::ComboBox::textColourId,              juce::Colour (textPrimary));
        setColour (juce::ComboBox::outlineColourId,           juce::Colour (outline));

        setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (panel));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (accent));
        setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colour (background));
    }

    void FreequencyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider&)
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
        const auto lineW  = juce::jmax (2.0f, radius * 0.18f);

        juce::Path back;
        back.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (outline));
        g.strokePath (back, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (accent));
        g.strokePath (arc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Point<float> thumb (centre.x + radius * std::cos (angle - juce::MathConstants<float>::halfPi),
                                  centre.y + radius * std::sin (angle - juce::MathConstants<float>::halfPi));
        g.setColour (juce::Colour (textPrimary));
        g.fillEllipse (juce::Rectangle<float> (lineW, lineW).withCentre (thumb));
    }

    void FreequencyLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

        if (style == juce::Slider::LinearVertical)
        {
            const auto trackX = bounds.getCentreX();
            g.setColour (juce::Colour (panel));
            g.fillRoundedRectangle (trackX - 3.0f, bounds.getY(), 6.0f, bounds.getHeight(), 3.0f);

            g.setColour (juce::Colour (accent));
            g.fillRoundedRectangle (trackX - 3.0f, sliderPos, 6.0f, bounds.getBottom() - sliderPos, 3.0f);

            g.setColour (juce::Colour (textPrimary));
            g.fillRoundedRectangle (bounds.getX() + 2.0f, sliderPos - 5.0f, bounds.getWidth() - 4.0f, 10.0f, 3.0f);
        }
        else
        {
            const auto trackY = bounds.getCentreY();
            g.setColour (juce::Colour (panel));
            g.fillRoundedRectangle (bounds.getX(), trackY - 3.0f, bounds.getWidth(), 6.0f, 3.0f);

            g.setColour (juce::Colour (accent));
            g.fillRoundedRectangle (bounds.getX(), trackY - 3.0f, sliderPos - bounds.getX(), 6.0f, 3.0f);

            g.setColour (juce::Colour (textPrimary));
            g.fillRoundedRectangle (sliderPos - 5.0f, bounds.getY() + 2.0f, 10.0f, bounds.getHeight() - 4.0f, 3.0f);
        }

        juce::ignoreUnused (slider);
    }

    void FreequencyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool highlighted, bool down)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto c = backgroundColour;

        if (down)        c = c.brighter (0.2f);
        else if (highlighted) c = c.brighter (0.1f);

        g.setColour (c);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (juce::Colour (outline));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
} // namespace freequency::ui
