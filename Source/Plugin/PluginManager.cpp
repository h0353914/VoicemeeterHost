/*
 * PluginManager.cpp
 * VoicemeeterHost — VST Plugin Scanning, Loading, and State Management
 */

#include "PluginManager.h"
#include "../Localization/LanguageManager.h"

// ─── PluginListWindow ────────────────────────────────────────

class PluginManager::PluginListWindow : public juce::DocumentWindow
{
public:
    PluginListWindow (PluginManager& mgr)
        : juce::DocumentWindow (LanguageManager::getInstance().getText ("availablePlugins"),
                                juce::Colour::fromRGB (236, 236, 236),
                                juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
          owner (mgr)
    {
        const auto deadMansPedalFile =
            getAppProperties().getUserSettings()->getFile().getSiblingFile ("RecentlyCrashedPluginsList");

        setContentOwned (new juce::PluginListComponent (mgr.formatManager,
                                                         mgr.knownPluginList,
                                                         deadMansPedalFile,
                                                         getAppProperties().getUserSettings()), true);

        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (300, 400, 800, 1500);
        setTopLeftPosition (60, 60);

        restoreWindowStateFromString (
            getAppProperties().getUserSettings()->getValue ("listWindowPos"));
        setVisible (true);
    }

    ~PluginListWindow() override
    {
        getAppProperties().getUserSettings()->setValue ("listWindowPos", getWindowStateAsString());
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        owner.removePluginsLackingInputOutput();
        owner.pluginListWindow = nullptr;
    }

private:
    PluginManager& owner;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginListWindow)
};

// ─── PluginManager ───────────────────────────────────────────

PluginManager::PluginManager()
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat (new juce::VST3PluginFormat());
#endif
#if JUCE_PLUGINHOST_VST
    formatManager.addFormat (new juce::VSTPluginFormat());
#endif
#if JUCE_PLUGINHOST_AU
    formatManager.addFormat (new juce::AudioUnitPluginFormat());
#endif

    // Restore persisted known-plugin list
    if (auto xml = getAppProperties().getUserSettings()->getXmlValue ("pluginList"))
        knownPluginList.recreateFromXml (*xml);

    knownPluginList.addChangeListener (this);
}

PluginManager::~PluginManager()
{
    knownPluginList.removeChangeListener (this);
}

void PluginManager::showPluginListEditor (juce::Component* /*parent*/)
{
    if (pluginListWindow == nullptr)
        pluginListWindow = std::make_unique<PluginListWindow> (*this);
    pluginListWindow->toFront (true);
}

void PluginManager::removePluginsLackingInputOutput()
{
    for (const auto& plugin : knownPluginList.getTypes())
    {
        if (plugin.numInputChannels < 2 || plugin.numOutputChannels < 2)
            knownPluginList.removeType (plugin);
    }
}

void PluginManager::savePluginStatesFromGraph (juce::AudioProcessorGraph& graph)
{
    for (const auto* node : graph.getNodes())
    {
        if (node == nullptr || node->getProcessor() == nullptr) continue;

        auto* proc = node->getProcessor();
        if (dynamic_cast<juce::AudioProcessorGraph::AudioGraphIOProcessor*> (proc) != nullptr)
            continue;

        if (auto* pi = dynamic_cast<juce::AudioPluginInstance*> (proc))
        {
            juce::PluginDescription desc;
            pi->fillInPluginDescription (desc);

            juce::MemoryBlock mb;
            proc->getStateInformation (mb);
            getAppProperties().getUserSettings()->setValue (getKey ("state", desc), mb.toBase64Encoding());
        }
    }
    getAppProperties().saveIfNeeded();
}

void PluginManager::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &knownPluginList)
    {
        if (auto xml = knownPluginList.createXml())
        {
            getAppProperties().getUserSettings()->setValue ("pluginList", xml.get());
            getAppProperties().saveIfNeeded();
        }
    }
}

juce::String PluginManager::getKey (const juce::String& type,
                                     const juce::PluginDescription& plugin)
{
    return "plugin-" + type.toLowerCase() + "-"
           + plugin.name + plugin.version + plugin.pluginFormatName;
}
