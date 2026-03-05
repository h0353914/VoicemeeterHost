/*
 * MainWindow.h
 * VoicemeeterHost — 應用程式主視窗
 *
 * 包含兩個主要類別：
 *
 * 1. MainWindowContent
 *    頂層元件，包含 GraphComponent、設定按鈕與尺度視窗連結。
 *    封裝從樹狀圖剪存/讀入的逻輯。
 *    這個類別的生命週期由 SystemTrayMenu 持有，不隨視窗常駮達。
 *
 * 2. MainWindow
 *    DocumentWindow 外殼，包裝 MainWindowContent。
 *    只對按下關閉按鈕做回調通知，不自行持有內容。
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

// ─── MainWindow（DocumentWindow 外殼）───────────────────────────────
// MainWindow 是視窗容器，不擁有 MainWindowContent。
// 關閉按鈕處理自動通知 SystemTrayMenu 以就釋指標。
class SystemTrayMenu;

class MainWindow : public juce::DocumentWindow
{
public:
    // 建構：傳入 MainWindowContent 指標（自身不擁有）
    MainWindow(MainWindowContent *content);

    // 關閉按鈕定義：调用 onWindowClosed 回調
    void closeButtonPressed() override;

    // 視窗關閉時由 SystemTrayMenu 清除安全指標
    std::function<void()> onWindowClosed;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
