/*
 * GraphComponent.h
 * VoicemeeterHost — Node Graph Canvas
 *
 * Hosts plugin nodes, I/O nodes, and wires.
 * Supports zoom, pan, and DPI-aware rendering.
 */

#pragma once

#include <JuceHeader.h>
#include "NodeComponent.h"
#include "ConnectionComponent.h"

class GraphComponent : public juce::Component
{
public:
    // Zone widths / header heights (before DPI scale)
    static constexpr int kZoneW = 170;
    static constexpr int kHdrH  = 34;

    static int getZoneWidth()    { return static_cast<int> (kZoneW * getDPIScaleFactor()); }
    static int getHeaderHeight() { return static_cast<int> (kHdrH  * getDPIScaleFactor()); }

    static constexpr juce::uint32 kInputNodeUID  = 1000000;
    static constexpr juce::uint32 kOutputNodeUID = 1000001;

    GraphComponent (juce::AudioDeviceManager&      dm,
                    juce::KnownPluginList&          knownPlugins,
                    juce::AudioPluginFormatManager& fmt,
                    juce::AudioProcessorGraph&      graph);
    ~GraphComponent() override = default;

    void addNode (const juce::String& name, NodeType type);

    const std::vector<PluginNode>& getNodes() const noexcept { return nodes; }

    // Callbacks
    std::function<void()>                      onManagePlugins;
    std::function<void()>                      onDoubleClickLeft;
    std::function<void()>                      onDoubleClickRight;
    std::function<void (int, NodeType)>        onEditNode;
    std::function<void()>                      onGraphChanged;

    // Serialisation
    std::unique_ptr<juce::XmlElement> saveState() const;
    void loadState (const juce::XmlElement& xml);

    // Component overrides
    void paint (juce::Graphics& g) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    juce::AudioDeviceManager&       deviceManager;
    juce::KnownPluginList&          knownPlugins;
    juce::AudioPluginFormatManager& formatManager;
    juce::AudioProcessorGraph&      graph;

    std::vector<PluginNode> nodes;
    std::vector<NodeWire>   wires;
    int nextId { 1 };

    int        selectedNode { -1 };
    int        draggingNode { -1 };
    bool       draggingWire { false };
    bool       wireDragFromInput { false };
    int        wireFrom     { -1 };
    juce::Point<int> wireCursor;

    // Geometry
    enum class Zone { Left, Center, Right };
    Zone                  zoneAt        (juce::Point<int> p) const;
    juce::Point<int>      inputPortPos  (const PluginNode& n) const;
    juce::Point<int>      outputPortPos (const PluginNode& n) const;
    juce::Rectangle<int>  nodeBounds    (const PluginNode& n) const;

    // Drawing helpers
    void drawZoneBackgrounds (juce::Graphics& g) const;

    // Hit testing
    int  nodeAtPoint    (juce::Point<int> p) const;
    bool nearOutputPort (juce::Point<int> p, int& outId) const;
    bool nearInputPort  (juce::Point<int> p, int& outId) const;
    bool isValidWire    (int fromId, int toId) const;

    // Graph connections
    void addGraphConnection    (const PluginNode& from, const PluginNode& to);
    void removeGraphConnection (const PluginNode& from, const PluginNode& to);
    void clearGraphInputConnections (const PluginNode& to);
    void disconnectNode (int nodeId);

    // Plugin interaction
    void showPluginPicker (juce::Point<int> canvasPos);
    void openPluginEditor (int nodeId);
    void removeNode (int nodeId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphComponent)
};
