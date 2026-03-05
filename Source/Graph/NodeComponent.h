/*
 * NodeComponent.h
 * VoicemeeterHost — Node data + rendering for the Graph canvas
 *
 * A PluginNode represents a draggable box on the canvas.
 * Input/Output nodes render in the side zones;
 * Plugin nodes render in the centre area.
 */

#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioDeviceSettings.h"
#include "../Localization/LanguageManager.h"

// ─── DPI helpers (inline, single definition) ─────────────────

inline float getDPIScaleFactor()
{
    return ScaleSettingsManager::getInstance().getScaleFactor();
}

inline float getFontScaleFactor()
{
    return getDPIScaleFactor() * LanguageManager::getInstance().getFontScaling();
}

// ─── Node types ──────────────────────────────────────────────

enum class NodeType { Input, Output, Plugin };

// ─── PluginNode data ─────────────────────────────────────────

struct PluginNode
{
    int                               id   { 0 };
    NodeType                          type { NodeType::Plugin };
    juce::String                      name;
    juce::Point<int>                  pos  { 200, 100 };
    juce::AudioProcessorGraph::NodeID graphNodeId { 0 };

    // Base dimensions (before DPI scale)
    static constexpr int kW      = 140;
    static constexpr int kH      = 56;
    static constexpr int kSideH  = 40;
    static constexpr int kPortR  = 7;

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

// ─── Drawing helpers — implemented in NodeComponent.cpp ───────

namespace NodeDraw
{
    /** Draw a side-zone node (Input or Output). */
    void drawSideNode (juce::Graphics& g, const PluginNode& n,
                       juce::Rectangle<int> bounds,
                       juce::Point<int> portPos,
                       bool isSelected);

    /** Draw a centre-area plugin node. */
    void drawPluginNode (juce::Graphics& g, const PluginNode& n,
                         bool isSelected);
}
