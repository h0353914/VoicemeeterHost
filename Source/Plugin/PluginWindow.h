/*
 * PluginWindow.h
 * VoicemeeterHost — VST Plugin Editor Window
 *
 * Opens a native window for each plugin's custom UI.
 * Falls back to GenericAudioProcessorEditor when no custom editor is available.
 */

#pragma once

#include <JuceHeader.h>

juce::ApplicationProperties& getAppProperties();

class PluginWindow : public juce::DocumentWindow
{
public:
    enum WindowFormatType { Normal = 0, Generic, Programs, Parameters, NumTypes };

    PluginWindow (juce::Component* pluginEditor,
                  juce::AudioProcessorGraph::Node* node,
                  WindowFormatType type);
    ~PluginWindow() override;

    static PluginWindow* getWindowFor (juce::AudioProcessorGraph::Node* node,
                                       WindowFormatType type);

    static void closeCurrentlyOpenWindowsFor (juce::uint32 nodeUid);
    static void closeAllCurrentlyOpenWindows();
    static bool containsActiveWindows();

    void moved() override;
    void closeButtonPressed() override;

private:
    juce::AudioProcessorGraph::Node* owner;
    WindowFormatType type;

    float getDesktopScaleFactor() const override { return 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

// Property-key helpers
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

inline juce::String getLastXProp (PluginWindow::WindowFormatType t) { return "uiLastX_" + toString (t); }
inline juce::String getLastYProp (PluginWindow::WindowFormatType t) { return "uiLastY_" + toString (t); }
inline juce::String getOpenProp  (PluginWindow::WindowFormatType t) { return "uiopen_"  + toString (t); }
