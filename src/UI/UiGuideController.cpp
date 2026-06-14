#include "UI/UiGuideController.h"

namespace freequency::ui
{
    UiGuideController::UiGuideController (juce::Component& h) : host (h) {}

    void UiGuideController::load()
    {
        loadGuideSettings (guideSettings);
    }

    void UiGuideController::save()
    {
        saveGuideSettings (guideSettings);
    }

    void UiGuideController::attachOverlay()
    {
        if (overlay == nullptr)
        {
            overlay = std::make_unique<UiGuideOverlay>();
            host.addAndMakeVisible (*overlay);
            overlay->setVisible (false);
        }

        overlay->setAnimationsEnabled (guideSettings.animationsEnabled);
        resizeToHost();
    }

    void UiGuideController::resizeToHost()
    {
        if (overlay != nullptr)
            overlay->setBounds (host.getLocalBounds());
    }

    void UiGuideController::applySettings()
    {
        if (overlay != nullptr)
            overlay->setAnimationsEnabled (guideSettings.animationsEnabled);
    }

    std::optional<juce::Rectangle<int>> UiGuideController::resolveAnchor (GuideAnchor anchor) const
    {
        if (anchorResolver)
            return anchorResolver (anchor);
        return std::nullopt;
    }

    GuideHint UiGuideController::hintForCustomPin (int index) const
    {
        GuideHint h;
        h.anchor = (GuideAnchor) ((int) GuideAnchor::customPin0 + index);
        const auto& p = guideSettings.customPins[(size_t) index];
        h.normX = p.normX;
        h.normY = p.normY;
        h.title = p.title;
        h.body  = p.body;
        h.effect = p.effect;
        h.holdMs = 5000;
        return h;
    }

    void UiGuideController::showHint (const GuideHint& hint, juce::Point<int> flyFromGlobal)
    {
        if (overlay == nullptr) return;

        const auto target = resolveAnchor (hint.anchor);
        if (! target.has_value() || target->isEmpty()) return;

        overlay->setAnimationsEnabled (guideSettings.animationsEnabled);
        overlay->onDismiss = [this] { tourIndex = -1; };
        overlay->onNext = nullptr;

        overlay->showHint (hint, *target, flyFromGlobal);
    }

    void UiGuideController::showPanelClosedHint (GuideAnchor anchor,
                                                   const juce::String& title,
                                                   const juce::String& body)
    {
        if (! guideSettings.panelCloseHints || ! guideSettings.animationsEnabled)
            return;

        GuideHint h;
        h.anchor = anchor;
        h.effect = GuideEffect::flyBeam;
        h.title  = title;
        h.body   = body;
        h.holdMs = 3800;

        const auto screenCentre = host.getScreenBounds().getCentre();
        showHint (h, screenCentre);
    }

    void UiGuideController::showTourIndex (int idx)
    {
        if (overlay == nullptr || ! juce::isPositiveAndBelow (idx, tour.size()))
        {
            tourIndex = -1;
            if (! guideSettings.welcomeCompleted)
            {
                guideSettings.welcomeCompleted = true;
                save();
            }
            return;
        }

        tourIndex = idx;
        const auto& hint = tour.getReference (idx);

        const auto target = resolveAnchor (hint.anchor);
        if (! target.has_value() || target->isEmpty())
        {
            showTourIndex (idx + 1);
            return;
        }

        overlay->setAnimationsEnabled (guideSettings.animationsEnabled);
        overlay->onDismiss = [this]
        {
            tourIndex = -1;
            guideSettings.welcomeCompleted = true;
            save();
        };
        overlay->onNext = [this] { showTourIndex (tourIndex + 1); };

        overlay->showHint (hint, *target, {});
    }

    void UiGuideController::startWelcomeTour()
    {
        if (guideSettings.welcomeCompleted)
            return;

        tour = defaultWelcomeTour();
        showTourIndex (0);
    }

    void UiGuideController::replayTour()
    {
        tour = defaultWelcomeTour();
        guideSettings.welcomeCompleted = false;
        showTourIndex (0);
    }

    void UiGuideController::showCustomPin (int index)
    {
        if (index < 0 || index > 2) return;
        showHint (hintForCustomPin (index));
    }

    void UiGuideController::setPinPlacementMode (int pinIndex, bool enabled)
    {
        placingPinIndex = enabled ? pinIndex : -1;
    }

    bool UiGuideController::handlePlacementClick (juce::Point<int> hostLocal)
    {
        if (placingPinIndex < 0 || placingPinIndex > 2)
            return false;

        const float nx = host.getWidth() > 0 ? (float) hostLocal.x / (float) host.getWidth() : 0.5f;
        const float ny = host.getHeight() > 0 ? (float) hostLocal.y / (float) host.getHeight() : 0.5f;

        auto& pin = guideSettings.customPins[(size_t) placingPinIndex];
        pin.normX = juce::jlimit (0.05f, 0.95f, nx);
        pin.normY = juce::jlimit (0.05f, 0.95f, ny);

        save();
        const int idx = placingPinIndex;
        placingPinIndex = -1;
        showCustomPin (idx);
        return true;
    }
} // namespace freequency::ui
