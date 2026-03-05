/*
 * ConnectionComponent.cpp
 * VoicemeeterHost — Wire / Connection rendering
 */

#include "ConnectionComponent.h"

namespace
{
    const juce::Colour wireCol    { 0xFF888888 };
    const juce::Colour wireActive { 0xFFE67E22 };
    const juce::Colour wireBad    { 0xFFCC2222 };
}

void WireDraw::drawWire (juce::Graphics& g,
                          juce::Point<int> a, juce::Point<int> b,
                          bool active)
{
    g.setColour (active ? wireActive : wireCol);
    juce::Path p;
    p.startNewSubPath (a.toFloat());
    const float cx = (a.x + b.x) * 0.5f;
    p.cubicTo (cx, static_cast<float> (a.y),
               cx, static_cast<float> (b.y),
               static_cast<float> (b.x), static_cast<float> (b.y));
    g.strokePath (p, juce::PathStrokeType (2.f));
}

void WireDraw::drawDragWire (juce::Graphics& g,
                              juce::Point<int> anchor, juce::Point<int> cursor,
                              bool valid)
{
    g.setColour (valid ? wireActive : wireBad);
    juce::Path p;
    const float cx = (anchor.x + cursor.x) * 0.5f;
    p.startNewSubPath (anchor.toFloat());
    p.cubicTo (cx, static_cast<float> (anchor.y),
               cx, static_cast<float> (cursor.y),
               static_cast<float> (cursor.x), static_cast<float> (cursor.y));
    g.strokePath (p, juce::PathStrokeType (2.f));
}
