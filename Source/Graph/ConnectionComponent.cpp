/*
 * ConnectionComponent.cpp
 * VoicemeeterHost — 連接線繪製實作
 *
 * 實作 WireDraw::drawWire 與 WireDraw::drawDragWire。
 * 兩字函式均使用三次貝諯曲線（Cubic Bezier Curve）繪製連接線。
 * 控制點的 X 坐標取在兩端為點的中點，以產生平滑的 S 型曲線。
 */

#include "ConnectionComponent.h"

namespace
{
    // 內尴0預定色彩表：
    const juce::Colour wireCol    { 0xFF888888 }; // 霧氾灰 — 連接線預設色
    const juce::Colour wireActive { 0xFFE67E22 }; // 橙色 — 活躍連接線色
    const juce::Colour wireBad    { 0xFFCC2222 }; // 紅色 — 無效候選連線色
}

// drawWire：繪製两點之間的 Bezier 連接線。
// 控制點 cx 為 a.x 與 b.x 的中點，若空间足夠平滑。
void WireDraw::drawWire (juce::Graphics& g,
                          juce::Point<int> a, juce::Point<int> b,
                          bool active)
{
    g.setColour (active ? wireActive : wireCol); // 依照狀態選色
    juce::Path p;
    p.startNewSubPath (a.toFloat());   // 起點
    const float cx = (a.x + b.x) * 0.5f; // 中點 X，產生平滑的 S 型
    p.cubicTo (cx, static_cast<float> (a.y),
               cx, static_cast<float> (b.y),
               static_cast<float> (b.x), static_cast<float> (b.y));
    g.strokePath (p, juce::PathStrokeType (2.f)); // 線寬 2px
}

// drawDragWire：繪製拖曳中的臨時連線。
// anchor 為連線起點（port 中心），cursor 為滑鼠目前位置。
void WireDraw::drawDragWire (juce::Graphics& g,
                              juce::Point<int> anchor, juce::Point<int> cursor,
                              bool valid)
{
    g.setColour (valid ? wireActive : wireBad); // 有效連接為橙色，無效為紅色
    juce::Path p;
    const float cx = (anchor.x + cursor.x) * 0.5f; // 中點 X
    p.startNewSubPath (anchor.toFloat()); // 起點
    p.cubicTo (cx, static_cast<float> (anchor.y),
               cx, static_cast<float> (cursor.y),
               static_cast<float> (cursor.x), static_cast<float> (cursor.y));
    g.strokePath (p, juce::PathStrokeType (2.f)); // 線寬 2px
}
