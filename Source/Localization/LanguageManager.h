/*
 * LanguageManager.h
 * VoicemeeterHost — 地區化/多語言支援
 *
 * LanguageManager 是單例類別，統一管理 UI 字串的加載與查詢。
 *
 * 流程概述：
 *   1. 十體人選擇語言 → setLanguageById()
 *   2. 先嘗試外部 JSON 覆寫檔（%APPDATA%/VoicemeeterHost/Strings/<id>.json）
 *   3. 如果外部檔不存在，則讀內嵌的 BinaryData 資源
 *   4. 將讓 getValue 映射到 JUCE 本地化字串網，讓選單項等內建 UI 元件自動讋譯
 *
 * 支援功能：
 *   - 多語言切換（getText(key) 回傳譯文）
 *   - 字型縮放倍數（CJK 等語言预預）
 *   - 語言標籤剪輯（右鍵菜單顯示用）
 */

#pragma once

#include <JuceHeader.h>

class LanguageManager
{
public:
    // 語言圓純資訊結構體：用於 getAvailableLanguages() 回傳列表
    struct LanguageInfo
    {
        juce::String id;          // 資源弹等名稱，例如 "English"
        juce::String displayName; // 顯示名稱，從 JSON "languageName" 讀入
    };

    // 取得單例實例（懸提建構，首次呼叫時預設載入英文）
    static LanguageManager& getInstance();

    // 切換語言（若與目前相同則無效）
    void   setLanguageById (const juce::String& languageId);
    // 取得目前已載入的語言 ID
    juce::String getCurrentLanguageId() const { return currentLanguageId; }

    /** 字型縮放倍數適用於 CJK / 其他腳本（預設 1.0）。
     *  從語言 JSON 的 languageInfo.fontScaling 欄位讀入。 */
    float  getFontScaling() const;

    /** 取得㌀ Language 」選單項目身的標籤文字。 */
    juce::String getLanguageLabel() const;

    /** 取得所有內嵌語言包的列表（進行 Binary所有 _json 資源）。 */
    juce::Array<LanguageInfo> getAvailableLanguages() const;

    /** 查詢譯文字串；未找到則回傳原始 `key`。 */
    juce::String getText (const juce::String& key) const;

private:
    LanguageManager();
    ~LanguageManager() = default;

    juce::String currentLanguageId; // 目前已載入的語言 ID
    juce::var    languageData;       // 載入的 JSON 樹，用於 getText() 查詢

    // 內部载入輔助方法
    void      loadLanguageById (const juce::String& languageId);
    juce::var loadJsonById     (const juce::String& languageId) const;
    void      applyJuceLocalisedStrings (const juce::var& data, const juce::String& languageId);

    LanguageManager (const LanguageManager&) = delete;
    LanguageManager& operator= (const LanguageManager&) = delete;
};
