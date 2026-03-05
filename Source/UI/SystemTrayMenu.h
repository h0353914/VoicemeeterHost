/*
 * SystemTrayMenu.h
 * VoicemeeterHost — 系統匣圖示與應用程式核心
 *
 * SystemTrayMenu 繼承自：
 *   - juce::SystemTrayIconComponent：提供工作列匣圖示、鼠標治對話方塊 + 雙擊記事
 *   - juce::Timer： 50ms 计時器用於延遲很出右鍵選單（適配 Windows 案例下專業程式的要求）
 *   - juce::ChangeListener：監聽插件渀單變化
 *
 * 擁有之物件：
 *   - AudioDeviceManager：音訊輸入/輸出設備管理器實例
 *     （Windows 上為 VoicemeeterDeviceManager）
 *   - AudioProcessorGraph：音訊處理圖
 *   - AudioProcessorPlayer：將圖接入輸出設備回調
 *   - PluginManager：插件掃描與狀態管理
 *   - MainWindowContent / MainWindow：主視窗內容與外殼
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

    /** 監聽插件清單變化，並在變化時持久化插件狀態至應用程式設定。 */
    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
    /** 計時器觸發（50ms 延遲），用於顯示右鍵選單，避免在 Windows 上的順序問題。 */
    void timerCallback() override;
    /** 開啟插件清單編輯器，讓使用者掃描與管理 VST3 插件。 */
    void reloadPlugins();
    /** 在音訊效果圖中建立輸入/輸出 I/O 節點。 */
    void loadActivePlugins();
    /** 將所有插件的狀態（preset 資料）持久化至應用程式設定檔。 */
    void savePluginStates();
    /** 設定工作列匣圖示（讀取 BinaryData 中的 PNG 圖片）。 */
    void setTrayIcon();

    /** 右鍵選單項目的回調，根據 id 執行離開、插件管理或語言切換。 */
    static void menuInvocationCallback(int id, SystemTrayMenu *);

    // ── 音訊裝置與效果圖 ──
    /** Windows 平台：使用 Voicemeeter 虛擬設備管理器；其他平台：使用標準 JUCE 設備管理器。 */
#if JUCE_WINDOWS
    VoicemeeterDeviceManager deviceManager;
#else
    juce::AudioDeviceManager deviceManager;
#endif
    /** 音訊處理效果圖，負責連接各插件節點。 */
    juce::AudioProcessorGraph graph;
    /** 將效果圖的輸出接入音訊設備回調的播放器。 */
    juce::AudioProcessorPlayer player;
    /** 效果圖中的音訊輸入 I/O 節點（graph 的音訊來源）。 */
    juce::AudioProcessorGraph::Node *inputNode = nullptr;
    /** 效果圖中的音訊輸出 I/O 節點（graph 的音訊目的地）。 */
    juce::AudioProcessorGraph::Node *outputNode = nullptr;

    // ── 插件管理 ──
    /** 負責 VST3 插件掃描、KnownPluginList 持久化與插件清單視窗的管理器。 */
    PluginManager pluginManager;

    // ── 使用者介面 ──
    /** 主視窗的內容元件（擁有音訊效果圖視覺化、設定按鈕等 UI）。 */
    std::unique_ptr<MainWindowContent> mainContent;
    /** 主視窗外殼（DocumentWindow），雙擊匣圖示時建立；關閉後設為 nullptr。 */
    std::unique_ptr<MainWindow> mainWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemTrayMenu)
};
