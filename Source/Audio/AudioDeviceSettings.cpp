/*
 * AudioDeviceSettings.cpp
 * VoicemeeterHost — UI Scale Manager & Device Selector Dialog/Window
 *
 * Uses LookAndFeel_V4 (replaces deprecated V3).
 * Uses modern Font(FontOptions{}) constructors.
 */

#include "AudioDeviceSettings.h"
#include "../Localization/LanguageManager.h"
#include "../Graph/NodeComponent.h"

#if JUCE_WINDOWS
#include <Windows.h>
#endif

// ═══════════════════════════════════════════════════════════════
// ScaleSettingsManager
// ═══════════════════════════════════════════════════════════════

ScaleSettingsManager &ScaleSettingsManager::getInstance()
{
    static ScaleSettingsManager instance;
    return instance;
}

float ScaleSettingsManager::getScaleFactor() const { return scaleFactor; }

void ScaleSettingsManager::setScaleFactor(float scale)
{
    scaleFactor = scale;
    saveSettings();
}

void ScaleSettingsManager::loadSettings()
{
    try
    {
        float v = getAppProperties().getUserSettings()->getValue("uiScaleFactor", "1.0").getFloatValue();
        if (v >= 0.5f && v <= 3.0f)
            scaleFactor = v;
    }
    catch (...)
    {
    }
}

void ScaleSettingsManager::saveSettings()
{
    try
    {
        getAppProperties().getUserSettings()->setValue("uiScaleFactor", juce::String(scaleFactor));
        getAppProperties().saveIfNeeded();
    }
    catch (...)
    {
    }
}

// ─── Helpers ─────────────────────────────────────────────────
// getDPIScaleFactor / getFontScaleFactor are defined inline in NodeComponent.h

static int getSystemFrameHeight()
{
#if JUCE_WINDOWS
    return GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYSIZE);
#else
    return 40;
#endif
}

// ─── Scaled LookAndFeel (V4) ────────────────────────────────

class ScaledSelectorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ScaledSelectorLookAndFeel()
    {
        // Override color scheme for light theme
        setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(255, 255, 255));
        setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(34, 34, 34));
        setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(180, 180, 180));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(255, 255, 255));
        setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(34, 34, 34));
        setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(180, 180, 180));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB(240, 240, 240));
        setColour(juce::PopupMenu::textColourId, juce::Colour::fromRGB(34, 34, 34));
        setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(236, 236, 236));
        setColour(juce::Label::textColourId, juce::Colour::fromRGB(34, 34, 34));
        // PropertyComponent labels ("Output:", "Sample Rate:", etc.)
        setColour(juce::PropertyComponent::backgroundColourId, juce::Colour::fromRGB(236, 236, 236));
        setColour(juce::PropertyComponent::labelTextColourId,  juce::Colour::fromRGB(34, 34, 34));
        setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(255, 255, 255));
        setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(180, 180, 180));
        setColour(juce::ListBox::textColourId, juce::Colour::fromRGB(34, 34, 34));
        // ScrollBar
        setColour(juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB(230, 230, 230));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour::fromRGB(170, 170, 170));
        setColour(juce::ScrollBar::trackColourId, juce::Colour::fromRGB(220, 220, 220));
        // ToggleButton (channel checkboxes)
        setColour(juce::ToggleButton::textColourId, juce::Colour::fromRGB(34, 34, 34));
        setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(70, 130, 180));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(180, 180, 180));
        // TextButton (Test button)
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(100, 130, 150));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(70, 100, 120));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        // Slider (volume meter background)
        setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(100, 140, 170));
        setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(200, 200, 200));
        setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(242, 242, 242));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(34, 34, 34));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(200, 200, 200));
    }

    juce::Font getComboBoxFont(juce::ComboBox &) override
    {
        return juce::Font(juce::FontOptions{}.withHeight(13.0f));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(juce::FontOptions{}.withHeight(13.0f));
    }

    juce::Font getTextButtonFont(juce::TextButton &, int btnH) override
    {
        float h = juce::jmin(13.0f, (float)btnH * 0.7f);
        return juce::Font(juce::FontOptions{}.withHeight(h));
    }

    // NOTE: Do NOT override getLabelFont — applyTotalScale() sets label fonts directly.

    // Override property component label so "Output:", "Sample Rate:" etc. use the
    // same font size as the channel ToggleButtons (both now use 15pt * scaleFactor).
    void drawPropertyComponentLabel (juce::Graphics& g, int /*width*/, int height,
                                     juce::PropertyComponent& comp) override
    {
        g.setColour (comp.findColour (juce::PropertyComponent::labelTextColourId)
                         .withMultipliedAlpha (comp.isEnabled() ? 1.0f : 0.6f));
        const float fontSize = juce::jmin (13.0f,
                                           static_cast<float> (height) * 0.85f);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (fontSize)));
        g.drawFittedText (comp.getName(),
                          3, 0, juce::roundToInt ((float) comp.getWidth() * 0.5f) - 5, height,
                          juce::Justification::centredLeft, 2);
    }

    void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider &slider) override
    {
        juce::ignoreUnused(slider, minSliderPos, maxSliderPos);

        if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearVertical)
        {
            // Track dimensions (wider for visibility)
            int trackDepth = 8;
            float trackRadius = 4.0f;

            juce::Rectangle<int> trackBounds;
            juce::Rectangle<int> trackFill;
            juce::Rectangle<int> thumbBounds;

            if (style == juce::Slider::LinearHorizontal)
            {
                // Horizontal slider
                trackBounds = juce::Rectangle<int>(x, y + height / 2 - trackDepth / 2, width, trackDepth);

                // Background track
                g.setColour(findColour(juce::Slider::backgroundColourId));
                g.fillRoundedRectangle(trackBounds.toFloat(), trackRadius);

                // Filled track (from left to slider position)
                int fillWidth = static_cast<int>(sliderPos - x);
                trackFill = juce::Rectangle<int>(x, trackBounds.getY(), fillWidth, trackDepth);
                g.setColour(findColour(juce::Slider::trackColourId));
                g.fillRoundedRectangle(trackFill.toFloat(), trackRadius);

                // Thumb (circular handle)
                int thumbSize = 16;
                thumbBounds = juce::Rectangle<int>(
                    static_cast<int>(sliderPos) - thumbSize / 2,
                    y + height / 2 - thumbSize / 2,
                    thumbSize, thumbSize);
            }
            else
            {
                // Vertical slider
                trackBounds = juce::Rectangle<int>(x + width / 2 - trackDepth / 2, y, trackDepth, height);

                // Background track
                g.setColour(findColour(juce::Slider::backgroundColourId));
                g.fillRoundedRectangle(trackBounds.toFloat(), trackRadius);

                // Filled track (from bottom to slider position)
                int fillHeight = trackBounds.getBottom() - static_cast<int>(sliderPos);
                trackFill = juce::Rectangle<int>(trackBounds.getX(),
                                                 static_cast<int>(sliderPos),
                                                 trackDepth, fillHeight);
                g.setColour(findColour(juce::Slider::trackColourId));
                g.fillRoundedRectangle(trackFill.toFloat(), trackRadius);

                // Thumb (circular handle)
                int thumbSize = 16;
                thumbBounds = juce::Rectangle<int>(
                    x + width / 2 - thumbSize / 2,
                    static_cast<int>(sliderPos) - thumbSize / 2,
                    thumbSize, thumbSize);
            }

            // Draw thumb
            g.setColour(findColour(juce::Slider::thumbColourId));
            g.fillEllipse(thumbBounds.toFloat());

            // Thumb outline
            g.setColour(juce::Colour::fromRGB(80, 110, 140));
            g.drawEllipse(thumbBounds.toFloat(), 1.5f);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                                   sliderPos, minSliderPos, maxSliderPos,
                                                   style, slider);
        }
    }

    void drawLevelMeter(juce::Graphics &g, int width, int height, float level) override
    {
        // Background
        g.setColour(juce::Colour::fromRGB(210, 210, 210));
        g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 2.0f);

        // Outline
        g.setColour(juce::Colour::fromRGB(170, 170, 170));
        g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, 2.0f, 1.0f);

        if (level > 0.0f)
        {
            // Number of segments
            int numSegments = width / 5;
            int numLit = (int)(level * numSegments);
            float clampLevel = juce::jmin(level, 1.0f);
            juce::ignoreUnused(clampLevel);

            for (int i = 0; i < numSegments; ++i)
            {
                float x1 = (float)i / numSegments * width + 1.0f;
                float x2 = (float)(i + 1) / numSegments * width - 1.0f;
                float segWidth = x2 - x1;

                if (i < numLit)
                {
                    // Green for normal, yellow for high, red for clip
                    float segLevel = (float)i / numSegments;
                    if (segLevel < 0.6f)
                        g.setColour(juce::Colour::fromRGB(72, 180, 90)); // green
                    else if (segLevel < 0.85f)
                        g.setColour(juce::Colour::fromRGB(210, 180, 40)); // yellow
                    else
                        g.setColour(juce::Colour::fromRGB(210, 60, 50)); // red

                    g.fillRoundedRectangle(x1, 2.0f, segWidth, (float)height - 4.0f, 1.0f);
                }
                else
                {
                    g.setColour(juce::Colour::fromRGB(190, 190, 190));
                    g.fillRoundedRectangle(x1, 2.0f, segWidth, (float)height - 4.0f, 1.0f);
                }
            }
        }
    }

    void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                          bool highlighted, bool down) override
    {
        const float fontSize  = juce::jmin (13.0f,
                                             static_cast<float>(button.getHeight()) * 0.85f);
        const float tickWidth = fontSize * 1.05f;

        drawTickBox(g, button, 4.0f,
                    (static_cast<float>(button.getHeight()) - tickWidth) * 0.5f,
                    tickWidth, tickWidth,
                    button.getToggleState(), button.isEnabled(), highlighted, down);

        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(fontSize);
        if (!button.isEnabled())
            g.setOpacity(0.5f);

        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds()
                             .withTrimmedLeft(juce::roundToInt(tickWidth) + 5)
                             .withTrimmedRight(2),
                         juce::Justification::centredLeft, 10);
    }

    void drawTickBox(juce::Graphics &g, juce::Component &component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        juce::Rectangle<float> tickBounds(x, y, w, h);

        // Box outline
        g.setColour(juce::Colour::fromRGB(160, 160, 160));
        g.drawRoundedRectangle(tickBounds, 2.5f, 1.5f);

        // Box fill
        g.setColour(isEnabled ? juce::Colours::white : juce::Colour::fromRGB(230, 230, 230));
        g.fillRoundedRectangle(tickBounds.reduced(1.5f), 1.5f);

        if (ticked)
        {
            g.setColour(isEnabled ? juce::Colour::fromRGB(70, 130, 180)
                                  : juce::Colour::fromRGB(170, 170, 170));
            auto tick = juce::Path();
            tick.startNewSubPath(tickBounds.getX() + w * 0.15f, tickBounds.getCentreY());
            tick.lineTo(tickBounds.getX() + w * 0.40f, tickBounds.getBottom() - h * 0.20f);
            tick.lineTo(tickBounds.getRight() - w * 0.12f, tickBounds.getY() + h * 0.18f);
            g.strokePath(tick, juce::PathStrokeType(w * 0.18f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }
    }

private:
    // (no per-instance state needed)
};

static ScaledSelectorLookAndFeel &getScaledSelectorLookAndFeel()
{
    static ScaledSelectorLookAndFeel laf;
    return laf;
}

// ═══════════════════════════════════════════════════════════════
// DeviceSelectorDialog
// ═══════════════════════════════════════════════════════════════

DeviceSelectorDialog::DeviceSelectorDialog(juce::AudioDeviceManager &dm, int maxIn, int maxOut)
    : mgr(dm), initialMaxIn(maxIn), initialMaxOut(maxOut)
{
    updateSelectorComponent();
    startTimer(150);
}

DeviceSelectorDialog::~DeviceSelectorDialog()
{
    stopTimer();
    if (sel)
        sel->setLookAndFeel(nullptr);
}

void DeviceSelectorDialog::timerCallback()
{
    float cur = getFontScaleFactor();
    if (std::abs(cur - lastScale) < 0.005f)
        return;
    updateSelectorComponent();
}

void DeviceSelectorDialog::updateSelectorComponent()
{
    if (sel)
    {
        sel->setLookAndFeel(nullptr);
        removeChildComponent(sel.get());
    }

    naturalContentHeight = 0;
    lastScale = getFontScaleFactor();

    sel = std::make_unique<juce::AudioDeviceSelectorComponent>(
        mgr, 0, initialMaxIn, 0, initialMaxOut, false, false, false, false);

    sel->setLookAndFeel(&getScaledSelectorLookAndFeel());
    addAndMakeVisible(sel.get());

    juce::MessageManager::callAsync([this]
    {
        if (!sel) return;

        // Measure content at natural item height (default 24) and natural width
        constexpr int kNaturalWidth = 380;
        sel->setItemHeight(20);
        sel->setSize(kNaturalWidth, 3000);
        sel->resized();

        int maxBottom = 0;
        for (int i = 0; i < sel->getNumChildComponents(); ++i)
        {
            auto *c = sel->getChildComponent(i);
            if (c->isVisible())
                maxBottom = juce::jmax(maxBottom, c->getBottom());
        }
        naturalContentHeight = (maxBottom > 30) ? maxBottom : 300;

        float scale = getFontScaleFactor();
        lastScale = scale;
        sel->setTransform(juce::AffineTransform::scale(scale));

        if (onScaleChanged) onScaleChanged();
        if (getHeight() > 0) resized();
    });
}

void DeviceSelectorDialog::resized()
{
    if (sel)
    {
        constexpr int kNaturalWidth = 380;
        int h = (naturalContentHeight > 30) ? naturalContentHeight : 300;
        sel->setBounds(0, 0, kNaturalWidth, h);

        float scale = getFontScaleFactor();
        if (std::abs(scale - lastScale) > 0.005f)
        {
            lastScale = scale;
            sel->setTransform(juce::AffineTransform::scale(scale));
            if (onScaleChanged) onScaleChanged();
        }
    }
}

juce::String DeviceSelectorDialog::getCurrentDeviceName() const
{
    if (auto *d = mgr.getCurrentAudioDevice())
        return d->getName();
    return LanguageManager::getInstance().getText("audioDevice");
}

void DeviceSelectorDialog::getPreferredSize(int &outWidth, int &outHeight) const
{
    float scale = getFontScaleFactor();
    constexpr int kNaturalWidth = 380;
    int h = (naturalContentHeight > 30) ? naturalContentHeight : 300;
    outWidth  = static_cast<int>(kNaturalWidth * scale);
    outHeight = static_cast<int>(h * scale) + getSystemFrameHeight();

    if (auto *primary = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        outHeight = juce::jmin(outHeight, primary->userArea.getHeight() - 50);
}

// ═══════════════════════════════════════════════════════════════
// DeviceSelectorWindow
// ═══════════════════════════════════════════════════════════════

DeviceSelectorWindow::DeviceSelectorWindow(const juce::String &title,
                                           juce::AudioDeviceManager &dm,
                                           int maxIn, int maxOut,
                                           std::function<void(const juce::String &)> cb)
    : juce::DocumentWindow(title, juce::Colour::fromRGB(236, 236, 236),
                           juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton, true),
      onConfirmCallback(std::move(cb))
{
    auto *dlg = new DeviceSelectorDialog(dm, maxIn, maxOut);
    dlg->onScaleChanged = [this]
    { updateWindowSize(); };

    setContentOwned(dlg, true);
    setUsingNativeTitleBar(true);
    setResizable(false, false);
    setBackgroundColour(juce::Colour::fromRGB(236, 236, 236));
    startTimerHz(10);
    updateWindowSize();
    setTopLeftPosition(250, 150);
}

DeviceSelectorWindow::~DeviceSelectorWindow() { stopTimer(); }

void DeviceSelectorWindow::timerCallback()
{
    float cur = getFontScaleFactor();
    if (std::abs(cur - lastAppliedScale) < 0.005f)
        return;
    lastAppliedScale = cur;
    // Let DeviceSelectorDialog's own timer rebuild the selector;
    // wait for it to finish (onScaleChanged → updateWindowSize) before resizing.
    // Force an immediate rebuild so the window resize follows correct content height.
    if (auto* dlg = dynamic_cast<DeviceSelectorDialog*>(getContentComponent()))
        dlg->updateSelectorComponent();
}

void DeviceSelectorWindow::closeWindow()
{
    if (auto *dlg = dynamic_cast<DeviceSelectorDialog *>(getContentComponent()))
    {
        auto name = dlg->getCurrentDeviceName();
        if (onConfirmCallback)
            onConfirmCallback(name);
    }
    removeFromDesktop();
    if (onWindowClosed)
        onWindowClosed();
    delete this;
}

void DeviceSelectorWindow::closeButtonPressed() { closeWindow(); }

void DeviceSelectorWindow::updateWindowSize()
{
    if (auto *dlg = dynamic_cast<DeviceSelectorDialog *>(getContentComponent()))
    {
        int w = 0, h = 0;
        dlg->getPreferredSize(w, h);
        setSize(w, h);
    }
}
