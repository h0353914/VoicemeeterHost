/*
 * NodeComponent.h
 * VoicemeeterHost — 節點資料結構与 圖形用戶上的繪製輔助
 *
 * 本檔案定義：
 *
 * 1. DPI 輔助 inline 函式
 *    getDPIScaleFactor() — 由 ScaleSettingsManager 提供的縮放倍數。
 *    getFontScaleFactor() — DPI 倍數 × 語言字型縮放（支援 CJK 字型）。
 *    inline 定義在此 .h 中以確保每個編譯單元都能直接圖内聯。
 *
 * 2. NodeType 遠舋強型列舉
 *    Input  — 音訊輸入端
 *    Output — 音訊輸出端
 *    Plugin — VST3 插件效果器
 *
 * 3. PluginNode 結構體
 *    包含識別符、類型、名稱、畫布位置、AudioProcessorGraph 節點 ID。
 *    所有尺度相關的引用 getDPIScaleFactor() 從而支援即時 DPI 渲染。
 *
 * 4. NodeDraw namespace
 *    drawSideNode   — 繪製左右區域的 Input/Output 節點方塊。
 *    drawPluginNode — 繪製中間區域的插件節點方塊。
 */

#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioDeviceSettings.h"
#include "../Localization/LanguageManager.h"

// ─── DPI 輔助（inline，單一定義） ──────────────────────────────────
// 下列兩個 inline 函式必須定義於此 header，
// 如此所有 #include "NodeComponent.h" 的編譯單元都可直接呼叫。

// 取得 UI 縮放倍數（來自 ScaleSettingsManager，預設 1.0）
inline float getDPIScaleFactor()
{
    return ScaleSettingsManager::getInstance().getScaleFactor();
}

// 取得字型縮放倍數（DPI 倍數 × 語言字型縮放，支援中文等字型）
inline float getFontScaleFactor()
{
    return getDPIScaleFactor() * LanguageManager::getInstance().getFontScaling();
}

// ─── 節點類型列舉 ─────────────────────────────────────────────
// Input  — 代表音訊輸入裝置（差裝於圖形左區）
// Output — 代表音訊輸出裝置（差裝於圖形右區）
// Plugin — 代表 VST3 插件效果器（差裝於圖形中間區）

enum class NodeType { Input, Output, Plugin };

// ─── PluginNode data ─────────────────────────────────────────

struct PluginNode
{
    int                               id   { 0 };
    NodeType                          type { NodeType::Plugin };
    juce::String                      name;
    juce::Point<int>                  pos  { 200, 100 };
    juce::AudioProcessorGraph::NodeID graphNodeId { 0 };

    // 未縮放的基礎尺度（投入 getDPIScaleFactor() 瞫諦縮放）
    static constexpr int kW      = 140; // 插件節點寬度
    static constexpr int kH      = 56;  // 插件節點高度
    static constexpr int kSideH  = 40;  // 側邊區節點高度
    static constexpr int kPortR  = 7;   // 接包外形半徑

    static int getWidth()      { return static_cast<int> (kW     * getDPIScaleFactor()); }
    static int getHeight()     { return static_cast<int> (kH     * getDPIScaleFactor()); }
    static int getSideHeight() { return static_cast<int> (kSideH * getDPIScaleFactor()); }
    static int getPortRadius() { return static_cast<int> (kPortR * getDPIScaleFactor()); }

    bool hasInputPort()  const { return type != NodeType::Input;  }
    bool hasOutputPort() const { return type != NodeType::Output; }

    juce::Point<int>     inputPort()  const { return { pos.x,              pos.y + getHeight() / 2 }; }
    juce::Point<int>     outputPort() const { return { pos.x + getWidth(), pos.y + getHeight() / 2 }; }
    juce::Rectangle<int> bounds()     const { return { pos.x, pos.y, getWidth(), getHeight() }; }
};

// ─── 繪製輔助函式 — 在 NodeComponent.cpp 定義 ─────────────

namespace NodeDraw
{
    /** 繪製側邊區域節點（Input 或 Output）的外觀。
     *  @param b       邊界矩形（對應圖形左、右區)
     *  @param portPos 接包中心畫布位置
     *  @param isSelected 是否被選取（顯示黃色選取框） */
    void drawSideNode (juce::Graphics& g, const PluginNode& n,
                       juce::Rectangle<int> bounds,
                       juce::Point<int> portPos,
                       bool isSelected);

    /** 繪製中間區域插件節點的外觀（含陰影、名稱、推引文字、輸出接包）。
     *  @param isSelected 是否被選取（顯示黃色選取框） */
    void drawPluginNode (juce::Graphics& g, const PluginNode& n,
                         bool isSelected);
}
