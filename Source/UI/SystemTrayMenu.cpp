/*
 * SystemTrayMenu.cpp
 * VoicemeeterHost — System Tray Icon Implementation
 *
 * Owns the core audio chain, creates the graph I/O nodes,
 * loads/saves state, and manages the tray right-click menu
 * (edit plugins, language, invert icon, quit).
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

namespace
{
    constexpr int languageMenuItemBase = 2000000000;
}

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

SystemTrayMenu::SystemTrayMenu()
{
    // Language
    juce::String savedLang = getAppProperties().getUserSettings()->getValue("language", "English");
    LanguageManager::getInstance().setLanguageById(savedLang);

    // Audio device
    std::unique_ptr<juce::XmlElement> savedAudio(
        getAppProperties().getUserSettings()->getXmlValue("audioDeviceState"));
    deviceManager.initialise(256, 256, savedAudio.get(), true);
    player.setProcessor(&graph);
    deviceManager.addAudioCallback(&player);

    // Main content
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

    // Graph I/O nodes
    loadActivePlugins();

    // Restore saved graph state
    std::unique_ptr<juce::XmlElement> savedGraph(
        getAppProperties().getUserSettings()->getXmlValue("nodeGraphState"));
    if (savedGraph != nullptr)
        mainContent->loadState(*savedGraph);

    mainContent->onGraphChanged();

    // Tray icon
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
// Tray icon
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::setTrayIcon()
{
    // Always use the black icon (for light taskbar backgrounds)
    juce::Image icon = juce::ImageFileFormat::loadFrom (BinaryData::menu_icon_png,
                                                          BinaryData::menu_icon_pngSize);
    setIconImage(icon, icon);
}

// ═══════════════════════════════════════════════════════════════
// Graph I/O setup
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
// ChangeListener
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    juce::ignoreUnused(source);
}

// ═══════════════════════════════════════════════════════════════
// Tray mouse events
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
// Timer → right-click menu
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::timerCallback()
{
    stopTimer();

    juce::PopupMenu menu;
    auto &lang = LanguageManager::getInstance();

    menu.addSectionHeader(lang.getText("appName"));
    menu.addItem(2, lang.getText("editPlugins"));

    // Language sub-menu
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
    if (id == 1) // Quit
    {
        im->savePluginStates();
        juce::JUCEApplication::getInstance()->quit();
        return;
    }
    if (id == 2) // Edit Plugins
    {
        im->reloadPlugins();
        return;
    }

    if (id >= languageMenuItemBase)
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
// Plugin list window
// ═══════════════════════════════════════════════════════════════

void SystemTrayMenu::reloadPlugins()
{
    pluginManager.showPluginListEditor(nullptr);
}
