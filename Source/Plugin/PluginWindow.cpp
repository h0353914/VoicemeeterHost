/*
 * PluginWindow.cpp
 * VoicemeeterHost — VST 插件編輯器視窗實作
 *
 * 實作細節：
 *   - 建構時由 Node::properties 恢復上次位置
 *   - 銷毀時從 activePluginWindows 清單移除自身
 *   - 提供靜態函式：關閉狀態訊詢與工廠方法
 *   - ProgramAudioProcessorEditor：若插件選擇程式模式，使用屬性面板顯示程式清單
 */

#include "PluginWindow.h"

// 全域已開啟視窗清單（靜態變數於此編譯單元內）
static juce::Array<PluginWindow*> activePluginWindows;

// ─── Construction / destruction ──────────────────────────────

PluginWindow::PluginWindow (juce::Component* pluginEditor,
                            juce::AudioProcessorGraph::Node* o,
                            WindowFormatType t)
    : juce::DocumentWindow (pluginEditor->getName(), juce::Colours::lightgrey,
                            juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
      owner (o), type (t)
{
    setSize (400, 300);
    setUsingNativeTitleBar (true);
    setContentOwned (pluginEditor, true);

    setTopLeftPosition (
        owner->properties.getWithDefault (getLastXProp (type), juce::Random::getSystemRandom().nextInt (500)),
        owner->properties.getWithDefault (getLastYProp (type), juce::Random::getSystemRandom().nextInt (500)));

    owner->properties.set (getOpenProp (type), true);
    setVisible (true);
    activePluginWindows.add (this);
}

PluginWindow::~PluginWindow()
{
    activePluginWindows.removeFirstMatchingValue (this);
    clearContentComponent();
}

// ─── 靜態輔助方法 ──────────────────────────────────────────────
// 關閉指定節點的所有視窗，對全域清單倒序遍訪避免索引錄序問題。
void PluginWindow::closeCurrentlyOpenWindowsFor (juce::uint32 nodeUid)
{
    for (int i = activePluginWindows.size(); --i >= 0;)
        if (activePluginWindows.getUnchecked (i)->owner->nodeID.uid == nodeUid)
            delete activePluginWindows.getUnchecked (i);
}

void PluginWindow::closeAllCurrentlyOpenWindows()
{
    // Replaced deprecated runDispatchLoopUntil with simple deletion loop
    for (int i = activePluginWindows.size(); --i >= 0;)
        delete activePluginWindows.getUnchecked (i);
}

bool PluginWindow::containsActiveWindows()
{
    return activePluginWindows.size() > 0;
}

// ─── ProgramAudioProcessorEditor (fallback) ──────────────────

class ProgramAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ProgramAudioProcessorEditor (juce::AudioProcessor& p)
        : juce::AudioProcessorEditor (p)
    {
        setOpaque (true);
        addAndMakeVisible (panel);

        juce::Array<juce::PropertyComponent*> programs;
        int totalHeight = 0;

        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            auto name = p.getProgramName (i).trim();
            if (name.isEmpty()) name = "Unnamed";

            auto* pc = new juce::TextPropertyComponent (
                juce::Value (name), name, 256, false);
            pc->setEnabled (false);
            programs.add (pc);
            totalHeight += pc->getPreferredHeight();
        }

        panel.addProperties (programs);
        setSize (400, juce::jlimit (25, 400, totalHeight));
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::grey); }
    void resized() override { panel.setBounds (getLocalBounds()); }

private:
    juce::PropertyPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
};

// ─── 工廠方法：根據節點與類型尋找或新建編輯器視窗 ──────────────

PluginWindow* PluginWindow::getWindowFor (juce::AudioProcessorGraph::Node* node,
                                           WindowFormatType type)
{
    jassert (node != nullptr);

    for (int i = activePluginWindows.size(); --i >= 0;)
        if (activePluginWindows.getUnchecked (i)->owner == node
            && activePluginWindows.getUnchecked (i)->type == type)
            return activePluginWindows.getUnchecked (i);

    auto* processor = node->getProcessor();
    juce::AudioProcessorEditor* ui = nullptr;

    if (type == Normal)
    {
        ui = processor->createEditorIfNeeded();
        if (ui == nullptr) type = Generic;
    }

    if (ui == nullptr)
    {
        if (type == Generic || type == Parameters)
            ui = new juce::GenericAudioProcessorEditor (*processor);
        else if (type == Programs)
            ui = new ProgramAudioProcessorEditor (*processor);
    }

    if (ui != nullptr)
    {
        if (auto* plugin = dynamic_cast<juce::AudioPluginInstance*> (processor))
            ui->setName (plugin->getName());
        return new PluginWindow (ui, node, type);
    }
    return nullptr;
}

// ─── 事件處理 ──────────────────────────────────────────────────
// 移動時寫入位置屬性，下次開啟時將從屬性恢復。
void PluginWindow::moved()
{
    owner->properties.set (getLastXProp (type), getX());
    owner->properties.set (getLastYProp (type), getY());
}

// 關閉按鈕時：清除屬性記錄並 delete 視窗自身。
void PluginWindow::closeButtonPressed()
{
    owner->properties.set (getOpenProp (type), false);
    delete this;
}
