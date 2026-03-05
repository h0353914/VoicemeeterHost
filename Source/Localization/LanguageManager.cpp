/*
 * LanguageManager.cpp
 * VoicemeeterHost — 地區化支援實作
 *
 * 實作詳情：
 *   - getInstance()：單例模式，第一次呼叫時預設載入英文。
 *   - loadJsonById()：先嘗試読取外部 JSON 覆寫檔，如果不存在則回落圖內嵌資源。
 *   - applyJuceLocalisedStrings()：將 JSON 內的 juceStrings 映射到
 *     juce::LocalisedStrings，讓 JUCE 內建 UI 元件（如對話方塊按鈕）自動讋譯。
 *   - getAvailableLanguages()：遍詣 BinaryData 所有 _json 資源，讀入語言清單。
 */

#include "LanguageManager.h"

// BinaryData 命名空間縮撓：對內嵌 JSON 檔的存取使用 BD:: 前置雅稱。
namespace BD = BinaryData;

// 單例訪問器：俩建構屬于小命名函式內的靜態變數，確保整屆1 實一個實例。
LanguageManager& LanguageManager::getInstance()
{
    static LanguageManager instance; // 首次呼叫时預設載入英文
    return instance;
}

// 建構元代碼：㗖随英文語言，載入對應 JSON。
LanguageManager::LanguageManager() : currentLanguageId ("English")
{
    loadLanguageById ("English");
}

// 切換語言：若語言 ID 不同於目前，則更新 ID 並重新載入。
void LanguageManager::setLanguageById (const juce::String& languageId)
{
    if (languageId != currentLanguageId)
    {
        currentLanguageId = languageId;
        loadLanguageById (languageId);
    }
}

// 查詢譯文：若 JSON 樹內有對應鍵則回傳譯文，否則回傳原始 key。
juce::String LanguageManager::getText (const juce::String& key) const
{
    if (languageData.isObject())
    {
        juce::var translation = languageData[juce::Identifier (key)];
        if (! translation.isVoid())
            return translation.toString();
    }
    return key; // 朔找不到則回傳原 key，對開發階段有助導找缺失字串
}

// 讀入 JSON：
//   1. 先嘗試 %APPDATA%/VoicemeeterHost/Strings/<id>.json（外部覆寫操作）
//   2. 如果外部檔不存在，則從 BinaryData 中讀入內嵌資源
juce::var LanguageManager::loadJsonById (const juce::String& languageId) const
{
    // Try external override first
    auto languageFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                            .getChildFile ("VoicemeeterHost")
                            .getChildFile ("Strings")
                            .getChildFile (languageId + ".json");

    if (languageFile.existsAsFile())
    {
        auto fileContent = languageFile.loadFileAsString();
        return juce::JSON::parse (fileContent);
    }

    // Fall back to embedded binary data
    juce::String resourceName = languageId + "_json";
    int dataSize = 0;
    const char* data = BD::getNamedResource (resourceName.toRawUTF8(), dataSize);

    if (data != nullptr && dataSize > 0)
    {
        auto jsonString = juce::String::fromUTF8 (data, dataSize);
        return juce::JSON::parse (jsonString);
    }

    return {};
}

void LanguageManager::loadLanguageById (const juce::String& languageId)
{
    auto data = loadJsonById (languageId);

    if (! data.isVoid())
    {
        languageData = data;
        applyJuceLocalisedStrings (data, languageId);
        return;
    }

    if (languageId != "English")
        loadLanguageById ("English");
}

void LanguageManager::applyJuceLocalisedStrings (const juce::var& data, const juce::String& languageId)
{
    if (languageId == "English")
    {
        juce::LocalisedStrings::setCurrentMappings (nullptr);
        return;
    }

    auto juceStrings = data[juce::Identifier ("juceStrings")];
    if (! juceStrings.isObject())
    {
        juce::LocalisedStrings::setCurrentMappings (nullptr);
        return;
    }

    juce::String mappingContent = "language: " + languageId + "\n\n";

    if (auto* obj = juceStrings.getDynamicObject())
    {
        for (auto& prop : obj->getProperties())
        {
            auto original   = prop.name.toString().replace ("\\", "\\\\").replace ("\"", "\\\"");
            auto translated = prop.value.toString().replace ("\\", "\\\\").replace ("\"", "\\\"");
            mappingContent += "\"" + original + "\" = \"" + translated + "\"\n";
        }
    }

    juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (mappingContent, false));
}

juce::Array<LanguageManager::LanguageInfo> LanguageManager::getAvailableLanguages() const
{
    juce::Array<LanguageInfo> languages;

    for (int i = 0; i < BD::namedResourceListSize; ++i)
    {
        juce::String resourceName (BD::namedResourceList[i]);

        if (! resourceName.endsWith ("_json"))
            continue;

        auto langId = resourceName.dropLastCharacters (5);
        auto data   = loadJsonById (langId);

        if (data.isObject())
        {
            auto langInfoVar = data[juce::Identifier ("languageInfo")];
            if (langInfoVar.isObject())
            {
                auto nameVar     = langInfoVar[juce::Identifier ("languageName")];
                auto displayName = nameVar.isVoid() ? langId : nameVar.toString();
                languages.add ({ langId, displayName });
            }
        }
    }

    return languages;
}

float LanguageManager::getFontScaling() const
{
    if (languageData.isObject())
    {
        auto langInfoVar = languageData[juce::Identifier ("languageInfo")];
        if (langInfoVar.isObject())
        {
            auto scaling = langInfoVar[juce::Identifier ("fontScaling")];
            if (! scaling.isVoid())
                return static_cast<float> (scaling);
        }
    }
    return 1.0f;
}

juce::String LanguageManager::getLanguageLabel() const
{
    if (languageData.isObject())
    {
        auto langInfoVar = languageData[juce::Identifier ("languageInfo")];
        if (langInfoVar.isObject())
        {
            auto label = langInfoVar[juce::Identifier ("language")];
            if (! label.isVoid())
                return label.toString();
        }
    }
    return "Language";
}
