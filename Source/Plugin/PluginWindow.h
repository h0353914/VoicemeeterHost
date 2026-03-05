/*
 * PluginWindow.h
 * VoicemeeterHost — VST 插件編輯器視窗
 *
 * PluginWindow 封裝插件編輯器 UI，展示於該插件自定義的視窗。
 * 若插件未提供自定義編輯器，則回落使用 GenericAudioProcessorEditor。
 *
 * 功能：
 *   - 管理全域已開啟視窗清單（activePluginWindows）
 *   - closeCurrentlyOpenWindowsFor / closeAllCurrentlyOpenWindows 允許從外部回除視窗
 *   - 位置儲存：視窗移動時自動寫入設定檔，getWindowFor 時恢復上次位置
 */

#pragma once

#include <JuceHeader.h>

juce::ApplicationProperties& getAppProperties();

class PluginWindow : public juce::DocumentWindow
{
public:
    // 視窗類型列舉（決定展示哪種編輯器）
    enum WindowFormatType { Normal = 0, Generic, Programs, Parameters, NumTypes };

    // 建構視窗：將編輯器元件設為內容，並恢復上次位置
    PluginWindow (juce::Component* pluginEditor,
                  juce::AudioProcessorGraph::Node* node,
                  WindowFormatType type);
    ~PluginWindow() override;

    // 工廠方法：尋找或建立指定節點的編輯器視窗
    static PluginWindow* getWindowFor (juce::AudioProcessorGraph::Node* node,
                                       WindowFormatType type);

    // 關閉連帶指定節點 ID 的所有視窗
    static void closeCurrentlyOpenWindowsFor (juce::uint32 nodeUid);
    // 關閉所有目前開啟的插件視窗
    static void closeAllCurrentlyOpenWindows();
    // 判斷目前是否有任何插件視窗開啟中
    static bool containsActiveWindows();

    // 移動時寫入位置屬性存入設定檔
    void moved() override;
    // 按下關閉按鈕時釋出小資源並加以 delete
    void closeButtonPressed() override;

private:
    juce::AudioProcessorGraph::Node* owner; // 擁有此視窗的圖形節點
    WindowFormatType type;                   // 編輯器類型

    // 覆寫 1.0 以不被 JUCE 自動縮放（插件自行處理縮放）
    float getDesktopScaleFactor() const override { return 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

// 插件視窗屬性鍵輔助函式
// 用於儲存/讀取視窗位置與開啟狀態到 Node::properties。
inline juce::String toString (PluginWindow::WindowFormatType t)
{
    switch (t)
    {
        case PluginWindow::Normal:     return "Normal";
        case PluginWindow::Generic:    return "Generic";
        case PluginWindow::Programs:   return "Programs";
        case PluginWindow::Parameters: return "Parameters";
        default:                       return {};
    }
}

// 回傳儲存最後 X 坐標的屬性鍵
inline juce::String getLastXProp (PluginWindow::WindowFormatType t) { return "uiLastX_" + toString (t); }
// 回傳儲存最後 Y 坐標的屬性鍵
inline juce::String getLastYProp (PluginWindow::WindowFormatType t) { return "uiLastY_" + toString (t); }
// 回傳記載視窗已開啟的屬性鍵
inline juce::String getOpenProp  (PluginWindow::WindowFormatType t) { return "uiopen_"  + toString (t); }
