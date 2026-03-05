/*
 * SystemTrayMenu.h
 * VoicemeeterHost — System Tray Icon & Application Core
 *
 * Owns the audio device manager, processor graph, player,
 * plugin manager, and persistent main-window content.
 * Tray icon: right-click menu, double-click opens main window.
 */

#pragma once

#include <JuceHeader.h>
#include "../Localization/LanguageManager.h"
#include "../Plugin/PluginManager.h"

#if JUCE_WINDOWS
#include "../Voicemeeter/VoicemeeterInterface.h"
#endif

class MainWindowContent;
class MainWindow;

class SystemTrayMenu : public juce::SystemTrayIconComponent,
                       private juce::Timer,
                       public juce::ChangeListener
{
public:
    SystemTrayMenu();
    ~SystemTrayMenu() override;

    void mouseDown(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;

    // ChangeListener — persists active-plugin list on change
    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
    void timerCallback() override;
    void reloadPlugins();
    void loadActivePlugins();
    void savePluginStates();
    void setTrayIcon();

    static void menuInvocationCallback(int id, SystemTrayMenu *);

    // ── Audio ──
#if JUCE_WINDOWS
    VoicemeeterDeviceManager deviceManager;
#else
    juce::AudioDeviceManager deviceManager;
#endif
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::AudioProcessorGraph::Node *inputNode = nullptr;
    juce::AudioProcessorGraph::Node *outputNode = nullptr;

    // ── Plugins ──
    PluginManager pluginManager;

    // ── UI ──
    std::unique_ptr<MainWindowContent> mainContent;
    std::unique_ptr<MainWindow> mainWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemTrayMenu)
};
