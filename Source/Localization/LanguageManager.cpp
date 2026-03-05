/*
 * LanguageManager.cpp
 * VoicemeeterHost — Localization Support
 */

#include "LanguageManager.h"

namespace BD = BinaryData;

LanguageManager& LanguageManager::getInstance()
{
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager() : currentLanguageId ("English")
{
    loadLanguageById ("English");
}

void LanguageManager::setLanguageById (const juce::String& languageId)
{
    if (languageId != currentLanguageId)
    {
        currentLanguageId = languageId;
        loadLanguageById (languageId);
    }
}

juce::String LanguageManager::getText (const juce::String& key) const
{
    if (languageData.isObject())
    {
        juce::var translation = languageData[juce::Identifier (key)];
        if (! translation.isVoid())
            return translation.toString();
    }
    return key;
}

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
