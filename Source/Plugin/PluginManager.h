/*
 * PluginManager.h
 * VoicemeeterHost — VST Plugin Scanning, Loading, and State Management
 *
 * Manages the known-plugin list, active-plugin list, the plugin scan UI,
 * and persisted plugin state.
 */

#pragma once

#include <JuceHeader.h>

juce::ApplicationProperties& getAppProperties();

class PluginManager : public juce::ChangeListener
{
public:
    PluginManager();
    ~PluginManager() override;

    /** Format manager used to instantiate plugins. */
    juce::AudioPluginFormatManager& getFormatManager()  { return formatManager; }

    /** Global known-plugin list (persisted). */
    juce::KnownPluginList& getKnownPluginList()         { return knownPluginList; }

    /** Open the "Available Plugins" scan / list window. */
    void showPluginListEditor (juce::Component* parent);

    /** Remove plugins with < 2 channels from known list. */
    void removePluginsLackingInputOutput();

    /** Save all plugin states from the given graph. */
    void savePluginStatesFromGraph (juce::AudioProcessorGraph& graph);

    // ChangeListener — persists known list on change
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    /** Utility: create a deterministic key for a plugin description. */
    static juce::String getKey (const juce::String& type, const juce::PluginDescription& plugin);

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList          knownPluginList;

    class PluginListWindow;
    std::unique_ptr<PluginListWindow> pluginListWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManager)
};
