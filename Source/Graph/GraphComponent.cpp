/*
 * GraphComponent.cpp
 * VoicemeeterHost — Node Graph Canvas (paint, zoom, pan, mouse events)
 *
 * Rewritten from LightHost NodeGraphCanvas with:
 *   - Split drawing into NodeComponent / ConnectionComponent
 *   - Replaced deprecated APIs
 *   - Uses modern Font(FontOptions{}) constructors
 */

#include "GraphComponent.h"
#include "../Plugin/PluginWindow.h"
#include "../Localization/LanguageManager.h"
#include "../Audio/AudioDeviceSettings.h"


// ─── Palette ─────────────────────────────────────────────────

namespace NP
{
    const juce::Colour canvas     { 0xFFECECEC };
    const juce::Colour grid       { 0xFFD8D8D8 };
    const juce::Colour zoneBg     { 0xFFDFDFDF };
    const juce::Colour zoneBorder { 0xFFBBBBBB };
    const juce::Colour zoneHeader { 0xFFD0D0D0 };
    const juce::Colour zoneTitle  { 0xFF333333 };
}

// ─── Plugin parameter listener (auto-save on tweak) ─────────

class PluginParameterListener : public juce::AudioProcessorListener
{
public:
    explicit PluginParameterListener (std::function<void()> cb)
        : callback (std::move (cb)) {}

    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override
    {
        if (callback) callback();
    }

private:
    std::function<void()> callback;
};

static std::map<juce::uint32, std::unique_ptr<PluginParameterListener>> g_pluginListeners;

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

GraphComponent::GraphComponent (juce::AudioDeviceManager& dm,
                                juce::KnownPluginList& kpl,
                                juce::AudioPluginFormatManager& fmt,
                                juce::AudioProcessorGraph& g)
    : deviceManager (dm), knownPlugins (kpl), formatManager (fmt), graph (g)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

// ═══════════════════════════════════════════════════════════════
// Geometry helpers
// ═══════════════════════════════════════════════════════════════

GraphComponent::Zone GraphComponent::zoneAt (juce::Point<int> p) const
{
    if (p.x < getZoneWidth())                return Zone::Left;
    if (p.x > getWidth() - getZoneWidth())   return Zone::Right;
    return Zone::Center;
}

juce::Point<int> GraphComponent::inputPortPos (const PluginNode& n) const
{
    if (n.type == NodeType::Input)   return { -999, -999 };
    if (n.type == NodeType::Output)  { auto b = nodeBounds (n); return { b.getX(), b.getCentreY() }; }
    return n.inputPort();
}

juce::Point<int> GraphComponent::outputPortPos (const PluginNode& n) const
{
    if (n.type == NodeType::Output) return { -999, -999 };
    if (n.type == NodeType::Input)  { auto b = nodeBounds (n); return { b.getRight(), b.getCentreY() }; }
    return n.outputPort();
}

juce::Rectangle<int> GraphComponent::nodeBounds (const PluginNode& n) const
{
    if (n.type == NodeType::Input || n.type == NodeType::Output)
    {
        int slot = 0;
        for (const auto& nd : nodes)
        {
            if (nd.type == n.type)
            {
                if (nd.id == n.id) break;
                ++slot;
            }
        }
        int y = getHeaderHeight() + 6 + slot * (PluginNode::getSideHeight() + 6);
        int x = (n.type == NodeType::Input) ? 0 : getWidth() - getZoneWidth();
        return { x, y, getZoneWidth(), PluginNode::getSideHeight() };
    }
    return n.bounds();
}

// ═══════════════════════════════════════════════════════════════
// addNode
// ═══════════════════════════════════════════════════════════════

void GraphComponent::addNode (const juce::String& name, NodeType type)
{
    PluginNode n;
    n.id   = nextId++;
    n.type = type;
    n.name = name;

    if (type == NodeType::Input)
    {
        int cnt = 0;
        for (const auto& x : nodes) if (x.type == NodeType::Input) ++cnt;
        n.pos = { 0, getHeaderHeight() + 6 + cnt * (PluginNode::getSideHeight() + 6) };
        n.graphNodeId = juce::AudioProcessorGraph::NodeID (kInputNodeUID);
    }
    else if (type == NodeType::Output)
    {
        int cnt = 0;
        for (const auto& x : nodes) if (x.type == NodeType::Output) ++cnt;
        n.pos = { getWidth() - getZoneWidth(),
                  getHeaderHeight() + 6 + cnt * (PluginNode::getSideHeight() + 6) };
        n.graphNodeId = juce::AudioProcessorGraph::NodeID (kOutputNodeUID);
    }

    nodes.push_back (n);
    if (onGraphChanged) onGraphChanged();
    repaint();
}

// ═══════════════════════════════════════════════════════════════
// AudioProcessorGraph helpers
// ═══════════════════════════════════════════════════════════════

void GraphComponent::addGraphConnection (const PluginNode& from, const PluginNode& to)
{
    if (! graph.getNodeForId (from.graphNodeId) || ! graph.getNodeForId (to.graphNodeId))
        return;

    graph.addConnection ({ { from.graphNodeId, 0 }, { to.graphNodeId, 0 } });
    graph.addConnection ({ { from.graphNodeId, 1 }, { to.graphNodeId, 1 } });
    graph.rebuild();
}

void GraphComponent::removeGraphConnection (const PluginNode& from, const PluginNode& to)
{
    graph.removeConnection ({ { from.graphNodeId, 0 }, { to.graphNodeId, 0 } });
    graph.removeConnection ({ { from.graphNodeId, 1 }, { to.graphNodeId, 1 } });
    graph.rebuild();
}

void GraphComponent::clearGraphInputConnections (const PluginNode& toNode)
{
    for (const auto& w : wires)
    {
        if (w.toNode != toNode.id) continue;
        for (const auto& fn : nodes)
            if (fn.id == w.fromNode)
            { removeGraphConnection (fn, toNode); break; }
    }
    wires.erase (std::remove_if (wires.begin(), wires.end(),
        [&toNode] (const NodeWire& w) { return w.toNode == toNode.id; }),
        wires.end());
}

void GraphComponent::disconnectNode (int nodeId)
{
    std::vector<int> indices;
    for (size_t i = 0; i < wires.size(); ++i)
        if (wires[i].fromNode == nodeId || wires[i].toNode == nodeId)
            indices.push_back (static_cast<int> (i));

    for (int idx = static_cast<int> (indices.size()) - 1; idx >= 0; --idx)
    {
        const auto& w = wires[indices[idx]];
        const PluginNode *fr = nullptr, *to = nullptr;
        for (const auto& nd : nodes)
        {
            if (nd.id == w.fromNode) fr = &nd;
            if (nd.id == w.toNode)   to = &nd;
        }
        if (fr && to) removeGraphConnection (*fr, *to);
        wires.erase (wires.begin() + indices[idx]);
    }

    graph.rebuild();
    if (onGraphChanged) onGraphChanged();
    repaint();
}

// ═══════════════════════════════════════════════════════════════
// Drawing
// ═══════════════════════════════════════════════════════════════

void GraphComponent::drawZoneBackgrounds (juce::Graphics& g) const
{
    const int w = getWidth(), h = getHeight();

    // Left zone
    g.setColour (NP::zoneBg);
    g.fillRect (0, 0, getZoneWidth(), h);
    g.setColour (NP::zoneBorder);
    g.drawLine (static_cast<float> (getZoneWidth()) - 0.5f, 0,
                static_cast<float> (getZoneWidth()) - 0.5f, static_cast<float> (h), 1);

    // Right zone
    g.setColour (NP::zoneBg);
    g.fillRect (w - getZoneWidth(), 0, getZoneWidth(), h);
    g.setColour (NP::zoneBorder);
    g.drawLine (static_cast<float> (w - getZoneWidth()) + 0.5f, 0,
                static_cast<float> (w - getZoneWidth()) + 0.5f, static_cast<float> (h), 1);

    auto drawHeader = [&] (int x, const juce::String& title)
    {
        juce::Rectangle<int> hdr (x, 0, getZoneWidth(), getHeaderHeight());
        g.setColour (NP::zoneHeader);
        g.fillRect (hdr);
        g.setColour (NP::zoneTitle);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.5f * getFontScaleFactor()).withStyle ("Bold")));
        g.drawText (title, hdr, juce::Justification::centred, false);
    };
    drawHeader (0,              LanguageManager::getInstance().getText ("inputPorts"));
    drawHeader (w - getZoneWidth(), LanguageManager::getInstance().getText ("outputPorts"));

    // Hints
    bool hasIn = false, hasOut = false;
    for (const auto& nd : nodes)
    {
        if (nd.type == NodeType::Input)  hasIn  = true;
        if (nd.type == NodeType::Output) hasOut = true;
    }
    g.setColour (juce::Colour (0xFF999999));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f * getFontScaleFactor())));
    auto hint = LanguageManager::getInstance().getText ("doubleClickToAdd");
    if (! hasIn)  g.drawText (hint, juce::Rectangle<int> (0, getHeaderHeight(), getZoneWidth(), 22), juce::Justification::centred, false);
    if (! hasOut) g.drawText (hint, juce::Rectangle<int> (w - getZoneWidth(), getHeaderHeight(), getZoneWidth(), 22), juce::Justification::centred, false);
}

void GraphComponent::paint (juce::Graphics& g)
{
    g.fillAll (NP::canvas);

    // Grid dots
    g.setColour (NP::grid);
    for (int x = getZoneWidth(); x < getWidth() - getZoneWidth(); x += 32)
        for (int y = 0; y < getHeight(); y += 32)
            g.fillRect (x, y, 1, 1);

    drawZoneBackgrounds (g);

    // Wires
    for (const auto& wire : wires)
    {
        const PluginNode *fr = nullptr, *to = nullptr;
        for (const auto& nd : nodes)
        {
            if (nd.id == wire.fromNode) fr = &nd;
            if (nd.id == wire.toNode)   to = &nd;
        }
        if (fr && to)
            WireDraw::drawWire (g, outputPortPos (*fr), inputPortPos (*to), false);
    }

    // Drag wire
    if (draggingWire && wireFrom >= 0)
    {
        for (const auto& nd : nodes)
        {
            if (nd.id != wireFrom) continue;
            int dummy = -1;
            bool valid;
            juce::Point<int> anchor;
            if (! wireDragFromInput)
            {
                anchor = outputPortPos (nd);
                valid  = nearInputPort (wireCursor, dummy) && isValidWire (wireFrom, dummy);
            }
            else
            {
                anchor = inputPortPos (nd);
                valid  = nearOutputPort (wireCursor, dummy) && isValidWire (dummy, wireFrom);
            }
            WireDraw::drawDragWire (g, anchor, wireCursor, valid);
            break;
        }
    }

    // Nodes
    for (const auto& nd : nodes)
    {
        if (nd.type == NodeType::Input || nd.type == NodeType::Output)
        {
            auto portPt = (nd.type == NodeType::Input) ? outputPortPos (nd) : inputPortPos (nd);
            NodeDraw::drawSideNode (g, nd, nodeBounds (nd), portPt, selectedNode == nd.id);
        }
        else
        {
            NodeDraw::drawPluginNode (g, nd, selectedNode == nd.id);
        }
    }

    // Empty centre hint
    bool hasPlugin = false;
    for (const auto& nd : nodes) if (nd.type == NodeType::Plugin) { hasPlugin = true; break; }
    if (! hasPlugin)
    {
        g.setColour (juce::Colour (0xFF999999));
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.f * getFontScaleFactor())));
        g.drawText (LanguageManager::getInstance().getText ("doubleClickToAddPlugin"),
                    juce::Rectangle<int> (getZoneWidth(), 0, getWidth() - 2 * getZoneWidth(), getHeight()),
                    juce::Justification::centred, false);
    }
}

// ═══════════════════════════════════════════════════════════════
// Hit testing
// ═══════════════════════════════════════════════════════════════

int GraphComponent::nodeAtPoint (juce::Point<int> p) const
{
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
        if (nodeBounds (*it).contains (p)) return it->id;
    return -1;
}

bool GraphComponent::nearOutputPort (juce::Point<int> p, int& outId) const
{
    const int snap = PluginNode::getPortRadius() + 6;
    for (const auto& nd : nodes)
        if (nd.hasOutputPort() && outputPortPos (nd).getDistanceFrom (p) <= snap)
        { outId = nd.id; return true; }
    return false;
}

bool GraphComponent::nearInputPort (juce::Point<int> p, int& outId) const
{
    const int snap = PluginNode::getPortRadius() + 6;
    for (const auto& nd : nodes)
        if (nd.hasInputPort() && inputPortPos (nd).getDistanceFrom (p) <= snap)
        { outId = nd.id; return true; }
    return false;
}

bool GraphComponent::isValidWire (int fromId, int toId) const
{
    if (fromId == toId) return false;
    const PluginNode *fr = nullptr, *to = nullptr;
    for (const auto& nd : nodes)
    {
        if (nd.id == fromId) fr = &nd;
        if (nd.id == toId)   to = &nd;
    }
    if (! fr || ! to) return false;
    if (! fr->hasOutputPort() || ! to->hasInputPort()) return false;
    if (fr->type == NodeType::Input  && to->type == NodeType::Input)  return false;
    if (fr->type == NodeType::Output && to->type == NodeType::Output) return false;
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Mouse events
// ═══════════════════════════════════════════════════════════════

void GraphComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = nodeAtPoint (e.getPosition());
    if (hit >= 0)
    {
        for (const auto& nd : nodes)
            if (nd.id == hit)
            {
                if (nd.type == NodeType::Plugin) openPluginEditor (hit);
                else if (onEditNode) onEditNode (hit, nd.type);
                return;
            }
    }
}

void GraphComponent::mouseDown (const juce::MouseEvent& e)
{
    selectedNode = nodeAtPoint (e.getPosition());
    repaint();

    if (e.mods.isRightButtonDown())
    {
        const int hitNode    = nodeAtPoint (e.getPosition());
        const Zone zone      = zoneAt (e.getPosition());
        const auto screenPos = localPointToGlobal (e.getPosition());

        auto menuAt = [&] (juce::PopupMenu& m, auto&& callback)
        {
            m.showMenuAsync (
                juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                std::move (callback));
        };

        auto& lang = LanguageManager::getInstance();

        if (hitNode >= 0)
        {
            for (const auto& nd : nodes)
            {
                if (nd.id != hitNode) continue;

                juce::PopupMenu m;
                if (nd.type == NodeType::Input)
                {
                    m.addItem (1, lang.getText ("addInputDevice"));
                    m.addItem (2, lang.getText ("disconnectAllWires"));
                    m.addSeparator();
                    m.addItem (3, lang.getText ("delete"));
                    menuAt (m, [this, hitNode] (int r) {
                        if (r == 1 && onDoubleClickLeft)  onDoubleClickLeft();
                        else if (r == 2) disconnectNode (hitNode);
                        else if (r == 3) removeNode (hitNode);
                    });
                }
                else if (nd.type == NodeType::Output)
                {
                    m.addItem (1, lang.getText ("addOutputDevice"));
                    m.addItem (2, lang.getText ("disconnectAllWires"));
                    m.addSeparator();
                    m.addItem (3, lang.getText ("delete"));
                    menuAt (m, [this, hitNode] (int r) {
                        if (r == 1 && onDoubleClickRight) onDoubleClickRight();
                        else if (r == 2) disconnectNode (hitNode);
                        else if (r == 3) removeNode (hitNode);
                    });
                }
                else
                {
                    m.addItem (1, lang.getText ("addPlugin"));
                    m.addItem (2, lang.getText ("disconnectAllWires"));
                    m.addSeparator();
                    m.addItem (3, lang.getText ("delete"));
                    menuAt (m, [this, hitNode, ePos = e.getPosition()] (int r) {
                        if (r == 1) showPluginPicker (ePos);
                        else if (r == 2) disconnectNode (hitNode);
                        else if (r == 3) removeNode (hitNode);
                    });
                }
                return;
            }
        }
        else
        {
            juce::PopupMenu m;
            if (zone == Zone::Left)
            {
                m.addItem (1, lang.getText ("addInputDevice"));
                menuAt (m, [this] (int r) { if (r == 1 && onDoubleClickLeft) onDoubleClickLeft(); });
            }
            else if (zone == Zone::Right)
            {
                m.addItem (1, lang.getText ("addOutputDevice"));
                menuAt (m, [this] (int r) { if (r == 1 && onDoubleClickRight) onDoubleClickRight(); });
            }
            else { showPluginPicker (e.getPosition()); }
        }
        return;
    }

    // Wire dragging from output port?
    int portNode = -1;
    if (nearOutputPort (e.getPosition(), portNode))
    {
        draggingWire      = true;
        wireDragFromInput = false;
        wireFrom          = portNode;
        wireCursor        = e.getPosition();
        draggingNode      = -1;
        return;
    }
    if (nearInputPort (e.getPosition(), portNode))
    {
        draggingWire      = true;
        wireDragFromInput = true;
        wireFrom          = portNode;
        wireCursor        = e.getPosition();
        draggingNode      = -1;
        return;
    }

    if (selectedNode >= 0)
    {
        grabKeyboardFocus();
        for (const auto& nd : nodes)
            if (nd.id == selectedNode && nd.type == NodeType::Plugin)
            { draggingNode = selectedNode; break; }
    }
    draggingWire = false;
}

void GraphComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingWire) { wireCursor = e.getPosition(); repaint(); return; }

    if (draggingNode >= 0)
    {
        for (auto& nd : nodes)
        {
            if (nd.id != draggingNode) continue;
            const int lo = getZoneWidth();
            const int hi = getWidth() - getZoneWidth() - PluginNode::getWidth();
            auto newPos = juce::Point<int> (
                std::max (lo, std::min (hi, e.x - PluginNode::getWidth() / 2)),
                std::max (0,  std::min (getHeight() - PluginNode::getHeight(), e.y - PluginNode::getHeight() / 2)));
            if (nd.pos != newPos)
            {
                nd.pos = newPos;
                if (onGraphChanged) onGraphChanged();
                repaint();
            }
            break;
        }
    }
}

void GraphComponent::mouseUp (const juce::MouseEvent& e)
{
    if (draggingWire && wireFrom >= 0)
    {
        int target = -1;
        if (! wireDragFromInput)
        {
            if (nearInputPort (e.getPosition(), target) && isValidWire (wireFrom, target))
            {
                const PluginNode *frNode = nullptr, *toNode = nullptr;
                for (const auto& nd : nodes)
                {
                    if (nd.id == wireFrom) frNode = &nd;
                    if (nd.id == target)   toNode = &nd;
                }
                if (frNode && toNode)
                {
                    clearGraphInputConnections (*toNode);
                    addGraphConnection (*frNode, *toNode);
                    wires.push_back ({ wireFrom, target });
                    if (onGraphChanged) onGraphChanged();
                }
            }
        }
        else
        {
            if (nearOutputPort (e.getPosition(), target) && isValidWire (target, wireFrom))
            {
                const PluginNode *frNode = nullptr, *toNode = nullptr;
                for (const auto& nd : nodes)
                {
                    if (nd.id == target)   frNode = &nd;
                    if (nd.id == wireFrom) toNode = &nd;
                }
                if (frNode && toNode)
                {
                    clearGraphInputConnections (*toNode);
                    addGraphConnection (*frNode, *toNode);
                    wires.push_back ({ target, wireFrom });
                    if (onGraphChanged) onGraphChanged();
                }
            }
        }
    }
    draggingWire      = false;
    wireDragFromInput = false;
    wireFrom          = -1;
    draggingNode      = -1;
    repaint();
}

// ═══════════════════════════════════════════════════════════════
// Plugin picker
// ═══════════════════════════════════════════════════════════════

void GraphComponent::showPluginPicker (juce::Point<int> canvasPos)
{
    juce::PopupMenu m;
    auto types = knownPlugins.getTypes();
    auto& lang = LanguageManager::getInstance();

    if (! types.isEmpty())
    {
        m.addSectionHeader (lang.getText ("availablePlugins"));
        for (int i = 0; i < types.size(); ++i)
            m.addItem (i + 1, types[i].name + "  [" + types[i].pluginFormatName + "]");
        m.addSeparator();
    }
    const int kManage = 100000;
    m.addItem (kManage, lang.getText ("addManagePlugins"));

    const auto sc = localPointToGlobal (canvasPos);
    m.showMenuAsync (
        juce::PopupMenu::Options().withTargetScreenArea ({ sc.x, sc.y, 1, 1 }),
        [this, types, canvasPos, kManage] (int result)
        {
            if (result == kManage && onManagePlugins) { onManagePlugins(); return; }
            if (result <= 0 || result > types.size()) return;

            const auto& desc = types[result - 1];
            juce::String err;
            double sr = 44100.0;
            int    bs = 512;
            if (auto* dev = deviceManager.getCurrentAudioDevice())
            {
                sr = dev->getCurrentSampleRate();
                bs = dev->getCurrentBufferSizeSamples();
            }

            auto instance = formatManager.createPluginInstance (desc, sr, bs, err);
            if (! instance)
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    LanguageManager::getInstance().getText ("cannotLoadPlugin"),
                    err.isEmpty() ? LanguageManager::getInstance().getText ("unknownError") : err);
                return;
            }
            instance->prepareToPlay (sr, bs);

            auto nodePtr = graph.addNode (std::unique_ptr<juce::AudioProcessor> (std::move (instance)));
            if (! nodePtr) return;

            // Attach parameter listener for auto-save
            if (auto* proc = nodePtr->getProcessor())
            {
                auto listener = std::make_unique<PluginParameterListener> (
                    [this] { if (onGraphChanged) onGraphChanged(); });
                proc->addListener (listener.get());
                g_pluginListeners[nodePtr->nodeID.uid] = std::move (listener);
            }

            PluginNode n;
            n.id          = nextId++;
            n.type        = NodeType::Plugin;
            n.name        = desc.name;
            n.graphNodeId = nodePtr->nodeID;

            const int cx = (getWidth() - getZoneWidth() * 2) / 2 + getZoneWidth() - PluginNode::getWidth() / 2;
            int cnt = 0;
            for (const auto& x : nodes) if (x.type == NodeType::Plugin) ++cnt;
            n.pos = { cx, 60 + cnt * (PluginNode::getHeight() + 20) };

            nodes.push_back (n);
            if (onGraphChanged) onGraphChanged();
            repaint();
        });
}

// ═══════════════════════════════════════════════════════════════
// Plugin editor
// ═══════════════════════════════════════════════════════════════

void GraphComponent::openPluginEditor (int nodeId)
{
    const PluginNode* cn = nullptr;
    for (const auto& nd : nodes)
        if (nd.id == nodeId) { cn = &nd; break; }
    if (! cn || cn->type != NodeType::Plugin) return;

    auto* graphNode = graph.getNodeForId (cn->graphNodeId);
    if (! graphNode) return;

    PluginWindow::getWindowFor (graphNode, PluginWindow::Normal);
}

// ═══════════════════════════════════════════════════════════════
// Remove node
// ═══════════════════════════════════════════════════════════════

void GraphComponent::removeNode (int nodeId)
{
    for (const auto& nd : nodes)
    {
        if (nd.id != nodeId) continue;

        if (nd.graphNodeId != juce::AudioProcessorGraph::NodeID (0))
        {
            g_pluginListeners.erase (nd.graphNodeId.uid);
            if (auto* gn = graph.getNodeForId (nd.graphNodeId))
                graph.removeNode (gn);
        }

        nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
            [nodeId] (const PluginNode& n) { return n.id == nodeId; }),
            nodes.end());

        wires.erase (std::remove_if (wires.begin(), wires.end(),
            [nodeId] (const NodeWire& w) { return w.fromNode == nodeId || w.toNode == nodeId; }),
            wires.end());

        selectedNode = -1;
        graph.rebuild();
        if (onGraphChanged) onGraphChanged();
        repaint();
        return;
    }
}

// ═══════════════════════════════════════════════════════════════
// Keyboard
// ═══════════════════════════════════════════════════════════════

bool GraphComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::deleteKey && selectedNode >= 0)
    {
        removeNode (selectedNode);
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
// Serialisation
// ═══════════════════════════════════════════════════════════════

std::unique_ptr<juce::XmlElement> GraphComponent::saveState() const
{
    auto xml  = std::make_unique<juce::XmlElement> ("NodeGraph");
    auto* xNodes = new juce::XmlElement ("Nodes");
    xml->addChildElement (xNodes);

    for (const auto& n : nodes)
    {
        auto* xn = new juce::XmlElement ("Node");
        xn->setAttribute ("id",   n.id);
        xn->setAttribute ("type", static_cast<int> (n.type));
        xn->setAttribute ("name", n.name);
        xn->setAttribute ("x",    n.pos.x);
        xn->setAttribute ("y",    n.pos.y);

        if (n.type == NodeType::Plugin)
        {
            if (auto* gNode = graph.getNodeForId (n.graphNodeId))
            {
                if (auto* proc = gNode->getProcessor())
                {
                    juce::PluginDescription desc;
                    if (auto* pi = dynamic_cast<juce::AudioPluginInstance*> (proc))
                        pi->fillInPluginDescription (desc);
                    xn->setAttribute ("pluginName",             desc.name);
                    xn->setAttribute ("pluginFormat",           desc.pluginFormatName);
                    xn->setAttribute ("pluginFileOrIdentifier", desc.fileOrIdentifier);

                    juce::MemoryBlock mb;
                    proc->getStateInformation (mb);
                    auto* stateXml = new juce::XmlElement ("PluginState");
                    stateXml->addTextElement (mb.toBase64Encoding());
                    xn->addChildElement (stateXml);
                }
            }
        }
        xNodes->addChildElement (xn);
    }

    auto* xWires = new juce::XmlElement ("Wires");
    xml->addChildElement (xWires);
    for (const auto& w : wires)
    {
        auto* xw = new juce::XmlElement ("Wire");
        xw->setAttribute ("from", w.fromNode);
        xw->setAttribute ("to",   w.toNode);
        xWires->addChildElement (xw);
    }
    return xml;
}

void GraphComponent::loadState (const juce::XmlElement& xml)
{
    nodes.clear();
    wires.clear();

    const auto* xNodes = xml.getChildByName ("Nodes");
    if (! xNodes) return;

    for (auto* xn : xNodes->getChildIterator())
    {
        PluginNode n;
        n.id   = xn->getIntAttribute ("id");
        n.type = static_cast<NodeType> (xn->getIntAttribute ("type"));
        n.name = xn->getStringAttribute ("name");
        n.pos  = { xn->getIntAttribute ("x"), xn->getIntAttribute ("y") };

        if (n.id >= nextId) nextId = n.id + 1;

        if (n.type == NodeType::Input)
            n.graphNodeId = juce::AudioProcessorGraph::NodeID (kInputNodeUID);
        else if (n.type == NodeType::Output)
            n.graphNodeId = juce::AudioProcessorGraph::NodeID (kOutputNodeUID);
        else if (n.type == NodeType::Plugin)
        {
            auto identifier = xn->getStringAttribute ("pluginFileOrIdentifier");
            auto pluginName = xn->getStringAttribute ("pluginName");
            auto formatName = xn->getStringAttribute ("pluginFormat");

            juce::PluginDescription desc;
            desc.fileOrIdentifier = identifier;
            desc.name             = pluginName;
            desc.pluginFormatName = formatName;

            for (const auto& d : knownPlugins.getTypes())
                if (d.fileOrIdentifier == identifier) { desc = d; break; }

            juce::String err;
            double sr = 44100.0;
            int    bs = 512;
            if (auto* dev = deviceManager.getCurrentAudioDevice())
            {
                sr = dev->getCurrentSampleRate();
                bs = dev->getCurrentBufferSizeSamples();
            }

            auto instance = formatManager.createPluginInstance (desc, sr, bs, err);
            if (instance)
            {
                if (const auto* xState = xn->getChildByName ("PluginState"))
                {
                    juce::MemoryBlock mb;
                    mb.fromBase64Encoding (xState->getAllSubText());
                    instance->setStateInformation (mb.getData(), static_cast<int> (mb.getSize()));
                }
                instance->prepareToPlay (sr, bs);

                auto nodePtr = graph.addNode (std::unique_ptr<juce::AudioProcessor> (std::move (instance)));
                if (nodePtr)
                {
                    n.graphNodeId = nodePtr->nodeID;
                    if (auto* proc = nodePtr->getProcessor())
                    {
                        auto listener = std::make_unique<PluginParameterListener> (
                            [this] { if (onGraphChanged) onGraphChanged(); });
                        proc->addListener (listener.get());
                        g_pluginListeners[nodePtr->nodeID.uid] = std::move (listener);
                    }
                }
            }
        }
        nodes.push_back (n);
    }

    const auto* xWires = xml.getChildByName ("Wires");
    if (xWires)
    {
        for (auto* xw : xWires->getChildIterator())
        {
            NodeWire w;
            w.fromNode = xw->getIntAttribute ("from");
            w.toNode   = xw->getIntAttribute ("to");
            wires.push_back (w);

            const PluginNode *fr = nullptr, *to = nullptr;
            for (const auto& nd : nodes)
            {
                if (nd.id == w.fromNode) fr = &nd;
                if (nd.id == w.toNode)   to = &nd;
            }
            if (fr && to && fr->graphNodeId.uid != 0 && to->graphNodeId.uid != 0)
                addGraphConnection (*fr, *to);
        }
    }

    graph.rebuild();
    repaint();
}
