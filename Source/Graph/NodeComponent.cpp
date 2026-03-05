/*
 * NodeComponent.cpp
 * VoicemeeterHost — Node rendering
 */

#include "NodeComponent.h"
#include "../Localization/LanguageManager.h"

// ─── Palette ─────────────────────────────────────────────────

namespace NP
{
    const juce::Colour nodePlugin { 0xFFDDDDDD };
    const juce::Colour nodeIn     { 0xFFB8CBE0 };
    const juce::Colour nodeOut    { 0xFFD8B8B8 };
    const juce::Colour nodeBorder { 0xFF999999 };
    const juce::Colour nodeText   { 0xFF111111 };
    const juce::Colour nodeHint   { 0xFF888888 };
    const juce::Colour rowText    { 0xFF222222 };
    const juce::Colour portIn     { 0xFF2980B9 };
    const juce::Colour portOut    { 0xFFE67E22 };
    const juce::Colour selCol     { 0xFFFFDD00 };
}

// ─── Side node (Input / Output zone) ────────────────────────

void NodeDraw::drawSideNode (juce::Graphics& g, const PluginNode& n,
                              juce::Rectangle<int> b,
                              juce::Point<int> portPt,
                              bool isSelected)
{
    const bool isInput = (n.type == NodeType::Input);

    g.setColour (isInput ? NP::nodeIn : NP::nodeOut);
    g.fillRoundedRectangle (b.reduced (6, 2).toFloat(), 4.f);
    g.setColour (NP::nodeBorder);
    g.drawRoundedRectangle (b.reduced (6, 2).toFloat(), 4.f, 1.f);

    if (isSelected)
    {
        g.setColour (NP::selCol);
        g.drawRoundedRectangle (b.reduced (6, 2).toFloat().expanded (2.f), 4.f, 3.f);
    }

    auto textRect = isInput ? b.reduced (8, 0).withTrimmedRight (16)
                            : b.reduced (8, 0).withTrimmedLeft (16);
    g.setColour (NP::rowText);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.f * getFontScaleFactor())));
    g.drawText (n.name, textRect, juce::Justification::centredLeft, true);

    // Port circle
    const float pr = static_cast<float> (PluginNode::getPortRadius());
    g.setColour (isInput ? NP::portOut : NP::portIn);
    g.fillEllipse (portPt.x - pr, portPt.y - pr, pr * 2, pr * 2);
    g.setColour (NP::nodeBorder);
    g.drawEllipse (portPt.x - pr, portPt.y - pr, pr * 2, pr * 2, 1.f);
}

// ─── Plugin node (centre area) ──────────────────────────────

void NodeDraw::drawPluginNode (juce::Graphics& g, const PluginNode& n,
                                bool isSelected)
{
    const auto bf = n.bounds().toFloat();

    // Shadow
    g.setColour (juce::Colour (0x40000000));
    g.fillRoundedRectangle (bf.translated (2, 2), 6.f);

    // Body
    g.setColour (NP::nodePlugin);
    g.fillRoundedRectangle (bf, 6.f);
    g.setColour (NP::nodeBorder);
    g.drawRoundedRectangle (bf, 6.f, 1.5f);

    if (isSelected)
    {
        g.setColour (NP::selCol);
        g.drawRoundedRectangle (bf.expanded (2.f), 6.f, 3.f);
    }

    // Name
    g.setColour (NP::nodeText);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.f * getFontScaleFactor()).withStyle ("Bold")));
    g.drawText (n.name, n.bounds().reduced (PluginNode::getPortRadius() + 4, 0),
                juce::Justification::centred, true);

    // Hint
    g.setColour (NP::nodeHint);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.f * getFontScaleFactor())));
    g.drawText (LanguageManager::getInstance().getText ("doubleClick"),
                n.bounds().withTrimmedTop (n.bounds().getHeight() / 2 + 2),
                juce::Justification::centred, false);

    // Ports
    auto drawPort = [&] (juce::Point<int> pt, juce::Colour col)
    {
        const float pr = static_cast<float> (PluginNode::getPortRadius());
        g.setColour (col);
        g.fillEllipse (pt.x - pr, pt.y - pr, pr * 2, pr * 2);
        g.setColour (NP::nodeBorder);
        g.drawEllipse (pt.x - pr, pt.y - pr, pr * 2, pr * 2, 1.f);
    };
    drawPort (n.inputPort(),  NP::portIn);
    drawPort (n.outputPort(), NP::portOut);
}
