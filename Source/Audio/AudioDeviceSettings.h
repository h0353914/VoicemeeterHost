/*
 * AudioDeviceSettings.h
 * VoicemeeterHost — 介面縮放管理器與裝置選擇對話方塊 / 視窗
 *
 * 本檔案提供三個主要類別：
 *
 * 1. ScaleSettingsManager
 *    單例類別，負責管理 UI 縮放倍數（影響所有 DPI 相關計算）。
 *    設定後將自動寫入 .settings 檔並在提供組件內處處生效。
 *
 * 2. DeviceSelectorDialog
 *    juce::Component 子類，內嵌 juce::AudioDeviceSelectorComponent。
 *    支援即時縮放回應（Timer 每 150ms 偵測尺度變化）。
 *
 * 3. DeviceSelectorWindow
 *    juce::DocumentWindow 子類，包裝 DeviceSelectorDialog。
 *    管理視窗尺度自動調整，並在關閉時回調確認處理函式。
 *
 * 相依資訊：
 *   使用 LookAndFeel_V4（已棄用 V3）。
 *   使用新式 Font(FontOptions{}) API。
 */

#pragma once

#include <JuceHeader.h>

// 全域存取函式宣告：由 Main.cpp 實作，其他編譯單元透過此函式取得 ApplicationProperties 實例。
juce::ApplicationProperties &getAppProperties();

// ─── UI 縮放管理器 ────────────────────────────────────
// ScaleSettingsManager 是單例類別，將 UI 縮放倍數存入使用者設定檔並展露全域查詢介面。
// 地圖層級：
//   getDPIScaleFactor() → ScaleSettingsManager::getScaleFactor()
//   getFontScaleFactor() → getDPIScaleFactor() * LanguageManager::getFontScaling()
class ScaleSettingsManager
{
public:
    // 取得單例實例（懸提建構、線程安全）
    static ScaleSettingsManager &getInstance();
    // 取得目前縮放倍數（預設 1.0f）
    float getScaleFactor() const;
    // 設定新縮放倍數並立即寫入設定檔
    void setScaleFactor(float scale);
    // 從使用者設定檔讀入儲存的縮放倍數（合法範圍 0.5 – 3.0）
    void loadSettings();
    // 將目前縮放倍數寫入使用者設定檔
    void saveSettings();

private:
    float scaleFactor = 1.0f; // 實際繃著的縮放倍數
};

// ─── 裝置選擇對話方塊 ─────────────────────────────────────────
// DeviceSelectorDialog 封裝 JUCE 內建的 AudioDeviceSelectorComponent。
// 支援即時根據字型尺度縮放內容並通知父視窗更新尺度。

class DeviceSelectorDialog : public juce::Component, private juce::Timer
{
public:
    // 小尺度變化時由外部設定（例如 DeviceSelectorWindow）來更新視窗尺度。
    std::function<void()> onScaleChanged;

    // 建構對話方塊：傳入音訊裝置管理器及輸入/輸出最大通道數
    DeviceSelectorDialog(juce::AudioDeviceManager &dm, int maxIn, int maxOut);
    ~DeviceSelectorDialog() override;

    // Timer 回呼：每 150ms 偵測字型尺度是否變化，變化則重建選擇元件
    void timerCallback() override;
    // 重新建立內部的 AudioDeviceSelectorComponent，並重新計算內容高度
    void updateSelectorComponent();
    void resized() override;
    // 取得目前裝置名稱（用於顯示於式樣指示器）
    juce::String getCurrentDeviceName() const;
    // 回傳適宜的視窗尺度（已乘以字型縮放）
    void getPreferredSize(int &outWidth, int &outHeight) const;

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> sel; // JUCE 內建選擇元件
    juce::AudioDeviceManager &mgr;    // 音訊裝置管理器引用
    int initialMaxIn, initialMaxOut;  // 僅建時記錄的最大通道數
    float lastScale = -1.0f;          // 上次檢查時的字型尺度，用於判斷是否有變化
    int naturalContentHeight = 0;     // 自然高度（未縮放），從子引用延伸算得

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorDialog)
};

// ─── 裝置選擇視窗 ────────────────────────────────────────────
// DeviceSelectorWindow 把 DeviceSelectorDialog 包裝在 DocumentWindow 內。
// 持有確認回呼，在視窗關閉時通知呼叫端裝置名稱。

class DeviceSelectorWindow : public juce::DocumentWindow, private juce::Timer
{
public:
    // 視窗關閉時由外部排除安全指標使用
    std::function<void()> onWindowClosed;

    // 建構視窗：傳入標題、裝置管理器、最大通道數與確認回呼
    DeviceSelectorWindow(const juce::String &title,
                         juce::AudioDeviceManager &dm,
                         int maxIn, int maxOut,
                         std::function<void(const juce::String &)> cb);
    ~DeviceSelectorWindow() override;

    // Timer 回呼：以 10Hz 偵測字型尺度變化，觸發對話重建
    void timerCallback() override;
    // 手動關閉視窗（收集裝置名稱、呼叫回呼、釋放資源）
    void closeWindow();
    void closeButtonPressed() override;
    // 根據 DeviceSelectorDialog 回傳的適宜尺度重新調整視窗大小
    void updateWindowSize();

private:
    float lastAppliedScale = -1.0f;                           // 上次處理的縮放備存
    std::function<void(const juce::String &)> onConfirmCallback; // 關閉時回傳裝置名稱

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorWindow)
};
