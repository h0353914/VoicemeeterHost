/*
 * PluginManager.h
 * VoicemeeterHost — VST 插件掃描、載入與狀態管理
 *
 * PluginManager 負責：
 *   - 結合 JUCE AudioPluginFormatManager 支援 VST3（放換 Windows/Mac/Linux）
 *   - 音端持久化已掃描插件清單（KnownPluginList 寫入 .settings）
 *   - 提供掃描對話方塊（showPluginListEditor）
 *   - 對輸入/輸出少於 2 通道的插件自動從清單移除（removePluginsLackingInputOutput）
 *   - 將每個插件的狀態寫入 .settings（savePluginStatesFromGraph）
 */

#pragma once

#include <JuceHeader.h>

// 全域存取函式宣告：由 Main.cpp 實作。
juce::ApplicationProperties& getAppProperties();

// PluginManager 繼承自 ChangeListener 以接收 KnownPluginList 解多變化通知。
class PluginManager : public juce::ChangeListener
{
public:
    PluginManager();
    ~PluginManager() override;

    /** 取得建構插件實例所需的格式管理器。 */
    juce::AudioPluginFormatManager& getFormatManager()  { return formatManager; }

    /** 取得全域持久化的已知插件清單。 */
    juce::KnownPluginList& getKnownPluginList()         { return knownPluginList; }

    /** 開啟㌀可用插件」掃描 / 清單視窗。 */
    void showPluginListEditor (juce::Component* parent);

    /** 從清單移除輸入/輸出少於 2 通道的插件（不符合音訊連接要求）。 */
    void removePluginsLackingInputOutput();

    /** 把圖形中所有插件的當前狀態寫入使用者設定檔。 */
    void savePluginStatesFromGraph (juce::AudioProcessorGraph& graph);

    // ChangeListener 回調 — KnownPluginList 變化時自動寫入設定檔
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    /** 用於產生插件 key（狀態/位置儲存用）的工具函式。 */
    static juce::String getKey (const juce::String& type, const juce::PluginDescription& plugin);

private:
    juce::AudioPluginFormatManager formatManager;  // 格式管理器（包含 VST3 等格式）
    juce::KnownPluginList          knownPluginList; // 已掃描插件清單

    class PluginListWindow;                          // 內部內臣類別，管理掃描視窗
    std::unique_ptr<PluginListWindow> pluginListWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManager)
};
