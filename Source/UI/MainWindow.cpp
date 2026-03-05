/*
 * MainWindow.cpp
 * VoicemeeterHost — Main Application Window Implementation
 *
 * Contains ScaleSettingsContentComponent, ScaleSettingsWindow,
 * MainWindowContent, and MainWindow.
 */

#include "MainWindow.h"
#include "../Plugin/PluginWindow.h"
#include "../Localization/LanguageManager.h"

juce::ApplicationProperties &getAppProperties(); // defined in Main.cpp

// ═══════════════════════════════════════════════════════════════
// ScaleSettingsContentComponent
// ═══════════════════════════════════════════════════════════════

class ScaleSettingsContentComponent : public juce::Component
{
public:
    std::function<void()> onScaleChanged;

    ScaleSettingsContentComponent()
    {
        label = std::make_unique<juce::Label>("lbl",
                                              LanguageManager::getInstance().getText("scaleFactorTitle"));
        label->setColour(juce::Label::textColourId, juce::Colours::black);
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label.get());

        combo = std::make_unique<juce::ComboBox>("scaleCombo");
        combo->addItem("100%", 1);
        combo->addItem("125%", 2);
        combo->addItem("150%", 3);
        combo->addItem("200%", 4);

        float cur = ScaleSettingsManager::getInstance().getScaleFactor();
        if (std::fabs(cur - 1.00f) < 0.01f)
            combo->setSelectedId(1, juce::dontSendNotification);
        else if (std::fabs(cur - 1.25f) < 0.01f)
            combo->setSelectedId(2, juce::dontSendNotification);
        else if (std::fabs(cur - 1.50f) < 0.01f)
            combo->setSelectedId(3, juce::dontSendNotification);
        else if (std::fabs(cur - 2.00f) < 0.01f)
            combo->setSelectedId(4, juce::dontSendNotification);
        else
            combo->setSelectedId(1, juce::dontSendNotification);

        combo->onChange = [this]
        {
            const float scales[] = {1.0f, 1.25f, 1.5f, 2.0f};
            int id = combo->getSelectedId();
            if (id >= 1 && id <= 4)
            {
                ScaleSettingsManager::getInstance().setScaleFactor(scales[id - 1]);
                if (onScaleChanged)
                    onScaleChanged();
            }
        };
        addAndMakeVisible(combo.get());

        okButton = std::make_unique<juce::TextButton>(
            LanguageManager::getInstance().getText("OK"));
        okButton->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(70, 130, 180));
        okButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        okButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(40, 100, 150));
        okButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        okButton->onClick = [this]
        {
            for (auto *p = getParentComponent(); p != nullptr; p = p->getParentComponent())
                if (auto *rw = dynamic_cast<juce::ResizableWindow *>(p))
                {
                    rw->removeFromDesktop();
                    delete rw;
                    break;
                }
        };
        addAndMakeVisible(okButton.get());
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colour::fromRGB(236, 236, 236));
    }

    void resized() override
    {
        const float scale = getFontScaleFactor();

        const int pad = juce::jmax(4, juce::roundToInt(10.0f * scale));
        const int rowH = juce::jmax(18, juce::roundToInt(26.0f * scale));
        const int lblH = juce::jmax(14, juce::roundToInt(18.0f * scale));
        const int gap = juce::jmax(2, juce::roundToInt(6.0f * scale));
        const int btnW = juce::jmax(50, juce::roundToInt(72.0f * scale));

        auto b = getLocalBounds().reduced(pad);

        label->setFont(juce::Font(juce::FontOptions{}.withHeight(14.0f * scale)));
        label->setBounds(b.removeFromTop(lblH));
        b.removeFromTop(gap);

        combo->setBounds(b.removeFromTop(rowH));
        b.removeFromTop(gap);

        auto btnRow = b.removeFromTop(rowH);
        okButton->setBounds(btnRow.removeFromRight(btnW));
    }

private:
    std::unique_ptr<juce::Label> label;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::TextButton> okButton;
};

// ═══════════════════════════════════════════════════════════════
// ScaleSettingsWindow
// ═══════════════════════════════════════════════════════════════

class ScaleSettingsWindow : public juce::ResizableWindow, private juce::Timer
{
public:
    std::function<void()> onScaleChanged;

    ScaleSettingsWindow()
        : juce::ResizableWindow(LanguageManager::getInstance().getText("scaleSettings"),
                                juce::Colour(0xFFECECEC), true)
    {
        auto *content = new ScaleSettingsContentComponent();
        content->onScaleChanged = [this]
        {
            updateWindowSize(true);
            if (onScaleChanged)
                onScaleChanged();
        };

        setContentOwned(content, true);
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setBackgroundColour(juce::Colour::fromRGB(236, 236, 236));

        updateWindowSize(false);
        lastAppliedScale = getFontScaleFactor();
        startTimerHz(10);
    }

    ~ScaleSettingsWindow() override { stopTimer(); }

private:
    float lastAppliedScale = -1.0f;

    void timerCallback() override
    {
        const float cur = getFontScaleFactor();
        if (std::abs(cur - lastAppliedScale) < 0.005f)
            return;
        lastAppliedScale = cur;
        updateWindowSize(true);
    }

    void updateWindowSize(bool keepCentre)
    {
        const float scale = getFontScaleFactor();
        const int w = juce::jmax(280, juce::roundToInt(320.0f * scale));
        const int h = juce::jmax(120, juce::roundToInt(135.0f * scale));

        if (keepCentre && getWidth() > 0 && getHeight() > 0)
        {
            const auto centre = getBounds().getCentre();
            setSize(w, h);
            setCentrePosition(centre);
        }
        else
        {
            centreWithSize(w, h);
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// MainWindowContent
// ═══════════════════════════════════════════════════════════════

MainWindowContent::MainWindowContent(juce::AudioDeviceManager &dm,
                                     juce::KnownPluginList &kpl,
                                     juce::AudioPluginFormatManager &fmt,
                                     juce::AudioProcessorGraph &g)
    : deviceManager(dm), knownPlugins(kpl), formatManager(fmt), graph(g)
{
    ScaleSettingsManager::getInstance().loadSettings();

    graphCanvas = std::make_unique<GraphComponent>(dm, kpl, fmt, g);

    graphCanvas->onDoubleClickLeft = [this]
    { showInputDialog(); };
    graphCanvas->onDoubleClickRight = [this]
    { showOutputDialog(); };
    graphCanvas->onManagePlugins = [this]
    { if (onManagePlugins) onManagePlugins(); };
    graphCanvas->onGraphChanged = [this]
    { if (onGraphChanged) onGraphChanged(); };
    graphCanvas->onEditNode = [this](int /*nodeId*/, NodeType type)
    {
        auto *wnd = new DeviceSelectorWindow(
            type == NodeType::Input
                ? LanguageManager::getInstance().getText("audioInput")
                : LanguageManager::getInstance().getText("audioOutput"),
            deviceManager,
            type == NodeType::Input ? 256 : 0,
            type == NodeType::Output ? 256 : 0,
            [this](const juce::String &)
            {
                std::unique_ptr<juce::XmlElement> audioState(deviceManager.createStateXml());
                getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
                getAppProperties().saveIfNeeded();
            });
        wnd->addToDesktop(juce::ComponentPeer::windowIsResizable);
        wnd->setVisible(true);
        wnd->toFront(true);
    };
    addAndMakeVisible(*graphCanvas);

    settingsBtn = std::make_unique<juce::TextButton>(
        LanguageManager::getInstance().getText("settings"));
    settingsBtn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(70, 130, 180));
    settingsBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    settingsBtn->onClick = [this]
    { showScaleSettings(); };
    settingsBtn->setBounds(8, 8, 70, 28);
    addAndMakeVisible(*settingsBtn);
}

void MainWindowContent::showInputDialog()
{
    auto *wnd = new DeviceSelectorWindow(
        LanguageManager::getInstance().getText("audioInput"),
        deviceManager, 256, 0,
        [this](const juce::String &name)
        {
            graphCanvas->addNode(name, NodeType::Input);
            std::unique_ptr<juce::XmlElement> audioState(deviceManager.createStateXml());
            getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
            getAppProperties().saveIfNeeded();
        });
    wnd->addToDesktop(juce::ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->toFront(true);
}

void MainWindowContent::showOutputDialog()
{
    auto *wnd = new DeviceSelectorWindow(
        LanguageManager::getInstance().getText("audioOutput"),
        deviceManager, 0, 256,
        [this](const juce::String &name)
        {
            graphCanvas->addNode(name, NodeType::Output);
            std::unique_ptr<juce::XmlElement> audioState(deviceManager.createStateXml());
            getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
            getAppProperties().saveIfNeeded();
        });
    wnd->addToDesktop(juce::ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->toFront(true);
}

void MainWindowContent::showScaleSettings()
{
    if (scaleSettingsWnd != nullptr)
    {
        scaleSettingsWnd->toFront(true);
        return;
    }
    auto *wnd = new ScaleSettingsWindow();
    scaleSettingsWnd = wnd;
    wnd->onScaleChanged = [this]
    {
        resized();
        repaint();
        if (onScaleChanged)
            onScaleChanged();
    };
    wnd->addToDesktop(juce::ComponentPeer::windowIsResizable);
    wnd->setVisible(true);
    wnd->grabKeyboardFocus();
    wnd->toFront(true);
}

void MainWindowContent::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xFFE8E8E8));
}

void MainWindowContent::resized()
{
    auto bounds = getLocalBounds();

    int btnHeight = static_cast<int>(28 * getDPIScaleFactor());
    int btnWidth = static_cast<int>(70 * getDPIScaleFactor());
    int padding = 8;

    graphCanvas->setBounds(bounds);

    int bottomY = getHeight() - btnHeight - padding;
    settingsBtn->setBounds(padding, bottomY, btnWidth, btnHeight);
}

std::unique_ptr<juce::XmlElement> MainWindowContent::saveState() const
{
    return graphCanvas->saveState();
}

void MainWindowContent::loadState(const juce::XmlElement &xml)
{
    graphCanvas->loadState(xml);
}

// ═══════════════════════════════════════════════════════════════
// MainWindow (DocumentWindow shell)
// ═══════════════════════════════════════════════════════════════

MainWindow::MainWindow(MainWindowContent *content)
    : juce::DocumentWindow(LanguageManager::getInstance().getText("appName"),
                           juce::Colour::fromRGB(26, 26, 26),
                           juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton)
{
    content->setSize(900, 560);
    setContentNonOwned(content, true);

    setUsingNativeTitleBar(true);
    setResizable(true, false);
    setResizeLimits(600, 400, 4096, 4096);
    centreWithSize(900, 560);
    setVisible(true);
}

void MainWindow::closeButtonPressed()
{
    if (onWindowClosed)
        onWindowClosed();
}
