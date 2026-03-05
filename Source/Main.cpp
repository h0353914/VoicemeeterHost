/*
 * Main.cpp
 * VoicemeeterHost — Application Entry Point
 *
 * JUCEApplication subclass.
 * Replaces deprecated LookAndFeel_V3 with LookAndFeel_V4.
 * Supports -multi-instance=<name> command-line argument.
 */

#include <JuceHeader.h>
#include "UI/SystemTrayMenu.h"
#include "Localization/LanguageManager.h"

#if ! (JUCE_PLUGINHOST_VST3)
 #error "VoicemeeterHost requires VST3 plugin hosting support."
#endif

// ═══════════════════════════════════════════════════════════════
// Application
// ═══════════════════════════════════════════════════════════════

class VoicemeeterHostApp : public juce::JUCEApplication
{
public:
    VoicemeeterHostApp() = default;

    const juce::String getApplicationName()    override { return "VoicemeeterHost"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    bool moreThanOneInstanceAllowed() override
    {
        return getParameter ("-multi-instance").size() == 2;
    }

    void initialise (const juce::String&) override
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = getApplicationName();
        opts.filenameSuffix  = "settings";

        checkArguments (opts);

        appProperties = std::make_unique<juce::ApplicationProperties>();
        appProperties->setStorageParameters (opts);

        // Apply light colour scheme globally
        lookAndFeel.setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xFFECECEC));
        lookAndFeel.setColour (juce::PopupMenu::textColourId,                 juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::PopupMenu::headerTextColourId,           juce::Colour (0xFF444444));
        lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xFF4682B4));
        lookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
        lookAndFeel.setColour (juce::ListBox::backgroundColourId,              juce::Colours::white);
        lookAndFeel.setColour (juce::ListBox::outlineColourId,                 juce::Colour (0xFFCCCCCC));
        lookAndFeel.setColour (juce::ListBox::textColourId,                    juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour (0xFFDDDDDD));
        lookAndFeel.setColour (juce::TableHeaderComponent::outlineColourId,    juce::Colour (0xFFBBBBBB));
        lookAndFeel.setColour (juce::TableHeaderComponent::textColourId,       juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::TableHeaderComponent::highlightColourId,  juce::Colour (0xFFCCCCCC));
        lookAndFeel.setColour (juce::ComboBox::backgroundColourId,             juce::Colours::white);
        lookAndFeel.setColour (juce::ComboBox::textColourId,                   juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::ComboBox::outlineColourId,                juce::Colour (0xFFBBBBBB));
        lookAndFeel.setColour (juce::ComboBox::arrowColourId,                  juce::Colour (0xFF555555));
        lookAndFeel.setColour (juce::TextButton::buttonColourId,               juce::Colour (0xFF7A8A9A));
        lookAndFeel.setColour (juce::TextButton::buttonOnColourId,             juce::Colour (0xFF547080));
        lookAndFeel.setColour (juce::TextButton::textColourOffId,              juce::Colours::white);
        lookAndFeel.setColour (juce::TextButton::textColourOnId,               juce::Colours::white);
        lookAndFeel.setColour (juce::TextEditor::backgroundColourId,           juce::Colours::white);
        lookAndFeel.setColour (juce::TextEditor::textColourId,                 juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::TextEditor::outlineColourId,              juce::Colour (0xFFBBBBBB));
        lookAndFeel.setColour (juce::ScrollBar::thumbColourId,                 juce::Colour (0xFFAAAAAA));
        lookAndFeel.setColour (juce::Label::textColourId,                      juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::Label::backgroundColourId,                juce::Colour (0x00000000));
        lookAndFeel.setColour (juce::ToggleButton::textColourId,               juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::ToggleButton::tickColourId,               juce::Colour (0xFF4682B4));
        lookAndFeel.setColour (juce::ToggleButton::tickDisabledColourId,       juce::Colour (0xFFCCCCCC));
        lookAndFeel.setColour (juce::Slider::thumbColourId,                    juce::Colour (0xFF4682B4));
        lookAndFeel.setColour (juce::Slider::trackColourId,                    juce::Colour (0xFFDDDDDD));
        lookAndFeel.setColour (juce::Slider::backgroundColourId,               juce::Colour (0xFFF2F2F2));
        lookAndFeel.setColour (juce::Slider::textBoxTextColourId,              juce::Colour (0xFF222222));
        lookAndFeel.setColour (juce::Slider::textBoxBackgroundColourId,        juce::Colours::white);
        lookAndFeel.setColour (juce::Slider::textBoxOutlineColourId,           juce::Colour (0xFFBBBBBB));

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        tray = std::make_unique<SystemTrayMenu>();
    }

    void shutdown() override
    {
        tray.reset();
        appProperties.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        juce::JUCEApplicationBase::quit();
    }

    juce::ApplicationCommandManager commandManager;
    std::unique_ptr<juce::ApplicationProperties> appProperties;
    juce::LookAndFeel_V4 lookAndFeel;

private:
    std::unique_ptr<SystemTrayMenu> tray;

    juce::StringArray getParameter (const juce::String& lookFor) const
    {
        juce::StringArray params = getCommandLineParameterArray();
        juce::StringArray found;
        for (const auto& param : params)
        {
            if (param.contains (lookFor))
            {
                found.add (lookFor);
                int delim = param.indexOf (0, "=") + 1;
                found.add (param.substring (delim));
                return found;
            }
        }
        return found;
    }

    void checkArguments (juce::PropertiesFile::Options& opts)
    {
        auto multiInst = getParameter ("-multi-instance");
        if (multiInst.size() == 2)
            opts.filenameSuffix = multiInst[1] + "." + opts.filenameSuffix;
    }
};

// ═══════════════════════════════════════════════════════════════
// Global accessors
// ═══════════════════════════════════════════════════════════════

static VoicemeeterHostApp& getApp()
{
    return *dynamic_cast<VoicemeeterHostApp*> (juce::JUCEApplication::getInstance());
}

juce::ApplicationCommandManager& getCommandManager()
{
    return getApp().commandManager;
}

juce::ApplicationProperties& getAppProperties()
{
    return *getApp().appProperties;
}

START_JUCE_APPLICATION (VoicemeeterHostApp)
