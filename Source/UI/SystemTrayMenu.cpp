/*
 * SystemTrayMenu.cpp
 * VoicemeeterHost — 系統匣圖示實作
 *
 * 核心逻輯：
 *   - 建構時載入語言、音訊設備狀態、建立 I/O 节點，還原圖形狀態。
 *   - 密圃鼠標右鍵： 50ms 延遲很出右鍵選單。
 *   - 雙擊建立 / 前置主視窗。
 *   - 選單項目：編輯插件、語言切換、離開。
 */

#include "SystemTrayMenu.h"
#include "MainWindow.h"
#include "../Plugin/PluginWindow.h"
#include "../Localization/LanguageManager.h"
#include "../Audio/AudioDeviceSettings.h"

#if JUCE_WINDOWS
#include <Windows.h>
#endif

juce::ApplicationProperties &getAppProperties();

// ─── Constants ───────────────────────────────────────────────

// 內郠常數：語言選單項首準 ID
// 使用廣大的數字剖長適變進厅與其他內嵌選單項衝突。
namespace
{
    constexpr int languageMenuItemBase = 2000000000;
}

// ═══════════════════════════════════════════════════════════════
// 建構 / 銷毀實作
// 建構時從設定檔讀入語言、音訊裝置狀態，建立圖 I/O 節點，還原圖形狀態。
// ═══════════════════════════════════════════════════════════════

SystemTrayMenu::SystemTrayMenu()
{
    // 讀取上次儲存的語言設定，預設為英文
    juce::String savedLang = getAppProperties().getUserSettings()->getValue("language", "English");
    LanguageManager::getInstance().setLanguageById(savedLang);

    // 讀取上次儲存的音訊設備狀態（若無則使用預設設備）
    std::unique_ptr<juce::XmlElement> savedAudio(
        getAppProperties().getUserSettings()->getXmlValue("audioDeviceState"));
    deviceManager.initialise(256, 256, savedAudio.get(), true);
    player.setProcessor(&graph);
    deviceManager.addAudioCallback(&player);

    // 建立主視窗內容元件，將設備管理器、插件清單、效果圖注入
    mainContent = std::make_unique<MainWindowContent>(
        deviceManager,
        pluginManager.getKnownPluginList(),
        pluginManager.getFormatManager(),
        graph);

    mainContent->onManagePlugins = [this]
    { reloadPlugins(); };
    mainContent->onGraphChanged = [this]
    {
        if (auto xml = mainContent->saveState())
        {
            getAppProperties().getUserSettings()->setValue("nodeGraphState", xml.get());
            getAppProperties().saveIfNeeded();
        }
    };
    mainContent->onScaleChanged = [this]
    {
        if (mainWindow != nullptr)
        {
            const float scale = ScaleSettingsManager::getInstance().getScaleFactor();
            int w = static_cast<int>(900 * scale);
            int h = static_cast<int>(560 * scale);
            if (auto *disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                w = juce::jmin(w, disp->userArea.getWidth());
                h = juce::jmin(h, disp->userArea.getHeight() - 50);
            }
            mainWindow->setSize(w, h);
        }
    };

    // 在效果圖中建立輸入/輸出 I/O 節點
    loadActivePlugins();

    // 還原上次儲存的節點圖狀態（連線、位置等）
    std::unique_ptr<juce::XmlElement> savedGraph(
        getAppProperties().getUserSettings()->getXmlValue("nodeGraphState"));
    if (savedGraph != nullptr)
        mainContent->loadState(*savedGraph);

    mainContent->onGraphChanged();

    // 設定工作列匣圖示并更新提示字號
    setTrayIcon();
    setIconTooltip(LanguageManager::getInstance().getText("appName"));
}

SystemTrayMenu::~SystemTrayMenu()
{
    savePluginStates();
    mainWindow.reset();
    mainContent.reset();
}

// ═══════════════════════════════════════════════════════════════
// 工作列匣圖示設定
// 讀取 BinaryData 中嵌入的 PNG 圖片，設定工作列匣圖示。
// 目前一律使用黑色圖示（適合亮色工作列即背景）。
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::setTrayIcon()
{
    // Always use the black icon (for light taskbar backgrounds)
    juce::Image icon = juce::ImageFileFormat::loadFrom (BinaryData::menu_icon_png,
                                                          BinaryData::menu_icon_pngSize);
    setIconImage(icon, icon);
}

// ═══════════════════════════════════════════════════════════════
// 音訊效果圖 I/O 節點設定
// loadActivePlugins(): 建立 ID=1000000 的 audioInputNode 與 ID=1000001 的 audioOutputNode。
// savePluginStates(): 將非 I/O 節點的插件狀態保存到 APP 設定檔。
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::loadActivePlugins()
{
    constexpr int INPUT = 1000000;
    constexpr int OUTPUT = INPUT + 1;

    PluginWindow::closeAllCurrentlyOpenWindows();
    graph.clear();

    inputNode = graph.addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode),
        juce::AudioProcessorGraph::NodeID(INPUT));

    outputNode = graph.addNode(
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode),
        juce::AudioProcessorGraph::NodeID(OUTPUT));
}

void SystemTrayMenu::savePluginStates()
{
    for (const auto *node : graph.getNodes())
    {
        if (node == nullptr || node->getProcessor() == nullptr)
            continue;
        auto *proc = node->getProcessor();
        if (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor *>(proc))
            continue;

        if (auto *pi = dynamic_cast<juce::AudioPluginInstance *>(proc))
        {
            juce::PluginDescription desc;
            pi->fillInPluginDescription(desc);
            juce::String key = PluginManager::getKey("state", desc);
            juce::MemoryBlock mb;
            proc->getStateInformation(mb);
            getAppProperties().getUserSettings()->setValue(key, mb.toBase64Encoding());
        }
    }
    getAppProperties().saveIfNeeded();
}

// ═══════════════════════════════════════════════════════════════
// ChangeListener 實作
// 目前釋放處理會影響主內容元件中的節點圖視覺化；未來可擴展自動儲存。
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    juce::ignoreUnused(source);
}

// ═══════════════════════════════════════════════════════════════
// 匣圖示鼠標事件處理
// mouseDown 右鍵: 啟動 50ms 計時器後呈現即岈選單。
// mouseDoubleClick: 雙擊匠匣圖示开啟主視窗（若已開則置前）。
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isRightButtonDown())
    {
        juce::Process::makeForegroundProcess();
        startTimer(50);
    }
}

void SystemTrayMenu::mouseDoubleClick(const juce::MouseEvent &)
{
    if (mainWindow == nullptr)
    {
        mainWindow = std::make_unique<MainWindow>(mainContent.get());
        mainWindow->onWindowClosed = [this]
        { mainWindow = nullptr; };
    }
    else
    {
        mainWindow->toFront(true);
    }
}

// ═══════════════════════════════════════════════════════════════
// 計時器回調 → 建立右鍵即岈選單
// 屈展岈利：標題組（app 名稱）、編輯插件選項、語言子選單、離開選項。
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::timerCallback()
{
    stopTimer();

    juce::PopupMenu menu;
    auto &lang = LanguageManager::getInstance();

    menu.addSectionHeader(lang.getText("appName"));
    menu.addItem(2, lang.getText("editPlugins"));

    // 語言子選單：列舉所有可用語言，目前使用中的語言所對應項目會勾選
    juce::PopupMenu langMenu;
    int langId = languageMenuItemBase;
    for (const auto &l : lang.getAvailableLanguages())
    {
        bool current = (l.id == lang.getCurrentLanguageId());
        langMenu.addItem(langId, l.displayName, true, current);
        ++langId;
    }
    menu.addSubMenu(lang.getLanguageLabel(), langMenu);


    menu.addSeparator();
    menu.addItem(1, lang.getText("quit"));

    menu.showMenuAsync(
        juce::PopupMenu::Options().withMousePosition(),
        juce::ModalCallbackFunction::forComponent(menuInvocationCallback, this));
}

void SystemTrayMenu::menuInvocationCallback(int id, SystemTrayMenu *im)
{
    if (id == 1) // 離開：儲存插件狀態後退出應用程式
    {
        im->savePluginStates();
        juce::JUCEApplication::getInstance()->quit();
        return;
    }
    if (id == 2) // 編輯插件：開啟插件清單管理視窗
    {
        im->reloadPlugins();
        return;
    }

    if (id >= languageMenuItemBase) // 語言切換：更新語言并重新展示選單
    {
        auto langs = LanguageManager::getInstance().getAvailableLanguages();
        int idx = id - languageMenuItemBase;
        if (idx >= 0 && idx < langs.size())
        {
            LanguageManager::getInstance().setLanguageById(langs[idx].id);
            getAppProperties().getUserSettings()->setValue("language", langs[idx].id);
            getAppProperties().saveIfNeeded();
            im->startTimer(50);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// 插件清單視窗管理
// reloadPlugins(): 委托 PluginManager 開啟插件清單視窗，不需要背景工作线程。
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::reloadPlugins()
{
    pluginManager.showPluginListEditor(nullptr);
}
