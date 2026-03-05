# ══════════════════════════════════════════════════════════════
# VoicemeeterHost — CMake Build Script
# ══════════════════════════════════════════════════════════════
cmake_minimum_required(VERSION 3.15)

project(VoicemeeterHost VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE submodule ────────────────────────────────────────────
add_subdirectory(JUCE)

# ── Binary resources (icons + JSON language packs) ────────────
juce_add_binary_data(VoicemeeterHostData
    SOURCES
        Resources/icon.png
        Resources/menu_icon.png
        Resources/menu_icon_white.png
        Resources/Strings/English.json
        Resources/Strings/TraditionalChinese.json
)

# ── Application target ────────────────────────────────────────
juce_add_gui_app(VoicemeeterHost
    PRODUCT_NAME          "VoicemeeterHost"
    COMPANY_NAME          "VoicemeeterHost"
    BUNDLE_ID             "com.voicemeeterhost.app"
    ICON_BIG              "Resources/icon.png"
    NEEDS_CURL            FALSE
    NEEDS_WEB_BROWSER     FALSE
)

juce_generate_juce_header(VoicemeeterHost)

target_sources(VoicemeeterHost PRIVATE
    # ── Entry point ──
    Source/Main.cpp

    # ── UI ──
    Source/UI/MainWindow.h
    Source/UI/MainWindow.cpp
    Source/UI/SystemTrayMenu.h
    Source/UI/SystemTrayMenu.cpp

    # ── Graph ──
    Source/Graph/NodeComponent.h
    Source/Graph/NodeComponent.cpp
    Source/Graph/ConnectionComponent.h
    Source/Graph/ConnectionComponent.cpp
    Source/Graph/GraphComponent.h
    Source/Graph/GraphComponent.cpp

    # ── Plugin ──
    Source/Plugin/PluginWindow.h
    Source/Plugin/PluginWindow.cpp
    Source/Plugin/PluginManager.h
    Source/Plugin/PluginManager.cpp

    # ── Audio ──
    Source/Audio/AudioDeviceSettings.h
    Source/Audio/AudioDeviceSettings.cpp

    # ── Voicemeeter ──
    Source/Voicemeeter/VoicemeeterRemote.h
    Source/Voicemeeter/VoicemeeterInterface.h
    Source/Voicemeeter/VoicemeeterInterface.cpp

    # ── Localization ──
    Source/Localization/LanguageManager.h
    Source/Localization/LanguageManager.cpp

    # ── Utils ──
    Source/Utils/MathUtils.h
)

target_compile_definitions(VoicemeeterHost PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_WINRT_MIDI=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VoicemeeterHost,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VoicemeeterHost,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_include_directories(VoicemeeterHost PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(VoicemeeterHost PRIVATE
    VoicemeeterHostData
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
