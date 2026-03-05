/*
 * Main.cpp
 * VoicemeeterHost — 應用程式進入點
 *
 * 本檔案定義 JUCE 應用程式的核心類別 VoicemeeterHostApp，
 * 繼承自 juce::JUCEApplication，負責：
 *   1. 全域外觀樣式（LookAndFeel_V4）初始化
 *   2. 使用者設定檔的產生與讀取
 *   3. 系統匣圖示（SystemTrayMenu）的建立
 *   4. 多實例（-multi-instance=<name>）命令列參數支援
 *
 * 注意：本專案已棄用 LookAndFeel_V3，全面改用 LookAndFeel_V4。
 */

#include <JuceHeader.h>
#include "UI/SystemTrayMenu.h"
#include "Localization/LanguageManager.h"

// 若編譯環境未啟用 VST3 插件主機支援，則在編譯期觸發錯誤。
// 需在 CMakeLists.txt 中設定 JUCE_PLUGINHOST_VST3=1。
#if ! (JUCE_PLUGINHOST_VST3)
 #error "VoicemeeterHost requires VST3 plugin hosting support."
#endif

// ═══════════════════════════════════════════════════════════════
// 應用程式主類別
// VoicemeeterHostApp 承擔從啟動到關閉的完整生命週期管理。
// ═══════════════════════════════════════════════════════════════

class VoicemeeterHostApp : public juce::JUCEApplication
{
public:
    VoicemeeterHostApp() = default;

    // 回傳應用程式名稱，用於設定檔資料夾命名與視窗標題顯示。
    const juce::String getApplicationName()    override { return "VoicemeeterHost"; }
    // 回傳應用程式版本字串，顯示於「關於」對話方塊等處。
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    // 是否允許同時執行多個實例。
    // 若命令列含 -multi-instance=<名稱> 參數，則允許多實例並以名稱區分設定檔。
    bool moreThanOneInstanceAllowed() override
    {
        return getParameter ("-multi-instance").size() == 2;
    }

    // 應用程式初始化，在主事件迴圈開始前被 JUCE 呼叫一次。
    // 依序完成：設定檔建立 → 外觀初始化 → 系統匣建立。
    void initialise (const juce::String&) override
    {
        // 設定使用者設定檔的存放位置與副檔名
        juce::PropertiesFile::Options opts;
        opts.applicationName = getApplicationName();
        opts.filenameSuffix  = "settings";

        // 處理命令列參數，必要時替多實例設定不同的檔名後綴
        checkArguments (opts);

        // 建立並初始化 ApplicationProperties，負責持久化儲存使用者設定
        appProperties = std::make_unique<juce::ApplicationProperties>();
        appProperties->setStorageParameters (opts);

        // 套用全域淺色主題色彩方案（LookAndFeel_V4）
        // 以下逐一設定各元件的前景、背景與強調色，確保整體視覺一致性。
        lookAndFeel.setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xFFECECEC));
        lookAndFeel.setColour (juce::PopupMenu::textColourId,                 juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::PopupMenu::headerTextColourId,           juce::Colour (0xFF444444));
        lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xFF4682B4));
        lookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        lookAndFeel.setColour (juce::ListBox::backgroundColourId,              juce::Colours::white);
        lookAndFeel.setColour (juce::ListBox::outlineColourId,                 juce::Colour (0xFFCCCCCC));
        lookAndFeel.setColour (juce::ListBox::textColourId,                    juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xFFDDDDDD));
        lookAndFeel.setColour (juce::TableHeaderComponent::outlineColourId,    juce::Colour (0xFFBBBBBB));
        lookAndFeel.setColour (juce::TableHeaderComponent::textColourId,       juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::TableHeaderComponent::highlightColourId,  juce::Colour (0xFFCCCCCC));
        lookAndFeel.setColour (juce::ComboBox::backgroundColourId,             juce::Colours::white);
        lookAndFeel.setColour (juce::ComboBox::textColourId,                   juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::ComboBox::outlineColourId,                juce::Colour (0xFFBBBBBB));
        lookAndFeel.setColour (juce::ComboBox::arrowColourId,                  juce::Colour (0xFF555555));
        lookAndFeel.setColour (juce::TextButton::buttonColourId,               juce::Colour (0xFF7A8A9A));
        lookAndFeel.setColour (juce::TextButton::buttonOnColourId,             juce::Colour (0xFF547080));
        lookAndFeel.setColour (juce::TextButton::textColourOffId,              juce::Colours::white);
        lookAndFeel.setColour (juce::TextButton::textColourOnId,               juce::Colours::white);
        lookAndFeel.setColour (juce::TextEditor::backgroundColourId,           juce::Colours::white);
        lookAndFeel.setColour (juce::TextEditor::textColourId,                 juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::TextEditor::outlineColourId,              juce::Colour (0xFFBBBBBB));
        lookAndFeel.setColour (juce::ScrollBar::thumbColourId,                 juce::Colour (0xFFAAAAAA));
        lookAndFeel.setColour (juce::Label::textColourId,                      juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::Label::backgroundColourId,                juce::Colour (0x00000000));
        lookAndFeel.setColour (juce::ToggleButton::textColourId,               juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::ToggleButton::tickColourId,               juce::Colour (0xFF4682B4));
        lookAndFeel.setColour (juce::ToggleButton::tickDisabledColourId,       juce::Colour (0xFFCCCCCC));
        lookAndFeel.setColour (juce::Slider::thumbColourId,                    juce::Colour (0xFF4682B4));
        lookAndFeel.setColour (juce::Slider::trackColourId,                    juce::Colour (0xFFDDDDDD));
        lookAndFeel.setColour (juce::Slider::backgroundColourId,               juce::Colour (0xFFF2F2F2));
        lookAndFeel.setColour (juce::Slider::textBoxTextColourId,              juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::Slider::textBoxBackgroundColourId,        juce::Colours::white);
        lookAndFeel.setColour (juce::Slider::textBoxOutlineColourId,           juce::Colour (0xFFBBBBBB));

        // 將自訂的外觀樣式設為全域預設，影響所有 JUCE 元件的繪製
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        // 建立系統匣圖示，這也是整個音訊處理鏈的擁有者
        tray = std::make_unique<SystemTrayMenu>();
    }

    // 應用程式關閉時的清理工作，JUCE 保證在事件迴圈結束後呼叫。
    // 必須先銷毀 tray（含音訊資源），再重置 appProperties，最後解除 LookAndFeel 綁定。
    void shutdown() override
    {
        tray.reset();           // 停止音訊、關閉所有視窗
        appProperties.reset();  // 確保設定已寫入磁碟
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr); // 恢復預設外觀
    }

    // 當作業系統（或使用者）要求結束應用程式時呼叫（例如工作管理員）。
    void systemRequestedQuit() override
    {
        juce::JUCEApplicationBase::quit();
    }

    // 全域命令管理器，供快捷鍵與選單命令使用（目前保留供未來擴充）
    juce::ApplicationCommandManager commandManager;
    // 使用者設定持久化管理器（讀寫 .settings 檔案）
    std::unique_ptr<juce::ApplicationProperties> appProperties;
    // 全域外觀樣式物件（淺色主題，LookAndFeel_V4）
    juce::LookAndFeel_V4 lookAndFeel;

private:
    // 系統匣圖示與音訊核心的擁有者（生命週期與 App 相同）
    std::unique_ptr<SystemTrayMenu> tray;

    // 從命令列參數陣列中搜尋指定參數名稱，並回傳 [名稱, 值] 的 StringArray。
    // 若未找到則回傳空數組。
    juce::StringArray getParameter (const juce::String& lookFor) const
    {
        juce::StringArray params = getCommandLineParameterArray();
        juce::StringArray found;
        for (const auto& param : params)
        {
            if (param.contains (lookFor))
            {
                found.add (lookFor);
                int delim = param.indexOf (0, "=") + 1;
                found.add (param.substring (delim));
                return found;
            }
        }
        return found;
    }

    // 解析命令列參數，若找到 -multi-instance=<名稱> 則修改設定檔後綴，
    // 讓不同實例使用獨立的設定檔（例如 MyInst.settings）。
    void checkArguments (juce::PropertiesFile::Options& opts)
    {
        auto multiInst = getParameter ("-multi-instance");
        if (multiInst.size() == 2)
            opts.filenameSuffix = multiInst[1] + "." + opts.filenameSuffix;
    }
};

// ═══════════════════════════════════════════════════════════════
// 全域存取函式
// 這三個函式讓其他編譯單元能安全地存取應用程式級別的單例物件，
// 而不需要直接 #include 或持有 VoicemeeterHostApp 的指標。
// ═══════════════════════════════════════════════════════════════

// 取得應用程式實例的強型別參考（斷言轉型，執行期若失敗會中止）
static VoicemeeterHostApp& getApp()
{
    return *dynamic_cast<VoicemeeterHostApp*> (juce::JUCEApplication::getInstance());
}

// 取得全域命令管理器（用於快捷鍵、選單指令綁定）
juce::ApplicationCommandManager& getCommandManager()
{
    return getApp().commandManager;
}

// 取得使用者設定持久化管理器（讀寫應用程式設定檔）
juce::ApplicationProperties& getAppProperties()
{
    return *getApp().appProperties;
}

// JUCE 巨集：設定應用程式進入點，展開為平台對應的 main() 或 WinMain() 實作
START_JUCE_APPLICATION (VoicemeeterHostApp)
