#include "UI/UiGuideOverlay.h"
#include "UI/Theme.h"

namespace freequency::ui
{
    namespace
    {
        juce::Rectangle<float> padded (juce::Rectangle<int> r, float pad)
        {
            return r.toFloat().expanded (pad);
        }

        juce::Colour auroraAt (float t, const Theme& th)
        {
            const float mix = 0.5f + 0.5f * std::sin (t * juce::MathConstants<float>::twoPi);
            return th.accent.interpolatedWith (th.accentWarm, mix);
        }
    }

    UiGuideOverlay::UiGuideOverlay()
    {
        setInterceptsMouseClicks (true, true);
        setAlwaysOnTop (true);

        gotIt.onClick = [this]
        {
            dismiss();
            if (onDismiss) onDismiss();
        };

        nextBtn.onClick = [this]
        {
            dismiss();
            if (onNext) onNext();
        };

        addChildComponent (gotIt);
        addChildComponent (nextBtn);
    }

    void UiGuideOverlay::showHint (const GuideHint& hint,
                                   juce::Rectangle<int> targetGlobal,
                                   juce::Point<int> flyFromGlobal)
    {
        current = hint;
        targetRect = targetGlobal;
        flyFrom = flyFromGlobal;
        hasFlyFrom = hint.effect == GuideEffect::flyBeam
                     || (flyFromGlobal.x != 0 || flyFromGlobal.y != 0);

        showing = true;
        phase = 0.0f;
        flyT = 0.0f;
        calloutT = animationsEnabled ? 0.0f : 1.0f;
        holdCounter = 0;

        setVisible (true);
        toFront (false);
        gotIt.setVisible (true);
        nextBtn.setVisible (onNext != nullptr);

        layoutCallout();
        startTimerHz (60);
        repaint();
    }

    void UiGuideOverlay::dismiss()
    {
        showing = false;
        stopTimer();
        setVisible (false);
        gotIt.setVisible (false);
        nextBtn.setVisible (false);
    }

    void UiGuideOverlay::resized()
    {
        layoutCallout();
    }

    void UiGuideOverlay::layoutCallout()
    {
        if (targetRect.isEmpty())
        {
            calloutBounds = getLocalBounds().withSizeKeepingCentre (300, 120);
        }
        else
        {
            const auto tl = getLocalPoint (nullptr, targetRect.getTopLeft());
            const auto br = getLocalPoint (nullptr, targetRect.getBottomRight());
            const juce::Rectangle<int> localTarget (tl, br);
            const int w = juce::jmin (340, getWidth() - 24);
            const int h = 110;
            auto box = juce::Rectangle<int> (0, 0, w, h);

            // Prefer below target; flip above if clipped.
            box.setCentre (localTarget.getCentreX(),
                           localTarget.getBottom() + h / 2 + 20);
            if (box.getBottom() > getHeight() - 8)
                box.setCentre (localTarget.getCentreX(),
                               localTarget.getY() - h / 2 - 20);

            box = box.constrainedWithin (getLocalBounds().reduced (8));
            calloutBounds = box;
        }

        auto btnRow = calloutBounds.removeFromBottom (32).reduced (8, 4);
        nextBtn.setBounds (btnRow.removeFromRight (72));
        btnRow.removeFromRight (6);
        gotIt.setBounds (btnRow.removeFromRight (72));
    }

    void UiGuideOverlay::mouseDown (const juce::MouseEvent& e)
    {
        if (! calloutBounds.contains (e.getPosition()))
        {
            dismiss();
            if (onDismiss) onDismiss();
        }
    }

    void UiGuideOverlay::timerCallback()
    {
        if (! showing) return;

        if (animationsEnabled)
        {
            phase += 0.018f;
            if (phase > 1.0f) phase -= 1.0f;

            if (hasFlyFrom && flyT < 1.0f)
                flyT = juce::jmin (1.0f, flyT + 0.045f);

            if (calloutT < 1.0f)
                calloutT = juce::jmin (1.0f, calloutT + 0.08f);
        }
        else
        {
            phase += 0.01f;
            flyT = 1.0f;
            calloutT = 1.0f;
        }

        ++holdCounter;
        repaint();
    }

    void UiGuideOverlay::drawDimWithCutout (juce::Graphics& g, juce::Rectangle<float> hole)
    {
        juce::Path full, cut;
        full.addRectangle (getLocalBounds().toFloat());
        cut.addRoundedRectangle (hole, hole.getHeight() * 0.22f);
        full.setUsingNonZeroWinding (false);
        full.addPath (cut);

        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillPath (full);
    }

    void UiGuideOverlay::drawEffect (juce::Graphics& g, juce::Rectangle<float> target)
    {
        const auto& th = theme();
        const auto centre = target.getCentre();
        const float baseR = juce::jmax (target.getWidth(), target.getHeight()) * 0.55f;

        switch (current.effect)
        {
            case GuideEffect::pulseRing:
            case GuideEffect::ripple:
            {
                for (int i = 0; i < 3; ++i)
                {
                    const float t = std::fmod (phase + i * 0.28f, 1.0f);
                    const float r = baseR + t * baseR * 1.4f;
                    const float a = (1.0f - t) * 0.55f;
                    g.setColour (th.accent.withAlpha (a));
                    g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 2.0f);
                }
                break;
            }
            case GuideEffect::auroraGlow:
            case GuideEffect::shimmer:
            {
                juce::ColourGradient grad (auroraAt (phase, th), centre.x - baseR, centre.y,
                                           th.accentWarm.withAlpha (0.0f), centre.x + baseR, centre.y, false);
                grad.addColour (0.35, th.accent.withAlpha (0.35f));
                grad.addColour (0.65, th.accentWarm.withAlpha (0.25f));
                g.setGradientFill (grad);
                g.fillEllipse (centre.x - baseR * 1.3f, centre.y - baseR * 1.3f,
                               baseR * 2.6f, baseR * 2.6f);

                g.setColour (th.accent.withAlpha (0.85f));
                g.drawRoundedRectangle (target.reduced (2.0f), 8.0f, 2.0f);
                break;
            }
            case GuideEffect::spotlight:
            {
                g.setColour (th.accent.withAlpha (0.25f));
                g.fillEllipse (centre.x - baseR * 1.5f, centre.y - baseR * 1.5f,
                               baseR * 3.0f, baseR * 3.0f);
                g.setColour (th.accent);
                g.drawRoundedRectangle (target, 6.0f, 2.5f);
                break;
            }
            case GuideEffect::flyBeam:
            default:
                break;
        }
    }

    void UiGuideOverlay::drawFlyBeam (juce::Graphics& g)
    {
        if (! hasFlyFrom || flyT <= 0.0f) return;

        const auto& th = theme();
        const auto localFrom = getLocalPoint (nullptr, flyFrom).toFloat();
        const auto tl = getLocalPoint (nullptr, targetRect.getTopLeft());
        const auto br = getLocalPoint (nullptr, targetRect.getBottomRight());
        const auto localTo = juce::Rectangle<float> (tl.toFloat(), br.toFloat()).getCentre();

        const auto pos = localFrom + (localTo - localFrom) * flyT;

        juce::Path beam;
        beam.startNewSubPath (localFrom);
        beam.lineTo (pos);
        g.setColour (th.accentWarm.withAlpha (0.35f + 0.45f * flyT));
        g.strokePath (beam, juce::PathStrokeType (3.0f));

        // Travelling spark
        g.setColour (th.accent.withAlpha (0.9f));
        g.fillEllipse (pos.x - 6.0f, pos.y - 6.0f, 12.0f, 12.0f);

        // Rings at destination while beam lands
        if (flyT > 0.6f)
        {
            const float t = (flyT - 0.6f) / 0.4f;
            g.setColour (th.accent.withAlpha ((1.0f - t) * 0.7f));
            g.drawEllipse (localTo.x - 20.0f - t * 30.0f, localTo.y - 20.0f - t * 30.0f,
                           40.0f + t * 60.0f, 40.0f + t * 60.0f, 2.0f);
        }
    }

    void UiGuideOverlay::drawCallout (juce::Graphics& g)
    {
        const auto& th = theme();
        const float slide = animationsEnabled ? calloutT : 1.0f;
        auto box = calloutBounds.toFloat();
        box.translate (0.0f, (1.0f - slide) * 18.0f);

        g.setColour (th.panel.withAlpha (0.94f * slide));
        g.fillRoundedRectangle (box, 10.0f);

        g.setColour (th.outline.withAlpha (0.8f * slide));
        g.drawRoundedRectangle (box, 10.0f, 1.0f);

        // Accent edge glow
        g.setColour (auroraAt (phase + 0.25f, th).withAlpha (0.45f * slide));
        g.drawRoundedRectangle (box.reduced (0.5f), 10.0f, 2.0f);

        auto textArea = box.reduced (14.0f, 10.0f);
        textArea.removeFromBottom (36.0f);

        g.setColour (th.textPrimary.withAlpha (slide));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (current.title, textArea.removeFromTop (22.0f), juce::Justification::topLeft, true);

        g.setColour (th.textDim.withAlpha (slide));
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (current.body, textArea.toNearestInt(), juce::Justification::topLeft, 4);
    }

    void UiGuideOverlay::paint (juce::Graphics& g)
    {
        if (! showing) return;

        const auto tl = getLocalPoint (nullptr, targetRect.getTopLeft());
        const auto br = getLocalPoint (nullptr, targetRect.getBottomRight());
        juce::Rectangle<float> localTarget (tl.toFloat(), br.toFloat());
        if (localTarget.isEmpty())
            localTarget = getLocalBounds().toFloat().withSizeKeepingCentre (120.0f, 48.0f);

        const auto hole = padded (localTarget.toNearestInt(), 10.0f);

        drawDimWithCutout (g, hole);
        drawFlyBeam (g);
        drawEffect (g, hole);
        drawCallout (g);
    }
} // namespace freequency::ui
