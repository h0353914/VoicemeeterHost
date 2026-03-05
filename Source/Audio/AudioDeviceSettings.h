/*
 * AudioDeviceSettings.h
 * VoicemeeterHost — UI Scale Manager & Device Selector Dialog/Window
 *
 * Replaces LookAndFeel_V3 with LookAndFeel_V4.
 * Uses modern Font(FontOptions{}) API.
 */

#pragma once

#include <JuceHeader.h>

juce::ApplicationProperties &getAppProperties();

// ─── UI Scale Manager ────────────────────────────────────────

class ScaleSettingsManager
{
public:
    static ScaleSettingsManager &getInstance();
    float getScaleFactor() const;
    void setScaleFactor(float scale);
    void loadSettings();
    void saveSettings();

private:
    float scaleFactor = 1.0f;
};

// ─── Device Selector Dialog ──────────────────────────────────

class DeviceSelectorDialog : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onScaleChanged;

    DeviceSelectorDialog(juce::AudioDeviceManager &dm, int maxIn, int maxOut);
    ~DeviceSelectorDialog() override;

    void timerCallback() override;
    void updateSelectorComponent();
    void resized() override;
    juce::String getCurrentDeviceName() const;
    void getPreferredSize(int &outWidth, int &outHeight) const;

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> sel;
    juce::AudioDeviceManager &mgr;
    int initialMaxIn, initialMaxOut;
    float lastScale = -1.0f;
    int naturalContentHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorDialog)
};

// ─── Device Selector Window ──────────────────────────────────

class DeviceSelectorWindow : public juce::DocumentWindow, private juce::Timer
{
public:
    std::function<void()> onWindowClosed;

    DeviceSelectorWindow(const juce::String &title,
                         juce::AudioDeviceManager &dm,
                         int maxIn, int maxOut,
                         std::function<void(const juce::String &)> cb);
    ~DeviceSelectorWindow() override;

    void timerCallback() override;
    void closeWindow();
    void closeButtonPressed() override;
    void updateWindowSize();

private:
    float lastAppliedScale = -1.0f;
    std::function<void(const juce::String &)> onConfirmCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorWindow)
};
