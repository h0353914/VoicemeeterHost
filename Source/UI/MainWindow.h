/*
 * MainWindow.h
 * VoicemeeterHost — Main Application Window
 *
 * MainWindowContent: top-level component with graph canvas + settings button.
 * MainWindow: DocumentWindow shell wrapping MainWindowContent.
 */

#pragma once

#include <JuceHeader.h>
#include "../Graph/GraphComponent.h"
#include "../Audio/AudioDeviceSettings.h"

// ─── MainWindowContent ───────────────────────────────────────
class MainWindowContent : public juce::Component
{
public:
    MainWindowContent(juce::AudioDeviceManager &deviceManager,
                      juce::KnownPluginList &knownPlugins,
                      juce::AudioPluginFormatManager &formatManager,
                      juce::AudioProcessorGraph &graph);
    ~MainWindowContent() override = default;

    void resized() override;
    void paint(juce::Graphics &g) override;

    std::function<void()> onManagePlugins;
    std::function<void()> onGraphChanged;
    std::function<void()> onScaleChanged;

    std::unique_ptr<juce::XmlElement> saveState() const;
    void loadState(const juce::XmlElement &xml);

    GraphComponent &getGraphCanvas() { return *graphCanvas; }

private:
    juce::AudioDeviceManager &deviceManager;
    juce::KnownPluginList &knownPlugins;
    juce::AudioPluginFormatManager &formatManager;
    juce::AudioProcessorGraph &graph;

    std::unique_ptr<GraphComponent> graphCanvas;
    std::unique_ptr<juce::TextButton> settingsBtn;
    juce::Component::SafePointer<juce::Component> scaleSettingsWnd;

    void showInputDialog();
    void showOutputDialog();
    void showScaleSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindowContent)
};

// ─── MainWindow (DocumentWindow shell) ──────────────────────
class SystemTrayMenu;

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(MainWindowContent *content);

    void closeButtonPressed() override;

    std::function<void()> onWindowClosed;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
