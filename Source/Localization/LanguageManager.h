/*
 * LanguageManager.h
 * VoicemeeterHost — Localization Support
 *
 * Manages loading of JSON-based language files from embedded binary resources
 * or external files.  Provides translated UI strings via getText().
 */

#pragma once

#include <JuceHeader.h>

class LanguageManager
{
public:
    struct LanguageInfo
    {
        juce::String id;          // Resource stem, e.g. "English"
        juce::String displayName; // Display name read from JSON "languageName"
    };

    static LanguageManager& getInstance();

    void   setLanguageById (const juce::String& languageId);
    juce::String getCurrentLanguageId() const { return currentLanguageId; }

    /** Font scale multiplier for CJK / other scripts (default 1.0). */
    float  getFontScaling() const;

    /** Label for the "Language" menu entry itself. */
    juce::String getLanguageLabel() const;

    /** All embedded language packs. */
    juce::Array<LanguageInfo> getAvailableLanguages() const;

    /** Look up a translated string; returns `key` if not found. */
    juce::String getText (const juce::String& key) const;

private:
    LanguageManager();
    ~LanguageManager() = default;

    juce::String currentLanguageId;
    juce::var    languageData;

    void      loadLanguageById (const juce::String& languageId);
    juce::var loadJsonById     (const juce::String& languageId) const;
    void      applyJuceLocalisedStrings (const juce::var& data, const juce::String& languageId);

    LanguageManager (const LanguageManager&) = delete;
    LanguageManager& operator= (const LanguageManager&) = delete;
};
