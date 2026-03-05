/*
 * ConnectionComponent.h
 * VoicemeeterHost — Wire / Connection data + rendering
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
    /** Draw a cubic-bezier wire between two points. */
    void drawWire (juce::Graphics& g,
                   juce::Point<int> a, juce::Point<int> b,
                   bool active);

    /** Draw a temporary wire while the user is dragging. */
    void drawDragWire (juce::Graphics& g,
                       juce::Point<int> anchor, juce::Point<int> cursor,
                       bool valid);
}
