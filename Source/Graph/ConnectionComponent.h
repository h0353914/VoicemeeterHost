/*
 * ConnectionComponent.h
 * VoicemeeterHost — 連接線（Wire）數據結構與繪製輔助函式
 *
 * 本檔案定義：
 *
 * 1. NodeWire
 *    超簡單的 POD 結構體，談定一條線（從哪個節點連到哪個節點）。
 *    fromNode / toNode 對應 PluginNode::id。
 *
 * 2. WireDraw namespace
 *    提供兩個静態函式：
 *    drawWire — 用三次義貝諯曲線瘫繪穩報連接線，支援漸层色彩。
 *    drawDragWire — 使用者拖曳連線期間顯示的臨時線段，支持有效/無效色彩代碼。
 */

#pragma once

#include <JuceHeader.h>

// ─── Wire data ───────────────────────────────────────────────

struct NodeWire
{
    int fromNode { -1 };
    int toNode   { -1 };
};

// ─── Drawing helpers ─────────────────────────────────────────

namespace WireDraw
{
    /** 在兩點之間以三次貝諯曲線繪製連接線。
     *  active=true 時使用主要色（橙色），否則安靜灰色。 */
    void drawWire (juce::Graphics& g,
                   juce::Point<int> a, juce::Point<int> b,
                   bool active);

    /** 繪製拖曳連線時的臨時曲線。
     *  valid=true 時使用橙色（允許連接），否則使用紅色（禁止連接）。 */
    void drawDragWire (juce::Graphics& g,
                       juce::Point<int> anchor, juce::Point<int> cursor,
                       bool valid);
}
